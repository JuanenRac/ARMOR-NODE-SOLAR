<p align="center">
  <img src="images/ARMOR_BANNER.svg" alt="ARMOR-NODE-SOLAR banner" width="100%">
</p>

# ☀️ ARMOR-NODE-SOLAR

<p align="center">
  <a href="README.md">🇺🇸 English</a> |
  <a href="README_spa.md">🇪🇸 Español</a> |
  <a href="README_fra.md">🇫🇷 Français</a> |
  <a href="README_ita.md">🇮🇹 Italiano</a> |
  <a href="README_deu.md">🇩🇪 Deutsch</a> |
  <a href="README_zho.md">🇨🇳 简体中文</a> |
  🇯🇵 <b>日本語</b> |
  <a href="README_por.md">🇵🇹 Português</a>
</p>

### 太陽光ゲートウェイノード：最大 10 個のシリアルポートでインバーターとバッテリーを読み取る（ESP32-S3 ファームウェア、その Web パネル、プロトコルライブラリ）

<p align="center">
  <img src="https://img.shields.io/badge/License-GPL%203.0-blue.svg" alt="GPL 3.0">
  <img src="https://img.shields.io/badge/Language-C%2B%2B17-00599c.svg" alt="Language">
  <img src="https://img.shields.io/badge/Devices-Voltronic%20%C2%B7%20Pylontech-ffb020.svg" alt="Devices">
  <img src="https://img.shields.io/badge/Version-0.5.7-0f6cbd.svg" alt="Version">
  <img src="https://img.shields.io/badge/Maturity-functional-00E5FF.svg" alt="Maturity">
</p>

---

**正直さのチェック - 今日動いているもの:** ファームウェアは ESP-IDF 5.5.5 コンテナーでビルドでき、そのコア（設定、各種機器とのやり取り、エミュレートした UART の計算、Bluetooth チャネル、バージョンと署名の検査：14 のテストプログラムで 9,319 項目）は代替機器を使ってコンピューター上でテストされ、生成するメッセージは ARMOR-COMMON に受け付けられ、パネルは代替ノードに対してブラウザーで動かしました。**基板上で動いたことはなく、インバーターやバッテリーも接続されていません**：Wi-Fi、TLS のパネル、アップデート、ハードウェアとエミュレートした UART、プロトコルの形式（公開資料と記憶から作成）は未検証です。テスト内の ANT-BMS フレームは他の人が自分の機器で取得したもので、このプロジェクト自身はまだ 1 つも読み取っていません。

---

## 📋 ひと目で

| | |
|---|---|
| バージョン | `0.5.7` |
| 成熟度 | 機能する |
| テスト | 14 個のプログラムで 10,527 件のチェック（`ctest`） |
| 技術 | C++17 (host-tested core); ESP-IDF 5.5.5 firmware for the ESP32-S3-WROOM-1 N16R8 (Wi-Fi) and the Waveshare ESP32-S3-ETH (Ethernet); JavaScript web panel |
| 実機での確認 | まだありません：基板は未製作で、インバーターやバッテリーも未接続（コンピューター上のテスト、ビルド、ブラウザーでのパネルのみ） |

## 🚦 プロジェクトの状況

各行はこのリポジトリの一部分で、信号は今日どこまで進んでいるかを示します。計画ではなく、リポジトリが示せるもの（テスト、ビルド、実行）に基づいて書かれています。

🟢 完了・検証済み · 🟡 動作する（既知の制限あり） · 🟠 一部のみ、または未検証 · 🔴 未実装または動作しない

