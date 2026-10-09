-- Experiment 3 (in gameplay): the vanilla scripts keep Tpp.DoMessage in a local, so the handlers are changed in
-- TppMission.messageExecTable itself: every UI handler is wrapped to log, the extra items' messages are intercepted.
-- _G refuses new globals (Fox metatable): the mod's state lives in a table set with rawset.
local S32 = StrCode32 or Fox.StrCode32
local state = rawget(_G, "MgsvSaves")
if not state then
  state = { log = {} }
  rawset(_G, "MgsvSaves", state)
end
local function note(text)
  local log = state.log
  log[#log + 1] = text
  if #log > 50 then
    table.remove(log, 1)
  end
end

local names = {}
for _, name in ipairs{ "PauseMenuFinishFobManualPlaecementMode", "PauseMenuCheckpoint", "PauseMenuRestart", "PauseMenuReturnToTitle",
  "PauseMenuAbortMission", "PauseMenuAbortMissionGoToAcc", "PauseMenuRestartFromHelicopter", "PauseMenuReturnToMission" } do
  names[S32(name)] = name
end
local intercepted = {
  [S32("PauseMenuReturnToMission")] = true,
  [S32("PauseMenuRestartFromHelicopter")] = true,
  [S32("PauseMenuAbortMissionGoToAcc")] = true,
  [S32("PauseMenuFinishFobManualPlaecementMode")] = true,
}

local ui = TppMission.messageExecTable and TppMission.messageExecTable[S32("UI")]
if not ui then
  return "TppMission.messageExecTable sem a classe UI"
end
local wrapped = 0
for messageId, receivers in pairs(ui) do
  local name = names[messageId] or tostring(messageId)
  if receivers.func and not receivers.mgsv_saves_original then
    receivers.mgsv_saves_original = receivers.func
  end
  local original = receivers.mgsv_saves_original
  if original then
    receivers.func = function(arg0, arg1, arg2, arg3)
      note("UI msg " .. name .. " args " .. tostring(arg0) .. " " .. tostring(arg1))
      if intercepted[messageId] then
        TppUI.FadeIn(TppUI.FADE_SPEED.FADE_NORMALSPEED)
        TppUiCommand.AnnounceLogView("MGSV Saves: " .. name)
        return
      end
      return original(arg0, arg1, arg2, arg3)
    end
    wrapped = wrapped + 1
  end
end
return "handlers de UI envolvidos: " .. wrapped
