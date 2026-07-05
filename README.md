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

MIT.
