#!/usr/bin/env bash
# verify.sh — reproduction-fidelity check (no real hardware).
#
# For each sample profile, run CamStreamEmu to a local port with a fixed test
# source, receive+decode with GStreamer, and confirm the decoded video's
# width/height matches the profile. Proves the reproduced stream carries the
# right format for every delivery mode. Uses videotestsrc (CAMEMU_SRC); on a Pi
# the same pipeline uses libcamerasrc + v4l2 HW H.264.
set -o pipefail
APP="$(cd "$(dirname "$0")/.." && pwd)"
CLI="$APP/camstreamemu"
export CAMEMU_SRC="videotestsrc is-live=true pattern=ball"
[ -x "$CLI" ] || { echo "build first: make"; exit 1; }
command -v gst-launch-1.0 >/dev/null || { echo "need gstreamer"; exit 1; }
PORT=5599
fail=0

# width/height straight from the profile JSON
dims() { python3 -c "import json;c=json.load(open('$1'))['camera_profile']['video']['codec'];print(f\"{c['width']}x{c['height']}\")"; }

# retarget profile $1 to 127.0.0.1:$2 (+ optional server_port $3) -> file $4
retarget() {
    local sp="${3:-0}"
    python3 -c "import json;p=json.load(open('$1'));v=p['camera_profile']['video'];v['dst_ip']='127.0.0.1';v['dst_port']=$2
sp=$sp
if sp: p['camera_profile'].setdefault('control',{})['server_port']=sp
json.dump(p,open('$4','w'))"
}

decoded_dims() { grep -oE 'width=\(int\)[0-9]+, height=\(int\)[0-9]+' "$1" | tail -1 | grep -oE '[0-9]+' | paste -sd 'x' -; }

# multicast: sender (retargeted) + udpsrc->depay->SW-decode receiver; the raw
# video caps carry width/height, so we compare the *decoded* frame size.
# Software decoders (avdec_*) are used so it's deterministic headless (no GL/HW).
check_mc() {
    local prof=$1 encname=$2 chain=$3 want tmp got
    PORT=$((PORT + 2))          # fresh port per test to avoid stale-sender collisions
    pkill -9 camstreamemu 2>/dev/null; pkill -9 -f 'gst-launch.*udpsrc' 2>/dev/null; sleep 0.3
    want=$(dims "$prof"); tmp=$(mktemp); retarget "$prof" $PORT "" "$tmp"
    # eval so `$chain` is re-tokenised by the shell (zsh doesn't word-split unquoted vars)
    eval "timeout 8 gst-launch-1.0 -v udpsrc port=$PORT \
        caps=\"application/x-rtp,media=video,clock-rate=90000,encoding-name=$encname\" \
        ! $chain ! video/x-raw ! fakesink >/tmp/rx.$$ 2>&1 &"
    local rx=$!; sleep 0.5
    timeout 6 "$CLI" --multicast "$tmp" >/tmp/snd.$$ 2>&1
    sleep 0.3; kill $rx 2>/dev/null; wait $rx 2>/dev/null
    [ -n "${VDBG:-}" ] && echo "    [dbg port=$PORT dst=$(python3 -c "import json;v=json.load(open('$tmp'))['camera_profile']['video'];print(v['dst_ip']+':'+str(v['dst_port']))" 2>/dev/null) rxlines=$(wc -l </tmp/rx.$$ 2>/dev/null) snd=\"$(grep -iE 'stream|error|unsupported' /tmp/snd.$$ 2>/dev/null | head -1)\"]"
    rm -f "$tmp" /tmp/snd.$$
    got=$(decoded_dims /tmp/rx.$$); rm -f /tmp/rx.$$
    if [ "$got" = "$want" ] && [ -n "$got" ]; then printf "  PASS  %-22s %s\n" "$(basename $prof)" "$got"
    else printf "  FAIL  %-22s want=%s got=%s\n" "$(basename $prof)" "$want" "${got:-none}"; fail=1; fi
}

