#include "script_api.hpp"
#include "keys.hpp"
#include "lang.hpp"
#include "lua.hpp"
#include "saves.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>

namespace mod::script_api
{
	namespace
	{
		uint32_t arg_key(lua::lua_State *L, int index)
		{
			return static_cast<uint32_t>(static_cast<int64_t>(lua::arg_number(L, index)));
		}

		// Plain file names only, inside the mod's folder.
		bool valid_name(const std::string &name)
		{
			return !name.empty() && name.size() < 64 && name.find_first_of("/\\:*?\"<>|") == std::string::npos && name.find("..") == std::string::npos;
		}

		int l_set_text(lua::lua_State *L)
		{
			const uint32_t key = arg_key(L, 1);
			const std::string text = lua::arg_string(L, 2);
			if (lua::arg_bool(L, 3))
			{
				lang::set_persistent(key, text);
				lua::push_bool(L, true);
			}
			else
			{
				lua::push_bool(L, lang::set(key, text));
			}
			return 1;
		}

		int l_restore_text(lua::lua_State *L)
		{
			lang::restore(arg_key(L, 1));
			return 0;
		}

		int l_get_text(lua::lua_State *L)
		{
			lua::push_string(L, lang::original(arg_key(L, 1)));
			return 1;
		}

		int l_read_file(lua::lua_State *L)
		{
			const std::string name = lua::arg_string(L, 1);
			std::string text;
			if (valid_name(name))
			{
				std::ifstream in(mod_dir() / name, std::ios::binary);
				std::ostringstream content;
				content << in.rdbuf();
				text = content.str();
			}
			lua::push_string(L, text);
			return 1;
		}

		int l_write_file(lua::lua_State *L)
		{
			const std::string name = lua::arg_string(L, 1);
			const std::string text = lua::arg_string(L, 2);
			bool ok = false;
			if (valid_name(name))
			{
				// Written next to the target and renamed over it, so a crash never leaves half a file.
				const std::filesystem::path path = mod_dir() / name;
				std::filesystem::path temporary = path;
				temporary += L".tmp";
				{
					std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
					out << text;
					ok = static_cast<bool>(out);
				}
				std::error_code ec;
				if (ok)
					std::filesystem::rename(temporary, path, ec);
				ok = ok && !ec;
			}
			lua::push_bool(L, ok);
			return 1;
		}

		int l_now(lua::lua_State *L)
		{
			SYSTEMTIME t;
			GetLocalTime(&t);
			lua::push_string(L, std::format("{:02}/{:02}/{:04} {:02}:{:02}", t.wDay, t.wMonth, t.wYear, t.wHour, t.wMinute));
			return 1;
		}

		int l_press_key(lua::lua_State *L)
		{
			const int vk = static_cast<int>(lua::arg_number(L, 1));
			const int delay = std::clamp(static_cast<int>(lua::arg_number(L, 2)), 0, 5000);
			lua::push_bool(L, vk > 0 && vk < 256 && keys::press(vk, delay));
			return 1;
		}

		int l_watch_keys(lua::lua_State *L)
		{
			std::vector<int> vks;
			for (int i = 1; i <= lua::gettop(L); ++i)
			{
				const int vk = static_cast<int>(lua::arg_number(L, i));
				if (vk > 0 && vk < 256)
					vks.push_back(vk);
			}
			keys::watch(std::move(vks));
			return 0;
		}

		int l_backup_saves(lua::lua_State *L)
		{
			lua::push_string(L, saves::backup());
			return 1;
		}

		int l_remove_slot(lua::lua_State *L)
		{
			lua::push_bool(L, saves::remove_slot(lua::arg_string(L, 1)));
			return 1;
		}
	}

	void install()
	{
		lua::register_function("mgsv_set_text", &l_set_text);
		lua::register_function("mgsv_restore_text", &l_restore_text);
		lua::register_function("mgsv_get_text", &l_get_text);
		lua::register_function("mgsv_read_file", &l_read_file);
		lua::register_function("mgsv_write_file", &l_write_file);
		lua::register_function("mgsv_now", &l_now);
		lua::register_function("mgsv_press_key", &l_press_key);
		lua::register_function("mgsv_watch_keys", &l_watch_keys);
		lua::register_function("mgsv_backup_saves", &l_backup_saves);
		lua::register_function("mgsv_remove_slot", &l_remove_slot);
	}
}
