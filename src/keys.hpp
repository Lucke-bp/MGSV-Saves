#pragma once

#include "common.hpp"

#include <vector>

// Keyboard and pad, only while the game window has the focus:
// - press: a key press by SendInput, which the game's Raw Input sees like a real one (used to open its pause menu
//   again, with the slot list in it, and to close it).
// - hotkeys: the watched keys are polled every 25 ms; a press runs MgsvSaves.OnKey(vk) on the game's Lua thread.
//   START held for a second on a pad (XInput) runs MgsvSaves.OnKey(k_pad_start_hold).
// - confirmations: when the game shows one of the mod's guarded texts (src/lang.*), MgsvSaves.OnConfirmation(key) runs.
namespace mod::keys
{
	constexpr int k_pad_start_hold = 0x100;

	void start();
	// Presses a key after delay_ms (down, 60 ms, up). False if the game does not have the focus now.
	bool press(int vk, int delay_ms);
	// Replaces the list of watched keys (virtual-key codes).
	void watch(std::vector<int> vks);
	void watch_pad(bool enabled);
}
