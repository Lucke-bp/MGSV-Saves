#pragma once

#include "common.hpp"

// Functions for the mod's Lua scripts (globals, set by src/lua.cpp before every job):
//   mgsv_set_text(key, text [, persistent [, guarded]]) -> found   replaces a game text by key (src/lang.*); guarded:
//                                                      the game showing it runs MgsvSaves.OnConfirmation(key)
//   mgsv_restore_text(key)                             puts the game's text back
//   mgsv_get_text(key) -> text                         the game's own text of a key ("" if not loaded)
//   mgsv_read_file(name) -> text                       <game>\mgsv_saves\<name> ("" if missing)
//   mgsv_write_file(name, text) -> ok
//   mgsv_now() -> "dd/mm/yyyy hh:mm"                   local date and time
//   mgsv_press_key(vk, delay_ms) -> pressed            a key press in the game, if it has the focus (src/keys.*)
//   mgsv_watch_keys(vk, ...)                           hotkeys: a press runs MgsvSaves.OnKey(vk)
//   mgsv_watch_pad(on)                                 START held 1 s on a pad runs MgsvSaves.OnKey(0x100)
//   mgsv_backup_saves() -> name                        copy of the game's save files ("" on failure; src/saves.*)
//   mgsv_backup_slot(name) -> ok                       copies a slot's files (MGSV_SAVES_*, local + cloud) to backups
//   mgsv_list_slots() -> "id|date" lines               slots whose files exist (to recover a lost slots.txt)
//   mgsv_slot_readable(name) -> ok                     a slot just written has the game's save size
namespace mod::script_api
{
	void install();
}
