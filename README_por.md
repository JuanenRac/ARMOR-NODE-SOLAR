<p align="center">
  <img src="images/ARMOR_BANNER.svg" alt="ARMOR-SOLAR banner" width="100%">
</p>

# ☀️ ARMOR-SOLAR

<p align="center">
  <a href="README.md">🇺🇸 English</a> |
  <a href="README_spa.md">🇪🇸 Español</a> |
  <a href="README_fra.md">🇫🇷 Français</a> |
  <a href="README_ita.md">🇮🇹 Italiano</a> |
  <a href="README_deu.md">🇩🇪 Deutsch</a> |
  <a href="README_zho.md">🇨🇳 简体中文</a> |
  <a href="README_jpn.md">🇯🇵 日本語</a> |
  🇵🇹 <b>Português</b>
</p>


### Nó de ligação solar: lê inversores e baterias através de até dez portas série (firmware ESP32-S3, o seu painel web e a biblioteca de protocolos)

<p align="center">
  <img src="https://img.shields.io/badge/License-GPL%203.0-blue.svg" alt="GPL 3.0">
  <img src="https://img.shields.io/badge/Language-C%2B%2B17-00599c.svg" alt="Language">
  <img src="https://img.shields.io/badge/Devices-Voltronic%20%C2%B7%20Pylontech-ffb020.svg" alt="Devices">
  <img src="https://img.shields.io/badge/Maturity-scaffolding-FFB020.svg" alt="Maturity">
</p>

---

**Verificação de honestidade - o que funciona hoje:** **Maturidade: andaime.** O firmware compila no contentor ESP-IDF 5.4.2, o seu núcleo (as definições, a troca com cada tipo de equipamento, a aritmética da UART emulada: 2 548 verificações) está testado num computador com equipamento substituto, as mensagens que produz são aceites pelo ARMOR-COMMON e o painel foi exercitado num navegador contra um nó substituto. **Nunca correu numa placa e nenhum inversor nem bateria foi ligado**: o Wi-Fi, o painel sobre TLS, a atualização, as UART de hardware e emuladas e os formatos dos protocolos (escritos a partir de documentos públicos e de memória) não foram experimentados. As tramas ANT-BMS dos seus testes foram capturadas por outras pessoas nas suas próprias unidades: este projeto não leu nenhuma ele próprio.

---

## 🎯 Visão geral

