# CamStreamEmu — camera stream emulator (v0.1.0)

Reproduces a real network camera's **video stream** from a profile captured by
[PacketScope](https://github.com/u44e/cardputerzero-packetscope). Reads the
camera-profile JSON, encodes the built-in camera to that exact format via
**GStreamer hardware encoding**, and delivers it the same way the real camera
did — multicast RTP, RTSP, or HTTP Motion-JPEG.

Video only, **no PTZF control** (hence "CamStream", not a full camera emulator).
Targets the CardputerZero (Raspberry Pi CM0) but builds on any Linux/macOS host.

PacketScope and CamStreamEmu are inverse operations: PacketScope observes a
camera and writes a profile; CamStreamEmu reads a profile and regenerates the
stream. That round-trip is the acceptance test.

## Build

```sh
# deps: gstreamer-1.0 + app + rtsp-server (+ base/good/bad/libav plugins)
# Pi: libcamera + v4l2 HW H.264;  dev host: videotestsrc + x264enc fallback
make            # -> ./camstreamemu
```

## Use

```sh
camstreamemu <profile.json>              # control=rtsp -> RTSP, http-mjpeg -> HTTP, else multicast
camstreamemu --rtsp <profile.json>       # serve as an RTSP camera (DESCRIBE/SETUP/PLAY)
camstreamemu --mjpeg <profile.json>      # serve HTTP multipart Motion-JPEG
camstreamemu --multicast <profile.json>  # force multicast RTP push
camstreamemu --dump <profile.json>       # show the parsed profile + gst pipeline
camstreamemu --version
```

Dev-host overrides: `CAMEMU_SRC` (default libcamerasrc / videotestsrc),
`CAMEMU_ENC` (`v4l2`|`x264`), `CAMEMU_DUMP` (print pipeline only).

## Reproduces

| 種別 | codec | 経路 | 状態 |
|---|---|---|---|
| mpeg2-ts | H.264 | mpegtsmux → rtpmp2tpay → multicast | ✅ 送受検証 |
| mpeg2-ts | MPEG-2 | avenc_mpeg2video(SW) → mpegtsmux → … | ✅ 送出検証 |
| mpeg2-tts | H.264 | mpegtsmux → **TTS(192)×6/PT103** → multicast | ✅ PT103/6×192/0x47@+4 |
| rtp-jpeg | JPEG | jpegenc → rtpjpegpay(PT26) | 実装済 |
| RTSP | H.264 | gst-rtsp-server (rtph264pay) | ✅ rtspsrc往復 |
| HTTP MJPEG | JPEG | 自作HTTPサーバ + multipart/x-mixed-replace | ✅ curl配信 |

Pi の HW エンコーダは H.264 のみ。MPEG-2 は SW(gst-libav)フォールバック。
プロファイルは SPS/PPS(base64)・SDP・AUD/SEI まで持つ(byte-exact 再現用)。

## UI (CardputerZero LVGL)

`./build-emu.sh --run`(Mac の cardputer-emu)。プロファイル一覧 → Enter で再現、
再現画面に配信モード / 宛先 / パケット・バイト数 / 経過をライブ表示、s/ESC で停止。
実機では `.deb` の LVGL dlopen アプリとして動く(GStreamer 再現はバックグラウンド)。

## 検証 (実機不要)

`bash tests/verify.sh` — 各サンプルを再現配信 → 受信・デコードし、**デコード解像度が
プロファイルと一致**することを確認(TTS は PT103/6×192/0x47@+4 の構造)。全モード緑:

```
PASS 01-mlit-h264-tts  PT103 240 units, 0x47@+4 all
PASS 02-mpeg2-ts       704x480      PASS 03-h264-rtp-es   1920x1080
PASS 06-mpeg2-es       704x480      PASS 07-rtsp-h264     1920x1080
PASS 08-http-mjpeg     30 JPEG frames
verify: ALL PASS
```

未検証: 実機(Pi CM0)の libcamera + v4l2 HW エンコード経路(11月着後)、取り込み
SPS/PPS の byte-exact 注入(現状は再エンコード)。

MIT.
