// msacm32.dll proxy: the game imports MSACM32.dll (audio compression manager) statically, so this DLL, placed next to
// mgsvtpp.exe, is loaded at startup. Its exports are the real System32 msacm32.dll's (same names and ordinals), each a
// thunk in msacm32_thunks.asm that jumps to the real function, resolved on its first call (never inside DllMain).
// Not dinput8.dll: that one is IHHook's (Infinite Heaven) and tpp-mod's, and dxgi/d3d11 are ReShade's.
#include "common.hpp"

#include <atomic>

#pragma comment(linker, "/EXPORT:acmDriverAddA=proxy_acmDriverAddA,@1")
#pragma comment(linker, "/EXPORT:acmDriverAddW=proxy_acmDriverAddW,@2")
#pragma comment(linker, "/EXPORT:acmDriverClose=proxy_acmDriverClose,@3")
#pragma comment(linker, "/EXPORT:acmDriverDetailsA=proxy_acmDriverDetailsA,@4")
#pragma comment(linker, "/EXPORT:acmDriverDetailsW=proxy_acmDriverDetailsW,@5")
#pragma comment(linker, "/EXPORT:acmDriverEnum=proxy_acmDriverEnum,@6")
#pragma comment(linker, "/EXPORT:acmDriverID=proxy_acmDriverID,@7")
#pragma comment(linker, "/EXPORT:acmDriverMessage=proxy_acmDriverMessage,@8")
#pragma comment(linker, "/EXPORT:acmDriverOpen=proxy_acmDriverOpen,@9")
#pragma comment(linker, "/EXPORT:acmDriverPriority=proxy_acmDriverPriority,@10")
#pragma comment(linker, "/EXPORT:acmDriverRemove=proxy_acmDriverRemove,@11")
#pragma comment(linker, "/EXPORT:acmFilterChooseA=proxy_acmFilterChooseA,@12")
#pragma comment(linker, "/EXPORT:acmFilterChooseW=proxy_acmFilterChooseW,@13")
#pragma comment(linker, "/EXPORT:acmFilterDetailsA=proxy_acmFilterDetailsA,@14")
#pragma comment(linker, "/EXPORT:acmFilterDetailsW=proxy_acmFilterDetailsW,@15")
#pragma comment(linker, "/EXPORT:acmFilterEnumA=proxy_acmFilterEnumA,@16")
#pragma comment(linker, "/EXPORT:acmFilterEnumW=proxy_acmFilterEnumW,@17")
#pragma comment(linker, "/EXPORT:acmFilterTagDetailsA=proxy_acmFilterTagDetailsA,@18")
#pragma comment(linker, "/EXPORT:acmFilterTagDetailsW=proxy_acmFilterTagDetailsW,@19")
#pragma comment(linker, "/EXPORT:acmFilterTagEnumA=proxy_acmFilterTagEnumA,@20")
#pragma comment(linker, "/EXPORT:acmFilterTagEnumW=proxy_acmFilterTagEnumW,@21")
#pragma comment(linker, "/EXPORT:acmFormatChooseA=proxy_acmFormatChooseA,@22")
#pragma comment(linker, "/EXPORT:acmFormatChooseW=proxy_acmFormatChooseW,@23")
#pragma comment(linker, "/EXPORT:acmFormatDetailsA=proxy_acmFormatDetailsA,@24")
#pragma comment(linker, "/EXPORT:acmFormatDetailsW=proxy_acmFormatDetailsW,@25")
#pragma comment(linker, "/EXPORT:acmFormatEnumA=proxy_acmFormatEnumA,@26")
#pragma comment(linker, "/EXPORT:acmFormatEnumW=proxy_acmFormatEnumW,@27")
#pragma comment(linker, "/EXPORT:acmFormatSuggest=proxy_acmFormatSuggest,@28")
#pragma comment(linker, "/EXPORT:acmFormatTagDetailsA=proxy_acmFormatTagDetailsA,@29")
#pragma comment(linker, "/EXPORT:acmFormatTagDetailsW=proxy_acmFormatTagDetailsW,@30")
#pragma comment(linker, "/EXPORT:acmFormatTagEnumA=proxy_acmFormatTagEnumA,@31")
#pragma comment(linker, "/EXPORT:acmFormatTagEnumW=proxy_acmFormatTagEnumW,@32")
#pragma comment(linker, "/EXPORT:acmGetVersion=proxy_acmGetVersion,@33")
#pragma comment(linker, "/EXPORT:acmMetrics=proxy_acmMetrics,@34")
#pragma comment(linker, "/EXPORT:acmStreamClose=proxy_acmStreamClose,@35")
#pragma comment(linker, "/EXPORT:acmStreamConvert=proxy_acmStreamConvert,@36")
#pragma comment(linker, "/EXPORT:acmStreamMessage=proxy_acmStreamMessage,@37")
#pragma comment(linker, "/EXPORT:acmStreamOpen=proxy_acmStreamOpen,@38")
#pragma comment(linker, "/EXPORT:acmStreamPrepareHeader=proxy_acmStreamPrepareHeader,@39")
#pragma comment(linker, "/EXPORT:acmStreamReset=proxy_acmStreamReset,@40")
#pragma comment(linker, "/EXPORT:acmStreamSize=proxy_acmStreamSize,@41")
#pragma comment(linker, "/EXPORT:acmStreamUnprepareHeader=proxy_acmStreamUnprepareHeader,@42")

