#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <cstdint>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>

#define MOD_NAME "MGSV Saves"
#define MOD_VERSION "1.0.0"

namespace mod
{
	enum class log_level { info, warning, error };

	// Log file: <game>\mgsv_saves\mgsv_saves.log (truncated on every launch, written unbuffered).
	void log_init();
	void log_write(log_level level, std::string_view message);

	template <typename... Args>
	void log_info(std::format_string<Args...> fmt, Args &&...args)
	{
		log_write(log_level::info, std::format(fmt, std::forward<Args>(args)...));
	}
	template <typename... Args>
	void log_warning(std::format_string<Args...> fmt, Args &&...args)
	{
		log_write(log_level::warning, std::format(fmt, std::forward<Args>(args)...));
	}
	template <typename... Args>
	void log_error(std::format_string<Args...> fmt, Args &&...args)
	{
		log_write(log_level::error, std::format(fmt, std::forward<Args>(args)...));
	}

	// Folder containing the game exe.
	const std::filesystem::path &game_dir();
	// Mod data folder: <game>\mgsv_saves (created on first use).
	const std::filesystem::path &mod_dir();
	// True only inside mgsvtpp.exe. The proxy DLL is also loaded by mgsvmgo.exe (online), where it must do nothing.
	bool is_target_process();

	// True if the foreground window belongs to the game (hotkeys only fire then).
	bool game_has_focus();

	// While one exists on a thread, its access violations (expected by guarded reads of memory the game may free) are
	// not logged by the exception logger.
	struct quiet_faults
	{
		quiet_faults();
		~quiet_faults();
		quiet_faults(const quiet_faults &) = delete;
		quiet_faults &operator=(const quiet_faults &) = delete;
	};
	bool faults_are_quiet();

	// Return addresses of the callers as " module+offset" (skip: frames above the caller of call_stack to leave out).
	std::string call_stack(unsigned skip = 0, unsigned max_frames = 24);
	// "module+offset" of an address.
	std::string describe_address(const void *address);

	std::string to_utf8(std::wstring_view text);
}
