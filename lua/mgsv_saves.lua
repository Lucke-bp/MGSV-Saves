-- MGSV Saves: save slots in the game's pause menu.
-- Loaded by the mod's msacm32.dll on the game's main Lua thread once the game's scripts are up; MgsvSaves.Tick() then
-- runs about twice a second. _G drops new globals (Fox metatable), so the module is registered with rawset. The dll
-- also sets mgsv_log, mgsv_set_text, mgsv_restore_text, mgsv_get_text, mgsv_read_file, mgsv_write_file, mgsv_now,
-- mgsv_press_key, mgsv_watch_keys, mgsv_backup_saves and mgsv_remove_slot (src/script_api.hpp).
--
-- How it fits in the game:
--  - Pause menu: an item of the game that never shows in normal missions ("return to mission", of emergency
--    missions) is added to the pages that have "restart from checkpoint" (missions, free roam) and to the ACC's
--    (helicopter space, where saving is the game's own mission start save) as SAVE / LOAD, which opens a page with
--    SAVE GAME, LOAD GAME and DELETE A SLOT. One item only: the menu shows 9 at most and drops the rest (when the game's
--    page is full, its STORE moves into ours). Its texts are
--    replaced in the game's language tables (by key, so in any language); in emergency missions it is left alone.
--  - The slot list is a pause menu page of its own: the 7 items that only send a message to Lua (one native class:
--    confirmation popup, then the message), with their names, descriptions and confirmations replaced by the slots'.
--    It is registered and the menu opened again by an Esc key press (the menu takes its list when it opens). The game
--    page comes back once that menu closes, which shows as the game's time standing still (Time.GetRawElapsed...).
--  - The items' messages are taken in TppMission.messageExecTable (the vanilla scripts keep Tpp.DoMessage local).
--  - Saving: a checkpoint where the player stands, then the game writes its save slot again as MGSV_SAVES_<id>
--    (files MGSV_SAVES_<id>0/1 next to the game's saves, same format). Loading (off-line only: Mother Base data is on
--    the servers): a copy of the game's saves goes to mgsv_saves\backups\auto, the game reads the slot's file back,
--    copies it into its slots and restarts from its checkpoint; for another mission its vars are restored and that
--    mission is loaded with the "continue" flag, like the game does for emergency missions.
--  - F5 / F9: quick save / quick load (slot Q).
local this = rawget(_G, "MgsvSaves") or {} -- the same table across reloads
rawset(_G, "MgsvSaves", this)

-- Settings: <game>\mgsv_saves\mgsv_saves.ini (written with these values when missing)
this.SLOT_COUNT = 10
this.QUICK_SAVE_KEY = 0x74 -- F5
this.QUICK_LOAD_KEY = 0x78 -- F9
local MAX_SLOTS = 50
local CONFIG_FILE = "mgsv_saves.ini"
-- Settings written at the end of an older mgsv_saves.ini when missing (name, lines).
local CONFIG_ADDITIONS = {
  { "pad_quick_save", "; Hold START on a controller for a second: quick save (true/false) / Segurar START no controle: save rapido\npad_quick_save=true\n" },
}
local DEFAULT_CONFIG = [[
; MGSV Saves
; Number of save slots (1 to 50) / Numero de slots
slots=10
; Quick save / quick load keys: F1-F12, A-Z, 0-9, INSERT, HOME, END, PAGEUP, PAGEDOWN, DELETE, or NONE
; Teclas do save rapido / carregar save rapido (NONE: desligada)
quick_save_key=F5
quick_load_key=F9
]]

local S32 = Fox.StrCode32
local SAVING = TppDefine.SAVE_SLOT.SAVING
local FILE_PREFIX = "MGSV_SAVES_"
local SLOTS_FILE = "slots.txt"
local QUICK = "Q"
local AUTO = "A" -- copy of the game's own last save (its autosave)
local PAD_START_HOLD = 0x100 -- src/keys.hpp
local QUESTION_POPUP = 5001 -- the game's MGO invitation popup (two buttons), with its text replaced
local VK_ESCAPE = 0x1B

-- Language keys (Fox.StrCode32 of the text ids: the same in every language)
local KEY = {
  saveItem = 0x27B5469D, saveInfo = 0x45701DD2, saveConfirm = 0x031F2DBC,
  loadItem = 0x5DF5E488, loadInfo = 0x3D5C47C6, loadConfirm = 0xDD2762BA,
  question = 0xA89965CC,
}

-- The pause menu items that only send a message to Lua, with their texts' keys (names and descriptions: the exe's
-- table of pause items; confirmations: their static objects). ABORT_MISSION_RETURN_TO_ACC is left out: its
-- confirmation text is ABORT_MISSION's. "main": the item is ours on the game's page too.
local ITEMS = {
  { id = "RESTART_FROM_CHECK_POINT", message = "PauseMenuCheckpoint", name = 0x39DF1083, info = 0x7273AA3B, confirm = 0xA59C6214 },
  { id = "RESTART_FROM_MISSION_START", message = "PauseMenuRestart", name = 0xD4387694, info = 0x97C8E4A4, confirm = 0xDC140814 },
  { id = "RESTART_FROM_HELICOPTER", message = "PauseMenuRestartFromHelicopter", name = 0x9EE0BDC0, info = 0x1A731241, confirm = 0xA70CFCD8 },
  { id = "RETURN_TO_TITLE", message = "PauseMenuReturnToTitle", name = 0x6CE1889C, info = 0xD04AE17D, confirm = 0x9F92F91D },
  { id = "RETURN_TO_MISSION_FROM_EMERGENCY_MISSION", message = "PauseMenuReturnToMission", name = KEY.saveItem, info = KEY.saveInfo, confirm = KEY.saveConfirm, main = true },
  { id = "ABORT_MISSION", message = "PauseMenuAbortMission", name = 0x8CD08509, info = 0xFCB0DCED, confirm = 0x57353167 },
  { id = "ABORT_MISSION_PLACEMENT_MODE", message = "PauseMenuFinishFobManualPlaecementMode", name = KEY.loadItem, info = KEY.loadInfo, confirm = KEY.loadConfirm },
}
local PAGE_SIZE = #ITEMS

