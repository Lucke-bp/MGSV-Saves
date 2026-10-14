#include "keys.hpp"
#include "lang.hpp"
#include "lua.hpp"

#include <atomic>
#include <map>
#include <mutex>
#include <thread>

namespace mod::keys
{
	namespace
	{
		constexpr ULONGLONG k_hold_ms = 1000;      // START held this long: a "hold"
		constexpr ULONGLONG k_rearm_ms = 1500;     // after a confirmation of ours showed, its guard comes back
		constexpr WORD k_pad_start = 0x0010;       // XINPUT_GAMEPAD_START

		std::mutex s_mutex;
		std::vector<int> s_watched;
		std::atomic<bool> s_watch_pad = false;

		struct pad_state
		{
			DWORD packet;
			WORD buttons;
			BYTE left_trigger, right_trigger;
			SHORT thumbs[4];
		};
		using xinput_get_state = DWORD(WINAPI *)(DWORD, pad_state *);

		void send(int vk, bool up)
		{
			INPUT input = {};
			input.type = INPUT_KEYBOARD;
			input.ki.wVk = static_cast<WORD>(vk);
			input.ki.wScan = static_cast<WORD>(MapVirtualKeyW(vk, MAPVK_VK_TO_VSC));
			input.ki.dwFlags = up ? KEYEVENTF_KEYUP : 0;
			SendInput(1, &input, sizeof(input));
		}

		void tap(int vk)
		{
			send(vk, false);
			Sleep(60);
			send(vk, true);
		}

		void on_press(int code)
		{
			if (!lua::available())
				return;
			lua::post([code](lua::lua_State *L) {
				const std::string error = lua::execute(L,
					std::format("local m=rawget(_G,'MgsvSaves') if m and m.OnKey then m.OnKey({}) end", code), "=MgsvSaves.OnKey", 0);
				if (!error.empty())
					log_warning("MgsvSaves.OnKey: {}", error);
			});
		}

		// A confirmation of ours is on screen (src/lang.*: its text is in the guarded page): the script decides
		// (MgsvSaves.OnConfirmation). Answering it from code does not work: a popup closed by code counts as NO, and the
		// game ignores the keyboard while a pad drives its menus.
		void on_confirmation(uint32_t key)
		{
			if (!lua::available())
				return;
			lua::post([key](lua::lua_State *L) {
				const std::string error = lua::execute(L,
					std::format("local m=rawget(_G,'MgsvSaves') if m and m.OnConfirmation then m.OnConfirmation({}) end", key),
					"=MgsvSaves.OnConfirmation", 0);
				if (!error.empty())
					log_warning("MgsvSaves.OnConfirmation: {}", error);
			});
		}

		// The game's own XInput (already loaded by it): reading a pad's state does not take anything from the game.
		xinput_get_state get_state()
		{
			static const xinput_get_state function = [] {
				HMODULE module = GetModuleHandleW(L"xinput1_3.dll");
				if (module == nullptr)
					module = LoadLibraryW(L"xinput1_3.dll");
				return module ? reinterpret_cast<xinput_get_state>(GetProcAddress(module, "XInputGetState")) : nullptr;
			}();
			return function;
		}

		void poll()
		{
			std::map<int, bool> down;
			ULONGLONG start_down_since = 0;
			bool hold_sent = false;
			ULONGLONG rearm_at = 0;
			for (;;)
			{
				Sleep(25);
				const ULONGLONG now = GetTickCount64();
				const bool focus = game_has_focus();

				if (const uint32_t key = lang::take_guard_hit())
				{
					log_info("Confirmacao do mod na tela (chave {:08X})", key);
					on_confirmation(key);
					rearm_at = GetTickCount64() + k_rearm_ms;
				}
				if (rearm_at != 0 && now >= rearm_at)
				{
					lang::arm_guard();
					rearm_at = 0;
				}

				std::vector<int> watched;
				{
					std::lock_guard lock(s_mutex);
					watched = s_watched;
				}
				for (const int vk : watched)
				{
					const bool now_down = focus && (GetAsyncKeyState(vk) & 0x8000) != 0;
					bool &was = down[vk];
					if (now_down && !was)
						on_press(vk);
					was = now_down;
				}

				// START held on a pad (any of the four)
				bool start = false;
				if (s_watch_pad && focus)
					if (const xinput_get_state state = get_state())
						for (DWORD pad = 0; pad < 4 && !start; ++pad)
						{
							pad_state s = {};
							if (state(pad, &s) == ERROR_SUCCESS && (s.buttons & k_pad_start) != 0)
								start = true;
						}
				if (!start)
				{
					start_down_since = 0;
					hold_sent = false;
				}
				else if (start_down_since == 0)
				{
					start_down_since = now;
				}
				else if (!hold_sent && now - start_down_since >= k_hold_ms)
				{
					hold_sent = true;
					on_press(k_pad_start_hold);
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
			if (game_has_focus())
				tap(vk);
		}).detach();
		return true;
	}

	void watch(std::vector<int> vks)
	{
		std::lock_guard lock(s_mutex);
		s_watched = std::move(vks);
	}

	void watch_pad(bool enabled)
	{
		s_watch_pad = enabled;
	}

}