namespace
{
	// In the order of the thunks (msacm32_thunks.asm).
	constexpr const char *k_msacm32_names[] = {
		"acmDriverAddA",
		"acmDriverAddW",
		"acmDriverClose",
		"acmDriverDetailsA",
		"acmDriverDetailsW",
		"acmDriverEnum",
		"acmDriverID",
		"acmDriverMessage",
		"acmDriverOpen",
		"acmDriverPriority",
		"acmDriverRemove",
		"acmFilterChooseA",
		"acmFilterChooseW",
		"acmFilterDetailsA",
		"acmFilterDetailsW",
		"acmFilterEnumA",
		"acmFilterEnumW",
		"acmFilterTagDetailsA",
		"acmFilterTagDetailsW",
		"acmFilterTagEnumA",
		"acmFilterTagEnumW",
		"acmFormatChooseA",
		"acmFormatChooseW",
		"acmFormatDetailsA",
		"acmFormatDetailsW",
		"acmFormatEnumA",
		"acmFormatEnumW",
		"acmFormatSuggest",
		"acmFormatTagDetailsA",
		"acmFormatTagDetailsW",
		"acmFormatTagEnumA",
		"acmFormatTagEnumW",
		"acmGetVersion",
		"acmMetrics",
		"acmStreamClose",
		"acmStreamConvert",
		"acmStreamMessage",
		"acmStreamOpen",
		"acmStreamPrepareHeader",
		"acmStreamReset",
		"acmStreamSize",
		"acmStreamUnprepareHeader",
	};
	constexpr size_t k_msacm32_count = sizeof(k_msacm32_names) / sizeof(k_msacm32_names[0]);
}

extern "C" void *msacm32_procs[k_msacm32_count]; // msacm32_thunks.asm

// Called by a thunk whose function is not resolved yet: loads the real DLL and fills the whole table.
extern "C" void *msacm32_resolve(unsigned index)
{
	static std::atomic<bool> logged = false;
	static const HMODULE real = [] {
		wchar_t path[MAX_PATH] = {};
		GetSystemDirectoryW(path, MAX_PATH);
		const std::wstring full = std::wstring(path) + L"\\msacm32.dll";
		return LoadLibraryW(full.c_str());
	}();
	if (real == nullptr)
	{
		if (!logged.exchange(true) && mod::is_target_process())
			mod::log_error("Falha ao carregar o msacm32.dll do sistema (erro {})", GetLastError());
		return nullptr;
	}
	for (size_t i = 0; i < k_msacm32_count; ++i)
		if (msacm32_procs[i] == nullptr)
			InterlockedExchangePointer(&msacm32_procs[i], reinterpret_cast<void *>(GetProcAddress(real, k_msacm32_names[i])));
	return index < k_msacm32_count ? msacm32_procs[index] : nullptr;
}
