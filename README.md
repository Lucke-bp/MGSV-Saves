# MGSV Saves

Save slots in the pause menu of **METAL GEAR SOLID V: THE PHANTOM PAIN** (PC, Steam, game version 1.0.15.4).

Download and screenshots: [Nexus Mods](https://www.nexusmods.com/metalgearsolidvtpp/mods/2654)

- SAVE / LOAD in the pause menu: quick save, checkpoint, save slots, load, delete.
- Load any slot from anywhere: missions, free roam, Mother Base, the helicopter.
- Autosave, F5 / F9 hotkeys, hold START on a controller for a quick save.
- Mission names the way the game shows them, in the game's own language.

## How it works

The mod is a proxy `MSACM32.dll`. The game loads it at startup; it passes Windows' audio compression calls on to the
real `msacm32.dll` and runs the game's own Lua functions (checkpoints, saves, the pause menu). It changes no game file
and patches no game code. It uses no networking libraries and does nothing in Metal Gear Online (`mgsvmgo.exe`).

- `src/` the DLL (C++20): proxy and exports, access to the game's Lua, text replacement, key and controller input.
- `lua/` the mod's scripts (`mgsv_saves.lua`, translations in `mgsv_saves_lang.lua`), copied next to the DLL.
- `package/` the files that go into the Nexus zip (README, license).

## Build

Needs Visual Studio with the C++ x64 tools (MSVC and `ml64`).

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build.ps1     # build\msacm32.dll
powershell -ExecutionPolicy Bypass -File scripts/package.ps1   # dist\MGSV_Saves_<version>.zip
```

`scripts/deploy.ps1` copies the build into the game folder (the game must be closed).

## Compatibility

Only game version 1.0.15.4: with any other version the mod stays inactive. Works with Infinite Heaven and IHHook
(IHHook hooks some of the game's Lua functions; the mod follows those hooks).

## Credits and license

Made by Luckebp, with the help of [Claude](https://claude.com/claude-code) (Anthropic).

[MIT](LICENSE). Thanks to TinManTex for [mgsv-deminified-lua](https://github.com/TinManTex/mgsv-deminified-lua),
the readable copy of the game's scripts used as reference.
