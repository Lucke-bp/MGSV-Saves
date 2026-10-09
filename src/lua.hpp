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

	// The exe set up the Lua API object (its vtable pointer is written by the exe's own startup code, after the DLLs
	// are loaded): install() can run.
	bool api_ready();
	bool install();
	// Until the main state is found: puts our vtable back if the exe set its own again.
	void keep_installed();
	// The main Lua state was found: work can be posted.
	bool available();

	// Runs Lua source on the game's main Lua thread. The results (or the error) are logged.
	void run(std::string code, std::string name);

	// Runs a C++ callback on the game's main Lua thread (the stack is restored afterwards).
	void post(std::function<void(lua_State *L)> work);

	// C functions for the mod's scripts: set as globals (with rawset) before every job runs.
	using cfunction = int (*)(lua_State *);
	void register_function(const char *name, cfunction function);

	// Helpers for callbacks and C functions (only valid on the Lua thread).
	std::string arg_string(lua_State *L, int index);
	double arg_number(lua_State *L, int index);
	bool arg_bool(lua_State *L, int index);
	void push_string(lua_State *L, const std::string &text);
	void push_number(lua_State *L, double value);
	void push_bool(lua_State *L, bool value);
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
