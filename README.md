# Lyre

A keyboard-first Jellyfin music player for [Omarchy](https://omarchy.org).
Sign in to your Jellyfin server once, then browse albums, artists, and
playlists by their covers and play them without leaving the keyboard.

Lyre follows the desktop the way Omarchy's own apps do:

- **Theme, live.** Colors come from the current Omarchy theme's
  `colors.toml` (light themes included), corners follow Hyprland's
  `decoration:rounding`, the type uses the same `monospace` family as the bar
  and scales with the shell's `[font] base-size`. Switching themes repaints
  the open window.
- **Media keys and the shell.** Lyre publishes MPRIS as
  `org.mpris.MediaPlayer2.lyre`, so media keys, the Omarchy shell's media
  panel, and track notifications control it and show its covers.
- **Language, live.** Text follows Omarchy's Setup > Language; Danish,
  German, Spanish, and French ship today.
- **One window.** Launching Lyre again brings the running window forward.

## Keys

| Key | Does |
| --- | --- |
| `Space` | Play or pause |
| `N` / `P` | Next or previous track |
| `Shift+←` / `Shift+→` | Back or forward 10 seconds |
| `+` / `-` | Volume up or down |
| `M` | Mute |
| `S` | Shuffle |
| `R` | Repeat: off, queue, track |
| `Enter` | Open, or play from the selected track |
| `Shift+Enter` | Play the selected album, playlist, or artist without opening it |
| `A` / `Shift+A` | Add to the queue, or play next |
| `Delete` / `X` | Remove from the queue |
| `Alt+J` / `Alt+K` | Move down or up in the queue |
| `H` `J` `K` `L` | Move around, like the arrow keys |
| `Esc` / `Backspace` | Go back |
| `1`–`5` | Albums, Artists, Playlists, Queue, Now Playing |
| `G` | Go to the playing album |
| `/` | Search |
| `Ctrl+R` | Reload the library |
| `?` | Show every shortcut |
| `Q` | Quit |

## Build and install

Everything Lyre needs is part of Omarchy (`qmake6`, `make`, Qt 6 with
Multimedia's FFmpeg backend).

```sh
bin/build      # build/lyre
bin/test       # unit tests, offscreen
bin/install    # makepkg -fsi: installs lyre, its icon, and its launcher entry
```

After `bin/install`, Lyre is in the app launcher (`Super + Space`).

## What it keeps

- `~/.config/lyre/session.json` (mode 0600): the server address, user name,
  this device's id, and the access token Jellyfin issued. Lyre never stores
  your password, and Sign out revokes the token on the server.
- `~/.config/lyre/lyre.conf`: the volume.
- `~/.cache/lyre/`: downloaded covers, so each one is fetched once.

## How it plays

Tracks stream directly from the server in their original format
(`/Audio/{id}/stream?static=true`) and decode locally through Qt
Multimedia's FFmpeg backend, so the server never transcodes. Lyre reports
playback to Jellyfin (start, progress, pause, stop), so play counts and the
dashboard's Now Playing work like Jellyfin's own clients. Lyre filters out
FFmpeg's stream summary, which would otherwise write the stream URL and its
token to the journal.

## License

MIT. See [LICENSE](LICENSE), which also credits the Omacut code the theme
and translator follow and the Material Icons Lyre uses.