| 領域 | 状態 | ここでの意味 |
|---|---|---|
| ソフトウェア | 🟢 | ファームウェアは ESP-IDF 5.5.5 コンテナで両方のボード（Wi-Fi とイーサネット）向けにビルドできます。 |
| テスト | 🟢 | 14 のホストプログラムで 10,527 件のチェックが通ります（`ctest`）：設定、各種機器とのやり取り、エミュレートした UART、Bluetooth チャネル、バージョン確認。 |
| プロトコルとデータ | 🟡 | Voltronic、Pylontech、ANT-BMS のフレーム、チェックサム、読み取り値は、文書化されたフレームと記録したサンプルに対して検証済みです。実際のインバーターやバッテリーとはまだ通信していません。 |
| インターフェース | 🟡 | Web パネルはブラウザーでノードの模擬に対して試しており、ボード上では試していません。 |
| ハードウェア | 🔴 | ボードは未製作で、インバーターやバッテリーも接続していません。実機ではまだ何も動かしていません。 |
| セキュリティ | 🟡 | 設定コード、誤ったパスワードの回数制限付きログイン、Bluetooth の PIN、署名付きアップデートはホストでテスト済みです。独立したレビューはまだありません。 |
| アップデート | 🟠 | ハッシュと Ed25519 署名の確認、ロールバックの処理はホストで通ります。無線アップデートはボード上で一度も実行していません。 |
| 機器への書き込み | 🟠 | Pylontech バッテリーへの書き込みは既定でオフで、シミュレーターに対してのみテストしています。 |
| 連携 | 🟢 | ノードのメッセージは ARMOR-COMMON の共有スキーマと適合ベクターに対して検証されています。 |
| ドキュメント | 🟢 | 8 言語、ファームウェアガイド、配線メモ、変更履歴は最新です。 |

## 🎯 概要

