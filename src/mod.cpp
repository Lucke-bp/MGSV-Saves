#include "mod.hpp"
#include "keys.hpp"
#include "lang.hpp"
#include "lua.hpp"
#include "script_api.hpp"

#include <atomic>
#include <fstream>
#include <sstream>
#include <thread>
#include <vector>

#pragma comment(lib, "version.lib")

namespace mod
{
	namespace
	{
		constexpr const char *k_supported_version = "1.0.15.4";

		std::string exe_version()
		{
			wchar_t path[MAX_PATH] = {};
			GetModuleFileNameW(nullptr, path, MAX_PATH);
			DWORD handle = 0;
			const DWORD size = GetFileVersionInfoSizeW(path, &handle);
			std::vector<uint8_t> data(size);
			VS_FIXEDFILEINFO *info = nullptr;
			UINT length = 0;
			if (size == 0 || !GetFileVersionInfoW(path, 0, size, data.data()) ||
				!VerQueryValueW(data.data(), L"\\", reinterpret_cast<void **>(&info), &length) || info == nullptr)
				return "?";
			return std::format("{}.{}.{}.{}", HIWORD(info->dwFileVersionMS), LOWORD(info->dwFileVersionMS), HIWORD(info->dwFileVersionLS),
				LOWORD(info->dwFileVersionLS));
		}

		std::string read_file(const std::filesystem::path &path)
		{
			std::ifstream in(path, std::ios::binary);
			std::ostringstream text;
			text << in.rdbuf();
			return text.str();
		}

		// The mod's main script (<mod_dir>\mgsv_saves.lua) is loaded once the game's own scripts are up, then its
		// MgsvSaves.Tick() runs about twice a second on the Lua thread. Only one of these jobs is pending at a time.
		std::atomic<bool> s_main_loaded = false;
		std::atomic<bool> s_tick_pending = false;

		void load_main_script(lua::lua_State *L)
		{
			const std::string code = read_file(mod_dir() / L"mgsv_saves.lua");
			if (code.empty())
			{
				log_error("Script principal ausente: {}", to_utf8((mod_dir() / L"mgsv_saves.lua").wstring()));
				s_main_loaded = true; // nothing to retry
				return;
			}
			const int top = lua::gettop(L);
			const std::string error = lua::execute(L, code, "=mgsv_saves.lua", 1);
			if (error.empty())
				log_info("Script principal carregado: {}", lua::to_text(L, top + 1));
			else
				log_error("Script principal: {}", error);
			s_main_loaded = true;
		}

		// Remote control for tests: <mod_dir>\command.txt, one command per line, deleted after reading.
		//   lua <code>          runs Lua on the game's main Lua thread (e.g. "lua return vars.missionCode")
		//   luafile <file.lua>  runs <mod_dir>\<file.lua>
		//   reload              runs mgsv_saves.lua again
		void poll_command_file()
		{
			const std::filesystem::path path = mod_dir() / L"command.txt";
			std::error_code ec;
			if (!std::filesystem::exists(path, ec))
				return;
			std::vector<std::string> lines;
			{
				std::ifstream in(path);
				for (std::string line; std::getline(in, line);)
					lines.push_back(line);
			}
			std::filesystem::remove(path, ec);
			if (!lines.empty() && lines[0].starts_with("\xEF\xBB\xBF"))
				lines[0].erase(0, 3); // UTF-8 BOM (PowerShell's Set-Content -Encoding utf8)
			for (const std::string &line : lines)
			{
				const size_t space = line.find(' ');
				const std::string verb = line.substr(0, space);
				const std::string rest = space == std::string::npos ? std::string() : line.substr(space + 1);
				if (verb == "lua")
					lua::run(rest, "command.txt");
				else if (verb == "reload") // runs mgsv_saves.lua again (development)
					lua::post([](lua::lua_State *L) { load_main_script(L); });
				else if (verb == "luafile")
				{
					const std::string code = read_file(mod_dir() / rest);
					if (code.empty())
						log_warning("command.txt: arquivo vazio ou inexistente: {}", rest);
					else
						lua::run(code, rest);
				}
				else if (!verb.empty())
					log_warning("command.txt: comando desconhecido '{}'", line);
			}
		}

		void tick()
		{
			if (!lua::available() || s_tick_pending.exchange(true))
				return;
			lua::post([](lua::lua_State *L) {
				if (!s_main_loaded.load())
				{
					// The game's scripts are ready when its main modules exist (globals are read with rawget: _G has
					// a metatable).
					const int top = lua::gettop(L);
					const std::string error = lua::execute(L,
						"return rawget(_G,'TppMission')~=nil and rawget(_G,'TppUiCommand')~=nil and rawget(_G,'TppSave')~=nil",
						"=mgsv_saves_ready", 1);
					if (error.empty() && lua::to_text(L, top + 1) == "true")
						load_main_script(L);
				}
				else
				{
					const std::string error = lua::execute(L,
						"local m=rawget(_G,'MgsvSaves') if m and m.Tick then m.Tick() end", "=MgsvSaves.Tick", 0);
					static int errors = 0;
					if (!error.empty() && errors++ < 10)
						log_warning("MgsvSaves.Tick: {}", error);
				}
				s_tick_pending = false;
			});
		}

		void background_loop()
		{
			ULONGLONG last_stats = GetTickCount64();
			for (;;)
			{
				Sleep(250);
				lua::keep_installed();
				poll_command_file();
				static int ticks = 0;
				if (++ticks % 2 == 0)
					tick();
				const ULONGLONG now = GetTickCount64();
				if (now - last_stats >= 60000)
				{
					const lua::stats s = lua::take_stats();
					if (s.jobs > 0)
						log_info("Lua no ultimo minuto: {} trabalhos do mod ({} ganchos)", s.jobs, s.hooks);
					last_stats = now;
				}
			}
		}
	}

	void start()
	{
		const std::string version = exe_version();
		if (version != k_supported_version)
		{
			log_warning("mgsvtpp.exe {}: versao nao suportada (o mod foi feito para a {}); nada sera alterado", version, k_supported_version);
			return;
		}
		log_info("mgsvtpp.exe {} (suportada)", version);
		// The DLL loads before the exe's own startup code has run: wait for it to set up the Lua API object.
		const ULONGLONG started = GetTickCount64();
		while (!lua::api_ready() && GetTickCount64() - started < 60000)
			Sleep(5);
		if (!lua::install())
			return;
		script_api::install();
		lang::start();
		keys::start();
		std::thread(background_loop).detach();
	}
}
