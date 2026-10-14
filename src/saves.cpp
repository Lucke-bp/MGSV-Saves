#include "saves.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <mutex>
#include <vector>

namespace mod::saves
{
	namespace
	{
		constexpr size_t k_kept_backups = 30;
		constexpr size_t k_kept_deleted = 90; // files in backups\apagados (3 per deleted slot)
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

		struct file_info
		{
			bool exists = false;
			uint64_t size = 0;
			FILETIME written = {};
			FILETIME created = {};
		};

		file_info info(const std::filesystem::path &path)
		{
			file_info result;
			WIN32_FILE_ATTRIBUTE_DATA data;
			if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data))
			{
				result.exists = true;
				result.size = (uint64_t(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
				result.written = data.ftLastWriteTime;
				result.created = data.ftCreationTime;
			}
			return result;
		}

		uint64_t ticks(const FILETIME &t)
		{
			return (uint64_t(t.dwHighDateTime) << 32) | t.dwLowDateTime;
		}

		// The newer of a save's two local copies (<name>0, <name>1).
		file_info newest_copy(const std::filesystem::path &dir, const std::wstring &name)
		{
			const file_info a = info(dir / (name + L"0"));
			const file_info b = info(dir / (name + L"1"));
			if (!a.exists)
				return b;
			if (!b.exists)
				return a;
			return ticks(b.written) > ticks(a.written) ? b : a;
		}

		std::string local_date(const FILETIME &t)
		{
			FILETIME local;
			SYSTEMTIME s;
			if (!FileTimeToLocalFileTime(&t, &local) || !FileTimeToSystemTime(&local, &s))
				return "?";
			return std::format("{:02}/{:02}/{:04} {:02}:{:02}", s.wDay, s.wMonth, s.wYear, s.wHour, s.wMinute);
		}

		// TPP keeps its Steam Cloud files in Ground Zeroes' app (311340: "appidredirect" in Steam's appinfo).
		std::vector<std::filesystem::path> cloud_folders(const std::filesystem::path &local)
		{
			const std::filesystem::path account = local.parent_path().parent_path();
			return { account / L"311340" / L"remote", account / L"287700" / L"remote" };
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

	bool backup_slot(const std::string &name)
	{
		const std::filesystem::path source = folder();
		if (source.empty() || !valid_slot_name(name))
			return false;
		const std::filesystem::path target = mod_dir() / L"backups" / L"apagados";
		std::error_code ec;
		std::filesystem::create_directories(target, ec);
		const std::wstring stamp = timestamp();
		const std::wstring wide = std::filesystem::path(name).wstring();
		// The game's two local copies, and its Steam Cloud copy.
		std::vector<std::filesystem::path> files = { source / (wide + L"0"), source / (wide + L"1") };
		for (const std::filesystem::path &cloud : cloud_folders(source))
			files.push_back(cloud / wide);
		bool ok = true;
		int copied = 0;
		for (const std::filesystem::path &file : files)
		{
			if (!std::filesystem::exists(file, ec))
				continue;
			const std::wstring kind = file.parent_path().filename() == L"remote" ? L"_nuvem_" : L"_";
			std::filesystem::copy_file(file, target / (file.filename().wstring() + kind + stamp), std::filesystem::copy_options::overwrite_existing, ec);
			if (ec)
			{
				log_error("Backup do slot: falha com {}: {}", to_utf8(file.wstring()), ec.message());
				ok = false;
			}
			else
			{
				++copied;
			}
		}
		// Only the newest copies are kept (the oldest go first, by when they were copied here).
		std::vector<std::pair<uint64_t, std::filesystem::path>> kept;
		for (const auto &entry : std::filesystem::directory_iterator(target, ec))
			if (entry.is_regular_file())
				kept.emplace_back(ticks(info(entry.path()).created), entry.path());
		std::sort(kept.begin(), kept.end());
		for (size_t i = 0; i + k_kept_deleted < kept.size(); ++i)
			std::filesystem::remove(kept[i].second, ec);
		if (ok)
			log_info("Slot {}: {} arquivo(s) copiados para backups\\apagados", name, copied);
		return ok;
	}

	std::string list_slots()
	{
		const std::filesystem::path source = folder();
		if (source.empty())
			return {};
		std::map<std::string, uint64_t> found; // id -> newest write time
		std::map<std::string, FILETIME> times;
		const auto note = [&](const std::string &id, const FILETIME &t) {
			if (id.empty() || !std::all_of(id.begin(), id.end(), [](char c) { return std::isalnum(static_cast<unsigned char>(c)); }))
				return;
			if (!found.contains(id) || ticks(t) > found[id])
			{
				found[id] = ticks(t);
				times[id] = t;
			}
		};
		std::error_code ec;
		for (const auto &entry : std::filesystem::directory_iterator(source, ec))
		{
			const std::string name = to_utf8(entry.path().filename().wstring());
			if (name.starts_with("MGSV_SAVES_") && name.size() > 12 && (name.back() == '0' || name.back() == '1'))
				note(name.substr(11, name.size() - 12), info(entry.path()).written);
		}
		for (const std::filesystem::path &cloud : cloud_folders(source))
			for (const auto &entry : std::filesystem::directory_iterator(cloud, ec))
			{
				const std::string name = to_utf8(entry.path().filename().wstring());
				if (name.starts_with("MGSV_SAVES_") && name.size() > 11)
					note(name.substr(11), info(entry.path()).written);
			}
		std::string lines;
		for (const auto &[id, t] : times)
			lines += std::format("{}|{}\n", id, local_date(t));
		return lines;
	}

	bool slot_readable(const std::string &name)
	{
		const std::filesystem::path source = folder();
		if (source.empty() || !valid_slot_name(name))
			return false;
		const file_info slot = newest_copy(source, std::filesystem::path(name).wstring());
		const file_info game = newest_copy(source, L"TPP_GAME_DATA");
		return slot.exists && game.exists && slot.size == game.size;
	}
}