* **2 種類のボード、1 つのファームウェア：** Wi-Fi の ESP32-S3-WROOM-1 N16R8（標準）と、Ethernet ケーブルの Waveshare ESP32-S3-ETH（DHCP または固定アドレス。スマートフォンから接続できるよう独自の Wi-Fi ネットワークは残ります）。イメージはビルド時に選びます（`tools/build_node.sh generic s3-wifi` または `generic s3-eth`）。ピン表、ポートの既定ピン、アクセス方法はボードに従い、イメージは対応するボード専用です。どちらもビルドでき、コンピューター上でテスト済みですが、実機では動かしていません。
* **ノードのファームウェア**（Wi-Fi の ESP32-S3-WROOM-1 N16R8、または有線の Waveshare ESP32-S3-ETH）：シリアルポートは 10 個（ハードウェア UART 3 つとエミュレート 7 つ、最大 19200 ボー）。それぞれ独立し、インバーター、バッテリー、ローモニターのいずれかを読むため、ノードはインバーターのみ、バッテリーのみ、または両方を読めます。
* **ノードの Web パネル**（レーダーノードと同じもの）：USB コンソールに表示されるコードによるセットアップ、ログインとユーザー、Wi-Fi（ステーションとアクセスポイント）、ブローカー、ロールバック付きのオーバーザエア更新、ログ、HTTPS、そしてポート（各ポートが受信している内容を表示。未デコードのプロトコル用）と測定値のページ。8 言語対応。
* **Voltronic / MPP Solar インバーター**（Axpert、PIP、InfiniSolar とその互換機）、RS232 2400 ボー：フレームとその CRC、および読み取り値 `QPIGS`（系統、出力、バッテリー、PV、状態ビット）、`QMOD`（モード）、`QPIWS`（名前付きの警告と故障）、`QPIRI`（定格）。組み立てられるのは読み取りコマンドだけです。設定は家への給電方法を変えてしまうからです。3 つの方言（ポートで選ぶか、ポート自身が見つけます＝*auto*）：PI30、REVO（別の配置の `QPIGS`、チェックサムで終わる応答）、PI18（`^P005GS`：InfiniSolar V、LV5048、SunGoldPower）。読み取り専用。
* **Pylontech バッテリー**（US2000、US3000、US5000）：コンソールの `pwr` 表（各モジュールの電圧、電流、温度、充電状態）、`bat <n>`（各セルの電圧と温度）、`info <n>`（型式、残容量と満容量、サイクル数）を、総容量と総エネルギーを持つ 1 つのスタックとして要約し、2 つのチェックを持つ RS485 フレームも扱います。書式は公開されているコンソールの記憶から書いたもので、ファームウェアによって異なる可能性があります。列はヘッダーの名前で見つけます（Id 列、MosTempr、SysAlarm.St を持つ US5000 V2.3 の形式を含みます）。残りの充電量とバランス状態は `bat` から、型式・定格容量・サイクル数は `info` と `stat` から得て、1 回だけ問い合わせて 30 分間保持します。
* **Pylontech モジュールが自分について伝える内容：** モデル、基板とソフトウェアのバージョン、セルと許容電流、寿命のカウンター（`stat`）、各セルの健康状態（`soh`、異常なものに印）、およびイベントログの最新項目を、保護名をコンソールが出力するとおりに表示します。コンソールは読み取り専用で、管理者はポートが受信した内容をテキストファイルとしてダウンロードできます。
* **ノードのメッセージ**（`armor/solar/<ノード>/<デバイス>/state`、インバーターまたはバッテリースタックごとに 1 つ）。ARMOR-COMMON でスキーマと適合性ベクトルとともに定義され、各ポートが作るものはそれに照らして検査されます。
* **ANT-BMS バッテリー**（自作バッテリーパック用の黒い基板、7S～32S）、3.3 V UART・19200 ボー：ファームウェアの 2 種類のプロトコルの両方に対応（ノードは一方で問い合わせ、答えがなければもう一方で問い合わせ、答えたほうを使い続けます）。セル、温度、充電状態、電流、容量、MOSFET とバランサーの状態を読み取り、実機 4 機種で採取されたフレームで検証しています。読み取り専用：書き込みコマンドは負荷のかかった電池を切断しかねません。ポートのページのボタンで BMS の型式、バージョン、保護とバランスの 56 の設定も読み取れます（新しいプロトコル、読み取り専用）。
* **スマートフォンから Bluetooth で設定：** レーダーノードと同じチャンネルです。ARMOR アプリがノードを `ARMOR-XXXXXX` として見つけ、パネルのユーザーとセットアップコードを使って、名前、Wi-Fi、アドレス、ブローカー、Bluetooth モードを設定します（[プロトコル](docs/BLE_PROVISIONING.md)）。設定を変えない限り、ノードにユーザーがいない間だけ待ち受けます。無線部分は基板ではまだ動作していません。
* **マルチプレクサ付きベース基板（*mux* プロファイル）：** 3 つのハードウェア UART が、それぞれ 74HC4052 を介して、最大 8 ポート（4・2・2 のグループ）を回線ごとに順番に受け持ちます。各ポートは独自の速度と極性を持ち、74HC595 でポートごとに LED が 1 つ付きます。背の高い Pylontech スタックのセルは順番に読めます。標準方言のインバーターでは、**2 つ目の PV 入力**（`QPIGS2`）と**並列システムのユニット**（`QPGS`）が任意の読み取りです。いずれも基板ではまだ動作していません。
* **署名付きの無線アップデート：**ノードが GitHub からインストールするのは、SHA-256 が一致し、管理者の有効な Ed25519 署名を持つイメージだけです（公開鍵はファームウェア内、秘密鍵は管理者の手元から出ません）。管理者がパネルで手動アップロードしたファイルは引き続き受け付けます。パネルのページとメッセージは 8 言語です。
* **PIN 付きの Bluetooth ペアリング：**6 桁の PIN は設定コードから導出され、USB コンソールに表示され、管理者にはパネルにも表示されます。スマートフォンはこれを入力して初めてノードの設定を書き込めます。
* **まだ：** ANT-BMS の Bluetooth 接続（ノードは、より安定なケーブルを使います）、そして実機と実際の機器での動作確認。
* **パネルを使えるのは家だけ：**パネルは既定でプライベートネットワーク（または所有者が指定した VPN などのネットワーク）にだけ応答し、保存する本人を締め出す一覧は拒否します。誤ったパスワードは人ごと・アドレスごとに数え、サーバーのプロキシの背後でも動くので、ノードを公開せずに家の外から使えます。**「不明」は「なし」ではありません：** REVO インバーターには警告の問い合わせがないため、メッセージに `warnings_known: false` が付き、Studio は警告を「なし」ではなく「不明」と表示します。
* **ブローカーとの暗号化通信(TLS、任意):** 初期状態ではオフで、ノードはこれまでどおり暗号化なしの MQTT で通信します。ブローカーページのスイッチと、設備独自の認証局の証明書(PEM、最大 2 KB)の入力欄により、`mqtts://` で接続し、その認証局だけを信頼し、到達したアドレス向けに発行された証明書かを確認します。PEM 証明書 1 つでないものは拒否されます。ホストテストで検証済みで、ハンドシェイクはまだ基板では実行していません。
* **状態報告:** ノードは約 1 分ごとに、稼働時間と直前の再起動の理由、空きメモリ、回線と Wi-Fi 信号、ブローカーへの接続回数、最後に記録したエラーを公開します（`armor/health/<ノード>`、ARMOR-COMMON の `node_health` 契約）。作成するコードは全ノードで共有され、コンピューター上でテストされていますが、ボード上ではまだ動作していません。
* **Pylontechバッテリーへの書き込み（初期状態は無効）：** 定型操作のみ——時計、カウンタとログ、再起動、容量、充電レベルのリセット、制限値と保護、出力、シリアル番号、外部メモリ、XMODEM-1Kによるファームウェア、ブロックリスト付きのエキスパートコンソール——を2段階（準備、続いてワンタイムトークンで実行）で行い、前後でバッテリーを読み取り、サービスパスワードはノード内にのみ保存し、テストには模擬コンソールを使います。実機のバッテリーでは未検証：[ベンチテスト計画](docs/BATTERY_BENCH_TEST_PLAN.md)を参照してください。
* **サーバーからの制御：** Studio はノードをきれいに再起動し、最大 1 時間まで一時停止し（自動的に再開）、作業を待機にし（期限なし、再起動後も維持、ノードは接続したまま応答を続けます）、自分を表示させ、診断のスナップショットを求め、メンテナンス中として印を付けることができます（`armor/control/<ノード>/...`、ARMOR-COMMON の `node_control` 契約）。命令はすべて、その操作専用の使い捨てトークンによる 2 段階で、ブローカーに書き込めるのはサーバーだけです。工場出荷時リセットも消去もディープスリープもありません。一時停止中または待機中のノードは、バッテリーへの書き込みを行いません（実行中の操作は取り消せます）。インバーターとバッテリーの読み取りは続けます。 このコードは全ノードで共有され、コンピューター上でテスト済みですが、実機ではまだ動作していません。
* **ありえない値は隠さず名前を付ける：** インバーターがありえない数値を送ると、ノードはそれを契約の上限に留め、`clamped` にその名前を載せるので、サーバーは実測でない値として表示できます。ポートの生ダンプはパネルが見ている間だけ生成されます。
* **ケーブルなしの診断：** 正常でない起動（クラッシュ、ウォッチドッグ、電源の電圧低下、メモリ確保の失敗）の後、ヘルスレポートが10分間その原因を伝えます。クラッシュするとコアダンプが残り、パネルから確認、ダウンロード、削除できます。保存された設定はファームウェアのインストールごとにコピーされ、更新で壊れた場合は復元されます。管理者はサーバー経由で、使い捨てトークンを使って、ノードに診断パッケージまたはログを要求できます。
* **保存された秘密情報の任意の封印:** 既定ではオフです(`security.seal_secrets`、またはサーバーの `nodes.nvs_encryption`)。オンにすると、ノードが保存するパスワード、トークン、鍵、PIN、パスワードのハッシュが ChaCha20-Poly1305 で封印され、それ以外は読めるままです。オン/オフの切り替えは何も失わずに書き直し、途中で止まっても大丈夫です。eFuse は焼かれず、フラッシュも暗号化されません。鍵は同じチップのパーティションにあるため、基板を手にした人に対する保護には**なりません**。このパーティションがない時期にインストールされたノードは、新しいパーティションテーブルで書き込むまで従来のままです。

