#pragma once

#include "common.hpp"

namespace mod
{
	// Runs once, on a thread of its own started when the DLL loads (only in mgsvtpp.exe); waits for the exe's startup.
	void start();
}
