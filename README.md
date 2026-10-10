# Vbot

A macro tool for Geometry Dash, built as a [Geode](https://geode-sdk.org) mod. Record your inputs, replay them frame by frame, and (soon) render the run to video. Vbot comes with its own compact macro format, **`.vb`**, which stores a recorded input in about 1.8 bytes.

**Platforms:** Windows and Android (64-bit) · **Game version:** 2.2081

> **Status:** early development. Recording and replaying work from the pause menu. Video rendering, hitbox view, hitbox trail, noclip and TPS bypass are planned and not in yet (see the [roadmap](#roadmap)).

---

## Features

- **Record** your inputs while you play.
- **Replay** a saved macro from frame 0.
- **Movable macro button:** a `macro_button.png` icon on the pause screen opens the macro menu. Tap it to open the menu, drag it to move it. Its position is saved.
- **Adjustable button** size and opacity in the mod settings.
- **`.vb` macro format:** tiny files, inputs only, easy to parse (full spec below).
- **FFmpeg rendering** (planned) through [FFmpeg API](https://geode-sdk.org/mods/eclipse.ffmpeg-api). If it isn't installed, Vbot shows a popup telling you to install it instead of failing.

## Using it

1. Pause a level and tap the **Vb** button.
2. **Record:** tap **Record**, restart the level, and play. Tap **Record** again to stop and save. Every restart clears the macro and records the new attempt, so the saved macro is your last attempt.
3. **Replay:** tap **Replay** to load the saved macro, then restart the level to play it back from frame 0.
4. **Render:** checks that FFmpeg API is installed. The rendering itself isn't finished yet.

The macro is saved as `last.vb` in the mod's save folder.

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

Builds run in GitHub Actions on every push (Windows and Android64). Open the **Actions** tab, pick the latest run, and download **Build Output** to get the `.geode` file.

To build locally you need the [Geode SDK](https://docs.geode-sdk.org/getting-started/) and the Geode CLI:

```
geode build
```

## Roadmap

- [x] Input recorder and replayer
- [x] `.vb` format
- [x] Movable, resizable macro button
- [ ] FFmpeg video rendering
- [ ] Hitbox view and hitbox trail
- [ ] Noclip
- [ ] TPS bypass

Noclip and TPS changes count as cheats. When they arrive, Vbot will stop runs from counting while they're on.

## Credits

- Made by [T1fyOwO](https://github.com/T1fyOwO)
- Built on the [Geode SDK](https://github.com/geode-sdk/geode)
- Video rendering by [FFmpeg API](https://geode-sdk.org/mods/eclipse.ffmpeg-api) by maxnut