## 🖼️ スクリーンショット

テストベンチで 1920×1080 で撮影。サンプルデータを使用しており、実際の設備は接続されていません。

<table>
<tr><td width="50%" valign="top"><img src="images/screenshots/overview.png" alt="概要：ネットワーク、ブローカー、ファームウェア、報告内容など、ノードの状態をひと目で確認できます。" width="100%"><br><sub>概要：ネットワーク、ブローカー、ファームウェア、報告内容など、ノードの状態をひと目で確認できます。</sub></td><td width="50%" valign="top"><img src="images/screenshots/ports.png" alt="ポート：ノードのシリアルポートと、それぞれに接続された機器。" width="100%"><br><sub>ポート：ノードのシリアルポートと、それぞれに接続された機器。</sub></td></tr>
<tr><td width="50%" valign="top"><img src="images/screenshots/readings.png" alt="測定値：ノードがいま読み取っている値と、各値の経過時間。" width="100%"><br><sub>測定値：ノードがいま読み取っている値と、各値の経過時間。</sub></td><td width="50%" valign="top"><img src="images/screenshots/network.png" alt="ネットワーク：ノードの Ethernet または Wi-Fi のアドレス設定を、専用の Web パネルから行います。" width="100%"><br><sub>ネットワーク：ノードの Ethernet または Wi-Fi のアドレス設定を、専用の Web パネルから行います。</sub></td></tr>
</table>

