#include "common.hpp"

#include <atomic>

namespace
{
	std::atomic<int> s_logged_exceptions = 0;
	constexpr int k_max_logged_exceptions = 64;

	// Diagnostic: logs first-chance exceptions so crashes can be traced to a module/offset.
	LONG CALLBACK exception_logger(EXCEPTION_POINTERS *info)
	{
		const DWORD code = info->ExceptionRecord->ExceptionCode;
		// Debug output, thread naming and C++ exceptions are routine.
		if (code == DBG_PRINTEXCEPTION_C || code == 0x4001000A || code == 0x406D1388 || code == 0xE06D7363)
			return EXCEPTION_CONTINUE_SEARCH;
		if (s_logged_exceptions.fetch_add(1) < k_max_logged_exceptions)
		{
			std::string extra;
			if (code == EXCEPTION_ACCESS_VIOLATION && info->ExceptionRecord->NumberParameters >= 2)
				extra = std::format(" ({} em 0x{:x})", info->ExceptionRecord->ExceptionInformation[0] ? "escrita" : "leitura",
					info->ExceptionRecord->ExceptionInformation[1]);
			mod::log_warning("Excecao 0x{:08X} em {}{}", code, mod::describe_address(info->ExceptionRecord->ExceptionAddress), extra);
		}
		return EXCEPTION_CONTINUE_SEARCH;
	}
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved)
{
	switch (reason)
	{
	case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(module);
		// Inside mgsvmgo.exe (Metal Gear Online) this DLL is a pure passthrough: no log, no hooks.
		if (mod::is_target_process())
		{
			mod::log_init();
			mod::log_info("{} {} carregado como dinput8.dll (pid {})", MOD_NAME, MOD_VERSION, GetCurrentProcessId());
			AddVectoredExceptionHandler(1, exception_logger);
		}
		break;
	case DLL_PROCESS_DETACH:
		if (mod::is_target_process())
			mod::log_info("DLL descarregada ({})", reserved != nullptr ? "processo encerrando" : "FreeLibrary");
		break;
	}
	return TRUE;
}
