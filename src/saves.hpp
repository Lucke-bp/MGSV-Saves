#pragma once

#include "common.hpp"

// The game's save folder and the mod's safety copies of it.
// The folder is Steam's <Steam>\userdata\<account>\287700\local: the Steam path and the signed-in account come from
// the registry (HKCU\Software\Valve\Steam); without them, the userdata folder with the newest TPP_GAME_DATA.
// The mod's slots are written there by the game itself (MGSV_SAVES_<id>0/1, same format as TPP_GAME_DATA0/1).
namespace mod::saves
{
	// Empty if not found.
	std::filesystem::path folder();
	// Copies the game's own save files (TPP_GAME_DATA*, PERSONAL_DATA*) to <mod>\backups\auto\<date time>; keeps the
	// newest 30 copies. Returns the copy's folder name, empty on failure.
	std::string backup();
	// Moves a slot's files (<name>0/1, names MGSV_SAVES_* only) from the save folder to <mod>\backups\apagados.
	bool remove_slot(const std::string &name);
}
