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
  🇨🇳 <b>简体中文</b> |
  <a href="README_jpn.md">🇯🇵 日本語</a> |
  <a href="README_por.md">🇵🇹 Português</a>
</p>

### 太阳能网关节点：通过最多十个串口读取逆变器和电池（ESP32-S3 固件、其网页面板和协议库）

<p align="center">
  <img src="https://img.shields.io/badge/License-GPL%203.0-blue.svg" alt="GPL 3.0">
  <img src="https://img.shields.io/badge/Language-C%2B%2B17-00599c.svg" alt="Language">
  <img src="https://img.shields.io/badge/Devices-Voltronic%20%C2%B7%20Pylontech-ffb020.svg" alt="Devices">
  <img src="https://img.shields.io/badge/Version-0.5.5-0f6cbd.svg" alt="Version">
  <img src="https://img.shields.io/badge/Maturity-functional-00E5FF.svg" alt="Maturity">
</p>

---

**诚实性检查 - 今天真正能运行的部分:** 固件可在 ESP-IDF 5.5.5 容器中构建，其核心（设置、与每种设备的交互、模拟 UART 的运算、蓝牙通道、版本和签名检查：14 个测试程序中的 10,527 项检查）在计算机上用替身设备测试，它生成的消息被 ARMOR-COMMON 接受，面板已在浏览器中对照替身节点验证。**它从未在板上运行，也没有连接过任何逆变器或电池**：Wi-Fi、TLS 上的面板、更新、硬件和模拟 UART 以及协议格式（根据公开文档和记忆编写）都未经尝试。其测试中的 ANT-BMS 帧由其他人在自己的设备上捕获：本项目自己还没有读取过任何一帧。

---

## 📋 速览

| | |
|---|---|
| 版本 | `0.5.5` |
| 成熟度 | 可用 |
| 测试 | 14 个程序中的 10,527 项检查（`ctest`） |
| 技术栈 | C++17 (host-tested core); ESP-IDF 5.5.5 firmware for the ESP32-S3-WROOM-1 N16R8 (Wi-Fi) and the Waveshare ESP32-S3-ETH (Ethernet); JavaScript web panel |
| 真实硬件 | 暂无：没有制作板子，也没有连接逆变器或电池（仅有计算机测试、构建和浏览器中的面板） |

## 🎯 概述

