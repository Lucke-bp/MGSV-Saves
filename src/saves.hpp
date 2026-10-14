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
	// Copies a slot's files (names MGSV_SAVES_* only: the two local ones and the Steam Cloud one) to
	// <mod>\backups\apagados (newest 90 files kept), before the game deletes them (TppScriptVars.DeleteFile). False if a
	// copy failed.
	bool backup_slot(const std::string &name);
	// The slots whose files exist (local copies or the Steam Cloud one): "id|dd/mm/yyyy hh:mm" lines, newest write.
	// For slots missing from slots.txt (deleted, or a new PC / reinstalled game with the files in the cloud).
	std::string list_slots();
	// A slot just written has the size of the game's own save (one written right after a read does not, and the game
	// cannot read it back).
	bool slot_readable(const std::string &name);
}
