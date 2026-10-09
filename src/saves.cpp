#include "saves.hpp"

#include <algorithm>
#include <cctype>
#include <mutex>
#include <vector>

namespace mod::saves
{
	namespace
	{
		constexpr size_t k_kept_backups = 30;
		const wchar_t *const k_game_files[] = { L"TPP_GAME_DATA0", L"TPP_GAME_DATA1", L"PERSONAL_DATA0", L"PERSONAL_DATA1" };

		std::mutex s_mutex;

		std::filesystem::path steam_path()
		{
			wchar_t value[MAX_PATH] = {};
			DWORD size = sizeof(value);
			if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath", RRF_RT_REG_SZ, nullptr, value, &size) != ERROR_SUCCESS)
				return {};
			return std::filesystem::path(value).make_preferred();
		}

		bool has_game_save(const std::filesystem::path &dir)
		{
			std::error_code ec;
			return std::filesystem::exists(dir / L"TPP_GAME_DATA0", ec) || std::filesystem::exists(dir / L"TPP_GAME_DATA1", ec);
		}

		std::filesystem::path find_folder()
		{
			const std::filesystem::path userdata = steam_path() / L"userdata";
			if (userdata == L"userdata")
				return {};
			DWORD user = 0;
			DWORD size = sizeof(user);
			if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam\\ActiveProcess", L"ActiveUser", RRF_RT_REG_DWORD, nullptr, &user, &size) ==
					ERROR_SUCCESS &&
				user != 0)
			{
				const std::filesystem::path dir = userdata / std::to_wstring(user) / L"287700" / L"local";
				if (has_game_save(dir))
					return dir;
			}
			// No signed-in account in the registry: the account that saved last.
			std::filesystem::path best;
			std::filesystem::file_time_type newest{};
			std::error_code ec;
			for (const auto &account : std::filesystem::directory_iterator(userdata, ec))
			{
				const std::filesystem::path dir = account.path() / L"287700" / L"local";
				for (const wchar_t *name : { L"TPP_GAME_DATA0", L"TPP_GAME_DATA1" })
				{
					const auto time = std::filesystem::last_write_time(dir / name, ec);
					if (!ec && (best.empty() || time > newest))
					{
						best = dir;
						newest = time;
					}
				}
			}
			return best;
		}

		std::wstring timestamp()
		{
			SYSTEMTIME t;
			GetLocalTime(&t);
			return std::format(L"{:04}-{:02}-{:02}_{:02}-{:02}-{:02}", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
		}

		bool valid_slot_name(const std::string &name)
		{
			return name.starts_with("MGSV_SAVES_") && name.size() < 40 &&
				std::all_of(name.begin(), name.end(), [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; });
		}
	}

	std::filesystem::path folder()
	{
		std::lock_guard lock(s_mutex);
		static std::filesystem::path found;
		if (found.empty())
		{
			found = find_folder();
			if (found.empty())
				log_warning("Pasta de saves do jogo nao encontrada");
			else
				log_info("Pasta de saves do jogo: {}", to_utf8(found.wstring()));
		}
		return found;
	}

	std::string backup()
	{
		const std::filesystem::path source = folder();
		if (source.empty())
			return {};
		const std::filesystem::path root = mod_dir() / L"backups" / L"auto";
		const std::wstring name = timestamp();
		std::error_code ec;
		std::filesystem::create_directories(root / name, ec);
		if (ec)
		{
			log_error("Backup: nao foi possivel criar {}: {}", to_utf8((root / name).wstring()), ec.message());
			return {};
		}
		int copied = 0;
		for (const wchar_t *file : k_game_files)
		{
			if (!std::filesystem::exists(source / file, ec))
				continue;
			std::filesystem::copy_file(source / file, root / name / file, std::filesystem::copy_options::overwrite_existing, ec);
			if (ec)
			{
				log_error("Backup: falha ao copiar {}: {}", to_utf8(file), ec.message());
				return {};
			}
			++copied;
		}
		if (copied == 0)
		{
			log_error("Backup: nenhum arquivo de save em {}", to_utf8(source.wstring()));
			return {};
		}
		// Only the mod's own automatic copies are pruned (names sort by date).
		std::vector<std::filesystem::path> copies;
		for (const auto &entry : std::filesystem::directory_iterator(root, ec))
			if (entry.is_directory())
				copies.push_back(entry.path());
		std::sort(copies.begin(), copies.end());
		for (size_t i = 0; i + k_kept_backups < copies.size(); ++i)
			std::filesystem::remove_all(copies[i], ec);
		log_info("Backup dos saves do jogo: backups\\auto\\{} ({} arquivos)", to_utf8(name), copied);
		return to_utf8(name);
	}

	bool remove_slot(const std::string &name)
	{
		const std::filesystem::path source = folder();
		if (source.empty() || !valid_slot_name(name))
			return false;
		const std::filesystem::path target = mod_dir() / L"backups" / L"apagados";
		std::error_code ec;
		std::filesystem::create_directories(target, ec);
		const std::wstring stamp = timestamp();
		bool ok = true;
		for (const wchar_t *suffix : { L"0", L"1" })
		{
			const std::filesystem::path file = source / (std::filesystem::path(name).wstring() + suffix);
			if (!std::filesystem::exists(file, ec))
				continue;
			const std::filesystem::path moved = target / (std::filesystem::path(name).wstring() + suffix + L"_" + stamp);
			std::filesystem::copy_file(file, moved, std::filesystem::copy_options::overwrite_existing, ec);
			if (!ec)
				std::filesystem::remove(file, ec);
			if (ec)
			{
				log_error("Apagar slot: falha com {}: {}", to_utf8(file.wstring()), ec.message());
				ok = false;
			}
		}
		if (ok)
			log_info("Slot {} movido para backups\\apagados", name);
		return ok;
	}
}
