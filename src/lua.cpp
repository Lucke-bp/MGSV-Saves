#include "lua.hpp"

#include <atomic>
#include <cstring>
#include <deque>
#include <mutex>

namespace mod::lua
{
	namespace
	{
		// mgsvtpp.exe 1.0.15.4 (offsets from the image base)
		constexpr uintptr_t k_api_object = 0x2A97488; // static instance of the engine's Lua API wrapper (vtable pointer only)
		constexpr uintptr_t k_api_vtable = 0x2305720;
		constexpr size_t k_api_slots = 134;           // up to the next vtable
		constexpr size_t k_slot_getfield = 11;        // thunk at +0xBF2F20 -> lua_getfield(L, index, key)
		constexpr size_t k_slot_getglobal = 12;       // thunk at +0xBF2F30 -> lua_getfield(L, LUA_GLOBALSINDEX, name)

		constexpr uintptr_t k_lua_pcall = 0x1A116C0;
		constexpr uintptr_t k_luaL_loadbuffer = 0x1A178E0;
		constexpr uintptr_t k_lua_gettop = 0x1A112E0;
		constexpr uintptr_t k_lua_settop = 0x1A11F70;
		constexpr uintptr_t k_lua_tolstring = 0x1A12150;
		constexpr uintptr_t k_lua_type = 0x1A12300;
		constexpr uintptr_t k_lua_typename = 0x1A12330;
		constexpr uintptr_t k_lua_toboolean = 0x1A120C0;
		constexpr uintptr_t k_lua_sethook = 0x1A20DC0;
		constexpr uintptr_t k_api_getfield = 0xBF2F20;
		constexpr uintptr_t k_api_getglobal = 0xBF2F30;

		struct code_check
		{
			const char *name;
			uintptr_t rva;
			uint8_t bytes[12];
		};
		constexpr code_check k_checks[] = {
			{ "lua_pcall", k_lua_pcall, { 0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x40, 0x41, 0x8B } },
			{ "luaL_loadbuffer", k_luaL_loadbuffer, { 0x48, 0x83, 0xEC, 0x38, 0x48, 0x89, 0x54, 0x24, 0x20, 0x4C, 0x89, 0x44 } },
			{ "lua_gettop", k_lua_gettop, { 0x48, 0x8B, 0x41, 0x10, 0x48, 0x2B, 0x41, 0x18, 0x48, 0xC1, 0xF8, 0x04 } },
			{ "lua_settop", k_lua_settop, { 0x85, 0xD2, 0x78, 0x42, 0x4C, 0x63, 0xC2, 0x48, 0x8B, 0x51, 0x18, 0x49 } },
			{ "lua_tolstring", k_lua_tolstring, { 0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10, 0x57, 0x48 } },
			{ "lua_type", k_lua_type, { 0x48, 0x83, 0xEC, 0x28, 0xE8, 0x97, 0x13, 0x00, 0x00, 0x48, 0x8D, 0x0D } },
			{ "lua_typename", k_lua_typename, { 0x83, 0xFA, 0xFF, 0x75, 0x08, 0x48, 0x8D, 0x05, 0x0C, 0x25, 0xAA, 0x00 } },
			{ "lua_toboolean", k_lua_toboolean, { 0x48, 0x83, 0xEC, 0x28, 0xE8, 0xD7, 0x15, 0x00, 0x00, 0x8B, 0x48, 0x08 } },
			{ "lua_sethook", k_lua_sethook, { 0x41, 0x8B, 0xC0, 0x48, 0x85, 0xD2, 0x74, 0x04, 0x85, 0xC0, 0x75, 0x04 } },
			{ "api_getfield", k_api_getfield, { 0x41, 0x8B, 0xC0, 0x48, 0x8B, 0xCA, 0x4D, 0x8B, 0xC1, 0x8B, 0xD0, 0xE9 } },
			{ "api_getglobal", k_api_getglobal, { 0x48, 0x8B, 0xCA, 0xBA, 0xEE, 0xD8, 0xFF, 0xFF, 0xE9, 0xA3, 0xE2, 0xE1 } },
		};

		// Lua 5.1 layout (lua_close reads l_G->mainthread at +0xC0)
		constexpr size_t k_state_global = 0x20;      // lua_State::l_G
		constexpr size_t k_global_mainthread = 0xC0; // global_State::mainthread
		constexpr int k_multret = -1;
		constexpr int k_mask_count = 1 << 3;         // LUA_MASKCOUNT
		constexpr int k_tnil = 0, k_tboolean = 1, k_tnumber = 3, k_tstring = 4;