-- The mod's texts: English here (also for any text a language lacks), the other languages of the game in
-- mgsv_saves_lang.lua (see the placeholders there).
local ENGLISH = {
  mainItem = "SAVE / LOAD",
  mainInfo = "Save, load and delete games in slots (MGSV Saves).",
  mainConfirm = "Open the saved games?",
  saveItem = "SAVE GAME",
  saveInfo = "Saves the game to a slot (MGSV Saves).",
  saveConfirm = "Choose the slot to save the game to?",
  loadItem = "LOAD GAME",
  loadInfo = "Loads a game saved to a slot (MGSV Saves).",
  loadConfirm = "Choose the slot to load?",
  slotTitle = "SLOT %d", quickTitle = "QUICK SAVE", autoTitle = "AUTOSAVE",
  slotWord = "slot %d", quickWord = "quick save", autoWord = "autosave",
  quickItem = "QUICK SAVE",
  quickInfo = "Saves to the quick save slot.",
  quickConfirm = "Quick save now?",
  entryName = "%s: %s",
  emptyName = "%s: EMPTY",
  saveEmptyInfo = "Save the game to this empty slot.",
  saveOverInfo = "Save to this slot, replacing: %s",
  saveEmptyConfirm = "Save the game to this slot?\n%s",
  saveOverConfirm = "Save to this slot?\nThe game saved in it will be replaced:\n%s: %s",
  loadSlotInfo = "Saved game: %s",
  loadSlotConfirm = "Load this saved game?\n%s: %s\n\nUnsaved progress will be lost.",
  deleteMenuName = "DELETE A SLOT",
  deleteMenuInfo = "Choose a slot to delete.",
  deleteMenuConfirm = "Choose a slot to delete?",
  deleteName = "DELETE %s: %s",
  deleteInfo = "The file is moved to mgsv_saves\\backups\\apagados.",
  deleteConfirm = "Delete this saved game?\n%s: %s",
  moreName = "MORE SLOTS (PAGE %d OF %d)",
  moreInfo = "Show the next page of slots.",
  moreConfirm = "Show the next page of slots?",
  saved = "MGSV Saves: game saved (%s)",
  saveFailed = "MGSV Saves: the game cannot be saved right now",
  loading = "MGSV Saves: loading... (%s)",
  loadFailed = "MGSV Saves: could not load (%s)",
  backupFailed = "MGSV Saves: the game's save could not be backed up; nothing was loaded",
  online = "MGSV Saves: loading only works in offline mode",
  noSlots = "MGSV Saves: no saved slots yet",
  noQuick = "MGSV Saves: no quick save yet",
  deleted = "MGSV Saves: saved game deleted (%s)",
  deleteFailed = "MGSV Saves: could not delete (%s)",
  openMenu = "MGSV Saves: open the pause menu to choose the slot",
  listFailed = "MGSV Saves: the slot list could not be opened",
  mission = "MISSION %d", prologue = "PROLOGUE", episode = "Episode %d: %s", unknown = "Unknown mission",
  afgh = "AFGHANISTAN", mafr = "AFRICA", mtbs = "MOTHER BASE", acc = "ACC",
}
local function log(text)
  mgsv_log(text)
end
this.Log = log

local function announce(text)
  TppUiCommand.AnnounceLogView(text)
end

-- ---------------------------------------------------------------------------------------------------------------
-- Language: the game's (AssetConfiguration "Language": jpn, eng, fre, ita, ger, spa, por, rus, ara, cht, kor).

local function load_languages()
  local chunk, err = loadstring(mgsv_read_file("mgsv_saves_lang.lua"), "=mgsv_saves_lang.lua")
  local ok, languages = false, nil
  if chunk then
    ok, languages = pcall(chunk)
  end
  if not ok or type(languages) ~= "table" then
    log("textos dos idiomas: " .. tostring(err or languages))
    return {}
  end
  for _, texts in pairs(languages) do
    setmetatable(texts, { __index = ENGLISH }) -- a text missing in a language comes in English
  end
  return languages
end

local LANGUAGES = load_languages()
local LANGUAGE_ALIASES = { deu = "ger" }

local function texts()
  return LANGUAGES[this.language] or ENGLISH
end

-- Checked every tick: the game's language can be changed while it runs. True if it changed.
local function detect_language()
  local ok, code = pcall(AssetConfiguration.GetDefaultCategory, "Language")
  if not ok or type(code) ~= "string" or code == "" then
    return false
  end
  code = LANGUAGE_ALIASES[code] or code
  if code == this.language then
    return false
  end
  this.language = code
  log("idioma do jogo: " .. code .. (LANGUAGES[code] and "" or " (textos do mod em ingles)"))
  return true
end

-- The game's page has one item of ours (it shows 9 items at most): SAVE / LOAD, which opens a page with SAVE GAME,
-- LOAD GAME and DELETE A SLOT.
local MENU_TEXTS = { { KEY.saveItem, "mainItem" }, { KEY.saveInfo, "mainInfo" }, { KEY.saveConfirm, "mainConfirm" } }

local function take_menu_texts()
  for _, text in ipairs(MENU_TEXTS) do
    mgsv_set_text(text[1], texts()[text[2]], true)
  end
end

local function give_back_menu_texts()
  for _, text in ipairs(MENU_TEXTS) do
    mgsv_restore_text(text[1])
  end
end