# MLIT TTS(192) can't go through rtpmp2tdepay (it assumes 188-byte TS), so the
# right check is structural: PT=103, 6×192 units per RTP, TS sync 0x47 at +4.
check_tts2() {
    local prof=$1 tmp res
    PORT=$((PORT + 2))
    pkill -9 camstreamemu 2>/dev/null; sleep 0.3
    tmp=$(mktemp); retarget "$prof" $PORT "" "$tmp"
    ( python3 "$APP/tests/tts_probe.py" $PORT >/tmp/tts.$$ 2>&1 ) & local rxpid=$!
    sleep 0.4; timeout 4 "$CLI" --multicast "$tmp" >/dev/null 2>&1
    wait $rxpid 2>/dev/null; res=$(cat /tmp/tts.$$ 2>/dev/null); rm -f "$tmp" /tmp/tts.$$
    local pt=$(echo "$res" | grep -oE 'pt=[0-9]+' | cut -d= -f2)
    local sync=$(echo "$res" | grep -oE 'sync=[0-9]+' | cut -d= -f2)
    local units=$(echo "$res" | grep -oE 'units=[0-9]+' | cut -d= -f2)
    if [ "${pt:-0}" = 103 ] && [ "${sync:-0}" -gt 0 ] && [ "${sync:-0}" = "${units:-x}" ]; then
        printf "  PASS  %-22s PT103 %s units, 0x47@+4 all\n" "$(basename $prof)" "$units"
    else printf "  FAIL  %-22s %s\n" "$(basename $prof)" "${res:-none}"; fail=1; fi
}

echo "== reproduction fidelity: decoded WxH must match the profile =="
check_tts2 "$APP/samples/01-mlit-h264-tts.json"
check_mc "$APP/samples/02-mpeg2-ts.json"      MP2T "rtpmp2tdepay ! tsdemux ! mpegvideoparse ! avdec_mpeg2video"
check_mc "$APP/samples/03-h264-rtp-es.json"   H264 "rtph264depay ! h264parse ! avdec_h264"
check_mc "$APP/samples/06-mpeg2-es.json"      MPV  "rtpmpvdepay ! mpegvideoparse ! avdec_mpeg2video"

# RTSP: retarget to 8554 (non-privileged), rtspsrc pulls + decodes
echo "== RTSP (rtspsrc round-trip) =="
tmp=$(mktemp); retarget "$APP/samples/07-rtsp-h264.json" 0 8554 "$tmp"
"$CLI" --rtsp "$tmp" >/tmp/rtsp.$$ 2>&1 & srv=$!; sleep 1.5
got=$(timeout 5 gst-launch-1.0 -v rtspsrc location=rtsp://127.0.0.1:8554/stream1 ! rtph264depay ! h264parse ! fakesink 2>/dev/null | { grep -oE 'width=\(int\)[0-9]+, height=\(int\)[0-9]+' | tail -1 | grep -oE '[0-9]+' | paste -sd 'x' -; })
kill $srv 2>/dev/null; rm -f /tmp/rtsp.$$ "$tmp"
want=$(dims "$APP/samples/07-rtsp-h264.json")
[ "$got" = "$want" ] && echo "  PASS  07-rtsp-h264          $got" || { echo "  FAIL  07-rtsp-h264 want=$want got=${got:-none}"; fail=1; }

# HTTP MJPEG: retarget to 8080, curl multipart, count JPEG frames
echo "== HTTP MJPEG (curl) =="
tmp=$(mktemp); retarget "$APP/samples/08-http-mjpeg.json" 0 8080 "$tmp"
"$CLI" --mjpeg "$tmp" >/tmp/mj.$$ 2>&1 & srv=$!; sleep 1.2
timeout 2 curl -s "http://127.0.0.1:8080/mjpg/video.mjpg" -o /tmp/mjo.$$ 2>/dev/null || true
frames=$(python3 -c "print(open('/tmp/mjo.$$','rb').read().count(b'\xff\xd8'))" 2>/dev/null || echo 0)
kill $srv 2>/dev/null; rm -f /tmp/mj.$$ /tmp/mjo.$$ "$tmp"
[ "${frames:-0}" -ge 3 ] && echo "  PASS  08-http-mjpeg         $frames JPEG frames" || { echo "  FAIL  08-http-mjpeg frames=${frames:-0}"; fail=1; }

[ $fail -eq 0 ] && echo "verify: ALL PASS" || echo "verify: FAIL"
exit $fail
