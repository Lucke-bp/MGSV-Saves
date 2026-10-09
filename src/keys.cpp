#include "keys.hpp"
#include "lua.hpp"

#include <map>
#include <mutex>
#include <thread>

namespace mod::keys
{
	namespace
	{
		std::mutex s_mutex;
		std::vector<int> s_watched;

		void send(int vk, bool up)
		{
			INPUT input = {};
			input.type = INPUT_KEYBOARD;
			input.ki.wVk = static_cast<WORD>(vk);
			input.ki.wScan = static_cast<WORD>(MapVirtualKeyW(vk, MAPVK_VK_TO_VSC));
			input.ki.dwFlags = up ? KEYEVENTF_KEYUP : 0;
			SendInput(1, &input, sizeof(input));
		}

		void on_press(int vk)
		{
			if (!lua::available())
				return;
			lua::post([vk](lua::lua_State *L) {
				const std::string error = lua::execute(L,
					std::format("local m=rawget(_G,'MgsvSaves') if m and m.OnKey then m.OnKey({}) end", vk), "=MgsvSaves.OnKey", 0);
				if (!error.empty())
					log_warning("MgsvSaves.OnKey: {}", error);
			});
		}

		void poll()
		{
			std::map<int, bool> down;
			for (;;)
			{
				Sleep(25);
				std::vector<int> watched;
				{
					std::lock_guard lock(s_mutex);
					watched = s_watched;
				}
				if (watched.empty())
					continue;
				const bool focus = game_has_focus();
				for (const int vk : watched)
				{
					const bool now = focus && (GetAsyncKeyState(vk) & 0x8000) != 0;
					bool &was = down[vk];
					if (now && !was)
						on_press(vk);
					was = now;
				}
			}
		}
	}

	void start()
	{
		std::thread(poll).detach();
	}

	bool press(int vk, int delay_ms)
	{
		if (!game_has_focus())
			return false;
		std::thread([vk, delay_ms] {
			Sleep(static_cast<DWORD>(delay_ms));
			if (!game_has_focus())
				return;
			send(vk, false);
			Sleep(60);
			send(vk, true);
		}).detach();
		return true;
	}

	void watch(std::vector<int> vks)
	{
		std::lock_guard lock(s_mutex);
		s_watched = std::move(vks);
	}
}
