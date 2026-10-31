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
  <img src="https://img.shields.io/badge/License-GPL%203.0--or--later-blue.svg" alt="GPL 3.0 or later">
  <img src="https://img.shields.io/badge/Language-C%2B%2B17-00599c.svg" alt="Language">
  <img src="https://img.shields.io/badge/Devices-Voltronic%20%C2%B7%20Pylontech-ffb020.svg" alt="Devices">
  <img src="https://img.shields.io/badge/Tests-11824%20checks-2ea44f.svg" alt="Tests">
  <img src="https://img.shields.io/badge/Version-0.6.1-0f6cbd.svg" alt="Version">
  <img src="https://img.shields.io/badge/Maturity-functional-00E5FF.svg" alt="Maturity">
</p>

---

**正直さのチェック - 今日動いているもの:** ファームウェアは ESP-IDF 5.5.5 コンテナーでビルドでき、そのコア（設定、各種機器とのやり取り、エミュレートした UART の計算、Bluetooth チャネル、バージョンと署名の検査：18 のテストプログラムで 9,319 項目）は代替機器を使ってコンピューター上でテストされ、生成するメッセージは ARMOR-COMMON に受け付けられ、パネルは代替ノードに対してブラウザーで動かしました。**基板上で動いたことはなく、インバーターやバッテリーも接続されていません**：Wi-Fi、TLS のパネル、アップデート、ハードウェアとエミュレートした UART、プロトコルの形式（公開資料と記憶から作成）は未検証です。テスト内の ANT-BMS フレームは他の人が自分の機器で取得したもので、このプロジェクト自身はまだ 1 つも読み取っていません。

---

## 📋 ひと目で

| | |
|---|---|
| バージョン | `0.6.1` |
| 成熟度 | 機能する |
| 役割 | ESP32-S3 用ソーラーフィールドノード：インバーターとバッテリー（ファミリー：Field Nodes、親：ARMOR-COMMON） |
| 技術 | C++17, ESP-IDF 5.5.5, ESP32-S3 |
| テスト | 11824 件の検査（ホスト用プログラム 18 本と Python 検査 6 個、`ctest`） |
| 機器 | Voltronic / MPP Solar インバーター、Pylontech バッテリー、ANT-BMS、シリアルポート 10 本 |
| 動作環境 | ESP32-S3-WROOM-1 N16R8 または Waveshare ESP32-S3-ETH、ホストテストは任意のコンピューター |
| 実機での確認 | まだありません：基板は未製作で、インバーターやバッテリーも未接続（コンピューター上のテスト、ビルド、ブラウザーでのパネルのみ） |
| ライセンス | GPL-3.0-or-later |

