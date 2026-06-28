# Moonglass

**Luminance-keyed window transparency for KWin (Plasma 6).**

Moonglass is a KWin compositor effect that fades each window pixel by its
brightness: dark pixels become transparent, bright pixels (text, UI accents)
stay opaque. The result is a window whose background dissolves into the desktop
while its content keeps floating, readable, on top.

Unlike ordinary terminal/window transparency — which dims the *whole* window by
a uniform alpha — Moonglass keys on per-pixel luminance, so a dark terminal
shows only its text, not a translucent gray slab. It also clips the windows
behind a Moonglass window so you see the desktop through the transparent areas,
not a soup of other app windows.

![Moonglass demo](docs/moonglass-demo.gif)

## Features

- Per-pixel luminance keying via a custom GLSL shader (real source-over alpha,
  not additive blending).
- Configurable opaque / transparent brightness thresholds with a linear fade
  between them.
- Optional solid background layer (color + opacity) behind faded pixels.
- Overlap clipping so transparent areas reveal the desktop, not lower windows.
- Per-window toggle (default **Meta+Z**).
- Optional auto-apply to new windows, with per-app default-on / default-off
  rules and an interactive "pick a window" rule helper.
- Color-management correct: routes through KWin's own brightness / saturation /
  HDR / ICC shader helpers, so Moonglass windows match normal ones.

## Requirements

- **Plasma 6 / KWin 6** on Wayland. (Plasma 5 is not supported — the effect API
  differs.)
- An OpenGL compositing backend.

## Installing

### Prebuilt (recommended)

KWin effects are compiled C++ and have **no stable ABI** — a binary built for
one KWin version is rejected by another. So prebuilt binaries are published
per KWin version. The installer picks the matching one:

```sh
curl -fsSL https://raw.githubusercontent.com/eric-frost/moonglass/main/install.sh | sh
```

The script detects your distro and KWin version and installs the matching
release asset, or falls back to a source build if none matches.

> **Platform support.** Binaries are verified on **Kubuntu**. Fedora and Arch
> builds are produced by CI and compile cleanly, but are *community-tested* —
> CI cannot run a compositor to verify runtime behavior. If a prebuilt doesn't
> match your KWin, the installer builds from source.

### From source

Needs the KWin and KF6 development packages (see
[`.github/workflows/build.yml`](.github/workflows/build.yml) for the exact
package names per distro).

```sh
cmake -B build -S . -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build -j
sudo cmake --install build
qdbus6 org.kde.KWin /KWin org.kde.KWin.replace   # reload compositor
```

After a KWin update the binary may become unsupported — rebuild and reinstall.
Packaged installs (PPA/AUR) handle this for you.

## Using it

- **Meta+Z** toggles Moonglass on the focused window.
- Configure under **System Settings → Window Management → Desktop Effects →
  Moonglass** (the wrench/settings icon): thresholds, background, clipping,
  auto-apply, and per-app rules.

## Why "no stable ABI" matters

KWin checks the effect's API version at load time and refuses a mismatch, so a
single binary can't span versions. Moonglass deals with this two ways:

1. **Packaged installs** (PPA `.deb`, AUR) depend on the exact KWin version and
   are rebuilt by their maintainers when KWin updates.
2. **CI** builds binaries per target so the installer can fetch a match instead
   of asking you to install a toolchain.

If you build from source, just rebuild after KWin upgrades.

## License

GPL-3.0-or-later. See [LICENSE](LICENSE).
