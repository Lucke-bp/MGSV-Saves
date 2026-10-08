#pragma once

#include "common.hpp"

namespace mod
{
	// Called once, on the game's main thread, at the first DirectInput8Create (before the game creates its device).
	void start();
}
