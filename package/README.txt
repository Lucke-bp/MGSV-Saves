MGSV Saves {VERSION}
Save slots for METAL GEAR SOLID V: THE PHANTOM PAIN (PC, Steam), in the game's own pause menu.
(Português mais abaixo.)


FEATURES
- SAVE / LOAD in the pause menu. It takes the place of CHECKPOINT, which moves into it:
  QUICK SAVE, CHECKPOINT, SAVE GAME, LOAD GAME, DELETE A SLOT.
- 10 save slots (up to 50, see Settings), a quick save, and an autosave: a copy of the game's
  own last automatic save, so a load can be undone.
- Saves shown the way the game names things: "Episode 4: C2W - 13/10 22:11".
- Load any slot from anywhere: missions, free roam, Mother Base, the helicopter (ACC).
- Quick save: F5 on the keyboard, or hold START on a controller for a second. Quick load: F9.
- The mod's texts follow the game's language (10 languages; Arabic shows English).
- Before every load, a copy of your game's save goes to mgsv_saves\backups\auto.


REQUIREMENTS
- The Steam version of the game, 1.0.15.4 (the current one). With any other version the mod
  stays inactive and changes nothing.


INSTALL
Copy MSACM32.dll and the mgsv_saves folder into the game's folder, next to mgsvtpp.exe
(Steam: right-click the game > Manage > Browse local files).


UNINSTALL
Delete MSACM32.dll and the mgsv_saves folder.
- The mod never writes the game's own save files: the game keeps loading your progress as usual
  (tested). Keep mgsv_saves\backups if you may need the copies made before loads.
- The slots' files (MGSV_SAVES_*, in your save folder and in Steam Cloud) stay, and the game
  ignores them. To remove them, delete the slots in the game (LOAD... DELETE A SLOT) first.


IMPORTANT
- Loading works offline only. Mother Base data is synchronized with Konami's servers (FOB):
  loading an older slot brings your whole progress back to that point, Mother Base included,
  and going online afterwards synchronizes it with the servers. If you play FOB, use it at
  your own risk.
- Loading a slot counts as a "continue from checkpoint" for the mission's rank.
- Saving follows the game's own rules: not during an alert, not in the helicopter during a
  mission, not during cutscenes, in the air or in a vehicle ("cannot be saved right now").
- Missions without checkpoints have no SAVE / LOAD item.
- Loading a slot makes it your current progress: the game saves it as its own save.
- Metal Gear Online (mgsvmgo.exe) also loads MSACM32.dll. There the mod does nothing: it only
  passes the calls on to Windows' own msacm32.dll.
- Other mods: no file conflict with mods that are a dinput8.dll (Infinite Heaven's IHHook,
  tpp-mod) or with ReShade. Not tested together with Infinite Heaven.


SETTINGS
mgsv_saves\mgsv_saves.ini (made on the first run):
  slots=10             number of save slots (1 to 50)
  quick_save_key=F5    quick save key: F1-F12, A-Z, 0-9, INSERT, HOME, END, PAGEUP, PAGEDOWN,
  quick_load_key=F9    DELETE, or NONE
  pad_quick_save=true  hold START on a controller for a second: quick save


TROUBLESHOOTING
The mod writes a log: mgsv_saves\mgsv_saves.log (in Portuguese). Please include it with
any report.


HOW IT WORKS
The mod changes no game file and no game code. MSACM32.dll is loaded by the game at startup
(it passes Windows' audio compression calls on to the real msacm32.dll) and runs the game's
own script functions: the ones the game uses for its checkpoints, saves and pause menu. The
slots are written by the game itself, in its own format.


CREDITS
- TinManTex, for the readable copy of the game's scripts (mgsv-deminified-lua), used as
  reference.

LICENSE
MIT (LICENSE.txt).


============================================================================================
PORTUGUÊS

MGSV Saves {VERSION} - slots de save para METAL GEAR SOLID V: THE PHANTOM PAIN (PC, Steam),
no próprio menu de pausa do jogo.

RECURSOS
- SALVAR / CARREGAR no menu de pausa, no lugar do CHECKPOINT (que vai para dentro dele):
  SALVAR RÁPIDO, CHECKPOINT, SALVAR JOGO, CARREGAR JOGO, APAGAR UM SLOT.
- 10 slots (até 50), save rápido e salvamento automático (cópia do último autosave do jogo,
  para desfazer um carregamento).
- Carrega qualquer slot de qualquer lugar: missões, mundo aberto, Mother Base, helicóptero.
- Save rápido: F5 no teclado ou segurar START no controle por 1 segundo. Carregar: F9.
- Antes de cada carregamento, uma cópia do save do jogo vai para mgsv_saves\backups\auto.

REQUISITOS
Versão Steam do jogo, 1.0.15.4 (a atual). Em outra versão o mod fica inativo.

INSTALAR
Copie MSACM32.dll e a pasta mgsv_saves para a pasta do jogo, ao lado do mgsvtpp.exe.

DESINSTALAR
Apague MSACM32.dll e a pasta mgsv_saves. O mod nunca grava nos arquivos de save do jogo: o
jogo continua carregando seu progresso normalmente (testado). Os arquivos dos slots
(MGSV_SAVES_*) ficam na pasta de saves e na Steam Cloud, e o jogo os ignora; para removê-los,
apague os slots no jogo antes.

IMPORTANTE
- Carregar só funciona off-line. Carregar um slot antigo volta todo o progresso àquele ponto,
  Mother Base incluída; ao ficar online depois, o jogo sincroniza com os servidores da Konami.
  Se você joga FOB, use por sua conta e risco.
- Carregar um slot conta como "continuar do checkpoint" na nota da missão.
- Salvar segue as regras do jogo: não em alerta, em cutscenes, no ar ou em veículos.
- No Metal Gear Online (mgsvmgo.exe) a DLL também é carregada, mas não faz nada.

CONFIGURAÇÃO: mgsv_saves\mgsv_saves.ini. LOG: mgsv_saves\mgsv_saves.log.
