# 公式 AppStore 提出手順（コピペ用）/ Submission steps

CardputerZero の CamStreamEmu（標準バイナリ）を `czdev` で提出する。`czdev login` は本人の
GitHub OAuth が要るので以下は**あなたの端末で実行**。メタ（`app-builder.json` の `store`、
日英）・アイコン（100×100）・スクショ（320×170）・OSS明記（`THIRD_PARTY_NOTICES.md`）は準備済み。

- 出力物: `port/dist/camstreamemu_0.1.0_arm64.deb`
- パッケージ名 `camstreamemu`・`.desktop` 同梱・Maintainer `u44e@users.noreply.github.com`
- **GStreamer 実行時依存**（`.deb` の `Depends`）: `gstreamer1.0-tools`,
  `gstreamer1.0-plugins-{base,good,bad}`, `gstreamer1.0-libav`, `libgstrtspserver-1.0-0`

## 0. リポジトリを公開 / make the repo public
```bash
gh repo edit u44e/CamStreamEmu --visibility public --accept-visibility-change-consequences
```

## 1. `.deb` をビルド（要 Docker）/ build the .deb
```bash
cd ~/Projects/CamStreamEmu
./port/build.sh          # -> port/dist/camstreamemu_0.1.0_arm64.deb
```
`build.sh` は arm64 Debian Bookworm コンテナで (1) headless 起動＋SIGTERM 終了検証、
(2) fbdev+evdev デバイスビルド、(3) CPack `.deb`（GStreamer 依存宣言・samples 同梱）、
(4) パッケージ関門チェックを実施。

## 2. ログイン → 提出（★アプリ dir で）/ login then publish
```bash
PYTHONPATH="$HOME/CardputerZero-AppBuilder/scripts" python3 -m czdev login
cd ~/Projects/CamStreamEmu
PYTHONPATH="$HOME/CardputerZero-AppBuilder/scripts" python3 -m czdev publish \
  --deb port/dist/camstreamemu_0.1.0_arm64.deb
```

## 3. 審査で必ず伝わるよう明記済み / disclosures already in app-builder.json
- **実機未検証 / not hardware-verified**: 物理 M5CardputerZero(Pi CM0)未検証。
  UI と全配信モードはデスクトップ・エミュレータ＋GStreamer で検証、`.deb` は arm64
  コンテナでビルド・関門確認済み。実機のカメラ(libcamera)・v4l2 HW エンコード・
  fbdev/keymap は未確認（実機 2026年11月予定）。
- **pcap サンプル不足 / limited samples**: 同梱プロファイルは少数（合成＋匿名化した
  実キャプチャ由来）。任意ベンダ全形式は網羅せず。
- **OSS 明記 / OSS**: 本体 MIT、依存は LGPL/MIT/FTL（`THIRD_PARTY_NOTICES.md`）。
  実機エンコードは HW v4l2＋LGPL gst-libav で **GPL 非依存**（GPL の x264 は開発時
  フォールバックのみ・`.deb` 依存に含めず）。
- **カメラ/ネットワーク権限 / camera+network**: 再現中は内蔵カメラ映像を LAN へ配信
  （設計上のデータ送出）。`permissions.camera=true`・`network=true`・`risk_flags` に記載済み。

## 検証（提出前セルフチェック）/ pre-submit checks
```bash
make                     # CLI + reproduction engine
bash tests/verify.sh     # 各サンプルを再現→受信デコードし形式一致を自動確認（ALL PASS）
./build-emu.sh --run     # エミュレータで UI 手動確認
```