## 🧩 A.R.M.O.R. での役割

ARMOR-NODE-SOLAR は家のソーラー機器とファミリーをつなぐゲートウェイです。最大 10 本のシリアルポートでインバーターとバッテリーを読み取り、読み取った内容を [ARMOR-SERVER](https://github.com/JuanenRac/ARMOR-SERVER) に送ります。サーバーが履歴、合計、警報を保持し、[ARMOR-STUDIO](https://github.com/JuanenRac/ARMOR-STUDIO) の*インバーター*と*バッテリー*メニュー、[ARMOR-ANDROID-CONTROL](https://github.com/JuanenRac/ARMOR-ANDROID-CONTROL) のソーラー画面が表示します。読み取るだけで、どの機器にも設定を書き込みません。

## 📂 リポジトリの構成

```text
ARMOR-NODE-SOLAR/
├── main/    the ESP-IDF component: app_main, solar_manager (one task per port, or per group in the mux profile), uart_ports, mux_board, port_leds_hw, network, web_server, api_shared, mqtt_link, node_store, tls_cert, ble_provision, board_ethernet
├── core/    voltronic, voltronic_pi18, pylontech, ant_bms, ant_registers, ant_settings, solar_json + solar_config, poller, soft_uart, netplan, auth, board_s3, ble_frame, ble_dispatch, mux_group, port_leds, console_probe, json (no hardware in them)
├── panel/   the web panel: index.html, app.js, text.js (8 languages), style.css
├── tools/   build_node.sh, check_sdkconfig.py, pack_panel.py, panel_mock.mjs, panel_browser_test.mjs, the shared CI checks (armor_ci_validate.py, armor_project_tool.py, _armor_readme_parity.py)
├── secrets/ node.conf.example (the real files are git-ignored)
├── CMakeLists.txt, partitions.csv, sdkconfig.defaults, sdkconfig.board.*   the ESP-IDF project, the flash layout and the settings of each board (s3-eth, s3-wifi)
├── tests/   test_solar.cpp, test_node.cpp, test_mqtt_tls.cpp, test_board_eth.cpp, test_ble.cpp, test_mux.cpp, test_parallel.cpp, test_console.cpp, emit_samples.cpp, emit_poller_samples.cpp, check_samples.py (+ the ANT-BMS frames)
├── docs/    NODE_FIRMWARE, NODE_HARDWARE, BLE_PROVISIONING, PROTOCOLS, SOLAR_MESSAGES, STUDIO_MENUS, BATTERY_MAINTENANCE, BATTERY_BENCH_TEST_PLAN, BENCH_TESTS, SETUP_CODE
└── images/  brand assets
```

## 🛠️ 開発環境

**必要なもの:** ホストテスト用の C++17 コンパイラ, イメージをビルドするための ESP-IDF 5.5.5 コンテナ, ESP32-S3-WROOM-1 N16R8 または Waveshare ESP32-S3-ETH

```bash
cmake -S tests -B build/host && cmake --build build/host
build/host/test_solar && build/host/test_node && build/host/test_board_eth && build/host/test_ble && build/host/test_mux && build/host/test_parallel && build/host/test_console && build/host/test_release      # 10,527 checks, -Werror
build/host/emit_poller_samples | python tests/check_samples.py   # what the ports make is accepted by ARMOR-COMMON
tools/build_node.sh generic                       # the firmware image for the N16R8 board in the ESP-IDF container: dist/generic-s3-wifi.bin
tools/build_node.sh generic s3-eth               # the same firmware for the Waveshare ESP32-S3-ETH (Ethernet): dist/generic-s3-eth.bin
node tools/panel_mock.mjs --user admin:adminpass123   # the panel without a board
```

[ファームウェアガイド](docs/NODE_FIRMWARE.md)（ボード、ポート、初回起動、ベンチ確認リスト）と[配線メモ](docs/NODE_HARDWARE.md)を参照。

## 🔗 関連プロジェクト

**A.R.M.O.R.**（Autonomous Radar & Multimodal Observation Range）は、独立したリポジトリで構成される周辺警備システムです。それぞれに独自のバージョン、テスト、README があります。ファミリーは次のとおりです：

* **[ARMOR-COMMON](https://github.com/JuanenRac/ARMOR-COMMON)** - メッセージ契約、検証器、適合性ベクトル、生成された型
* **[ARMOR-NODE-RADAR](https://github.com/JuanenRac/ARMOR-NODE-RADAR)** - ESP32-S3 用フィールドノードのファームウェア。レーダー 3 基と独自の Web パネル付き
* **ARMOR-NODE-SOLAR** (このリポジトリ) - 太陽光インバーターとバッテリーのプロトコル、およびゲートウェイノードのメッセージ
* **[ARMOR-NODE-ELECTRICAL](https://github.com/JuanenRac/ARMOR-NODE-ELECTRICAL)** - 電気ノード：電力量計、電力網の計測メッセージ、開閉のルール
* **[ARMOR-NODE-ALARM](https://github.com/JuanenRac/ARMOR-NODE-ALARM)** - 警報ノードと警報盤：警戒区域、警戒セット、遅延、サイレン、PIN。サーバーがあってもなくても
* **[ARMOR-NODE-HMI](https://github.com/JuanenRac/ARMOR-NODE-HMI)** - タッチパネル：壁面ディスプレイでのシステム状態表示、警戒・確認操作、音声アシスタントの拠点
* **[ARMOR-NETWORK](https://github.com/JuanenRac/ARMOR-NETWORK)** - ローカルネットワーク：機器、インターネット、そして変化
* **[ARMOR-SERVER](https://github.com/JuanenRac/ARMOR-SERVER)** - 中央コーディネーター：テレメトリ、アラーム、デバイス、太陽光の測定値、カメラ
* **[ARMOR-STUDIO](https://github.com/JuanenRac/ARMOR-STUDIO)** - Web コンソール：カメラ、レーダー、アラーム、太陽光発電、2D/3D サイト設計
* **[ARMOR-ANDROID-CONTROL](https://github.com/JuanenRac/ARMOR-ANDROID-CONTROL)** - リアルタイム 2D/3D レーダー付きの Android オペレータークライアント
* **[ARMOR-IOS-CONTROL](https://github.com/JuanenRac/ARMOR-IOS-CONTROL)** - iPhone オペレータークライアント
* **[ARMOR-SERVER-AI](https://github.com/JuanenRac/ARMOR-SERVER-AI)** - 判断を説明し、決して動作しない視覚推論ポリシー
* **[ARMOR-VISION](https://github.com/JuanenRac/ARMOR-VISION)** - Jetson Orin NX 向けのカメラのゾーン、マスク、検知ルール
* **[ARMOR-VOICE-AI](https://github.com/JuanenRac/ARMOR-VOICE-AI)** - 偽造できない確認を備えたオフライン音声インテント
* **[ARMOR-HARDWARE](https://github.com/JuanenRac/ARMOR-HARDWARE)** - 筐体、電子部品、ベンチ受け入れマトリクス
* **[ARMOR-DEVOPS](https://github.com/JuanenRac/ARMOR-DEVOPS)** - デプロイ、CM5 テストベンチ、バックアップ、TLS
* **[ARMOR-SIMULATOR](https://github.com/JuanenRac/ARMOR-SIMULATOR)** - 再現可能な故障を備えたオフラインのテレメトリシミュレーター
* **[ARMOR-UPDATER](https://github.com/JuanenRac/ARMOR-UPDATER)** - エコシステム自身のリポジトリを検出し、インストールし、更新する
* **[ARMOR-DOCS](https://github.com/JuanenRac/ARMOR-DOCS)** - アーキテクチャ、セキュリティ基準、機能マトリクス

## 📚 ドキュメントとコミュニティ

詳しくは：

* [機能マトリクス：実証済みのものとそうでないもの](https://github.com/JuanenRac/ARMOR-DOCS/blob/main/docs/CAPABILITY_MATRIX.md)
* [プロジェクト一覧：バージョンとリポジトリ間の依存関係](https://github.com/JuanenRac/ARMOR-DOCS/blob/main/docs/PROJECT_CATALOG.md)
* [このリポジトリの変更履歴](CHANGELOG.md)
* [ライセンス（GPL-3.0-or-later）](LICENSE)
* 質問・提案・報告：electrohobby3d@gmail.com

## 👤 作者

**JuanenRac (Electro Hobby 3D)** · electrohobby3d@gmail.com

## 📜 ライセンス

GPL-3.0-or-later - [LICENSE](LICENSE) を参照。
