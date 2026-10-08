// dinput8.dll proxy: the game imports DirectInput8Create statically, so placing this DLL next to
// mgsvtpp.exe gets us loaded at startup. Every export forwards to the real System32 dinput8.dll.
#include "common.hpp"
#include "mod.hpp"

#include <mutex>

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#pragma comment(linker, "/EXPORT:DirectInput8Create=proxy_DirectInput8Create")
#pragma comment(linker, "/EXPORT:DllCanUnloadNow=proxy_DllCanUnloadNow,PRIVATE")
#pragma comment(linker, "/EXPORT:DllGetClassObject=proxy_DllGetClassObject,PRIVATE")
#pragma comment(linker, "/EXPORT:DllRegisterServer=proxy_DllRegisterServer,PRIVATE")
#pragma comment(linker, "/EXPORT:DllUnregisterServer=proxy_DllUnregisterServer,PRIVATE")
#pragma comment(linker, "/EXPORT:GetdfDIJoystick=proxy_GetdfDIJoystick")

namespace
{
	HMODULE real_dinput8()
	{
		static const HMODULE module = [] {
			wchar_t path[MAX_PATH] = {};
			GetSystemDirectoryW(path, MAX_PATH);
			const std::wstring full = std::wstring(path) + L"\\dinput8.dll";
			const HMODULE m = LoadLibraryW(full.c_str());
			if (m == nullptr && mod::is_target_process())
				mod::log_error("Falha ao carregar {} (erro {})", mod::to_utf8(full), GetLastError());
			return m;
		}();
		return module;
	}

	template <typename T>
	T real_proc(const char *name)
	{
		const HMODULE m = real_dinput8();
		return m ? reinterpret_cast<T>(GetProcAddress(m, name)) : nullptr;
	}
}

extern "C" HRESULT WINAPI proxy_DirectInput8Create(HINSTANCE hinst, DWORD version, REFIID riid, LPVOID *out, LPUNKNOWN outer)
{
	// First call: the game's main thread, outside the loader lock.
	static std::once_flag once;
	std::call_once(once, [] {
		if (mod::is_target_process())
			mod::start();
	});

	using fn = HRESULT(WINAPI *)(HINSTANCE, DWORD, REFIID, LPVOID *, LPUNKNOWN);
	static const auto real = real_proc<fn>("DirectInput8Create");
	return real ? real(hinst, version, riid, out, outer) : E_FAIL;
}

extern "C" HRESULT WINAPI proxy_DllCanUnloadNow()
{
	using fn = HRESULT(WINAPI *)();
	static const auto real = real_proc<fn>("DllCanUnloadNow");
	return real ? real() : S_FALSE;
}

extern "C" HRESULT WINAPI proxy_DllGetClassObject(REFCLSID clsid, REFIID riid, LPVOID *out)
{
	using fn = HRESULT(WINAPI *)(REFCLSID, REFIID, LPVOID *);
	static const auto real = real_proc<fn>("DllGetClassObject");
	return real ? real(clsid, riid, out) : CLASS_E_CLASSNOTAVAILABLE;
}

extern "C" HRESULT WINAPI proxy_DllRegisterServer()
{
	using fn = HRESULT(WINAPI *)();
	static const auto real = real_proc<fn>("DllRegisterServer");
	return real ? real() : E_FAIL;
}

extern "C" HRESULT WINAPI proxy_DllUnregisterServer()
{
	using fn = HRESULT(WINAPI *)();
	static const auto real = real_proc<fn>("DllUnregisterServer");
	return real ? real() : E_FAIL;
}

extern "C" LPCDIDATAFORMAT WINAPI proxy_GetdfDIJoystick()
{
	using fn = LPCDIDATAFORMAT(WINAPI *)();
	static const auto real = real_proc<fn>("GetdfDIJoystick");
	return real ? real() : nullptr;
}
