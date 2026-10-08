#include "common.hpp"

#include <algorithm>
#include <cwctype>

namespace mod
{
	namespace
	{
		// Append-only handle: each WriteFile is one atomic append, so no mutex is needed
		// (a mutex could deadlock in DLL_PROCESS_DETACH if a killed thread held it).
		HANDLE s_log = INVALID_HANDLE_VALUE;
	}

	void log_init()
	{
		if (s_log != INVALID_HANDLE_VALUE)
			return;
		const std::wstring path = (mod_dir() / L"mgsv_saves.log").wstring();
		const HANDLE truncate = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (truncate != INVALID_HANDLE_VALUE)
			CloseHandle(truncate);
		s_log = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	}

	void log_write(log_level level, std::string_view message)
	{
		if (s_log == INVALID_HANDLE_VALUE)
			return;
		SYSTEMTIME t;
		GetLocalTime(&t);
		const char *tag = level == log_level::error ? "ERROR" : level == log_level::warning ? "WARN " : "INFO ";
		const std::string line = std::format("{:02}:{:02}:{:02}.{:03} [{:5}] {} | {}\r\n",
			t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, GetCurrentThreadId(), tag, message);
		DWORD written = 0;
		WriteFile(s_log, line.data(), static_cast<DWORD>(line.size()), &written, nullptr);
	}

	const std::filesystem::path &game_dir()
	{
		static const std::filesystem::path dir = [] {
			wchar_t path[MAX_PATH] = {};
			GetModuleFileNameW(nullptr, path, MAX_PATH);
			return std::filesystem::path(path).parent_path();
		}();
		return dir;
	}

	const std::filesystem::path &mod_dir()
	{
		static const std::filesystem::path dir = [] {
			std::filesystem::path p = game_dir() / L"mgsv_saves";
			std::error_code ec;
			std::filesystem::create_directories(p, ec);
			return p;
		}();
		return dir;
	}

	bool is_target_process()
	{
		static const bool target = [] {
			wchar_t path[MAX_PATH] = {};
			GetModuleFileNameW(nullptr, path, MAX_PATH);
			std::wstring exe = std::filesystem::path(path).filename().wstring();
			for (wchar_t &c : exe)
				c = static_cast<wchar_t>(std::towlower(c));
			return exe == L"mgsvtpp.exe";
		}();
		return target;
	}

	bool game_has_focus()
	{
		DWORD pid = 0;
		GetWindowThreadProcessId(GetForegroundWindow(), &pid);
		return pid == GetCurrentProcessId();
	}

	std::string describe_address(const void *address)
	{
		HMODULE module = nullptr;
		if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				static_cast<LPCWSTR>(address), &module))
			return std::format("{}", address);
		wchar_t path[MAX_PATH] = {};
		GetModuleFileNameW(module, path, MAX_PATH);
		return std::format("{}+0x{:x}", to_utf8(std::filesystem::path(path).filename().wstring()),
			reinterpret_cast<uintptr_t>(address) - reinterpret_cast<uintptr_t>(module));
	}

	std::string call_stack(unsigned skip, unsigned max_frames)
	{
		void *frames[64] = {};
		const USHORT count = RtlCaptureStackBackTrace(1 + skip, std::min(max_frames, 64u), frames, nullptr);
		std::string s;
		for (USHORT i = 0; i < count; ++i)
			s += " " + describe_address(frames[i]);
		return s;
	}

	std::string to_utf8(std::wstring_view text)
	{
		if (text.empty())
			return {};
		const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
		std::string out(static_cast<size_t>(size), '\0');
		WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), size, nullptr, nullptr);
		return out;
	}
}
