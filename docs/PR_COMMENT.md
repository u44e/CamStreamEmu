<!-- 0.1.1 re-submission PR comment — follows the submission format (cf. netterm #54). -->
## 変更点 / Changes in 0.1.1
**JA:** ① **byte-exact SPS/PPS**: 再現するH.264が、エンコーダ生成でなく**プロファイルに取り込んだ実カメラのパラメータセット**をそのまま送出（マルチキャスト・RTSP両対応、送出SPSが実カメラとバイト完全一致を確認）。② **RTP-JPEGの送出バグ修正**（rtpjpegpay が jpegenc 出力を拒否し0パケット→I420強制で解消、往復デコード確認）。③ `tests/verify.sh` を全7配信経路に拡張。
**EN:** ① **Byte-exact SPS/PPS**: the reproduced H.264 now emits the exact parameter sets captured in the profile (not the encoder's), over multicast and RTSP (verified byte-identical to the source SPS). ② **Fix RTP-JPEG send** (rtpjpegpay rejected jpegenc's output → 0 packets; forcing I420 fixes it, round-trip verified). ③ `tests/verify.sh` now covers all 7 delivery paths.

## 用途 / Purpose
**JA:** PacketScope で取得したプロファイルから、実ネットワークカメラの**映像ストリームを再現**する。内蔵カメラを GStreamer（Pi ではHW H.264）でその形式に符号化し、実カメラと同じ方式で配信: マルチキャスト RTP（H.264/MPEG2-TS・MPEG-2・国交省 MPEG2-TTS/PT103・RTP-JPEG）、RTSP サーバ、HTTP Motion-JPEG サーバ。配信モード・宛先・パケット/バイト数をライブ表示。**映像のみ（PTZF 制御なし）**。320×170。
**EN:** Regenerates a real network camera's **video stream** from a PacketScope profile: encodes the built-in camera to that format via GStreamer (HW H.264 on the Pi) and delivers it as the camera did — multicast RTP (H.264/MPEG2-TS, MPEG-2, MLIT MPEG2-TTS/PT103, RTP-JPEG), an RTSP server, or an HTTP Motion-JPEG server — with live delivery-mode / destination / packet & byte counters. Video only (no PTZF). 320x170.

## テストしたデバイスと OS image / Tested device & OS image
**JA:** UI と全配信モード（MLIT TTS(192)/PT103・マルチキャスト H.264/MPEG-2・RTSP・HTTP-MJPEG・RTP-JPEG）を デスクトップ・エミュレータ＋GStreamer で送受検証（`tests/verify.sh` でデコード解像度/構造の一致を自動確認 = ALL PASS）。`.deb` は arm64 Debian Bookworm コンテナでビルドし、headless 起動マーカー・SIGTERM クリーン終了・パッケージ関門を確認。スクショ2枚は cardputer-emu で撮影。**実機 M5CardputerZero（Pi CM0）では未テスト** — 内蔵カメラ（libcamera）・v4l2 HW エンコード・fbdev/LCD・keymap は未確認（デバイス未所持・2026年11月予定、開示）。
**EN:** UI and all delivery modes (MLIT TTS(192)/PT103, multicast H.264/MPEG-2, RTSP, HTTP-MJPEG, RTP-JPEG) verified send+receive on a desktop emulator with GStreamer (`tests/verify.sh` auto-checks decoded resolution/structure — ALL PASS). The .deb builds in an arm64 Debian Bookworm container with a headless startup marker, clean SIGTERM exit, and passing package gates. 2 screenshots from cardputer-emu. **Not tested on physical M5CardputerZero (Pi CM0)** — on-device camera (libcamera), v4l2 HW encode, fbdev/LCD and keymap unconfirmed (device not owned; expected Nov 2026 — disclosed).

## 提出した .deb からインストールして確認したか / Installed from the submitted .deb?
**JA:** .deb は構造検証済（dpkg-deb、関門全通過、strip済、`.desktop`＋サンプルJSON同梱、GStreamer 依存を宣言）。コンテナでビルド済バイナリを headless 実行したが、**実機へのインストールはしていない**。
**EN:** The .deb is structurally verified (passes the gates; stripped; ships a `.desktop` + sample JSONs; declares the GStreamer deps). Run headless from the built binary in the container; **not installed on a device**.

## 権限・ネットワーク・外部機器・バックグラウンド / Access
**JA:** キーボード=あり。**ネットワーク=あり** — 再現中は内蔵カメラの符号化映像をLANへ配信（マルチキャストRTP / RTSPサーバ / HTTP-MJPEG、設計上のデータ送出）。**カメラ=あり**（映像ソースに内蔵カメラ／libcamera・v4l2）。マイク=なし。外部機器=なし。特権=通常不要（<1024の待受ポートを使う場合のみ）。ファイル=フル（プロファイルJSON読取）。BGサービス=なし（再現中のみ）。
**EN:** Keyboard yes. **Network: yes** — while reproducing, the built-in camera's encoded video is streamed to the LAN (multicast RTP / RTSP server / HTTP-MJPEG; data leaves the device by design). **Camera: yes** (built-in camera as the video source; libcamera/v4l2). Microphone: no. External hardware: none. Privileged: not normally (only if a <1024 listen port is used). Filesystem: full (reads profile JSON). Background service: none (only while reproducing).

## プライバシー動作とデータ保持 / Privacy & retention
**JA:** カメラプロファイルJSONはローカル保存。**録画は保持しない**。再現中のみ、プロファイルが指定する宛先へ内蔵カメラ映像をLAN配信（それ以外の送信・テレメトリ・第三者共有は無し）。停止で送出終了。
**EN:** Camera-profile JSON stays on local storage. **No recordings are kept.** Only while reproducing, the built-in camera's video is streamed on the LAN to the destination the profile specifies; no other transmission, telemetry or third-party sharing. Stopping ends the stream.

## 既知の制限 / Known limitations
**JA:** ① 実機未検証（上記）。② 同梱プロファイルが少数（合成＋匿名化した実キャプチャ由来）で任意ベンダ全形式は網羅せず。③ 映像のみ（PTZF制御なし）。
**EN:** ① Not hardware-verified (above). ② Few bundled profiles (synthetic + anonymized real-derived); not exhaustive vendor coverage. ③ Video only (no PTZF control).

## OSS / ライセンス / Open source & license
**JA:** 本体 MIT。**GStreamer 1.0（LGPLv2.1）を実行時依存**（`.deb` Depends: `gstreamer1.0-plugins-{base,good,bad}`, `gstreamer1.0-libav`, `libgstrtspserver-1.0-0`）。**配布経路にGPLは含まない** — 実機既定エンコーダはHW **v4l2h264enc**、MPEG-2は**LGPLの avenc_mpeg2video**。GPLの **x264** はmacOS開発時フォールバック専用で**依存に含めない**（`plugins-ugly` 非依存）。`THIRD_PARTY_NOTICES.md` 参照。
**EN:** App is MIT. **Runtime-depends on GStreamer 1.0 (LGPLv2.1)** (`.deb` Depends: `gstreamer1.0-plugins-{base,good,bad}`, `gstreamer1.0-libav`, `libgstrtspserver-1.0-0`). **No GPL in the distributed path** — device encoder is HW **v4l2h264enc**; MPEG-2 uses the **LGPL avenc_mpeg2video**. The GPL **x264** is a macOS-dev-only fallback, **not** a dependency (no `plugins-ugly`). See `THIRD_PARTY_NOTICES.md`.

## リンク / Links
- Source: https://github.com/u44e/CamStreamEmu
- Package: camstreamemu 0.1.1 arm64
- Screenshots: store/screenshots/01-02 320x170 (profile list / live reproduce)
- Build / test log: `./port/build.sh` (container build + headless run verify), `tests/verify.sh` (all 7 delivery paths)
- Release (.deb): czdev uploads to the fork Release; URL + sha256 in this PR's `*.deb.release.json`