		struct lua_Debug;
		using hook_fn = void (*)(lua_State *, lua_Debug *);
		using pcall_fn = int (*)(lua_State *, int, int, int);
		using loadbuffer_fn = int (*)(lua_State *, const char *, size_t, const char *);
		using gettop_fn = int (*)(lua_State *);
		using settop_fn = void (*)(lua_State *, int);
		using tolstring_fn = const char *(*)(lua_State *, int, size_t *);
		using type_fn = int (*)(lua_State *, int);
		using typename_fn = const char *(*)(lua_State *, int);
		using toboolean_fn = int (*)(lua_State *, int);
		using sethook_fn = int (*)(lua_State *, hook_fn, int, int);
		using api_getfield_fn = void (*)(void *self, lua_State *, int, const char *);
		using api_getglobal_fn = void (*)(void *self, lua_State *, const char *);

		pcall_fn s_pcall = nullptr;
		loadbuffer_fn s_loadbuffer = nullptr;
		gettop_fn s_gettop = nullptr;
		settop_fn s_settop = nullptr;
		tolstring_fn s_tolstring = nullptr;
		type_fn s_type = nullptr;
		typename_fn s_typename = nullptr;
		toboolean_fn s_toboolean = nullptr;
		sethook_fn s_sethook = nullptr;
		api_getfield_fn s_original_getfield = nullptr;
		api_getglobal_fn s_original_getglobal = nullptr;

		void *s_vtable[k_api_slots] = {};
		std::atomic<bool> s_installed = false;
		std::atomic<lua_State *> s_main = nullptr; // the main Lua thread (captured from the engine's API calls)

		std::mutex s_mutex;
		std::deque<std::function<void(lua_State *)>> s_jobs;
		std::atomic<uint64_t> s_hooks = 0, s_jobs_run = 0;

		uint8_t *base()
		{
			return reinterpret_cast<uint8_t *>(GetModuleHandleW(nullptr));
		}

		// Plain function so SEH can guard reads of the game's memory.
		bool read(const void *source, void *destination, size_t size)
		{
			__try
			{
				std::memcpy(destination, source, size);
				return true;
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				return false;
			}
		}

		lua_State *main_thread_of(lua_State *L)
		{
			const uint8_t *global = nullptr;
			lua_State *main = nullptr;
			if (L == nullptr || !read(reinterpret_cast<const uint8_t *>(L) + k_state_global, &global, sizeof(global)) || global == nullptr ||
				!read(global + k_global_mainthread, &main, sizeof(main)))
				return nullptr;
			return main;
		}

		void note_state(lua_State *L)
		{
			if (s_main.load(std::memory_order_relaxed) != nullptr)
				return;
			lua_State *main = main_thread_of(L);
			lua_State *expected = nullptr;
			if (main != nullptr && s_main.compare_exchange_strong(expected, main))
				log_info("Lua: estado principal do jogo encontrado ({})", static_cast<void *>(main));
		}

		void hook_api_getfield(void *self, lua_State *L, int index, const char *key)
		{
			note_state(L);
			s_original_getfield(self, L, index, key);
		}

		void hook_api_getglobal(void *self, lua_State *L, const char *name)
		{
			note_state(L);
			s_original_getglobal(self, L, name);
		}

		// Count hook armed by post(): runs the queued work on the main Lua thread, at its next instruction.
		void lua_hook(lua_State *L, lua_Debug *)
		{
			s_sethook(L, nullptr, 0, 0); // one shot
			s_hooks.fetch_add(1, std::memory_order_relaxed);
			for (;;)
			{
				std::function<void(lua_State *)> job;
				{
					std::lock_guard lock(s_mutex);
					if (s_jobs.empty())
						break;
					job = std::move(s_jobs.front());
					s_jobs.pop_front();
				}
				const int top = s_gettop(L);
				try
				{
					job(L);
				}
				catch (const std::exception &e)
				{
					log_error("Lua: excecao C++ num trabalho do mod: {}", e.what());
				}
				s_settop(L, top); // whatever the job left on the stack
				s_jobs_run.fetch_add(1, std::memory_order_relaxed);
			}
		}
	}