* **两种开发板，一套固件：** 走 Wi-Fi 的 ESP32-S3-WROOM-1 N16R8（默认）和走以太网线的 Waveshare ESP32-S3-ETH（DHCP 或固定地址；保留自己的 Wi-Fi 网络以便从手机访问）。镜像在构建时选择（`tools/build_node.sh generic s3-wifi` 或 `generic s3-eth`）；引脚表、端口的默认引脚和接入方式随开发板而定，镜像只适用于对应的开发板。两者都能构建并已在电脑上测试，但都没在开发板上运行过。
* **节点固件**（Wi-Fi 的 ESP32-S3-WROOM-1 N16R8，或有线的 Waveshare ESP32-S3-ETH）：十个串口，三个硬件 UART 和七个模拟 UART（最高 19200 波特），每个都独立，可读取逆变器、电池或原始监视，因此一个节点可以只读逆变器、只读电池或两者混合。
* **节点的网页面板，** 与雷达节点相同：用 USB 控制台显示的代码进行设置、登录和用户、Wi-Fi（站点和接入点）、代理、带回滚的空中更新、日志、HTTPS，以及串口页面（显示每个端口收到的内容，用于尚未解码的协议）和读数页面，支持八种语言。
* **Voltronic / MPP Solar 逆变器**（Axpert、PIP、InfiniSolar 及其克隆），RS232 2400 波特：帧及其 CRC，以及读数 `QPIGS`（电网、输出、电池、光伏、状态位）、`QMOD`（模式）、`QPIWS`（按名称列出的警告和故障）和 `QPIRI`（额定值）。只能构造读取命令：设置会改变房屋的供电方式。三种方言，可在端口上选择或由端口自动找到（*auto*）：PI30、REVO（另一种 `QPIGS`，以校验和结尾的应答）和 PI18（`^P005GS`：InfiniSolar V、LV5048、SunGoldPower），仅读取。
* **Pylontech 电池**（US2000、US3000、US5000）：控制台的 `pwr` 表（每个模块的电压、电流、温度和电量）、`bat <n>`（每个电芯的电压和温度）以及 `info <n>`（型号、剩余和满容量、循环次数），汇总为一个电池组及其总容量和能量，还有带两项校验的 RS485 帧。这些格式是凭对公开控制台的记忆写成的，不同固件可能不同。各列按表头名称查找（包括带 Id 列、MosTempr 和 SysAlarm.St 的 US5000 V2.3 格式），剩余电量和均衡来自 `bat`，型号、额定容量和循环次数来自 `info` 和 `stat`，只询问一次并保留半小时。
* **Pylontech 模块自述的信息：** 型号、电路板和软件版本、电芯数量和允许的电流、寿命计数器（`stat`）、每个电芯的健康状态（`soh`，标出异常的一个），以及其事件日志的最新条目，保护名称与控制台打印的完全一致。控制台只读，管理员可以把端口收到的内容下载为文本文件。
* **节点的消息**（`armor/solar/<节点>/<设备>/state`，每台逆变器或电池组一条），在 ARMOR-COMMON 中以模式和一致性向量定义；各端口生成的内容会对照它们检查。
* **ANT-BMS 电池**（自制电池组上的黑色板，7S 至 32S），3.3 V UART，19200 波特：支持其固件的两种协议（节点先用一种询问，无应答再用另一种，并保持在有应答的那种），读取电芯、温度、荷电状态、电流、容量以及 MOSFET 和均衡器状态，并用在四种真实型号上抓取的帧做了验证。只读：其写入命令可能在带载时断开电池。端口页面上的按钮还可读取 BMS 的型号、版本以及 56 项保护和均衡设置（新协议，只读）。
* **通过蓝牙用手机配置：** 与雷达节点相同的通道。ARMOR 应用会以 `ARMOR-XXXXXX` 找到节点，并使用面板的用户和设置码来设置其名称、Wi-Fi、地址、代理和蓝牙模式（[协议](docs/BLE_PROVISIONING.md)）。除非另行设置，它只在节点还没有用户时监听。无线部分从未在电路板上运行过。
* **带多路复用器的底板（*mux* 配置）：** 三个硬件 UART，每个前面有一个 74HC4052，为最多八个端口（分为 4、2、2 三组）轮流使用各自的线路；每个端口有自己的速度和极性，并通过 74HC595 为每个端口提供一个指示灯；较高的 Pylontech 电池堆的电芯可以轮流读取。对标准方言的逆变器，**第二路光伏输入**（`QPIGS2`）和**并联系统的单元**（`QPGS`）是可选的读数。以上均未在电路板上运行过。
* **签名的无线更新：**节点只会从 GitHub 安装 SHA-256 匹配且带有维护者有效 Ed25519 签名的镜像（公钥在固件中，私钥从不离开维护者）；管理员在面板中手动上传的文件仍然被接受。面板的页面和消息有八种语言。
* **带 PIN 的蓝牙配对：**六位 PIN 由设置码派生，打印在 USB 控制台上并向管理员显示在面板中；手机必须输入它才能写入节点的设置。
* **尚未完成：** ANT-BMS 的蓝牙连接（节点使用更稳定的线缆）、Android 标签页，以及在真实开发板上用真实设备的运行。
* **只有家中才能使用面板：**面板默认只响应私有网络（或所有者列出的网络，如 VPN），拒绝会把保存者自己挡在外面的列表，按人和按地址统计错误密码，并可在服务器代理之后工作，从而无需公开节点就能在家外访问。**未知不等于没有：** REVO 逆变器没有警告查询，因此其消息带有 `warnings_known: false`，Studio 会把警告显示为“未知”而不是“无”。
* **可选的与代理加密连接(TLS):** 默认关闭,因此节点仍像以前一样使用未加密的 MQTT。代理页面中的一个开关和一个用于填写本安装自有证书颁发机构证书(PEM,最大 2 KB)的框,会让节点用 `mqtts://` 连接、只信任该机构,并检查代理证书是否为其所到达的地址而签发;不是单个 PEM 证书的内容会被拒绝。已有主机测试覆盖;握手尚未在电路板上运行。
* **健康报告：** 节点大约每分钟发布一次：已运行多久、上次为何重启、可用内存、连接与 Wi-Fi 信号、连接代理的次数以及记录的最后一个错误（`armor/health/<节点>`，即 ARMOR-COMMON 的 `node_health` 契约）。生成它的代码由所有节点共享并在计算机上测试；尚未在开发板上运行。
* **写入 Pylontech 电池（默认关闭）：** 只有定型操作——时钟、计数器和日志、重启、容量、电量重置、限值与保护、输出、序列号、外部存储器、通过 XMODEM-1K 升级固件，以及带禁用清单的专家控制台——分两步进行（准备，再用一次性令牌执行），操作前后都读取电池，服务密码只保存在节点中，测试使用模拟控制台。尚未在真实电池上试过：见[台架测试计划](docs/BATTERY_BENCH_TEST_PLAN.md)。
* **由服务器控制：** Studio 可以干净地重启节点、将其暂停至多一小时（自动恢复）、让其工作进入待机（无期限，重启后仍保持，节点保持连接并继续应答）、让它显示自己、请求诊断快照，并把它标记为维护中（`armor/control/<节点>/...`，ARMOR-COMMON 的 `node_control` 契约）。每条命令分两步，使用只属于该操作的一次性令牌，只有服务器可以在代理上写入，并且没有恢复出厂设置、擦除或深度睡眠。节点处于暂停或待机时，不向任何电池写入（正在进行的操作仍可取消），并继续读取逆变器和电池。 该代码由所有节点共用，并已在计算机上测试；尚未在电路板上运行过。
* **荒谬的数值会被点名，而不是被隐藏：** 当逆变器发来不可能的数字时，节点会把它限制在合同的上限，并在 `clamped` 中点名，因此服务器会把它显示为不真实；端口的原始转储只在面板正在查看时才生成。
* **无需线缆的诊断：** 在一次不干净的启动之后（崩溃、看门狗、电源跌落、内存分配失败），健康报告会在十分钟内说明原因；崩溃会留下核心转储（core dump），可在面板中查看、下载或删除；已保存的设置会在每次固件安装前被复制，并在更新损坏它们时恢复；管理员还可以通过服务器，用一次性令牌向节点索取诊断包或其日志。