* **Duas placas, um só firmware:** um ESP32-S3-WROOM-1 N16R8 em Wi-Fi (o predefinido) e a Waveshare ESP32-S3-ETH no seu cabo Ethernet (DHCP ou um endereço fixo; a sua própria rede Wi-Fi mantém-se para lhe chegar a partir de um telemóvel). A imagem escolhe-se ao compilar (`tools/build_node.sh generic s3-wifi` ou `generic s3-eth`); a tabela de pinos, os pinos predefinidos das portas e a forma de entrada seguem a placa, e uma imagem só serve para a sua própria placa. Ambas compilam e estão testadas em anfitrião; nenhuma correu numa placa.
* **O firmware do nó** (ESP32-S3-WROOM-1 N16R8 em Wi-Fi, ou a Waveshare ESP32-S3-ETH no seu cabo): dez portas série, três UART de hardware e sete emuladas (até 19200 baud), cada uma independente e cada uma a ler um inversor, uma bateria ou um monitor em bruto, de modo que um nó pode ler só inversores, só baterias ou uma mistura.
* **O painel web do nó,** também o dos nós de radar: configuração com um código mostrado na consola USB, início de sessão e utilizadores, Wi-Fi (estação e ponto de acesso), broker, atualização pelo ar com retrocesso, registo, HTTPS, e as páginas das portas (com o que cada uma ouve, para protocolos ainda não descodificados) e das leituras, em sete idiomas.
* **Inversores Voltronic / MPP Solar** (Axpert, PIP, InfiniSolar e clones), RS232 a 2400 baud: as tramas e o seu CRC, e as leituras `QPIGS` (rede, saída, bateria, FV, bits de estado), `QMOD` (modo), `QPIWS` (avisos e avarias por nome) e `QPIRI` (valores nominais). Só se podem construir comandos de leitura: uma definição muda a forma como a casa é alimentada. Três dialetos, escolhidos na porta ou encontrados por ela (*auto*): PI30, REVO (outro `QPIGS`, respostas com soma de controlo) e PI18 (`^P005GS`: InfiniSolar V, LV5048, SunGoldPower), só de leitura.
* **Baterias Pylontech** (US2000, US3000, US5000): a tabela `pwr` da consola (tensão, corrente, temperaturas e estado de carga de cada módulo), `bat <n>` (a tensão e temperatura de cada célula) e `info <n>` (modelo, capacidade restante e total, ciclos), resumidas como um só conjunto com a sua capacidade e energia totais, e a trama RS485 com as suas duas verificações. Os formatos estão escritos de memória da consola pública e podem diferir entre firmwares. As colunas encontram-se pelos nomes do cabeçalho (o formato do US5000 V2.3 com as suas colunas Id, MosTempr e SysAlarm.St incluídas), a carga restante e o equilíbrio vêm de `bat`, e o modelo, a capacidade nominal e os ciclos de `info` e `stat`, pedidos uma vez e guardados durante meia hora.
* **As mensagens do nó** (`armor/solar/<node>/<device>/state`, uma por inversor ou conjunto de baterias), definidas no ARMOR-COMMON com esquemas e vetores de conformidade; o que as portas produzem verifica-se contra elas.
* **Baterias ANT-BMS** (as placas pretas de baterias caseiras, 7S a 32S), UART de 3,3 V a 19200 baud: os dois protocolos do seu firmware (o nó pergunta num e depois no outro e fica com o que responde), com as células, temperaturas, estado de carga, corrente, capacidades e os estados dos MOSFET e do equilibrador, verificados contra tramas capturadas em quatro modelos reais. Só leitura: os seus comandos de escrita podem desligar uma bateria em carga. Um botão na página das portas lê também o modelo, a versão e as 56 definições de proteção e equilíbrio do BMS (protocolo mais recente), só de leitura.
* **Configuração a partir de um telemóvel por Bluetooth,** o mesmo canal que o do nó de radar: a aplicação ARMOR encontra o nó como `ARMOR-XXXXXX` e define o seu nome, Wi-Fi, endereço, broker e modo Bluetooth com os utilizadores do painel e o código de configuração ([o protocolo](docs/BLE_PROVISIONING.md)). Só escuta enquanto o nó não tem utilizador, a menos que se diga o contrário. O lado de rádio nunca correu numa placa.
* **A placa base com multiplexadores (o perfil *mux*):** três UART de hardware, cada uma atrás de um 74HC4052, servem até oito portas em grupos de 4, 2 e 2 que se revezam na sua linha, cada porta com a sua própria velocidade e polaridade, e um LED por porta através de um 74HC595; as células de um conjunto Pylontech alto podem ler-se em rotação. Para um inversor no dialeto padrão, a **segunda entrada FV** (`QPIGS2`) e as **unidades de um sistema em paralelo** (`QPGS`) são leituras opcionais. Nada disto correu numa placa.
* **Ainda não:** a ligação Bluetooth do ANT-BMS (o nó usa o cabo, o mais estável dos dois) e uma execução numa placa real com equipamento real.

## 📂 Estrutura do repositório

```text
ARMOR-SOLAR/
├── main/    the ESP-IDF component: app_main, solar_manager (one task per port, or per group in the mux profile), uart_ports, mux_board, port_leds_hw, network, web_server, api_shared, mqtt_link, node_store, tls_cert, ble_provision, board_ethernet
├── core/    voltronic, voltronic_pi18, pylontech, ant_bms, ant_registers, ant_settings, solar_json + solar_config, poller, soft_uart, netplan, auth, board_s3, ble_frame, ble_dispatch, mux_group, port_leds, console_probe, json (no hardware in them)
├── panel/   the web panel: index.html, app.js, text.js (7 languages), style.css
├── tools/   build_node.sh, pack_panel.py, panel_mock.mjs, panel_browser_test.mjs
├── tests/   test_solar.cpp, test_node.cpp, test_board_eth.cpp, test_ble.cpp, test_mux.cpp, test_parallel.cpp, test_console.cpp, emit_samples.cpp, emit_poller_samples.cpp, check_samples.py (+ the ANT-BMS frames)
└── docs/    NODE_FIRMWARE, NODE_HARDWARE, BLE_PROVISIONING, PROTOCOLS, SOLAR_MESSAGES, STUDIO_MENUS
```

## 🛠️ Ambiente de desenvolvimento

```bash
cmake -S tests -B build/host && cmake --build build/host
build/host/test_solar && build/host/test_node && build/host/test_board_eth && build/host/test_ble && build/host/test_mux && build/host/test_parallel && build/host/test_console      # 2,548 checks, -Werror
build/host/emit_poller_samples | python tests/check_samples.py   # what the ports make is accepted by ARMOR-COMMON
tools/build_node.sh generic                       # the firmware image for the N16R8 board in the ESP-IDF container: dist/generic-s3-wifi.bin
tools/build_node.sh generic s3-eth               # the same firmware for the Waveshare ESP32-S3-ETH (Ethernet): dist/generic-s3-eth.bin
node tools/panel_mock.mjs --user admin:adminpass123   # the panel without a board
```

