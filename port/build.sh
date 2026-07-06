#!/usr/bin/env bash
# port/build.sh — build the standalone CamStreamEmu binary + arm64 .deb for the
# CardputerZero AppStore, inside an arm64 Debian Bookworm container (matches the
# device ABI; native on Apple Silicon).
#
#   ./port/build.sh
#     1. verify build (off-screen memory display) — runs headless, checks the
#        "camstreamemu-ui: ready" startup marker, exits on SIGTERM.
#     2. device build (-DPORT_FBDEV: lv_linux_fbdev + lv_evdev) — compile+link.
#     3. CPack -> port/dist/camstreamemu_<ver>_arm64.deb, then gate-check it.
#
# OSS (see THIRD_PARTY_NOTICES.md): LVGL (MIT), FreeType (FTL), GStreamer 1.0 +
# app + rtsp-server (LGPLv2.1) and runtime plugins base/good/bad/libav (LGPL).
# The device encoder is v4l2 hardware H.264 + LGPL gst-libav for MPEG-2; the GPL
# x264 encoder is a dev-host-only fallback and is NOT a package dependency.
set -euo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"

docker run --rm --platform linux/arm64 -v "$REPO":/work -w /work debian:bookworm bash -euo pipefail -c '
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq >/dev/null
apt-get install -y -qq --no-install-recommends \
  cmake ninja-build build-essential pkg-config git ca-certificates file dpkg-dev \
  libfreetype-dev libpng-dev libjpeg-dev zlib1g-dev libevdev-dev \
  libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev libgstrtspserver-1.0-dev >/dev/null

PORT=/work/port
LVGL="$PORT/.lvgl"
[ -f "$LVGL/lvgl.h" ] || git clone --depth 1 -b v9.5.0 https://github.com/lvgl/lvgl.git "$LVGL" >/dev/null 2>&1

CONF="$PORT/lv_conf.h"
cp "$LVGL/lv_conf_template.h" "$CONF"
sed -i \
  -e "0,/#if 0/s//#if 1/" \
  -e "s/#define LV_FONT_MONTSERRAT_12 0/#define LV_FONT_MONTSERRAT_12 1/" \
  -e "s/#define LV_FONT_MONTSERRAT_14 0/#define LV_FONT_MONTSERRAT_14 1/" \
  -e "s/#define LV_FONT_MONTSERRAT_16 0/#define LV_FONT_MONTSERRAT_16 1/" \
  -e "s/#define LV_FONT_MONTSERRAT_20 0/#define LV_FONT_MONTSERRAT_20 1/" \
  -e "s/#define LV_FONT_UNSCII_8  0/#define LV_FONT_UNSCII_8  1/" \
  -e "s/#define LV_USE_FREETYPE 0/#define LV_USE_FREETYPE 1/" \
  -e "s/#define LV_USE_LINUX_FBDEV      0/#define LV_USE_LINUX_FBDEV      1/" \
  -e "s/#define LV_USE_EVDEV    0/#define LV_USE_EVDEV    1/" \
  "$CONF"

cfg() { cmake -S "$PORT" -B "$1" -G Ninja -DCMAKE_BUILD_TYPE=Release -DLVGL_DIR="$LVGL" ${2:-} >"$1/cfg.log" 2>&1 \
    || { echo "CONFIGURE FAILED"; tail -20 "$1/cfg.log"; exit 1; }; }
mkdir -p /tmp/bv /tmp/bf

echo "### 1. verify build (memory display, arm64) ###"
cfg /tmp/bv ""
cmake --build /tmp/bv -j"$(nproc)" >/tmp/bv/b.log 2>&1 \
  || { echo "BUILD FAILED"; grep -nE "error:|undefined reference|fatal error" /tmp/bv/b.log | head; exit 1; }
echo "verify binary: $(file -b /tmp/bv/camstreamemu | cut -d, -f1-2)"
CSE_DIR=/work/samples /tmp/bv/camstreamemu >/tmp/bv/run.log 2>&1 & PID=$!
sleep 2
kill -0 $PID 2>/dev/null && { kill -TERM $PID; sleep 1; }
kill -0 $PID 2>/dev/null && { echo "verify: DID NOT EXIT"; kill -9 $PID; exit 1; } || true
grep -q "camstreamemu-ui: ready" /tmp/bv/run.log \
  && echo "verify: ran (marker seen) + exited cleanly on SIGTERM" \
  || { echo "verify: startup marker missing"; cat /tmp/bv/run.log; exit 1; }

echo "### 2. device build (fbdev + evdev, arm64) ###"
cfg /tmp/bf "-DPORT_FBDEV=ON"
cmake --build /tmp/bf -j"$(nproc)" >/tmp/bf/b.log 2>&1 \
  || { echo "BUILD FAILED"; grep -nE "error:|undefined reference|fatal error" /tmp/bf/b.log | head; exit 1; }
echo "device binary: $(file -b /tmp/bf/camstreamemu | cut -d, -f1-2)"

echo "### 3. .deb (CPack) ###"
mkdir -p "$PORT/dist"
( cd /tmp/bf && cpack -G DEB >/tmp/bf/cpack.log 2>&1 ) || { echo "CPACK FAILED"; tail -20 /tmp/bf/cpack.log; exit 1; }
DEB=$(ls "$PORT"/dist/camstreamemu_*_arm64.deb | head -1)
echo "== $(basename "$DEB") =="
dpkg-deb -I "$DEB" | sed -n "1,20p"
echo "--- contents ---"; dpkg-deb -c "$DEB" | awk "{print \$6}" | grep -v "/$"
PKG=$(dpkg-deb -f "$DEB" Package); echo -n "pkg-name regex: "; echo "$PKG" | grep -qP "^[a-z0-9][a-z0-9.+\-]+\$" && echo "PASS ($PKG)" || echo "FAIL ($PKG)"
MAINT=$(dpkg-deb -f "$DEB" Maintainer); echo -n "maintainer email: "; echo "$MAINT" | grep -q "u44e@users.noreply.github.com" && echo "PASS" || echo "FAIL ($MAINT)"
dpkg-deb -c "$DEB" >/tmp/c.txt 2>/dev/null || true
echo -n "contains .desktop: "; grep -q "\.desktop\$" /tmp/c.txt && echo PASS || echo FAIL
echo -n "bundles samples: "; grep -q "share/samples/.*\.json\$" /tmp/c.txt && echo PASS || echo FAIL
echo "sha256: $(sha256sum "$DEB" | cut -d" " -f1)"
'