**移動先：** [概要](#-概要) · [アーキテクチャ](#-アーキテクチャ) · [セキュリティ](#-セキュリティと安全) · [インターフェース](#-インターフェース) · [設定](#-設定) · [クイックスタート](#-クイックスタート) · [テスト](#-テスト) · [トラブルシューティング](#-トラブルシューティング) · [構成](#-リポジトリの構成) · [関連](#-関連プロジェクト)

## 🚦 プロジェクトの状況

各行はこのリポジトリの一部分で、信号は今日どこまで進んでいるかを示します。計画ではなく、リポジトリが示せるもの（テスト、ビルド、実行）に基づいて書かれています。

🟢 完了・検証済み · 🟡 動作する（既知の制限あり） · 🟠 一部のみ、または未検証 · 🔴 未実装または動作しない

| 領域 | 状態 | ここでの意味 |
|---|---|---|
| ソフトウェア | 🟢 | ファームウェアは ESP-IDF 5.5.5 コンテナで両方のボード（Wi-Fi とイーサネット）向けにビルドできます。 |
| テスト | 🟢 | 18 のホストプログラムで 11,824 件のチェックが通ります（`ctest`）：設定、各種機器とのやり取り、エミュレートした UART、Bluetooth チャネル、バージョン確認。 |
| プロトコルとデータ | 🟡 | Voltronic、Pylontech、ANT-BMS のフレーム、チェックサム、読み取り値は、文書化されたフレームと記録したサンプルに対して検証済みです。実際のインバーターやバッテリーとはまだ通信していません。 |
| インターフェース | 🟡 | Web パネルはブラウザーでノードの模擬に対して試しており、ボード上では試していません。 |
| ハードウェア | 🔴 | ボードは未製作で、インバーターやバッテリーも接続していません。実機ではまだ何も動かしていません。 |
| セキュリティ | 🟡 | 設定コード、誤ったパスワードの回数制限付きログイン、Bluetooth の PIN、署名付きアップデートはホストでテスト済みです。独立したレビューはまだありません。 |
| アップデート | 🟠 | ハッシュと Ed25519 署名の確認、ロールバックの処理はホストで通ります。無線アップデートはボード上で一度も実行していません。 |
| 機器への書き込み | 🟠 | Pylontech バッテリーへの書き込みは既定でオフで、シミュレーターに対してのみテストしています。 |
| 連携 | 🟠 | ノードのメッセージは ARMOR-COMMON の共有スキーマと適合ベクターに対して検証されています。MQTT クライアントはテストで本物の Mosquitto を相手に動かしていますが、ボードからサーバーへ送ったことはまだありません。 |
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
* **パネルを使えるのは家だけ：**パネルは既定でプライベートネットワーク（または所有者が指定した VPN などのネットワーク）にだけ応答し、保存する本人を締め出す一覧は拒否します。誤ったパスワードは人ごと・アドレスごとに数え、サーバーのプロキシの背後でも動くので、ノードを公開せずに家の外から使えます。
* **「不明」は「なし」ではありません：** REVO インバーターには警告の問い合わせがないため、メッセージに `warnings_known: false` が付き、Studio は警告を「なし」ではなく「不明」と表示します。
* **ブローカーとの暗号化通信(TLS、任意):** 初期状態ではオフで、ノードはこれまでどおり暗号化なしの MQTT で通信します。ブローカーページのスイッチと、設備独自の認証局の証明書(PEM、最大 2 KB)の入力欄により、`mqtts://` で接続し、その認証局だけを信頼し、到達したアドレス向けに発行された証明書かを確認します。PEM 証明書 1 つでないものは拒否されます。ホストテストで検証済みで、ハンドシェイクはまだ基板では実行していません。
* **状態報告:** ノードは約 1 分ごとに、稼働時間と直前の再起動の理由、空きメモリ、回線と Wi-Fi 信号、ブローカーへの接続回数、最後に記録したエラーを公開します（`armor/health/<ノード>`、ARMOR-COMMON の `node_health` 契約）。作成するコードは全ノードで共有され、コンピューター上でテストされていますが、ボード上ではまだ動作していません。
* **Pylontechバッテリーへの書き込み（初期状態は無効）：** 定型操作のみ——時計、カウンタとログ、再起動、容量、充電レベルのリセット、制限値と保護、出力、シリアル番号、外部メモリ、XMODEM-1Kによるファームウェア、ブロックリスト付きのエキスパートコンソール——を2段階（準備、続いてワンタイムトークンで実行）で行い、前後でバッテリーを読み取り、サービスパスワードはノード内にのみ保存し、テストには模擬コンソールを使います。実機のバッテリーでは未検証：[ベンチテスト計画](docs/BATTERY_BENCH_TEST_PLAN.md)を参照してください。
* **サーバーからの制御：** Studio はノードをきれいに再起動し、最大 1 時間まで一時停止し（自動的に再開）、作業を待機にし（期限なし、再起動後も維持、ノードは接続したまま応答を続けます）、自分を表示させ、診断のスナップショットを求め、メンテナンス中として印を付けることができます（`armor/control/<ノード>/...`、ARMOR-COMMON の `node_control` 契約）。命令はすべて、その操作専用の使い捨てトークンによる 2 段階で、ブローカーに書き込めるのはサーバーだけです。工場出荷時リセットも消去もディープスリープもありません。一時停止中または待機中のノードは、バッテリーへの書き込みを行いません（実行中の操作は取り消せます）。インバーターとバッテリーの読み取りは続けます。 このコードは全ノードで共有され、コンピューター上でテスト済みですが、実機ではまだ動作していません。
* **ありえない値は隠さず名前を付ける：** インバーターがありえない数値を送ると、ノードはそれを契約の上限に留め、`clamped` にその名前を載せるので、サーバーは実測でない値として表示できます。ポートの生ダンプはパネルが見ている間だけ生成されます。
* **ケーブルなしの診断：** 正常でない起動（クラッシュ、ウォッチドッグ、電源の電圧低下、メモリ確保の失敗）の後、ヘルスレポートが10分間その原因を伝えます。クラッシュするとコアダンプが残り、パネルから確認、ダウンロード、削除できます。保存された設定はファームウェアのインストールごとにコピーされ、更新で壊れた場合は復元されます。管理者はサーバー経由で、使い捨てトークンを使って、ノードに診断パッケージまたはログを要求できます。
* **保存された秘密情報の任意の封印:** 既定ではオフです(`security.seal_secrets`、またはサーバーの `nodes.nvs_encryption`)。オンにすると、ノードが保存するパスワード、トークン、鍵、PIN、パスワードのハッシュが ChaCha20-Poly1305 で封印され、それ以外は読めるままです。オン/オフの切り替えは何も失わずに書き直し、途中で止まっても大丈夫です。eFuse は焼かれず、フラッシュも暗号化されません。鍵は同じチップのパーティションにあるため、基板を手にした人に対する保護には**なりません**。このパーティションがない時期にインストールされたノードは、新しいパーティションテーブルで書き込むまで従来のままです。

## 🧩 A.R.M.O.R. での役割

ARMOR-NODE-SOLAR は家のソーラー機器とファミリーをつなぐゲートウェイです。最大 10 本のシリアルポートでインバーターとバッテリーを読み取り、読み取った内容を [ARMOR-SERVER](https://github.com/JuanenRac/ARMOR-SERVER) に送ります。サーバーが履歴、合計、警報を保持し、[ARMOR-STUDIO](https://github.com/JuanenRac/ARMOR-STUDIO) の*インバーター*と*バッテリー*メニュー、[ARMOR-ANDROID-CONTROL](https://github.com/JuanenRac/ARMOR-ANDROID-CONTROL) のソーラー画面が表示します。読み取るだけで、どの機器にも設定を書き込みません。

| 名前 | 方法 | 用途 |
|---|---|---|
| ARMOR-SERVER | MQTT `armor/solar/<node>/<device>/state` | インバーターまたはバッテリースタックごとに 1 メッセージを受け取り、バッテリーの保守を命令する |
| ARMOR-STUDIO | メニュー、パネルへのリンク | インバーターとバッテリーを表示し、ノードのパネルを開く |
| ARMOR-ANDROID-CONTROL | Bluetooth | 現場でノードを設定し、エネルギー画面を表示 |
| ARMOR-COMMON | ソーラーのコントラクト | メッセージはそのスキーマで検証し、共通ファームウェアはここからコピー |

## 🏛️ アーキテクチャ

<p align="center">
  <img src="images/architecture.svg" alt="ARMOR-NODE-SOLAR のアーキテクチャ：呼び出し元、内部、依存先" width="100%">
</p>

## 🖼️ スクリーンショット

テストベンチで 1920×1080 で撮影。サンプルデータを使用しており、実際の設備は接続されていません。

<table>
<tr><td width="50%" valign="top"><img src="images/screenshots/overview.png" alt="概要：ネットワーク、ブローカー、ファームウェア、報告内容など、ノードの状態をひと目で確認できます。" width="100%"><br><sub>概要：ネットワーク、ブローカー、ファームウェア、報告内容など、ノードの状態をひと目で確認できます。</sub></td><td width="50%" valign="top"><img src="images/screenshots/ports.png" alt="ポート：ノードのシリアルポートと、それぞれに接続された機器。" width="100%"><br><sub>ポート：ノードのシリアルポートと、それぞれに接続された機器。</sub></td></tr>
<tr><td width="50%" valign="top"><img src="images/screenshots/readings.png" alt="測定値：ノードがいま読み取っている値と、各値の経過時間。" width="100%"><br><sub>測定値：ノードがいま読み取っている値と、各値の経過時間。</sub></td><td width="50%" valign="top"><img src="images/screenshots/network.png" alt="ネットワーク：ノードの Ethernet または Wi-Fi のアドレス設定を、専用の Web パネルから行います。" width="100%"><br><sub>ネットワーク：ノードの Ethernet または Wi-Fi のアドレス設定を、専用の Web パネルから行います。</sub></td></tr>
</table>

## 🔒 セキュリティと安全

| 脅威 | 対策 |
|---|---|
| 平文またはコンパイル済みのパスワード | PBKDF2-HMAC-SHA256 10 000 ラウンド。ファームウェアにパスワードはなく（ユーザーのないノードはセットアップに入る）、ユーザーは最大 4 人（管理者または閲覧者）、8〜64 文字、セッションは最大 6、無操作 30 分 |
| 書き込み直後のノードの乗っ取り | セットアップには起動コードが必要です。ビルドのもの（`CONFIG_ARMOR_SETUP_CODE`）か、フラッシュに保存して USB コンソールに表示するランダムなもの。フリートシークレットがあれば `HMAC-SHA256(シークレット, MAC)`。これはセットアップ用 Wi-Fi（WPA2、ブリッジなし）の鍵でもあります |
| インターネットからのパネルへのアクセス | 初期設定では家庭のプライベートネットワーク（10/8、172.16/12、192.168/16、169.254/16、127/8、一部の VPN が使う 100.64/10）にだけ応答します。設定 `web.admin_sources` で他の IPv4 アドレスやネットワークを追加でき（8 件、120 文字）、それ以外は公開の `GET /api/v1/session` を除き拒否します。保存した本人を締め出すリストは拒否されます |
| HTTP で平文のパネル | 443 番ポートで HTTPS。証明書はノード自身が作成（自己署名、フラッシュ保存、比較用に SHA-256 フィンガープリントを表示）。パネルは http、https、または両方を選べ、セッション Cookie は常に HttpOnly と SameSite=Strict（TLS では Secure も） |
| GitHub からの偽ファームウェア | SHA-256 が一致し、メンテナーの鍵による有効な Ed25519 署名があるイメージだけをインストールします（`no_checksum`、`no_signature`、`bad_signature`、`checksum_mismatch`、`wrong_firmware`、`not_newer`、`version_mismatch` は拒否）。管理者の手動アップロードは検証されません（明示した制限） |
| Bluetooth セットアップの乗っ取り | セットアップコードから導出した 6 桁の PIN を USB コンソールと管理者に表示し、設定を書き込む前にスマホで入力させます。セットアップの時間枠は `CONFIG_ARMOR_BLE_SETUP_MINUTES`（初期 10、1〜120） |
| 害を及ぼす遠隔命令 | その操作に結び付いた使い捨てトークンによる 2 段階。ブローカーのそのトピックに書き込めるのはサーバーだけで、一時停止は最長 1 時間。遠隔命令による工場出荷状態への復元、消去、ディープスリープはありません |
| 基板を持つ人による保存済みシークレットの読み取り | 任意（初期はオフ、スイッチ `security.seal_secrets` またはサーバーの `nodes.nvs_encryption`）：パスワード、トークン、鍵、PIN、ハッシュを ChaCha20-Poly1305 で保護。フラッシュ全体の暗号化や eFuse の焼き込みはしません。鍵は同じチップの別パーティションにあるため、基板を手にした人には防御になりません |
| ブローカーとの平文通信 | 任意の TLS（`mqtts://`）。所有者自身の認証局（PEM 2 KB まで）と名前の検証を使い、初期はオフです。ハンドシェイクはホストのテストでしか動いておらず、基板では未実行です |
| 誤りまたは攻撃者によるバッテリーへの書き込み | 3 つの錠：ノードの設定 `battery.write_enabled`（管理者）とバッテリーのサービスパスワード、サーバーの `ARMOR_BATTERY_WRITE=1`、`armor/solar/<ノード>/maintenance/command` と `.../result` に対するブローカーのアクセスリスト |
| 繰り返された、または盗まれた書き込み命令 | 2 段階（アームしてから使い捨てトークンで実行）、アーム猶予 120 秒、ポートへの書き込み間隔 30 秒、10 分間に 3 回失敗すると 5 分ロック。バッテリーのシリアル番号を再入力し、バッテリーは待機中で重大アラームがないこと、ファームウェアイメージは 30 分保持して検証 |
| インバーターや ANT-BMS への書き込み | インバーター（設定で家への給電が変わる）と ANT-BMS（負荷中のバッテリーを切り離す恐れ）の書き込みコマンドは、組み立てることすらできません |
| ありえない数値が実測値として表示される | ありえない値は契約の上限で止め、`clamped` に名前を載せるので、サーバーは実測でないものとして表示できます。REVO インバーターには警告の問い合わせがないため、メッセージは「警告なし」とは言わず `warnings_known: false` と伝えます |

実際のインバーターやバッテリーからフレームを読んだことはありません。フォーマットは公開文書と記憶から書いたもので、テストの ANT-BMS フレームは他の人が取得したものです。[docs/BATTERY_MAINTENANCE.md](docs/BATTERY_MAINTENANCE.md) と [SECURITY.md](SECURITY.md) を参照してください。

## 🔌 インターフェース

| 部分 | 設定内容 |
|---|---|
| MQTT | `armor/solar/<ノード>/<機器>/state`（インバーターまたはバッテリースタックごとに 1 メッセージ、ARMOR-COMMON のコントラクトの種別 `inverter` または `battery`）、`armor/solar/<ノード>/maintenance/{command,result}`、`armor/health/<ノード>`（遺言）、`armor/control/<id>/{command,result,maintenance}`、`armor/control/all/command` |
| ポート | シリアルポート 10 本（ハードウェア UART 3 本とエミュレート 7 本、エミュレート時は最大 19200 ボー）。ポートごとの種別：`voltronic`（2400 ボー、5 秒ごと）、`pylontech`（115200、10 秒ごと）、`ant`（19200、5 秒ごと）、`raw`（9600、受信のみで何も発行しない）。インバーターの方言は `auto`、`pi30`、`revo`、`pi18`。mux プロファイルにはポートのグループが 3 つあります |
| 問い合わせ | Voltronic：`QPIGS`、`QMOD`、`QPIWS`、`QPIRI`（mux 付きの標準インバーターでは `QPIGS2`、`QPGS` も）。Pylontech コンソール：`pwr`、`bat <n>`、`info <n>`、`stat`、`soh`、イベントログ。最大 8 モジュールをセル単位で問い合わせます |
| パネル | ファミリー共通のルート（`tests/check_routes.py` がパネルの呼ぶルートの存在を検査）に加え、ポートと測定値のページ。8 言語の文言つきエラーコード 51 個（`panel/text.js` に 772 行）。Studio のメニューは [docs/STUDIO_MENUS.md](docs/STUDIO_MENUS.md) |

## ⚙️ 設定

| 変数 | 既定値 | 設定内容 |
|---|---|---|
| `enabled`, `kind`, `name`, `baud`, `rx`, `tx`, `de`, `poll_s`, `modules`, `dialect` | - | ポートごとの設定：有効かどうか、種別、トピック内の機器名、速度（0 = 種別の既定値）、ピン（RS485 の DE/RE ピンを含む）、問い合わせ周期（0 = 種別の既定値）、モジュール数、インバーターの方言。予備ネットワークは最大 3、予備ブローカーは最大 2 |
| `battery.write_enabled` | off | Pylontech バッテリーへの書き込みに対するノードの錠（管理者、バッテリーのサービスパスワードを使い、パスワードはノードの外に出ません） |
| `CONFIG_ARMOR_BOARD_S3_WIFI` / `_S3_ETH` | wifi | 基板：ESP32-S3-WROOM-1 N16R8（Wi-Fi、既定）または Waveshare ESP32-S3-ETH |
| `CONFIG_ARMOR_NODE_ID`, `_SETUP_CODE`, `_FLEET_SECRET`, `_MQTT_*`, `_HEARTBEAT_S`, `_BLE_SETUP_MINUTES`, `_BOOT_FACTORY_RESET` | - | すべてのノードに共通する設定：名前（空なら `armor-` と MAC の下位バイト）、セットアップコード（空ならランダム）、フリートシークレット（16 文字以上、リポジトリには置かない）、最初のブローカー設定、ハートビート（2〜300 秒）、Bluetooth セットアップの分数（1〜120）、BOOT によるリセット（最初の 30 秒に 8 秒） |

## 🚀 クイックスタート

**必要なもの:** ホストテスト用の C++17 コンパイラ、イメージのビルド用の ESP-IDF 5.5.5 コンテナ、ESP32-S3-WROOM-1 N16R8 または Waveshare ESP32-S3-ETH。

```bash
cmake -S tests -B build/host && cmake --build build/host && ctest --test-dir build/host   # host tests, -Werror
build/host/emit_poller_samples | python tests/check_samples.py   # what the ports make is accepted by ARMOR-COMMON
tools/build_node.sh generic                       # the image for the N16R8 board in the ESP-IDF container: dist/generic-s3-wifi.bin
tools/build_node.sh generic s3-eth               # the same firmware for the Waveshare ESP32-S3-ETH (Ethernet): dist/generic-s3-eth.bin
node tools/panel_mock.mjs --user admin:adminpass123   # the panel without a board
```

[ファームウェアガイド](docs/NODE_FIRMWARE.md)（ボード、ポート、初回起動、ベンチ確認リスト）と[配線メモ](docs/NODE_HARDWARE.md)を参照。

## 🧪 テスト

```bash
cmake -S tests -B build/host && cmake --build build/host && ctest --test-dir build/host
python -I tools/armor_ci_validate.py
```

ホスト用テストプログラム 18 個と Python の検査 6 個を `ctest` で実行します。ホストのプログラムが数えた検査は 11824 件で、`-Werror` でビルドします。

Python の検査：8 言語のパネル文言、タスク、保存パス、ロック下の Bluetooth セッション、パネルが呼ぶルート（ノードによっては HTTP コードやフォントのグリフも）

ホストテストには任意の C++17 コンパイラ（Linux、WSL、MSYS2）が必要です。Web パネルは代役ノード（`tools/panel_mock.mjs`、`tools/panel_browser_test.mjs`）を相手に実際のブラウザでも検証します。

`test_clock` は `ok` だけを出力しカウンターを持ちません。他のプログラムは検査数を報告します。テストの実フレームは `tests/pylontech_real.hpp`、`pylontech_us2000_real.hpp`、`pylontech_logs_real.hpp`、`ant_frames.hpp`、`battery_bench.hpp` にあります。

## 🏗️ ビルドとリリース

`tools/build_node.sh` が ESP-IDF 5.5.5 コンテナでイメージをビルドし、リリースにはイメージ、その SHA-256 と Ed25519 署名が基板ごとに付きます。イメージは初回のみ USB-C で書き込み、その後はパネルから更新します。

## 🩺 トラブルシューティング

| 症状 | 原因 | 対処 |
|---|---|---|
| パスワードを忘れた、またはノードがネットワークに参加しない | セットアップ中でアドレスがないノードは 90 秒後に独自の Wi-Fi `ARMOR-SETUP-xxxxxx` を開きます（`http://192.168.4.1/` を開く） | 最初からやり直すには、電源投入後 30 秒以内に BOOT を 8 秒押し続ける（設定とユーザーを消去）か、パネルの*工場出荷状態に戻す*を使います |
| GitHub からの更新が拒否される | リリースにチェックサムや署名がない、一致しない（`e_no_checksum`、`e_no_signature`、`e_checksum_mismatch`、`e_bad_signature`）、この基板用のイメージがない（`e_no_asset_for_board`）、または別プロジェクトのもの（`e_wrong_firmware`） | ノードは変更されていません。再試行するか、管理者が手動でファイルをアップロード |
| バッテリーへの書き込みがコード付きで拒否される | `serial_mismatch`、`not_pylontech`、`no_battery`、`unknown_model`、`not_idle`、`critical_alarm`、`busy`、`cooldown`、`locked`、`not_armed`、`bad_token`、`expired`、`disabled`、`image_missing`、`image_mismatch` など：アームした操作と完全に同じでないものは拒否します | 結果のコードを読み、原因を直して操作をもう一度アームする（[docs/BATTERY_MAINTENANCE.md](docs/BATTERY_MAINTENANCE.md) を参照） |
| ポートが何も発行しない | ポートがオフ、種別が `raw`（受信のみ）、速度やピンが機器と合っていない、または機器が応答しない | ポートの設定と配線を [docs/NODE_HARDWARE.md](docs/NODE_HARDWARE.md) で確認する。パネルは見ている間だけポートの生ダンプを表示 |

## 📂 リポジトリの構成

<p align="center">
  <img src="images/repository-structure.svg" alt="ARMOR-NODE-SOLAR のリポジトリ構造" width="100%">
</p>

<details>
<summary>ツリーのテキスト版</summary>

```text
ARMOR-NODE-SOLAR/
├── main/                                ファームウェア（ESP-IDF コンポーネント）
├── core/                                ESP-IDF なしのロジック（コンピューターでテスト）
├── panel/                               ファームウェアに埋め込まれた Web パネル
├── tests/                               ホストテストと Python 検査
├── docs/                                10 のガイド：プロトコル、メッセージ、保守、ハードウェア
├── tools/                               ビルド・パッケージ・検査のツール
├── .github/                             CI ワークフローと Issue・Pull Request のテンプレート
├── images/                              バナー、アイコン、この README の 2 つの図
├── build.sh build.bat                   共通のプロジェクトワークフローでビルド
├── test.sh test.bat                     共通のワークフローでテストを実行
├── run.sh run.bat                       共通のプロジェクトワークフローで起動
├── armor.project.json                   マニフェスト：バージョン、役割、成熟度、ビルドコマンド
├── CHANGELOG.md                         各バージョンの変更点
├── SECURITY.md SUPPORT.md LICENSE       セキュリティポリシー、サポート、ライセンス
├── CONTRIBUTING.md CODE_OF_CONDUCT.md   貢献方法と行動規範
├── CMakeLists.txt                       ESP-IDF プロジェクト
├── partitions.csv                       フラッシュのテーブル
├── sdkconfig.defaults                   ファームウェアの設定
├── sdkconfig.board.s3-eth               基板ごとの設定
├── sdkconfig.board.s3-wifi              基板ごとの設定
├── secrets/                             ノードファイルの例（本物はバージョン管理しません）
├── build-test.sh build-test.bat         共通のワークフローでテストを実行
└── .gitattributes .gitignore            Git の設定
```

</details>

## 🗺️ ロードマップと既知の制限

**現時点の既知の制限**

* Voltronic、Pylontech、ANT-BMS のフレーム、チェックサム、読み取り値は、文書化されたフレームと記録したサンプルに対して検証済みです。実際のインバーターやバッテリーとはまだ通信していません。
* Web パネルはブラウザーでノードの模擬に対して試しており、ボード上では試していません。
* ボードは未製作で、インバーターやバッテリーも接続していません。実機ではまだ何も動かしていません。
* 設定コード、誤ったパスワードの回数制限付きログイン、Bluetooth の PIN、署名付きアップデートはホストでテスト済みです。独立したレビューはまだありません。
* ハッシュと Ed25519 署名の確認、ロールバックの処理はホストで通ります。無線アップデートはボード上で一度も実行していません。
* Pylontech バッテリーへの書き込みは既定でオフで、シミュレーターに対してのみテストしています。

**今後**

* 実際の基板で実際のインバーターとバッテリーを使ってノードを動かし、続いて ANT-BMS の Bluetooth 接続（現在はケーブルを使用）、OTA 更新、バッテリーへの書き込みを試す。

## 🌍 言語

README とテキストは、英語、スペイン語、フランス語、イタリア語、ドイツ語、中国語、日本語、ヨーロッパポルトガル語で提供されています。

## 🔗 関連プロジェクト

**A.R.M.O.R.**（Autonomous Radar & Multimodal Observation Range）は、独立したリポジトリで構成される境界セキュリティシステムです。各リポジトリは独自のバージョン、テスト、README を持ちます。ファミリーは次のとおりです：

* **[ARMOR-COMMON](https://github.com/JuanenRac/ARMOR-COMMON)** - メッセージのコントラクト、バリデーター、適合ベクトル、生成された型
* **[ARMOR-NODE-RADAR](https://github.com/JuanenRac/ARMOR-NODE-RADAR)** - 3 つのレーダーと独自の Web パネルを備えた ESP32-S3 向けフィールドノードのファームウェア
* **ARMOR-NODE-SOLAR**（このリポジトリ） - ソーラーインバーターとバッテリーのプロトコル、ゲートウェイノードのメッセージ
* **[ARMOR-NODE-ELECTRICAL](https://github.com/JuanenRac/ARMOR-NODE-ELECTRICAL)** - 電気ノード：メーター、系統の読み取り値のメッセージ、切り替えのルール
* **[ARMOR-NODE-ALARM](https://github.com/JuanenRac/ARMOR-NODE-ALARM)** - 警報ノードとパネル：ゾーン、警戒、遅延、サイレン、PIN。サーバーありでもなしでも動作
* **[ARMOR-NODE-HMI](https://github.com/JuanenRac/ARMOR-NODE-HMI)** - タッチパネル：壁面画面でのシステムの状態、警戒と確認応答、そして音声アシスタントの置き場
* **[ARMOR-NETWORK](https://github.com/JuanenRac/ARMOR-NETWORK)** - ローカルネットワーク：そのデバイス、インターネット、そして変化
* **[ARMOR-SERVER](https://github.com/JuanenRac/ARMOR-SERVER)** - 中央コーディネーター：テレメトリ、警報、デバイス、ソーラーの読み取り値、カメラ
* **[ARMOR-STUDIO](https://github.com/JuanenRac/ARMOR-STUDIO)** - Web コンソール：カメラ、レーダー、警報、太陽光エネルギー、2D/3D のサイトデザイナー
* **[ARMOR-ANDROID-CONTROL](https://github.com/JuanenRac/ARMOR-ANDROID-CONTROL)** - ライブの 2D/3D レーダーを備えた Android オペレータークライアント
* **[ARMOR-IOS-CONTROL](https://github.com/JuanenRac/ARMOR-IOS-CONTROL)** - iPhone オペレータークライアント
* **[ARMOR-SERVER-AI](https://github.com/JuanenRac/ARMOR-SERVER-AI)** - 判断を説明し、決して作動させない視覚推論ポリシー
* **[ARMOR-VISION](https://github.com/JuanenRac/ARMOR-VISION)** - Jetson Orin NX 向けのカメラゾーン、マスク、検出ルール
* **[ARMOR-VOICE-AI](https://github.com/JuanenRac/ARMOR-VOICE-AI)** - 偽造できない確認つきのオフライン音声インテント
* **[ARMOR-HARDWARE](https://github.com/JuanenRac/ARMOR-HARDWARE)** - 筐体、電子回路、ベンチ受け入れマトリクス
* **[ARMOR-DEVOPS](https://github.com/JuanenRac/ARMOR-DEVOPS)** - デプロイ、CM5 テストベンチ、バックアップ、TLS
* **[ARMOR-SIMULATOR](https://github.com/JuanenRac/ARMOR-SIMULATOR)** - 再現可能な障害を持つオフラインのテレメトリシミュレーター
* **[ARMOR-UPDATER](https://github.com/JuanenRac/ARMOR-UPDATER)** - エコシステム自身のリポジトリを検出、インストール、更新
* **[ARMOR-DOCS](https://github.com/JuanenRac/ARMOR-DOCS)** - アーキテクチャ、セキュリティのベースライン、機能マトリクス

## 📚 ドキュメント

* [ノードのファームウェア](docs/NODE_FIRMWARE.md)：基板、ポート、初回起動、ベンチのチェックリスト
* [配線](docs/NODE_HARDWARE.md)、[プロトコル](docs/PROTOCOLS.md)、[メッセージ](docs/SOLAR_MESSAGES.md)
* [バッテリー保守](docs/BATTERY_MAINTENANCE.md)と[ベンチテスト計画](docs/BATTERY_BENCH_TEST_PLAN.md)
* [Studio のメニュー](docs/STUDIO_MENUS.md)、[ベンチテスト](docs/BENCH_TESTS.md)、[セットアップコード](docs/SETUP_CODE.md)、[Bluetooth プロビジョニング](docs/BLE_PROVISIONING.md)
* [機能マトリクス：何が証明され、何が証明されていないか](https://github.com/JuanenRac/ARMOR-DOCS/blob/main/docs/CAPABILITY_MATRIX.md)
* [プロジェクトカタログ：バージョンと、リポジトリ同士の依存関係](https://github.com/JuanenRac/ARMOR-DOCS/blob/main/docs/PROJECT_CATALOG.md)
* [このリポジトリの変更履歴](CHANGELOG.md)

## 🤝 コントリビュートとコミュニティ

* Pull Request の前に `ctest --test-dir build/host` and `python -I tools/armor_ci_validate.py` を実行してください。通る必要があります。[CONTRIBUTING.md](CONTRIBUTING.md) と[行動規範](CODE_OF_CONDUCT.md)を参照してください。
* README は 8 言語で提供され、ARMOR-DOCS のソースから生成されます。文面の変更はそちらで行います。
* コンポーネント間のメッセージの変更は ARMOR-COMMON から始めます。
* ビルドしただけのものを、ボード上で動いているかのように書かないでください。
* 脆弱性は非公開で報告してください：[SECURITY.md](SECURITY.md)。質問、アイデア、報告：electrohobby3d@gmail.com（[SUPPORT.md](SUPPORT.md)）。

## 👤 作者

**JuanenRac (Electro Hobby 3D)** · electrohobby3d@gmail.com

## 📜 ライセンス

GPL-3.0-or-later - [LICENSE](LICENSE) を参照してください。
