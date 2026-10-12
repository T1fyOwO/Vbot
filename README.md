# Vbot

A macro tool for Geometry Dash, built as a [Geode](https://geode-sdk.org) mod. Record your inputs, replay them frame by frame, and (soon) render the run to video. Vbot comes with its own compact macro format, **`.vb`**, which stores a recorded input in about 1.8 bytes.

**Platforms:** Windows and Android (64-bit) · **Game version:** 2.2081

> **Status:** early development. Recording and replaying work from the pause menu. Version 1.0.2 is in development and adds a full menu, noclip, hitboxes and a TPS bypass; video rendering and the hitbox trail are still to come (see the [roadmap](#roadmap)).

---

## Features

- **Record** your inputs while you play.
- **Replay** a saved macro from frame 0.
- **Movable macro button:** a `macro_button.png` icon on the pause screen opens the macro menu. Tap it to open the menu, drag it to move it. Its position is saved.
- **Adjustable button** size and opacity in the mod settings.
- **`.vb` macro format:** tiny files, inputs only, easy to parse (full spec below).
- **Full menu** *(1.0.2, in development)*: one popup with three tabs, **Macro**, **Cheats** and **Render**. Every value is saved as soon as you change it.
- **Custom macro names** *(1.0.2)*: save and load macros by name, with arrows to flip through the ones you already have.
- **Noclip**, **hitboxes** and an experimental **TPS bypass** *(1.0.2)*.
- **Render settings** *(1.0.2)*: resolution, render FPS, bitrate, codec and output name. The renderer itself comes in the next build.
- **FFmpeg rendering** (planned) through [FFmpeg API](https://geode-sdk.org/mods/eclipse.ffmpeg-api). If it isn't installed, Vbot shows a popup telling you to install it instead of failing.

## Installing

1. Install [Geode](https://geode-sdk.org) for Geometry Dash 2.2081 (Windows or Android).
2. Download the latest `.geode` file from the [Releases page](https://github.com/T1fyOwO/Vbot/releases).
3. Put it in Geode's `mods` folder (or open the file with Geode), then restart the game.

## Using it

1. Pause a level and tap the **Vb** button. In 1.0.2 this opens the full menu; in 1.0.1 it opens a small Record / Replay / Render list.
2. **Record:** tap **Record**, restart the level, and play. Recording keeps going across deaths until you tap **Record** again, which stops it and saves the macro. In practice mode, respawning from a checkpoint cuts the macro back to that checkpoint and keeps recording, so the macro always ends up as one clean run. Restarting from the very beginning starts a fresh macro.
3. **Replay:** tap **Replay** to load the saved macro, then restart the level to play it back from frame 0.
4. **Render:** checks that FFmpeg API is installed. The rendering itself isn't finished yet.

**Where files go.** In 1.0.2, macros are saved as `config/t1fy.vbot/Macros/<name>.vb` (inside your Geode folder), and videos will go in `config/t1fy.vbot/Renders`. Type a name in the **Macro name** box to save or load a specific macro. In 1.0.1, the macro is a single `last.vb` in the mod's save folder.

### The menu (1.0.2)

| Tab | What's in it |
| --- | --- |
| **Macro** | macro name box with previous/next arrows, **Record**, **Replay**, **Stop**, and a status line |
| **Cheats** | **Noclip**, **Hitboxes**, **TPS Bypass** (experimental) and the TPS value |
| **Render** | width, height, render FPS, bitrate (Mbps), codec, output name, and the **Render** button |

**Cheats.** Noclip and any TPS other than 240 are cheats. While either is on, finishing a level is treated like a test run and doesn't count. The TPS bypass is experimental: at 240 it runs the game's original code untouched, so leave it at 240 for normal play. TPS must be 240 or higher. More TPS makes physics more precise, it does not make the game faster, so the level looks the same. The Cheats tab shows the measured physics steps per second and how many hitbox vertices were drawn, so you can check that both are working. Macros remember the TPS they were recorded at, and Vbot warns you if you replay one at a different TPS.

### Settings

| Setting | What it does | Default |
| --- | --- | --- |
| Button size | Size of the macro button on screen, in pixels | 60 |
| Button opacity | How see-through the button is, in percent | 100 |

To move the button, drag it on the pause screen. Its position is saved between sessions.

## The `.vb` format

Binary (everything is a plain byte or a varint, so byte order never matters). Vbot stores **inputs only**, not player positions, so replays rely on the game's physics behaving the same way each time.

```
"VBOT"          4 bytes   magic
version         1 byte    currently 1
fps             varint    physics ticks per second (240 by default)
event count     varint    number of inputs that follow
events          count x varint
```

Each event is one varint:

```
(delta << 4) | (button << 2) | (down << 1) | player
```

| Field | Meaning |
| --- | --- |
| `delta` | frames since the previous input (the first input counts from frame 0) |
| `button` | `0` jump, `1` left, `2` right |
| `down` | `1` press, `0` release |
| `player` | `0` player 1, `1` player 2 |

Varints are LEB128: 7 bits per byte, with the top bit set when more bytes follow. A gap of fewer than 8 frames fits in a single byte per input.

The reference reader and writer is [`src/vb.hpp`](src/vb.hpp), a single header with no dependencies. It rejects empty, truncated and corrupted files.

## Building

Every push is built by GitHub Actions (Windows and Android64). Those build files need a GitHub login to download and they expire, so to just use the mod, get the `.geode` from the [Releases page](https://github.com/T1fyOwO/Vbot/releases) instead.

To build locally you need the [Geode SDK](https://docs.geode-sdk.org/getting-started/) and the Geode CLI:

```
geode build
```

## Roadmap

- [x] Input recorder and replayer
- [x] `.vb` format
- [x] Movable, resizable macro button
- [x] Full menu with Macro, Cheats and Render tabs *(1.0.2, in development)*
- [x] Custom macro names and the `config/t1fy.vbot` folders *(1.0.2)*
- [x] Noclip and hitboxes *(1.0.2)*
- [x] TPS bypass, experimental *(1.0.2)*
- [ ] FFmpeg video rendering
- [ ] Hitbox trail
- [ ] Optional position data for more accurate replays

Noclip and TPS changes count as cheats, so Vbot stops runs from counting while they're on.

## Credits

- Made by [T1fyOwO](https://github.com/T1fyOwO)
- Built on the [Geode SDK](https://github.com/geode-sdk/geode)
- Video rendering by [FFmpeg API](https://geode-sdk.org/mods/eclipse.ffmpeg-api) by maxnut