## 🖼️ 屏幕截图

在测试台上以 1920×1080 截取，使用示例数据，背后没有真实的安装。

<table>
<tr><td width="50%" valign="top"><img src="images/screenshots/overview.png" alt="概览：节点状态一目了然——网络、代理、固件及其上报内容。" width="100%"><br><sub>概览：节点状态一目了然——网络、代理、固件及其上报内容。</sub></td><td width="50%" valign="top"><img src="images/screenshots/ports.png" alt="端口：节点的串行端口以及每个端口上的设备。" width="100%"><br><sub>端口：节点的串行端口以及每个端口上的设备。</sub></td></tr>
<tr><td width="50%" valign="top"><img src="images/screenshots/readings.png" alt="读数：节点此刻读取到的内容，以及每个数值的时效。" width="100%"><br><sub>读数：节点此刻读取到的内容，以及每个数值的时效。</sub></td><td width="50%" valign="top"><img src="images/screenshots/network.png" alt="网络：节点的以太网或 Wi-Fi 地址设置，在其自带的网页面板中配置。" width="100%"><br><sub>网络：节点的以太网或 Wi-Fi 地址设置，在其自带的网页面板中配置。</sub></td></tr>
</table>

## 🧩 在 A.R.M.O.R. 中的角色

ARMOR-NODE-SOLAR 是住宅太阳能设备与家族之间的网关。它通过最多十个串口读取逆变器和电池，并把读到的内容发布给 [ARMOR-SERVER](https://github.com/JuanenRac/ARMOR-SERVER)，由后者保存历史、合计和报警；[ARMOR-STUDIO](https://github.com/JuanenRac/ARMOR-STUDIO) 的*逆变器*和*电池*菜单以及 [ARMOR-ANDROID-CONTROL](https://github.com/JuanenRac/ARMOR-ANDROID-CONTROL) 的太阳能界面显示这些内容。它只读取：从不向任何设备写入设置。

## 📂 仓库结构

```text
ARMOR-NODE-SOLAR/
├── main/    the ESP-IDF component: app_main, solar_manager (one task per port, or per group in the mux profile), uart_ports, mux_board, port_leds_hw, network, web_server, api_shared, mqtt_link, node_store, tls_cert, ble_provision, board_ethernet
├── core/    voltronic, voltronic_pi18, pylontech, ant_bms, ant_registers, ant_settings, solar_json + solar_config, poller, soft_uart, netplan, auth, board_s3, ble_frame, ble_dispatch, mux_group, port_leds, console_probe, json (no hardware in them)
├── panel/   the web panel: index.html, app.js, text.js (8 languages), style.css
├── tools/   build_node.sh, pack_panel.py, panel_mock.mjs, panel_browser_test.mjs
├── tests/   test_solar.cpp, test_node.cpp, test_mqtt_tls.cpp, test_board_eth.cpp, test_ble.cpp, test_mux.cpp, test_parallel.cpp, test_console.cpp, emit_samples.cpp, emit_poller_samples.cpp, check_samples.py (+ the ANT-BMS frames)
└── docs/    NODE_FIRMWARE, NODE_HARDWARE, BLE_PROVISIONING, PROTOCOLS, SOLAR_MESSAGES, STUDIO_MENUS
```

## 🛠️ 开发环境

**环境要求:** 用于主机测试的 C++17 编译器, 用于构建镜像的 ESP-IDF 5.5.5 容器, ESP32-S3-WROOM-1 N16R8 或 Waveshare ESP32-S3-ETH

```bash
cmake -S tests -B build/host && cmake --build build/host
build/host/test_solar && build/host/test_node && build/host/test_board_eth && build/host/test_ble && build/host/test_mux && build/host/test_parallel && build/host/test_console && build/host/test_release      # 10,527 checks, -Werror
build/host/emit_poller_samples | python tests/check_samples.py   # what the ports make is accepted by ARMOR-COMMON
tools/build_node.sh generic                       # the firmware image for the N16R8 board in the ESP-IDF container: dist/generic-s3-wifi.bin
tools/build_node.sh generic s3-eth               # the same firmware for the Waveshare ESP32-S3-ETH (Ethernet): dist/generic-s3-eth.bin
node tools/panel_mock.mjs --user admin:adminpass123   # the panel without a board
```

参见[固件指南](docs/NODE_FIRMWARE.md)（开发板、端口、首次启动和台架检查清单）以及[接线说明](docs/NODE_HARDWARE.md)。

## 🔗 相关项目

**A.R.M.O.R.**（Autonomous Radar & Multimodal Observation Range）是由若干独立仓库组成的周界安防系统。每个仓库都有自己的版本、测试和 README；家族成员如下：

* **[ARMOR-COMMON](https://github.com/JuanenRac/ARMOR-COMMON)** - 消息契约、验证器、一致性向量和生成的类型
* **[ARMOR-NODE-RADAR](https://github.com/JuanenRac/ARMOR-NODE-RADAR)** - 适用于 ESP32-S3 的现场节点固件，带三个雷达和自带网页面板
* **ARMOR-NODE-SOLAR** (本仓库) - 太阳能逆变器与电池的协议，以及网关节点的消息
* **[ARMOR-NODE-ELECTRICAL](https://github.com/JuanenRac/ARMOR-NODE-ELECTRICAL)** - 电气节点：电表、电网读数消息和开关规则
* **[ARMOR-NODE-ALARM](https://github.com/JuanenRac/ARMOR-NODE-ALARM)** - 报警节点与报警主机：防区、布防、延时、警笛和 PIN，有无服务器均可
* **[ARMOR-NODE-HMI](https://github.com/JuanenRac/ARMOR-NODE-HMI)** - 触摸面板：墙面屏幕上的系统状态、布防与确认，以及语音助手的所在
* **[ARMOR-NETWORK](https://github.com/JuanenRac/ARMOR-NETWORK)** - 本地网络：其设备、互联网以及变化
* **[ARMOR-SERVER](https://github.com/JuanenRac/ARMOR-SERVER)** - 中央协调器：遥测、报警、设备、太阳能读数和摄像头
* **[ARMOR-STUDIO](https://github.com/JuanenRac/ARMOR-STUDIO)** - 网页控制台：摄像头、雷达、报警、太阳能和 2D/3D 场地设计器
* **[ARMOR-ANDROID-CONTROL](https://github.com/JuanenRac/ARMOR-ANDROID-CONTROL)** - 带实时 2D/3D 雷达的 Android 操作员客户端
* **[ARMOR-SERVER-AI](https://github.com/JuanenRac/ARMOR-SERVER-AI)** - 会解释决策且从不执行动作的视觉推理策略
* **[ARMOR-VISION](https://github.com/JuanenRac/ARMOR-VISION)** - Jetson Orin NX 上的摄像头区域、遮罩与检测规则
* **[ARMOR-VOICE-AI](https://github.com/JuanenRac/ARMOR-VOICE-AI)** - 带无法伪造确认的离线语音意图
* **[ARMOR-HARDWARE](https://github.com/JuanenRac/ARMOR-HARDWARE)** - 外壳、电子器件和台架验收矩阵
* **[ARMOR-DEVOPS](https://github.com/JuanenRac/ARMOR-DEVOPS)** - 部署、CM5 测试台、备份与 TLS
* **[ARMOR-SIMULATOR](https://github.com/JuanenRac/ARMOR-SIMULATOR)** - 带可重复故障的离线遥测模拟器
* **[ARMOR-UPDATER](https://github.com/JuanenRac/ARMOR-UPDATER)** - 发现、安装并更新生态系统自身的仓库
* **[ARMOR-DOCS](https://github.com/JuanenRac/ARMOR-DOCS)** - 架构、安全基线和能力矩阵

## 📚 文档与社区

更多阅读：

* [能力矩阵：哪些已被证实，哪些没有](https://github.com/JuanenRac/ARMOR-DOCS/blob/main/docs/CAPABILITY_MATRIX.md)
* [项目目录：版本以及各仓库之间的依赖](https://github.com/JuanenRac/ARMOR-DOCS/blob/main/docs/PROJECT_CATALOG.md)
* [本仓库的变更记录](CHANGELOG.md)
* [许可证（GPL-3.0-or-later）](LICENSE)
* 问题、想法与反馈：electrohobby3d@gmail.com

## 👤 作者

**JuanenRac (Electro Hobby 3D)** · electrohobby3d@gmail.com

## 📜 许可证

GPL-3.0-or-later - 见 [LICENSE](LICENSE)。
