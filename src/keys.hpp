#pragma once

#include "common.hpp"

#include <vector>

// Keyboard, only while the game window has the focus:
// - press: a key press by SendInput, which the game's Raw Input sees like a real one (used to open its pause menu
//   again, with the slot list in it).
// - hotkeys: the watched keys are polled every 25 ms; a press runs MgsvSaves.OnKey(vk) on the game's Lua thread.
namespace mod::keys
{
	void start();
	// Presses a key after delay_ms (down, 60 ms, up). False if the game does not have the focus now.
	bool press(int vk, int delay_ms);
	// Replaces the list of watched keys (virtual-key codes).
	void watch(std::vector<int> vks);
}
