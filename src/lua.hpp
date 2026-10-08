#pragma once

#include "common.hpp"

#include <functional>

// Access to the game's Lua (5.1, linked into the exe), without touching its code.
// - The main lua_State: the engine calls the Lua API through a wrapper object, a static instance (mgsvtpp+0x2A97488)
//   that only holds a vtable pointer, each slot forwarding to one lua_* function. The mod points that instance at a
//   copy of the vtable whose getfield/getglobal slots note the state they get (used thousands of times at startup).
// - Running code on the Lua thread: lua_sethook with a count hook on the main state (safe from another thread); at
//   the main thread's next Lua instruction the hook runs the queued work and removes itself.
// Addresses are for mgsvtpp.exe 1.0.15.4 and their code is checked before anything is touched.
namespace mod::lua
{
	struct lua_State;

	bool install();
	// The main Lua state was found: work can be posted.
	bool available();

	// Runs Lua source on the game's main Lua thread. The results (or the error) are logged.
	void run(std::string code, std::string name);

	// Runs a C++ callback on the game's main Lua thread (the stack is restored afterwards).
	void post(std::function<void(lua_State *L)> work);

	// Helpers for callbacks (only valid on the Lua thread).
	int gettop(lua_State *L);
	void settop(lua_State *L, int index);
	// Loads and runs a chunk; returns the error message, or an empty string on success. Results stay on the stack.
	std::string execute(lua_State *L, const std::string &code, const std::string &name, int results);
	// A value on the stack as text (strings, numbers, booleans; the type name otherwise).
	std::string to_text(lua_State *L, int index);

	struct stats
	{
		uint64_t hooks = 0; // times the count hook ran
		uint64_t jobs = 0;  // work items run
	};
	stats take_stats();
}
