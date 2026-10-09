-- Repair: restores Tpp.DoMessage (replaced by the first pause menu experiments with a wrapper whose original was lost:
-- _G refused the global that held it). Same code as the game's Tpp.DoMessage (Infinite Heaven version of Tpp.lua).
local function DoMessage(messageExecTable, CheckMessageOption, messageClass, messageId, arg0, arg1, arg2, arg3, strLogText)
  if not messageExecTable then
    return
  end
  local classMessages = messageExecTable[messageClass]
  if not classMessages then
    return
  end
  local messageIdRecievers = classMessages[messageId]
  if not messageIdRecievers then
    return
  end
  local unkBool = true
  if InfCore and InfCore.debugMode and Ivars and Ivars.debugMessages and Ivars.debugMessages:Get() == 1 then
    InfCore.PCall(Tpp.DoMessageAct, messageIdRecievers, CheckMessageOption, arg0, arg1, arg2, arg3, strLogText, unkBool)
  else
    Tpp.DoMessageAct(messageIdRecievers, CheckMessageOption, arg0, arg1, arg2, arg3, strLogText, unkBool)
  end
end
Tpp.DoMessage = DoMessage
local state = rawget(_G, "MgsvSaves")
if state then
  state.DoMessage = DoMessage
end
TppUI.FadeIn(TppUI.FADE_SPEED.FADE_NORMALSPEED)
return "Tpp.DoMessage restaurado", type(Tpp.DoMessageAct), type(rawget(_G, "InfCore"))
