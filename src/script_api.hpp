#pragma once

#include "common.hpp"

// Functions for the mod's Lua scripts (globals, set by src/lua.cpp before every job):
//   mgsv_set_text(key, text [, persistent]) -> found   replaces a game text by key (src/lang.*)
//   mgsv_restore_text(key)                             puts the game's text back
//   mgsv_get_text(key) -> text                         the game's own text of a key ("" if not loaded)
//   mgsv_read_file(name) -> text                       <game>\mgsv_saves\<name> ("" if missing)
//   mgsv_write_file(name, text) -> ok
//   mgsv_now() -> "dd/mm/yyyy hh:mm"                   local date and time
//   mgsv_press_key(vk, delay_ms) -> pressed            a key press in the game, if it has the focus (src/keys.*)
//   mgsv_watch_keys(vk, ...)                           hotkeys: a press runs MgsvSaves.OnKey(vk)
//   mgsv_backup_saves() -> name                        copy of the game's save files ("" on failure; src/saves.*)
//   mgsv_remove_slot(name) -> ok                       moves a slot's files (MGSV_SAVES_*) to backups\apagados
namespace mod::script_api
{
	void install();
}