	bool install()
	{
		uint8_t *b = base();
		for (const code_check &c : k_checks)
		{
			uint8_t bytes[sizeof(c.bytes)] = {};
			if (!read(b + c.rva, bytes, sizeof(bytes)) || std::memcmp(bytes, c.bytes, sizeof(bytes)) != 0)
			{
				log_error("Lua: codigo de {} diferente do esperado (+0x{:X}); acesso ao Lua desativado", c.name, c.rva);
				return false;
			}
		}
		void **object = reinterpret_cast<void **>(b + k_api_object);
		void *vtable = nullptr;
		if (!read(object, &vtable, sizeof(vtable)) || vtable != b + k_api_vtable)
		{
			log_error("Lua: o objeto da API do Lua (+0x{:X}) nao aponta para a vtable esperada ({}); acesso ao Lua desativado",
				k_api_object, vtable);
			return false;
		}
		if (!read(vtable, s_vtable, sizeof(s_vtable)) || s_vtable[k_slot_getfield] != b + k_api_getfield ||
			s_vtable[k_slot_getglobal] != b + k_api_getglobal)
		{
			log_error("Lua: vtable da API do Lua inesperada; acesso ao Lua desativado");
			return false;
		}
		s_pcall = reinterpret_cast<pcall_fn>(b + k_lua_pcall);
		s_loadbuffer = reinterpret_cast<loadbuffer_fn>(b + k_luaL_loadbuffer);
		s_gettop = reinterpret_cast<gettop_fn>(b + k_lua_gettop);
		s_settop = reinterpret_cast<settop_fn>(b + k_lua_settop);
		s_tolstring = reinterpret_cast<tolstring_fn>(b + k_lua_tolstring);
		s_type = reinterpret_cast<type_fn>(b + k_lua_type);
		s_typename = reinterpret_cast<typename_fn>(b + k_lua_typename);
		s_toboolean = reinterpret_cast<toboolean_fn>(b + k_lua_toboolean);
		s_sethook = reinterpret_cast<sethook_fn>(b + k_lua_sethook);
		s_original_getfield = reinterpret_cast<api_getfield_fn>(s_vtable[k_slot_getfield]);
		s_original_getglobal = reinterpret_cast<api_getglobal_fn>(s_vtable[k_slot_getglobal]);
		s_vtable[k_slot_getfield] = reinterpret_cast<void *>(&hook_api_getfield);
		s_vtable[k_slot_getglobal] = reinterpret_cast<void *>(&hook_api_getglobal);

		// The instance lives in the exe's writable data (.data2): only its vtable pointer changes, no code does.
		DWORD old_protect = 0;
		if (!VirtualProtect(object, sizeof(void *), PAGE_READWRITE, &old_protect))
		{
			log_error("Lua: VirtualProtect falhou ({})", GetLastError());
			return false;
		}
		InterlockedExchangePointer(object, s_vtable);
		VirtualProtect(object, sizeof(void *), old_protect, &old_protect);
		s_installed = true;
		log_info("Lua: acesso instalado (vtable da API do Lua copiada)");
		return true;
	}

	bool available()
	{
		return s_installed.load() && s_main.load() != nullptr;
	}

	void post(std::function<void(lua_State *L)> work)
	{
		lua_State *main = s_main.load();
		if (main == nullptr)
		{
			log_warning("Lua: o estado principal do jogo ainda nao foi encontrado; trabalho descartado");
			return;
		}
		{
			std::lock_guard lock(s_mutex);
			s_jobs.push_back(std::move(work));
		}
		// lua_sethook may be called from another thread (the standalone Lua interpreter does it from a signal handler):
		// the main thread runs lua_hook at its next instruction.
		s_sethook(main, &lua_hook, k_mask_count, 1);
	}

	int gettop(lua_State *L)
	{
		return s_gettop(L);
	}

	void settop(lua_State *L, int index)
	{
		s_settop(L, index);
	}

	std::string to_text(lua_State *L, int index)
	{
		const int type = s_type(L, index);
		switch (type)
		{
		case k_tnil: return "nil";
		case k_tboolean: return s_toboolean(L, index) ? "true" : "false";
		case k_tnumber:
		case k_tstring:
		{
			size_t length = 0;
			const char *text = s_tolstring(L, index, &length);
			return text != nullptr ? std::string(text, length) : std::string();
		}
		default:
			return std::string("<") + s_typename(L, type) + ">";
		}
	}

	std::string execute(lua_State *L, const std::string &code, const std::string &name, int results)
	{
		int status = s_loadbuffer(L, code.data(), code.size(), name.c_str());
		if (status == 0)
			status = s_pcall(L, 0, results, 0);
		if (status == 0)
			return {};
		std::string error = to_text(L, -1);
		s_settop(L, -2);
		return error.empty() ? std::format("erro {}", status) : error;
	}

	void run(std::string code, std::string name)
	{
		post([code = std::move(code), name = std::move(name)](lua_State *L) {
			const int top = gettop(L);
			const std::string error = execute(L, code, "=" + name, k_multret);
			if (!error.empty())
			{
				log_warning("Lua ({}): {}", name, error);
				return;
			}
			std::string results;
			for (int i = top + 1; i <= gettop(L); ++i)
				results += (results.empty() ? "" : ", ") + to_text(L, i);
			log_info("Lua ({}): ok{}{}", name, results.empty() ? "" : " -> ", results);
		});
	}

	stats take_stats()
	{
		stats s;
		s.hooks = s_hooks.exchange(0);
		s.jobs = s_jobs_run.exchange(0);
		return s;
	}
}
