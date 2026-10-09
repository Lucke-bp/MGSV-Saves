-- Experiment (in gameplay): the two items the mod will use (emergency "return to mission" and FOB "finish placement
-- mode"), their messages intercepted in TppMission.messageExecTable (logged in MgsvSaves.log).
local S32 = Fox.StrCode32
local state = rawget(_G, "MgsvSaves")
local function note(text)
  local log = state.log or {}
  state.log = log
  log[#log + 1] = text
  mgsv_log(text)
end
local items = {
  [S32("PauseMenuReturnToMission")] = "SALVAR JOGO",
  [S32("PauseMenuFinishFobManualPlaecementMode")] = "CARREGAR JOGO",
}
local ui = TppMission.messageExecTable[S32("UI")]
for messageId, label in pairs(items) do
  local receivers = ui[messageId]
  if receivers then
    receivers.mgsv_saves_original = receivers.mgsv_saves_original or receivers.func
    receivers.func = function()
      note("item escolhido: " .. label)
      TppUI.FadeIn(TppUI.FADE_SPEED.FADE_NORMALSPEED)
      TppUiCommand.AnnounceLogView("MGSV Saves: " .. label)
    end
  else
    note("sem tratador para " .. label)
  end
end
TppUiCommand.RegisterPauseMenuPage{
  GamePauseMenu.RESTART_FROM_CHECK_POINT,
  GamePauseMenu.RETURN_TO_MISSION_FROM_EMERGENCY_MISSION,
  GamePauseMenu.ABORT_MISSION_PLACEMENT_MODE,
  GamePauseMenu.RETURN_TO_TITLE,
  GamePauseMenu.OPEN_OPTION_MENU,
}
return "menu de teste com SALVAR/CARREGAR registrado"
