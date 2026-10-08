#include "mod.hpp"
#include "lua.hpp"

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

		// Remote control for tests: <mod_dir>\command.txt, one command per line, deleted after reading.
		//   lua <code>          runs Lua on the game's main Lua thread (e.g. "lua return vars.missionCode")
		//   luafile <file.lua>  runs <mod_dir>\<file.lua>
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

		void background_loop()
		{
			ULONGLONG last_stats = GetTickCount64();
			for (;;)
			{
				Sleep(250);
				poll_command_file();
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
		lua::install();
		std::thread(background_loop).detach();
	}
}
