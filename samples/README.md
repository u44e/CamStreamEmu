# サンプル カメラプロファイル

CamStreamEmu に渡す例。01–06 は PacketScope が実 pcap から書き出した本物の
プロファイル、07–08 は代表的な実カメラ構成の手書き例。

| ファイル | 種別 | 配信 | 備考 |
|---|---|---|---|
| 01-mlit-h264-tts | H.264 / MPEG2-TTS(192) | multicast PT=103 | 国交省CCTV(MLIT準拠) |
| 02-mpeg2-ts | MPEG-2 / MPEG2-TS | multicast PT=33 | 旧CCTV(SWエンコード) |
| 03-h264-rtp-es | H.264 / raw-ES over RTP | multicast PT=96 | RTP直載せ |
| 04-rtp-jpeg | JPEG / RTP-JPEG | multicast PT=26 | RFC2435 |
| 05-rtsp-tts | H.264 / TTS (RTSP interleaved) | PT=103 | RTSP制御+TTS |
| 06-mpeg2-es | MPEG-2 / raw-ES over RTP | multicast PT=96 | 旧CCTV生ES |
| 07-rtsp-h264 | H.264 | RTSPサーバ(unicast) | 一般的なRTSP IPカメラ(手書き) |
| 08-http-mjpeg | JPEG | HTTP multipart | Motion-JPEG IPカメラ(手書き) |

```sh
./camstreamemu --dump samples/07-rtsp-h264.json    # 解釈内容 + gstパイプライン確認
./camstreamemu samples/01-mlit-h264-tts.json        # 実際に再現配信
```

01,05 は SPS/PPS(base64)・AUD/SEI・SDP まで含み、byte-exact 再現の材料になる。

## 実キャプチャ由来 (匿名化)

`real-*.json` は実運用CCTVのキャプチャを PacketScope で解析して得た本物の形式
プロファイル(**アドレス・SSRC・出所は匿名化済み**、映像本体は含まない)。CamStreamEmu
で全数 再現・デコード一致を確認済み。実在カメラで往復(観測→再現)が成立する実証。

| ファイル | 形式 |
|---|---|
| real-h264-tts | H.264 MPEG2-TTS(192) PT103 640×480 |
| real-h264-es  | H.264 raw-ES PT99 640×480 |
| real-mpeg2-a  | MPEG-2 raw-ES PT96 720×480 |
| real-mpeg2-b  | MPEG-2 raw-ES PT96 720×480 |
| real-sony-h264 | Sony(SNC-VB340型) H.264 生Annex-B over RTP+Sony SEI PT105 1920×1080 |
| real-rtsp-h264-4k | H.264 4K 3840×2160 RTSP(Axis, SPSはSDP sprop) PT96 |
