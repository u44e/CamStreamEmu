# Third-party / open-source components — オープンソース構成

CamStreamEmu 本体のコードは **MIT ライセンス**（`LICENSE`）です。以下のオープン
ソースソフトウェア(OSS)を利用しています。配布物(`.deb`)はこれらを**同梱せず、
動的リンク／実行時依存**（Debian パッケージへの `Depends`）として利用します。

CamStreamEmu's own code is under the **MIT License** (`LICENSE`). It uses the
open-source components below. The distributed `.deb` does **not bundle** them —
they are dynamic / runtime dependencies (declared as Debian `Depends`).

| Component | Use | License | Linkage |
|---|---|---|---|
| **LVGL 9.5** | 画面UI / on-device UI | MIT | compiled in (permissive) |
| **FreeType** | フォント描画 / font rendering | FTL or GPLv2 (dual; FTL used) | dynamic |
| **GStreamer 1.0 core** | メディアパイプライン / media pipeline | LGPLv2.1 | dynamic |
| **gstreamer-app / gstreamer-rtsp-server** | appsink / RTSPサーバ | LGPLv2.1 | dynamic |
| gstreamer1.0-plugins-base | videoconvert, mux 等 | LGPLv2.1 | runtime plugin |
| gstreamer1.0-plugins-good | v4l2h264enc, rtp payloaders, jpegenc | LGPLv2.1 | runtime plugin |
| gstreamer1.0-plugins-bad | mpegtsmux, libcamerasrc 等 | LGPLv2.1 | runtime plugin |
| gstreamer1.0-libav (gst-libav) | avenc_mpeg2video (MPEG-2 SWエンコード) | LGPLv2.1 (wraps FFmpeg, LGPL build) | runtime plugin |

## ライセンス適合性 / License compliance

- **本体は MIT**。上記 OSS はすべて **LGPL / MIT / FTL（コピーレフト伝播なし、
  動的リンク）** であり、MIT の本アプリと組み合わせて配布して問題ありません。
  The app is MIT; all dependencies are LGPL/MIT/FTL used via dynamic linking, so
  distributing the MIT app alongside them is compliant.

- **GPL コンポーネントは配布経路に含みません / No GPL in the distributed path.**
  実機(Raspberry Pi)の既定エンコーダは **v4l2h264enc（カーネルV4L2のHW H.264）**、
  MPEG-2 は **avenc_mpeg2video（gst-libav = LGPL）** です。GPL の **x264
  (`x264enc` / gstreamer1.0-plugins-ugly)** は **Mac 開発ホストのフォールバック
  専用**で、`.deb` の依存には**含めていません**（`Depends` は base/good/bad/libav
  のみ、ugly なし）。実機で GPL エンコーダは呼ばれません。
  On device the default encoder is hardware **v4l2h264enc**, and MPEG-2 uses the
  LGPL **avenc_mpeg2video**. The GPL **x264** encoder is a macOS-dev fallback
  only and is **not** a package dependency (`Depends` lists base/good/bad/libav,
  not `-ugly`), so no GPL encoder is invoked on the device.

- 各ライセンス全文: LVGL/streamkit=リポジトリの `LICENSE`、GStreamer/FreeType は
  各 Debian パッケージ (`/usr/share/doc/<pkg>/copyright`) を参照。

MIT © 2026 u44e
