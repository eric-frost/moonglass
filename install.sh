#!/bin/sh
# Moonglass installer.
# Detects your distro + KWin version, downloads the matching prebuilt release
# asset, and installs it. Falls back to a source build when no asset matches.
# Usage:  curl -fsSL .../install.sh | sh
#         ./install.sh --from-source     # force a local build
set -eu

REPO="${MOONGLASS_REPO:-eric-frost/moonglass}"   # override with MOONGLASS_REPO=owner/name
API="https://api.github.com/repos/$REPO"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

say() { printf '\033[1;36m=>\033[0m %s\n' "$*"; }
die() { printf '\033[1;31merror:\033[0m %s\n' "$*" >&2; exit 1; }

# 1. identify distro
[ -r /etc/os-release ] || die "cannot read /etc/os-release"
. /etc/os-release
DISTRO="$ID"; DISTROVER="${VERSION_ID:-rolling}"

# 2. identify KWin version (e.g. "kwin 6.6.5" -> 6.6.5)
kwinver() {
    for bin in kwin_wayland kwin_x11 kwin; do
        if command -v "$bin" >/dev/null 2>&1; then
            "$bin" --version 2>/dev/null | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -1 && return 0
        fi
    done
    return 1
}
KWIN="$(kwinver || true)"
[ -n "$KWIN" ] || die "KWin not found — is this a Plasma 6 session?"
case "$KWIN" in 6.*) ;; *) die "Moonglass needs KWin 6 (Plasma 6); found $KWIN" ;; esac
say "Detected: $DISTRO $DISTROVER, KWin $KWIN"

ASSET="moonglass-${DISTRO}-${DISTROVER}-kwin${KWIN}.tar.gz"

reload_kwin() {
    if command -v qdbus6 >/dev/null 2>&1 && [ -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ]; then
        say "Reloading KWin"
        qdbus6 org.kde.KWin /KWin org.kde.KWin.replace 2>/dev/null || \
            say "Could not reload automatically — log out/in to load Moonglass."
    else
        say "Log out and back in to load Moonglass."
    fi
}

build_from_source() {
    say "Building from source"
    command -v git >/dev/null 2>&1 || die "git required for source build"
    command -v cmake >/dev/null 2>&1 || die "cmake required — install the KWin/KF6 -dev packages (see README)"
    git clone --depth 1 "https://github.com/$REPO" "$TMP/src"
    cmake -B "$TMP/src/build" -S "$TMP/src" -DCMAKE_INSTALL_PREFIX=/usr >/dev/null
    cmake --build "$TMP/src/build" -j"$(nproc)"
    sudo cmake --install "$TMP/src/build"
    reload_kwin
    say "Done (source build)."
}

# 3. force-source flag
if [ "${1:-}" = "--from-source" ]; then build_from_source; exit 0; fi

# 4. try the matching prebuilt asset from the latest release
say "Looking for prebuilt: $ASSET"
URL="$(curl -fsSL "$API/releases/latest" 2>/dev/null \
        | grep -oE "https://[^\"]*${ASSET}" | head -1 || true)"
if [ -z "$URL" ]; then
    say "No prebuilt for this distro/KWin combo."
    build_from_source
    exit 0
fi

# 5. download + install (tarball mirrors the / filesystem layout)
say "Downloading $URL"
curl -fsSL "$URL" -o "$TMP/$ASSET"
say "Installing (sudo)"
sudo tar -C / -xzf "$TMP/$ASSET"
reload_kwin
say "Installed Moonglass. Toggle a window with Meta+Z."