Veja o [guia do firmware](docs/NODE_FIRMWARE.md) (a placa, as portas, o primeiro arranque e a lista de verificação de bancada) e as [notas de ligação](docs/NODE_HARDWARE.md).

## 🔗 Projetos relacionados

O **A.R.M.O.R.** (Autonomous Radar & Multimodal Observation Range) é um sistema de segurança perimetral feito de repositórios independentes. Cada um tem a sua própria versão, os seus próprios testes e o seu próprio README; esta é a família:

* **[ARMOR-COMMON](https://github.com/JuanenRac/ARMOR-COMMON)** - Contratos de mensagens, validadores, vetores de conformidade e tipos gerados
* **[ARMOR-RADAR](https://github.com/JuanenRac/ARMOR-RADAR)** - Firmware de nó de campo para ESP32-S3 com três radares e o seu próprio painel web
* **ARMOR-SOLAR** (este repositório) - Protocolos de inversores e baterias solares e as mensagens de um nó de ligação
* **[ARMOR-ELECTRICAL](https://github.com/JuanenRac/ARMOR-ELECTRICAL)** - Nó elétrico: contadores, a mensagem das leituras da rede e as regras de comutação
* **[ARMOR-HMI](https://github.com/JuanenRac/ARMOR-HMI)** - Painel tátil: o estado do sistema num ecrã de parede, armar e reconhecer, e a casa do assistente de voz
* **[ARMOR-NETWORK](https://github.com/JuanenRac/ARMOR-NETWORK)** - A rede local: os seus dispositivos, a internet e o que muda
* **[ARMOR-SERVER](https://github.com/JuanenRac/ARMOR-SERVER)** - Coordenador central: telemetria, alarmes, dispositivos, leituras solares e câmaras
* **[ARMOR-STUDIO](https://github.com/JuanenRac/ARMOR-STUDIO)** - Consola web: câmaras, radar, alarmes, energia solar e o desenhador de local 2D/3D
* **[ARMOR-ANDROID-CONTROL](https://github.com/JuanenRac/ARMOR-ANDROID-CONTROL)** - Cliente Android de operador com um radar 2D/3D em direto
* **[ARMOR-SERVER-AI](https://github.com/JuanenRac/ARMOR-SERVER-AI)** - Política de inferência visual que explica as suas decisões e nunca atua
* **[ARMOR-VOICE-AI](https://github.com/JuanenRac/ARMOR-VOICE-AI)** - Intenções de voz offline com uma confirmação que não se pode falsificar
* **[ARMOR-HARDWARE](https://github.com/JuanenRac/ARMOR-HARDWARE)** - Caixas, eletrónica e a matriz de aceitação de bancada
* **[ARMOR-DEVOPS](https://github.com/JuanenRac/ARMOR-DEVOPS)** - Implementação, a bancada de testes CM5, cópias de segurança e TLS
* **[ARMOR-SIMULATOR](https://github.com/JuanenRac/ARMOR-SIMULATOR)** - Simulador de telemetria offline com falhas repetíveis
* **[ARMOR-UPDATER](https://github.com/JuanenRac/ARMOR-UPDATER)** - Deteta, instala e atualiza os próprios repositórios do ecossistema
* **[ARMOR-DOCS](https://github.com/JuanenRac/ARMOR-DOCS)** - Arquitetura, linha de base de segurança e a matriz de capacidades

## 📚 Documentação e comunidade

Onde ler mais:

* [Matriz de capacidades: o que está provado e o que não](https://github.com/JuanenRac/ARMOR-DOCS/blob/main/docs/CAPABILITY_MATRIX.md)
* [Catálogo de projetos: versões e como os repositórios dependem uns dos outros](https://github.com/JuanenRac/ARMOR-DOCS/blob/main/docs/PROJECT_CATALOG.md)
* [Registo de alterações deste repositório](CHANGELOG.md)
* [Licença (GPL-3.0-or-later)](LICENSE)
* Perguntas, ideias e relatórios: electrohobby3d@gmail.com

## 👤 AUTOR

**JuanenRac (Electro Hobby 3D)** · electrohobby3d@gmail.com

## 📜 LICENÇA

GPL-3.0-or-later - veja [LICENSE](LICENSE).