-- ---------------------------------------------------------------------------------------------------------------
-- Slots: <game>\mgsv_saves\slots.txt, one line per slot: "id|missionCode|date|label" (id: 1..SLOT_COUNT, or Q; label:
-- the mission's name when it was saved, like the game shows it, "Episódio 4: C2W")

local function slot_id(text)
  if text == QUICK or text == AUTO then
    return text
  end
  return tonumber(text)
end

local function read_slots()
  local slots = {}
  for line in string.gmatch(mgsv_read_file(SLOTS_FILE), "[^\r\n]+") do
    local id, missionCode, date, label = string.match(line, "^([%dQA]+)|(%d+)|([^|]*)|?([^|]*)")
    id = id and slot_id(id)
    if id then
      slots[id] = { missionCode = tonumber(missionCode), date = date, label = label ~= "" and label or nil }
    end
  end
  -- Slots whose files exist but are not listed (slots.txt lost, or a new PC / reinstalled game with the files in the
  -- Steam Cloud): listed with an unknown mission (0) and the files' date.
  if mgsv_list_slots then
    for line in string.gmatch(mgsv_list_slots(), "[^\r\n]+") do
      local id, date = string.match(line, "^([%dQA]+)|(.*)$")
      id = id and slot_id(id)
      if id and not slots[id] then
        slots[id] = { missionCode = 0, date = date }
      end
    end
  end
  return slots
end

-- All slots are kept, also numbers above SLOT_COUNT (the setting may have been lowered: they come back with it).
local function write_slots(slots)
  local ids = {}
  for id in pairs(slots) do
    if type(id) == "number" then
      ids[#ids + 1] = id
    end
  end
  table.sort(ids)
  if slots[QUICK] then
    table.insert(ids, 1, QUICK)
  end
  if slots[AUTO] then
    table.insert(ids, 1, AUTO)
  end
  local lines = {}
  for _, id in ipairs(ids) do
    local slot = slots[id]
    local label = string.gsub(slot.label or "", "[|\r\n]", " ")
    lines[#lines + 1] = string.format("%s|%d|%s|%s", tostring(id), slot.missionCode, slot.date, label)
  end
  return mgsv_write_file(SLOTS_FILE, table.concat(lines, "\n") .. "\n")
end

local function slot_title(id)
  if id == QUICK then
    return texts().quickTitle
  elseif id == AUTO then
    return texts().autoTitle
  end
  return string.format(texts().slotTitle, id)
end

local function slot_word(id)
  if id == QUICK then
    return texts().quickWord
  elseif id == AUTO then
    return texts().autoWord
  end
  return string.format(texts().slotWord, id)
end

local FREE_ROAM = { [30010] = "afgh", [30020] = "mafr", [30050] = "mtbs", [30150] = "mtbs", [30250] = "mtbs",
  [40010] = "acc", [40020] = "acc", [40050] = "acc", [40060] = "acc" }
local EPISODE_FORMAT = 0x8957DA05 -- the game's "Episode %d: %s"
-- The game's place names ("Northern Kabul, Afghanistan", "Angola-Zaire border region", "MOTHER BASE", "Aerial Command
-- Center"), in the block of the mission names.
local PLACE_TEXT = { afgh = 0x22CBFD42, mafr = 0x3D76F180, mtbs = 0x64A4C511, acc = 0x51E1E526 }

-- The game's name of a mission ("C2W", "[SUBSISTENCE] C2W"): text id name_mission_<location>_<mission>. Empty if
-- that text is not loaded (it is during missions).
local function mission_name(missionCode)
  local locationName = TppPackList.GetLocationNameFormMissionCode(missionCode)
  local location = locationName and TppDefine.LOCATION_ID[locationName]
  if not location then
    return ""
  end
  return mgsv_get_text(S32(string.format("name_mission_%02d_%05d", location, missionCode)))
end

-- "Episódio 4: C2W" (the game's own texts), "AFEGANISTÃO", "CCA"; "MISSÃO 4" when the names are not loaded. The
-- second result is true when the label is final (not that fallback).
local function mission_label(missionCode)
  if missionCode == 0 then
    return texts().unknown, false -- a slot found without its entry in slots.txt
  end
  local free = FREE_ROAM[missionCode]
  if free then
    local name = mgsv_get_text(PLACE_TEXT[free])
    if name ~= "" then
      return name, true
    end
    return texts()[free], false
  end
  local index
  for i, code in ipairs(TppDefine.MISSION_LIST) do
    if tonumber(code) == missionCode then
      index = i
      break
    end
  end
  local name = mission_name(missionCode)
  if name ~= "" then
    if not index or index == 1 then -- the list starts at the prologue, which has no number
      return name, true
    end
    local format = mgsv_get_text(EPISODE_FORMAT)
    if not string.match(format, "^[^%%]*%%d[^%%]*%%s[^%%]*$") then
      format = texts().episode
    end
    return string.format(format, index - 1, name), true
  end
  if index then
    return index == 1 and texts().prologue or string.format(texts().mission, index - 1)
  end
  return tostring(missionCode)
end

-- "Episódio 4: C2W - 08/10 22:50" (short: no year) or "Episódio 4: C2W - 08/10/2026 22:50"
local function slot_label(slot, long)
  local date = slot.date
  if not long then
    date = string.gsub(date, "^(%d+/%d+)/%d+", "%1")
  end
  return (slot.label or mission_label(slot.missionCode)) .. " - " .. date
end

-- ---------------------------------------------------------------------------------------------------------------
-- The game's time stands still while its pause menu is open (and in a few other places): that is how the mod sees
-- the slot list being opened and closed.

local function update_pause()
  local ok, now = pcall(Time.GetRawElapsedTimeSinceStartUp)
  if not ok or type(now) ~= "number" then
    return
  end
  this.paused = this.gameTime ~= nil and now == this.gameTime
  this.gameTime = now
end

-- ---------------------------------------------------------------------------------------------------------------
-- The game's pause menu page: our item after "restart from checkpoint" (missions and free roam).

this.active = this.active or false

local function in_helicopter_space()
  return TppMission.IsHelicopterSpace(vars.missionCode)
end

-- The menu shows 9 items at most (the game drops the rest; missions already have 9). Missions and free roam: our item
-- takes the place of "restart from checkpoint", which moves into the page it opens. The ACC (helicopter space, no
-- checkpoints): our item goes before "return to title" and, if the page is full, the first of MOVABLE_ITEMS moves into
-- ours (game items work the same from there). Returns the page and the items moved.
local MAX_PAUSE_ITEMS = 9
local MOVABLE_ITEMS = { "TERMS_OF_USE_MENU_ITEM", "ONLINE_NEWS", "STORE_ITEM" }

local function with_our_items(items)
  local checkpointAt, titleAt
  for i, item in ipairs(items) do
    if item == GamePauseMenu.RESTART_FROM_CHECK_POINT then
      checkpointAt = i
    elseif item == GamePauseMenu.RETURN_TO_TITLE and in_helicopter_space() then
      titleAt = i
    elseif item == GamePauseMenu.RETURN_TO_MISSION_FROM_EMERGENCY_MISSION or item == GamePauseMenu.ABORT_MISSION_PLACEMENT_MODE then
      return nil -- the game uses these items here
    end
  end
  local moved = {}
  if checkpointAt then
    moved[1] = GamePauseMenu.RESTART_FROM_CHECK_POINT
  elseif titleAt then
    if #items + 1 > MAX_PAUSE_ITEMS then
      for _, name in ipairs(MOVABLE_ITEMS) do
        for _, item in ipairs(items) do
          if item == GamePauseMenu[name] and #moved == 0 then
            moved[1] = item
          end
        end
      end
      if #moved == 0 then
        return nil -- no room: only the hotkeys then
      end
    end
  else
    return nil
  end
  local result = {}
  for i, item in ipairs(items) do
    if i == checkpointAt or i == titleAt then
      result[#result + 1] = GamePauseMenu.RETURN_TO_MISSION_FROM_EMERGENCY_MISSION -- SAVE / LOAD
    end
    if item ~= moved[1] then
      result[#result + 1] = item
    end
  end
  return result, moved
end

-- The page TppUI.Init registers in missions and free roam (vanilla), for when the game's own is not known (the
-- script was reloaded since the mission started).
local function vanilla_game_page()
  local P = GamePauseMenu
  local items
  if in_helicopter_space() then -- TppUI.RegisterHeliSpacePauseMenuPage(true)
    items = { P.RETURN_TO_TITLE, P.ONLINE_NEWS, P.RECORDS_ITEM, P.CONTROLS_AND_TIPS_ITEM, P.TERMS_OF_USE_MENU_ITEM, P.OPEN_OPTION_MENU, P.GOTO_MGO }
    table.insert(items, 2, P.STORE_ITEM)
    if not (TppGameMode.GetUserMode() <= TppGameMode.U_SIGN_OUT) then
      table.insert(items, 2, P.SIGN_IN)
    end
    return items
  end
  if TppMission.IsFreeMission(vars.missionCode) then
    items = { P.RESTART_FROM_CHECK_POINT, P.RETURN_TO_TITLE, P.SIGN_IN, P.STORE_ITEM, P.RECORDS_ITEM, P.CONTROLS_AND_TIPS_ITEM, P.OPEN_OPTION_MENU }
    if TppMission.IsMbFreeMissions(vars.missionCode) then
      if vars.missionCode == 30050 then
        table.insert(items, 2, P.RESTART_FROM_MISSION_START)
      end
      table.insert(items, 3, P.RESTART_FROM_HELICOPTER)
    else
      table.insert(items, 2, P.RESTART_FROM_HELICOPTER)
    end
    return items
  end
  items = { P.RESTART_FROM_MISSION_START, P.RETURN_TO_TITLE, P.SIGN_IN, P.STORE_ITEM, P.CONTROLS_AND_TIPS_ITEM, P.OPEN_OPTION_MENU }
  if gvars.str_storySequence >= TppDefine.STORY_SEQUENCE.CLEARD_TO_MATHER_BASE then
    table.insert(items, 6, P.RECORDS_ITEM)
  end
  if gvars.mis_isStartFromHelispace then
    table.insert(items, 3, P.ABORT_MISSION_RETURN_TO_ACC)
  end
  if gvars.mis_isStartFromFreePlay then
    table.insert(items, 3, P.ABORT_MISSION)
  end
  if vars.missionCode ~= 10115 then
    table.insert(items, 1, P.RESTART_FROM_CHECK_POINT)
  end
  return items
end

local function register_game_page(items)
  this.gamePage = items
  local ok, ours, moved = pcall(with_our_items, items)
  if ok and ours then
    this.active = true
    this.movedItems = moved
    if this.language then
      take_menu_texts()
    end
    return this.originalRegisterPauseMenuPage(ours)
  end
  if not ok then
    log("erro ao montar o menu de pausa: " .. tostring(ours))
  end
  this.active = false
  give_back_menu_texts()
  return this.originalRegisterPauseMenuPage(items)
end

local close_page -- below

if not this.originalRegisterPauseMenuPage then
  this.originalRegisterPauseMenuPage = TppUiCommand.RegisterPauseMenuPage
end
TppUiCommand.RegisterPauseMenuPage = function(items)
  if this.page then
    close_page(true) -- the game is setting up another page (mission change): the slot list goes away
  end
  return register_game_page(items)
end

-- ---------------------------------------------------------------------------------------------------------------
-- The slot list pages

local function page_entries(kind)
  local slots = read_slots()
  -- Slots saved without the mission's name (older versions) get it once the game's names are loaded.
  local named = false
  for _, slot in pairs(slots) do
    if not slot.label then
      local label, final = mission_label(slot.missionCode)
      if final then
        slot.label = label
        named = true
      end
    end
  end
  if named then
    write_slots(slots)
  end
  local list = {}
  if kind == "hub" then -- what SAVE / LOAD opens
    -- Quick save first. Items of the game's page moved here (see with_our_items): the checkpoint is an entry of ours (its
    -- message is one of ITEMS'), with the game's own texts; any other game item comes last, as it is.
    list[#list + 1] = { kind = "quick" }
    local natives = {}
    for _, item in ipairs(this.movedItems or {}) do
      if item == GamePauseMenu.RESTART_FROM_CHECK_POINT then
        list[#list + 1] = { kind = "checkpoint" }
      else
        natives[#natives + 1] = { kind = "native", item = item }
      end
    end
    list[#list + 1] = { kind = "goto", target = "save" }
    if next(slots) ~= nil then
      list[#list + 1] = { kind = "goto", target = "load" }
      list[#list + 1] = { kind = "goto", target = "delete" }
    end
    for _, entry in ipairs(natives) do
      list[#list + 1] = entry
    end
  elseif kind == "save" then
    for n = 1, this.SLOT_COUNT do
      list[#list + 1] = { kind = "save", id = n, slot = slots[n] }
    end
  else
    local entryKind = kind == "delete" and "delete" or "load"
    if slots[AUTO] and kind == "load" then -- the game's own last save: loaded, never deleted (it comes back)
      list[#list + 1] = { kind = entryKind, id = AUTO, slot = slots[AUTO] }
    end
    if slots[QUICK] then
      list[#list + 1] = { kind = entryKind, id = QUICK, slot = slots[QUICK] }
    end
    for n = 1, this.SLOT_COUNT do
      if slots[n] then
        list[#list + 1] = { kind = entryKind, id = n, slot = slots[n] }
      end
    end
  end
  return list
end

-- Up to PAGE_SIZE entries; a longer list goes in pages of PAGE_SIZE - 1 plus "more slots".
local function paginate(list, page)
  if #list <= PAGE_SIZE then
    return list, 1
  end
  local per = PAGE_SIZE - 1
  local pages = math.ceil(#list / per)
  page = (page - 1) % pages + 1
  local entries = {}
  for i = (page - 1) * per + 1, math.min(page * per, #list) do
    entries[#entries + 1] = list[i]
  end
  entries[#entries + 1] = { kind = "more", page = page % pages + 1, pages = pages }
  return entries, page
end

-- Name, description and confirmation (every pause item has one: the game always asks).
local function entry_texts(entry)
  local t = texts()
  if entry.kind == "more" then
    return string.format(t.moreName, entry.page, entry.pages), t.moreInfo, t.moreConfirm
  elseif entry.kind == "quick" then
    return t.quickItem, t.quickInfo, t.quickConfirm
  elseif entry.kind == "checkpoint" then -- the game's own texts (ITEMS[1] is "restart from checkpoint")
    local name, info, confirm = mgsv_get_text(ITEMS[1].name), mgsv_get_text(ITEMS[1].info), mgsv_get_text(ITEMS[1].confirm)
    return name ~= "" and name or "CHECKPOINT", info, confirm ~= "" and confirm or "CHECKPOINT?"
  elseif entry.kind == "goto" then
    if entry.target == "save" then
      return t.saveItem, t.saveInfo, t.saveConfirm
    elseif entry.target == "load" then
      return t.loadItem, t.loadInfo, t.loadConfirm
    end
    return t.deleteMenuName, t.deleteMenuInfo, t.deleteMenuConfirm
  end
  local title, slot = slot_title(entry.id), entry.slot
  if entry.kind == "save" then
    if not slot then
      return string.format(t.emptyName, title), t.saveEmptyInfo, string.format(t.saveEmptyConfirm, title)
    end
    return string.format(t.entryName, title, slot_label(slot)), string.format(t.saveOverInfo, slot_label(slot, true)),
      string.format(t.saveOverConfirm, title, slot_label(slot, true))
  elseif entry.kind == "load" then
    return string.format(t.entryName, title, slot_label(slot)), string.format(t.loadSlotInfo, slot_label(slot, true)),
      string.format(t.loadSlotConfirm, title, slot_label(slot, true))
  else -- delete
    return string.format(t.deleteName, title, slot_label(slot)), t.deleteInfo, string.format(t.deleteConfirm, title, slot_label(slot, true))
  end
end

local function give_back_page_texts()
  for _, item in ipairs(ITEMS) do
    if not item.main then
      mgsv_restore_text(item.name)
      mgsv_restore_text(item.info)
      mgsv_restore_text(item.confirm)
    end
  end
  if this.language and this.active then
    take_menu_texts()
  end
end

-- The game's page and texts come back. gameChanging: the game is registering a page itself right now.
close_page = function(gameChanging)
  if not this.page then
    return
  end
  this.page = nil
  this.lastPage = page
  this.pageClosedAt = this.ticks
  give_back_page_texts()
  if not gameChanging then
    register_game_page(this.gamePage or vanilla_game_page())
  end
end

local function open_page(kind, page)
  local list = page_entries(kind)
  if #list == 0 then
    announce(texts().noSlots)
    return
  end
  local entries
  entries, page = paginate(list, page or 1)
  for i, entry in ipairs(entries) do
    if entry.kind ~= "native" and (mgsv_get_text(ITEMS[i].name) == "" or mgsv_get_text(ITEMS[i].confirm) == "") then
      -- Never show an item with the game's own text (it would do what it says if this broke).
      log("texto do item " .. ITEMS[i].id .. " nao encontrado na memoria")
      announce(texts().listFailed)
      return
    end
  end
  local menu = {}
  local quickKey
  for i, entry in ipairs(entries) do
    if entry.kind == "native" then -- an item of the game (always after ours: our items keep their positions)
      menu[i] = entry.item
    else
      local item = ITEMS[i]
      local name, info, confirm = entry_texts(entry)
      mgsv_set_text(item.name, name, true)
      mgsv_set_text(item.info, info, true)
      -- The quick save's confirmation is guarded: the game showing it runs OnConfirmation (no question, like F5)
      mgsv_set_text(item.confirm, confirm, true, entry.kind == "quick")
      if entry.kind == "quick" then
        quickKey = item.confirm
      end
      menu[i] = GamePauseMenu[item.id]
    end
  end
  this.page = { kind = kind, page = page, entries = entries, state = "opening", wait = 0, since = this.gameTime, quickKey = quickKey }
  this.originalRegisterPauseMenuPage(menu)
  if not mgsv_press_key or not mgsv_press_key(VK_ESCAPE, 300) then
    this.page.hinted = true
    announce(texts().openMenu)
  end
  log(string.format("lista de slots: %s, pagina %d, %d itens", kind, page, #entries))
end

-- Twice a second: is the menu with the list open, or closed again?
local function track_page()
  local page = this.page
  if not page then
    return
  end
  page.wait = page.wait + 1
  if page.state == "opening" then
    -- Open: the time stands still after having moved since the list was registered (the game's own menu may still
    -- be closing then).
    if this.gameTime ~= page.since then
      page.moved = true
    end
    if this.paused and page.moved then
      page.state = "open"
    else
      if page.wait == 8 and not page.hinted then
        page.hinted = true
        announce(texts().openMenu)
      elseif page.wait > 120 then
        log("lista de slots nao foi aberta; pagina do jogo de volta")
        close_page()
      end
    end
  elseif page.state == "open" and this.paused == false then
    -- Closed without a choice (a choice's message comes while the time still stands still, before this tick): back,
    -- to the game's pause menu.
    close_page()
    if this.padSave then
      log("lista de slots fechada para o save rapido")
    else
      if mgsv_press_key then
        mgsv_press_key(VK_ESCAPE, 100)
      end
      log("lista de slots fechada sem escolha: menu de pausa de volta")
    end
  end
end

-- ---------------------------------------------------------------------------------------------------------------
-- The items' messages, taken in TppMission's message table (rebuilt by the game on mission changes: checked every
-- tick). Each script load wraps them again around the game's own function.

local HOOK = {}

local function dispatch(index, original, ...)
  local item = ITEMS[index]
  if this.page then
    return this.OnPageItem(index)
  end
  if this.lastPage and this.ticks - this.pageClosedAt <= 1 then
    -- Not expected (the choice comes before the list is seen closed), but it can only be from the list.
    log("mensagem " .. item.message .. " logo depois da lista de slots: escolha nela")
    return this.OnPageItem(index, this.lastPage)
  end
  if item.main and this.active then
    return this.OnItem()
  end
  return original(...)
end

local function hook_item_messages()
  local exec = TppMission.messageExecTable
  local ui = exec and exec[S32("UI")]
  if not ui then
    return
  end
  for index, item in ipairs(ITEMS) do
    local receivers = ui[S32(item.message)]
    if receivers and receivers.func and receivers.mgsvSavesHook ~= HOOK then
      local original = receivers.mgsvSavesOriginal or receivers.func
      receivers.mgsvSavesOriginal = original
      receivers.mgsvSavesHook = HOOK
      receivers.func = function(...)
        return dispatch(index, original, ...)
      end
    end
  end
end

-- ---------------------------------------------------------------------------------------------------------------
-- The checkpoint save we ask for is seen in TppSave.DoSave (it is over in less than a tick, too fast to poll).

if not this.originalDoSave then
  this.originalDoSave = TppSave.DoSave
end
-- Any other save of the game's file is its own (autosave): copied as the automatic save (copy_autosave, below).
TppSave.DoSave = function(saveParams, force)
  local result = this.originalDoSave(saveParams, force)
  if type(saveParams) == "table" and saveParams.fileName == TppDefine.GAME_SAVE_FILE_NAME then
    local flow = this.flow
    if flow and flow.state == "checkpoint" then
      flow.state = "checkpointSaving"
    elseif not flow and not (this.autoSkipUntil and os.clock() < this.autoSkipUntil) then
      -- (the game saves again during and right after the mod's own saves and loads: not autosaves)
      this.autoPending = os.clock()
    end
  end
  return result
end

-- ---------------------------------------------------------------------------------------------------------------
-- Saving, loading, deleting (a small state machine driven by Tick)

local function busy()
  return TppScriptVars.IsSavingOrLoading() or TppSave.IsSaving()
end

-- Slots are loaded only off-line (Mother Base data is kept on the servers): the game's own test, signed in to
-- Konami's servers (user mode 4, U_KONAMI_LOGIN). Mode 3 (signed in to Steam only) shows as OFF-LINE in the pause menu.
local function online()
  local ok, mode = pcall(TppGameMode.GetUserMode)
  return not ok or type(mode) ~= "number" or mode >= TppGameMode.U_KONAMI_LOGIN
end

-- A yes/no question in the game's two-button popup; onYes runs on YES.
local function ask(text, onYes)
  if not mgsv_set_text(KEY.question, text) then
    log("texto da pergunta nao encontrado na memoria") -- never show that popup with its own text (MGO invitation)
    return false
  end
  TppUiCommand.ShowErrorPopup(QUESTION_POPUP, Popup.TYPE_TWO_BUTTON)
  this.flow = { state = "question", onYes = onYes, wait = 0 }
  return true
end

-- Only where the game itself would save at the player's position (TppCheckPoint's "safety" checks): in a mission's
-- play (no cutscene that cannot be played, no game over, no mission clear, not its start), the player in a state the
-- game saves (not in the air, in a vehicle...), no vehicle being fultoned. The ACC has none of these.
local function playing()
  if in_helicopter_space() then
    return true
  end
  local ok, state = pcall(TppMission.CheckMissionState)
  return ok and state == true
end

local function can_save_here()
  if in_helicopter_space() then
    return true
  end
  if not playing() then
    return false, "estado da missao"
  end
  local ok, can = pcall(function() return not Player.CanSaveAsCheckPoint or Player.CanSaveAsCheckPoint() end)
  if not ok or not can then
    return false, "posicao do jogador"
  end
  ok, can = pcall(function() return not (Gimmick.IsVehicleFultonUpdating and Gimmick.IsVehicleFultonUpdating()) end)
  if not ok or not can then
    return false, "fulton de veiculo"
  end
  return true
end

local function start_save(id)
  local ok, reason = can_save_here()
  if not ok then
    announce(texts().saveFailed)
    log("save recusado: " .. reason)
    return
  end
  this.flow = { action = "save", id = id, state = "checkpoint", wait = 0 }
  if in_helicopter_space() then
    -- No checkpoints in the ACC: the save the game makes there itself (mission start of the helicopter space), now.
    TppSave.VarSave(vars.missionCode, true)
    TppSave.SaveGameData(vars.missionCode)
  else
    TppMission.UpdateCheckPointAtCurrentPosition()
  end
end

local function start_load(id)
  local slot = read_slots()[id]
  if not slot then
    return
  end
  if online() then
    announce(texts().online)
    return
  end
  if busy() or not playing() then
    announce(string.format(texts().loadFailed, slot_word(id)))
    return
  end
  local backup = mgsv_backup_saves()
  if backup == "" then
    announce(texts().backupFailed)
    return
  end
  announce(string.format(texts().loading, slot_word(id)))
  -- The game saves right after a load: that is not an autosave of the player's game (the automatic save stays).
  this.autoPending = nil
  this.autoSkipUntil = os.clock() + 30
  TppScriptVars.ReadSlotFromFile(SAVING, FILE_PREFIX .. tostring(id))
  this.flow = { action = "load", id = id, slot = slot, state = "reading", wait = 0 }
  log("carregando " .. slot_word(id) .. " (missao " .. slot.missionCode .. "); backup " .. backup)
end

-- A copy of the slot's files (local and Steam Cloud) goes to backups\apagados, then the game deletes them itself (its
-- delete reaches its Steam Cloud copy too, which a file deleted by hand would not).
local function delete_slot(id)
  local slots = read_slots()
  if not slots[id] then
    return
  end
  if busy() or not mgsv_backup_slot(FILE_PREFIX .. tostring(id)) then
    announce(string.format(texts().deleteFailed, slot_word(id)))
    return
  end
  TppScriptVars.DeleteFile(FILE_PREFIX .. tostring(id))
  slots[id] = nil
  write_slots(slots)
  announce(string.format(texts().deleted, slot_word(id)))
end

local function call(name, f)
  local ok, err = pcall(f)
  if not ok then
    log(name .. ": " .. tostring(err))
  end
end

-- The slot's mission is not the current one (or one of them is the ACC): its vars come back now and that mission is
-- loaded with the "continue from checkpoint" flag, so it restores the slot's checkpoint again as it starts
-- (TppMain.OnAllocate). The ACC has no checkpoints: a slot saved there starts it again like the game does.
local function load_other_mission(slot)
  local current = vars.missionCode
  call("TppQuest.OnMissionGameEnd", function() TppQuest.OnMissionGameEnd() end)
  call("TppWeather.OnEndMissionPrepareFunction", function() TppWeather.OnEndMissionPrepareFunction() end)
  call("TppMission.SafeStopSettingOnMissionReload", function() TppMission.SafeStopSettingOnMissionReload() end)
  TppSave.VarRestoreOnContinueFromCheckPoint()
  if slot.missionCode ~= 0 and vars.missionCode ~= slot.missionCode then
    log(string.format("missao restaurada %s, esperada %s", tostring(vars.missionCode), tostring(slot.missionCode)))
    vars.missionCode = slot.missionCode
  end
  local locationName = TppPackList.GetLocationNameFormMissionCode(vars.missionCode)
  local location = locationName and TppDefine.LOCATION_ID[locationName]
  if location and vars.locationCode ~= location then
    log(string.format("local restaurado %s, esperado %s", tostring(vars.locationCode), tostring(location)))
    vars.locationCode = location
  end
  if TppMission.IsHelicopterSpace(vars.missionCode) then
    gvars.sav_varRestoreForContinue = false
    TppMain.ReservePlayerLoadingPosition(TppDefine.MISSION_LOAD_TYPE.MISSION_RESTART)
  else
    TppSave.ReserveVarRestoreForContinue()
    TppMain.ReservePlayerLoadingPosition(TppDefine.MISSION_LOAD_TYPE.CONTINUE_FROM_CHECK_POINT)
  end
  TppUiStatusManager.SetStatus("AnnounceLog", "INVALID_LOG")
  log(string.format("carregando a missao %d (local %s) a partir da %d", vars.missionCode, tostring(vars.locationCode), current))
  if gvars.str_storySequence >= TppDefine.STORY_SEQUENCE.CLEARD_ESCAPE_THE_HOSPITAL then
    TppMission.RequestLoad(vars.missionCode, current, { force = true })
  else
    TppMission.Load(vars.missionCode, current, { force = true })
  end
end

local function step()
  local flow = this.flow
  if not flow then
    return
  end
  flow.wait = (flow.wait or 0) + 1
  if flow.state == "openPage" then
    this.flow = nil
    open_page(flow.kind, flow.page)
  elseif flow.state == "question" then
    if TppUiCommand.IsShowPopup() then
      return
    end
    local select = TppUiCommand.GetPopupSelect()
    mgsv_restore_text(KEY.question)
    this.flow = nil
    if select == Popup.SELECT_OK then
      flow.onYes()
    end
  elseif flow.state == "checkpoint" then
    -- TppSave.DoSave (above) moves on to "checkpointSaving"; if the save never comes, the game refused it here.
    if flow.wait > 8 then
      announce(texts().saveFailed)
      log("checkpoint recusado pelo jogo")
      this.flow = nil
    end
  elseif flow.state == "checkpointSaving" then
    if not busy() then
      TppScriptVars.WriteSlotToFile(SAVING, FILE_PREFIX .. tostring(flow.id), false)
      flow.state = "writing"
      flow.wait = 0
    end
  elseif flow.state == "writing" then
    if busy() then
      return
    end
    this.flow = nil
    local result = TppScriptVars.GetLastResult()
    if result == TppScriptVars.RESULT_OK and mgsv_slot_readable and not mgsv_slot_readable(FILE_PREFIX .. tostring(flow.id)) then
      log("falha ao gravar " .. slot_word(flow.id) .. ": arquivo com tamanho diferente do save do jogo")
      result = "tamanho"
      if flow.action == "auto" then
        local slots = read_slots()
        slots[AUTO] = nil
        write_slots(slots)
      end
    end
    if result == TppScriptVars.RESULT_OK then
      local slots = read_slots()
      slots[flow.id] = { missionCode = vars.missionCode, date = mgsv_now(), label = mission_label(vars.missionCode) }
      write_slots(slots)
      if flow.action ~= "auto" then
        announce(string.format(texts().saved, slot_word(flow.id)))
        this.autoPending = nil
        this.autoSkipUntil = os.clock() + 5
      end
      log(slot_word(flow.id) .. " salvo (missao " .. tostring(vars.missionCode) .. ")")
    elseif flow.action ~= "auto" then
      announce(texts().saveFailed)
      log("falha ao gravar " .. slot_word(flow.id) .. ": resultado " .. tostring(result))
    end
  elseif flow.state == "reading" then
    if busy() then
      return
    end
    this.flow = nil
    local word = slot_word(flow.id)
    local result = TppScriptVars.GetLastResult()
    if result ~= TppScriptVars.RESULT_OK and result ~= TppScriptVars.RESULT_ERROR_LOAD_BACKUP then
      announce(string.format(texts().loadFailed, word))
      log("falha ao ler " .. word .. ": resultado " .. tostring(result))
      return
    end
    local version = TppSave.CheckGameDataVersion()
    if version ~= TppDefine.SAVE_FILE_LOAD_RESULT.OK then
      announce(string.format(texts().loadFailed, word))
      log(word .. " com versao incompativel: " .. tostring(version))
      return
    end
    TppSave.CopyGameDataFromSavingSlot()
    if flow.slot.missionCode == vars.missionCode and not in_helicopter_space() then
      log(word .. " lido; reiniciando do checkpoint dele")
      TppMission.ContinueFromCheckPoint()
    else
      TppUI.FadeOut(TppUI.FADE_SPEED.FADE_NORMALSPEED)
      this.flow = { action = "load", id = flow.id, slot = flow.slot, state = "otherMission", wait = 0 }
    end
  elseif flow.state == "otherMission" then
    if flow.wait >= 2 then -- the fade out is over
      this.flow = nil
      local ok, err = pcall(load_other_mission, flow.slot)
      if not ok then
        log("erro ao carregar outra missao: " .. tostring(err))
        TppUI.FadeIn(TppUI.FADE_SPEED.FADE_NORMALSPEED)
        announce(string.format(texts().loadFailed, slot_word(flow.id)))
      end
    end
  end
end

-- ---------------------------------------------------------------------------------------------------------------
-- Entry points

-- SAVE / LOAD on the game's page, and the entries of that page (hub)
local function open_list(kind)
  if kind == "load" or kind == "delete" then
    if kind == "load" and online() then
      announce(texts().online)
      return
    end
    if next(read_slots()) == nil then
      announce(texts().noSlots)
      return
    end
  end
  this.flow = { state = "openPage", kind = kind, page = 1 }
end

function this.OnItem()
  if this.flow or gvars.ini_isTitleMode then
    return
  end
  TppUI.FadeIn(TppUI.FADE_SPEED.FADE_NORMALSPEED) -- some items fade out before sending their message
  open_list("hub")
end

-- An entry of the slot list (page: a list closed a moment ago, else the open one)
function this.OnPageItem(index, page)
  page = page or this.page
  local entry = page and page.entries[index]
  close_page()
  this.lastPage = nil
  if entry and entry.kind == "checkpoint" then
    -- What the game's own item does (TppMission: PauseMenuCheckpoint -> ContinueFromCheckPoint)
    log("lista de slots: checkpoint")
    local ui = TppMission.messageExecTable and TppMission.messageExecTable[S32("UI")]
    local receivers = ui and ui[S32("PauseMenuCheckpoint")]
    local original = receivers and receivers.mgsvSavesOriginal
    if original then
      return original()
    end
    return TppMission.ContinueFromCheckPoint()
  end
  TppUI.FadeIn(TppUI.FADE_SPEED.FADE_NORMALSPEED)
  if not entry or this.flow then
    return
  end
  log(string.format("lista de slots: escolha %d (%s %s)", index, entry.kind, tostring(entry.id or entry.target or entry.page or "")))
  if entry.kind == "save" then
    start_save(entry.id)
  elseif entry.kind == "quick" then
    start_save(QUICK)
  elseif entry.kind == "load" then
    start_load(entry.id)
  elseif entry.kind == "delete" then
    delete_slot(entry.id)
  elseif entry.kind == "goto" then
    open_list(entry.target)
  elseif entry.kind == "more" then
    this.flow = { state = "openPage", kind = page.kind, page = entry.page }
  end
end

-- A guarded confirmation on screen (src/keys.*): QUICK SAVE's. Closed at once (a popup closed by code counts as NO:
-- the game does nothing more), then the menu closes and the quick save is made, as with the hotkey.
function this.OnConfirmation(key)
  local page = this.page
  if page and page.quickKey == key then
    TppUiCommand.ErasePopup()
    this.padSave = { wait = 0, escSent = false }
  end
end

-- Hotkeys (src/keys.*)
local function can_use_keys(evenPaused)
  return this.active and not this.flow and not this.page and (evenPaused or this.paused == false) and
    not gvars.ini_isTitleMode and not TppUiCommand.IsShowPopup()
end

function this.OnKey(vk)
  if vk == PAD_START_HOLD then
    -- START held on a pad: its press opened the pause menu, which closes again before the quick save
    if this.padQuickSave and can_use_keys(true) then
      this.padSave = { wait = 0, escSent = false }
    end
    return
  end
  if not can_use_keys() then
    return
  end
  if vk == this.QUICK_SAVE_KEY then
    start_save(QUICK)
  elseif vk == this.QUICK_LOAD_KEY then
    if online() then
      announce(texts().online)
      return
    end
    local slot = read_slots()[QUICK]
    if not slot then
      announce(texts().noQuick)
      return
    end
    ask(string.format(texts().loadSlotConfirm, slot_title(QUICK), slot_label(slot, true)), function()
      start_load(QUICK)
    end)
  end
end

-- START held on a pad, or QUICK SAVE chosen in the menu: the pause menu is closed (Esc), then the quick save is made.
local function pad_quick_save()
  local pending = this.padSave
  if not pending then
    return
  end
  pending.wait = pending.wait + 1
  if this.paused then
    if not pending.escSent then
      pending.escSent = true
      mgsv_press_key(VK_ESCAPE, 0)
    elseif pending.wait > 10 then
      this.padSave = nil -- the menu did not close
    end
    return
  end
  this.padSave = nil
  if this.page then
    close_page() -- the list the menu showed (QUICK SAVE chosen in it): closed, without the pause menu coming back
  end
  if not this.flow then
    start_save(QUICK)
  end
end

-- The game's own save (an autosave: seen in TppSave.DoSave) is still in its saving slot: written again as the
-- automatic save, right away (a few seconds later the slot may hold something else: a slot written after a read is not
-- readable), at most every 30 s. Not right after a load (the game saves the loaded game then).
local AUTO_COPY_INTERVAL = 30
local AUTO_COPY_WINDOW = 5

local function copy_autosave()
  if not this.autoPending or this.flow or this.page or busy() then
    return
  end
  local now = os.clock()
  if this.autoSkipUntil and now < this.autoSkipUntil then
    this.autoPending = nil
    return
  end
  if now - this.autoPending > AUTO_COPY_WINDOW or (this.autoLast and now - this.autoLast < AUTO_COPY_INTERVAL) then
    this.autoPending = nil -- too late, or too soon after the last copy: this save is not copied
    return
  end
  this.autoPending = nil
  this.autoLast = now
  this.flow = { action = "auto", id = AUTO, state = "writing", wait = 0 }
  TppScriptVars.WriteSlotToFile(SAVING, FILE_PREFIX .. AUTO, false)
end

function this.Tick()
  if rawget(_G, "gvars") == nil then -- the game's script vars do not exist yet (early boot)
    return
  end
  this.ticks = (this.ticks or 0) + 1
  update_pause()
  if detect_language() and this.active then
    take_menu_texts()
  end
  hook_item_messages()
  local ok, err = pcall(pad_quick_save)
  if not ok then
    log("erro no save rapido do controle: " .. tostring(err))
    this.padSave = nil
  end
  ok, err = pcall(copy_autosave)
  if not ok then
    log("erro no salvamento automatico: " .. tostring(err))
    this.autoPending = nil
  end
  ok, err = pcall(step)
  if not ok then
    log("erro: " .. tostring(err))
    this.flow = nil
    mgsv_restore_text(KEY.question)
  end
  ok, err = pcall(track_page)
  if not ok then
    log("erro na lista de slots: " .. tostring(err))
    this.page = nil
    give_back_page_texts()
  end
end

-- Console helpers (development): "lua return MgsvSaves.Status()"
function this.Status()
  return string.format("missao %s, ativo %s, idioma %s, pausa %s, fluxo %s, lista %s, slots %s", tostring(vars and vars.missionCode),
    tostring(this.active), tostring(this.language), tostring(this.paused),
    this.flow and ((this.flow.action or "") .. "/" .. this.flow.state) or "-",
    this.page and (this.page.kind .. "/" .. this.page.state) or "-", string.gsub(mgsv_read_file(SLOTS_FILE), "\n", " "))
end

-- ---------------------------------------------------------------------------------------------------------------
-- Settings

local NAMED_KEYS = { INSERT = 0x2D, HOME = 0x24, END = 0x23, PAGEUP = 0x21, PAGEDOWN = 0x22, DELETE = 0x2E, ENTER = 0x0D,
  SPACE = 0x20, NONE = 0 }

-- A key name as a virtual-key code (0: none), or nil if unknown.
local function key_code(name)
  name = string.upper(name)
  if NAMED_KEYS[name] then
    return NAMED_KEYS[name]
  end
  local f = string.match(name, "^F(%d+)$")
  if f and tonumber(f) >= 1 and tonumber(f) <= 12 then
    return 0x6F + tonumber(f)
  end
  if string.match(name, "^[A-Z0-9]$") then
    return string.byte(name)
  end
  return nil
end

local function read_config()
  local text = mgsv_read_file(CONFIG_FILE)
  if text == "" then
    text = DEFAULT_CONFIG
  end
  local added = false
  for _, addition in ipairs(CONFIG_ADDITIONS) do
    if not string.find(text, "\n" .. addition[1] .. "%s*=") then
      text = text .. (string.sub(text, -1) == "\n" and "" or "\n") .. addition[2]
      added = true
    end
  end
  if added or mgsv_read_file(CONFIG_FILE) == "" then
    mgsv_write_file(CONFIG_FILE, text)
  end
  this.padQuickSave = true
  for line in string.gmatch(text, "[^\r\n]+") do
    local name, value = string.match(line, "^%s*([%w_]+)%s*=%s*(.-)%s*$")
    if name == "slots" then
      local n = tonumber(value)
      if n and n >= 1 then
        this.SLOT_COUNT = math.min(math.floor(n), MAX_SLOTS)
      else
        log("mgsv_saves.ini: slots invalido: " .. value)
      end
    elseif name == "quick_save_key" or name == "quick_load_key" then
      local code = key_code(value)
      if code then
        this[name == "quick_save_key" and "QUICK_SAVE_KEY" or "QUICK_LOAD_KEY"] = code
      else
        log("mgsv_saves.ini: tecla desconhecida: " .. value)
      end
    elseif name == "pad_quick_save" then
      this.padQuickSave = string.lower(value) ~= "false"
    end
  end
  if mgsv_watch_pad then
    mgsv_watch_pad(this.padQuickSave)
  end
  if mgsv_watch_keys then
    mgsv_watch_keys(this.QUICK_SAVE_KEY, this.QUICK_LOAD_KEY) -- 0 (none) is left out by the dll
  end
  log(string.format("configuracao: %d slots, teclas 0x%02X / 0x%02X", this.SLOT_COUNT, this.QUICK_SAVE_KEY, this.QUICK_LOAD_KEY))
end

read_config()

-- A reload while playing (development, or a newer version): older versions had LOAD GAME on the game's page too.
if not this.page then
  mgsv_restore_text(KEY.loadItem)
  mgsv_restore_text(KEY.loadInfo)
  mgsv_restore_text(KEY.loadConfirm)
  if this.active then
    register_game_page(this.gamePage or vanilla_game_page())
  end
end

log("MgsvSaves carregado")
return "MgsvSaves pronto"
