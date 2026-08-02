#!/usr/bin/env bash
# build-emu.sh — build the CamStreamEmu LVGL UI as a .dylib for cardputer-emu.
#
#   ./build-emu.sh           # build only
#   ./build-emu.sh --shot    # build + headless screenshot -> build-emu/shot.bmp
#   ./build-emu.sh --run     # build + interactive emulator window
#
# The UI (src/app.c) links the reproduction engine (repro/profile/video_pipe/
# tts_wrap/rtsp_serve/mjpeg_serve) which needs GStreamer, plus the emulator's
# lv_sdl_keyboard for real keys. On macOS GStreamer runs videotestsrc+x264enc.
set -euo pipefail
APP="$(cd "$(dirname "$0")" && pwd)"
EMU="${EMU_DIR:-$HOME/cardputer-zero/emulator}"
EMUBIN="$EMU/build/cardputer-emu"
OUT="$APP/build-emu"
DYLIB="$OUT/libcamstreamemu.dylib"
mkdir -p "$OUT"

INC_COMPAT=()
[ -f "$EMU/src/emu_compat.h" ] && INC_COMPAT=(-include "$EMU/src/emu_compat.h")

GSTFLAGS=$(pkg-config --cflags --libs gstreamer-1.0 gstreamer-app-1.0 gstreamer-rtsp-server-1.0)

clang -dynamiclib -O2 -std=gnu11 -arch arm64 -fPIC \
  -DLV_CONF_INCLUDE_SIMPLE -DLV_LVGL_H_INCLUDE_SIMPLE -DAPP_EMU -DPS_TEST_HOOKS \
  "${INC_COMPAT[@]}" \
  -I "$EMU" -I "$EMU/lib" -I "$EMU/lib/lvgl" -I "$APP/emu" -I "$APP/src" -I "$APP" \
  $(pkg-config --cflags sdl2) \
  "$APP/src/app.c" "$APP/profile.c" "$APP/repro.c" "$APP/video_pipe.c" \
  "$APP/tts_wrap.c" "$APP/rtsp_serve.c" "$APP/mjpeg_serve.c" "$APP/h264_inject.c" \
  "$EMU/lib/lvgl/src/drivers/sdl/lv_sdl_keyboard.c" \
  $GSTFLAGS \
  -undefined dynamic_lookup -lpthread -lm \
  -o "$DYLIB"

echo "built: $DYLIB"

case "${1:-}" in
  --shot)
    SDL_VIDEODRIVER=dummy EMU_SHOT="$OUT/shot.bmp" EMU_SHOT_MS=1500 EMU_SHOT_QUIT=1 \
      CSE_DIR="$APP/samples" "$EMUBIN" "$DYLIB" || true
    ls -la "$OUT/shot.bmp" 2>/dev/null || echo "(no screenshot)"
    ;;
  --run)
    CSE_DIR="$APP/samples" "$EMUBIN" "$DYLIB" ;;
  *)
    echo "run:  CSE_DIR=$APP/samples $EMUBIN $DYLIB" ;;
esac
