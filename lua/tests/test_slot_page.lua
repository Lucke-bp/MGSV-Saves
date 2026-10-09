-- Test: a pause menu page made of the 8 items that only send a message to Lua, with marker texts.
--  - every item shows "TESTE n" (name), "Item de teste n: <enum>" (description) and "Confirmar teste n?"
--  - picking one logs "teste: item n (<message>)" and logs "teste: reabrir" on the next tick (tools/press_key.ps1 is
--    run by the watcher then, to open the menu again by an injected Esc)
--  - 2 s later the normal page is registered again and the texts are given back (does the open menu change?)
--  - every tick logs a few clocks for 90 s, to find one that stops while the game is paused
-- Run: luafile test_slot_page.lua ; finish early: lua MgsvSavesTest.Finish()
local S32 = Fox.StrCode32
local M = MgsvSaves
local T = rawget(_G, "MgsvSavesTest") or {}
rawset(_G, "MgsvSavesTest", T)

local ITEMS = {
  { "RESTART_FROM_CHECK_POINT", "PauseMenuCheckpoint", 0x39df1083, 0x7273aa3b, 0xA59C6214, 0x3F1CBE7A },
  { "RESTART_FROM_MISSION_START", "PauseMenuRestart", 0xd4387694, 0x97c8e4a4, 0xDC140814, 0xF33834F7 },
  { "RESTART_FROM_HELICOPTER", "PauseMenuRestartFromHelicopter", 0x9ee0bdc0, 0x1a731241, 0xA70CFCD8, 0x0ACDA646 },
  { "RETURN_TO_TITLE", "PauseMenuReturnToTitle", 0x6ce1889c, 0xd04ae17d, 0x9F92F91D, 0x940871D9 },
  { "RETURN_TO_MISSION_FROM_EMERGENCY_MISSION", "PauseMenuReturnToMission", 0x27b5469d, 0x45701dd2, 0x031F2DBC, 0x16344BFE },
  { "ABORT_MISSION", "PauseMenuAbortMission", 0x8cd08509, 0xfcb0dced, 0x57353167, 0x379D3400 },
  { "ABORT_MISSION_RETURN_TO_ACC", "PauseMenuAbortMissionGoToAcc", 0x7344633d, 0x5801b2a5, 0x57353167, 0x0894145C },
  { "ABORT_MISSION_PLACEMENT_MODE", "PauseMenuFinishFobManualPlaecementMode", 0x5df5e488, 0x3d5c47c6, 0xDD2762BA, 0x8156DE4C },
}

local function log(text)
  mgsv_log("teste: " .. text)
end

local function try(f, ...)
  local ok, value = pcall(f, ...)
  return ok and tostring(value) or "x"
end

local function clocks()
  return string.format("frame %s raw %s elapsed %s clock %s popup %s", try(function() return Time.GetFrameIndex() end),
    try(function() return Time.GetRawElapsedTimeSinceStartUp() end), try(function() return Time.GetFrameTime() end),
    try(function() return vars.clock end), try(function() return TppUiCommand.IsShowPopup() end))
end

local function vanilla_page()
  local P = GamePauseMenu
  local items = { P.RESTART_FROM_MISSION_START, P.RETURN_TO_TITLE, P.SIGN_IN, P.STORE_ITEM, P.CONTROLS_AND_TIPS_ITEM, P.OPEN_OPTION_MENU }
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

local function give_back_texts()
  for _, item in ipairs(ITEMS) do
    for k = 3, 5 do
      mgsv_restore_text(item[k])
    end
  end
end

function T.Finish()
  if not T.on then
    return "ja terminado"
  end
  T.on = false
  local ui = TppMission.messageExecTable[S32("UI")]
  for i, item in ipairs(ITEMS) do
    local receivers = ui[S32(item[2])]
    if receivers and T.previous and T.previous[i] then
      receivers.func = T.previous[i]
    end
  end
  if T.previousTick then
    M.Tick = T.previousTick
    T.previousTick = nil
  end
  if not T.normalAgain then
    give_back_texts()
    TppUiCommand.RegisterPauseMenuPage(vanilla_page())
  end
  log("fim")
  return "ok"
end

local function on_item(i)
  log(string.format("item %d (%s)", i, ITEMS[i][2]))
  TppUI.FadeIn(TppUI.FADE_SPEED.FADE_NORMALSPEED)
  if not T.reopenAt then
    T.reopenAt = T.ticks + 1
  end
end

local function test_tick()
  T.ticks = T.ticks + 1
  if T.ticks <= 180 then
    log("t" .. T.ticks .. " " .. clocks())
  end
  if T.reopenAt and T.ticks == T.reopenAt then
    log("reabrir")
  end
  if T.reopenAt and T.ticks == T.reopenAt + 4 and not T.normalAgain then
    give_back_texts()
    TppUiCommand.RegisterPauseMenuPage(vanilla_page()) -- through MgsvSaves' wrapper: SAVE/LOAD come back
    T.normalAgain = true
    log("pagina normal registrada de novo (o menu aberto mudou?)")
  end
  if T.ticks >= 240 then
    T.Finish()
  end
end

if T.on then
  T.Finish()
end
T.on, T.ticks, T.reopenAt, T.normalAgain, T.previous = true, 0, nil, false, {}

-- message ids: the enum objects' hashes in the exe vs Fox.StrCode32 of the names
for i, item in ipairs(ITEMS) do
  local found = {}
  for k = 3, 5 do
    found[#found + 1] = tostring(mgsv_set_text(item[k], ({ "TESTE " .. i, "Item de teste " .. i .. ": " .. item[1], "Confirmar teste " .. i .. "?" })[k - 2], true))
  end
  log(string.format("%d %s: hash %s (exe %08x), textos %s", i, item[2], string.format("%08x", S32(item[2])), item[6], table.concat(found, "/")))
end

local ui = TppMission.messageExecTable[S32("UI")]
for i, item in ipairs(ITEMS) do
  local receivers = ui[S32(item[2])]
  if receivers and receivers.func then
    local previous = receivers.func
    T.previous[i] = previous
    receivers.func = function(...)
      if T.on then
        on_item(i)
      else
        return previous(...)
      end
    end
  else
    log("sem receptor para " .. item[2])
  end
end

local page = {}
for _, item in ipairs(ITEMS) do
  page[#page + 1] = GamePauseMenu[item[1]]
end
M.originalRegisterPauseMenuPage(page)
T.previousTick = M.Tick
M.Tick = function()
  T.previousTick()
  local ok, err = pcall(test_tick)
  if not ok then
    log("erro " .. tostring(err))
  end
end
log("pagina de teste registrada; " .. clocks())
return "teste pronto"
