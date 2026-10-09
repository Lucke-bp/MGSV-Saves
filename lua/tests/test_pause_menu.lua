-- Experiment (in gameplay): adds pause menu items that do not belong to normal missions and logs the UI messages,
-- intercepting the ones those items send (the screen is faded back in, since the menu fades out before sending).
local S32 = StrCode32 or Fox.StrCode32
local UI = S32("UI")
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

if not mgsv_saves_DoMessage then
  mgsv_saves_DoMessage = Tpp.DoMessage
end
local original = mgsv_saves_DoMessage
Tpp.DoMessage = function(execTable, checkOption, messageClass, messageId, arg0, arg1, arg2, arg3, strLogText)
  if messageClass == UI then
    if execTable == TppMission.messageExecTable then
      mgsv_log("UI msg " .. (names[messageId] or tostring(messageId)) .. " args " .. tostring(arg0) .. " " .. tostring(arg1))
      if intercepted[messageId] then
        mgsv_log("interceptada: " .. names[messageId])
        TppUI.FadeIn(TppUI.FADE_SPEED.FADE_NORMALSPEED)
        TppUiCommand.AnnounceLogView("MGSV Saves: " .. names[messageId])
        return
      end
    elseif names[messageId] then
      mgsv_log("UI msg " .. names[messageId] .. " (outra tabela)")
    end
  end
  return original(execTable, checkOption, messageClass, messageId, arg0, arg1, arg2, arg3, strLogText)
end

TppUiCommand.RegisterPauseMenuPage{
  GamePauseMenu.RESTART_FROM_CHECK_POINT,
  GamePauseMenu.RETURN_TO_MISSION_FROM_EMERGENCY_MISSION,
  GamePauseMenu.RESTART_FROM_HELICOPTER,
  GamePauseMenu.ABORT_MISSION_RETURN_TO_ACC,
  GamePauseMenu.RETURN_TO_TITLE,
  GamePauseMenu.OPEN_OPTION_MENU,
}
TppUiCommand.AnnounceLogView("MGSV Saves: teste 2 do menu de pausa")
return "menu registrado (teste 2)"
