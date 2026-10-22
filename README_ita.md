<p align="center">
  <img src="images/ARMOR_BANNER.svg" alt="ARMOR-SOLAR banner" width="100%">
</p>

# ☀️ ARMOR-SOLAR

<p align="center">
  <a href="README.md">🇺🇸 English</a> |
  <a href="README_spa.md">🇪🇸 Español</a> |
  <a href="README_fra.md">🇫🇷 Français</a> |
  🇮🇹 <b>Italiano</b> |
  <a href="README_deu.md">🇩🇪 Deutsch</a> |
  <a href="README_zho.md">🇨🇳 简体中文</a> |
  <a href="README_jpn.md">🇯🇵 日本語</a> |
  <a href="README_por.md">🇵🇹 Português</a>
</p>

### Nodo gateway solare: legge inverter e batterie da fino a dieci porte seriali (firmware ESP32-S3, il suo pannello web e la libreria dei protocolli)

<p align="center">
  <img src="https://img.shields.io/badge/License-GPL%203.0-blue.svg" alt="GPL 3.0">
  <img src="https://img.shields.io/badge/Language-C%2B%2B17-00599c.svg" alt="Language">
  <img src="https://img.shields.io/badge/Devices-Voltronic%20%C2%B7%20Pylontech-ffb020.svg" alt="Devices">
  <img src="https://img.shields.io/badge/Version-0.5.4-0f6cbd.svg" alt="Version">
  <img src="https://img.shields.io/badge/Maturity-functional-00E5FF.svg" alt="Maturity">
</p>

---

**Controllo di onestà - cosa funziona oggi:** Il firmware si compila nel contenitore ESP-IDF 5.5.5, il suo nucleo (le impostazioni, lo scambio con ogni tipo di apparecchio, l'aritmetica dell'UART emulata, il canale Bluetooth, i controlli di versione e firma: 3.013 controlli in 10 programmi di test) è provato su un computer con apparecchi sostitutivi, i messaggi che produce sono accettati da ARMOR-COMMON e il pannello è stato esercitato in un browser contro un nodo sostitutivo. **Non ha mai girato su una scheda e nessun inverter o batteria è stato collegato**: il Wi-Fi, il pannello su TLS, l'aggiornamento, le UART hardware ed emulate e i formati dei protocolli (scritti da documenti pubblici e a memoria) non sono provati. I frame ANT-BMS dei suoi test sono stati catturati da altre persone sulle loro unità: questo progetto non ne ha letto nessuno di persona.

---

## 📋 In breve

| | |
|---|---|
| Versione | `0.5.4` |
| Maturità | Funzionale |
| Test | 9.319 controlli in 14 programmi (`ctest`) |
| Tecnologia | C++17 (host-tested core); ESP-IDF 5.5.5 firmware for the ESP32-S3-WROOM-1 N16R8 (Wi-Fi) and the Waveshare ESP32-S3-ETH (Ethernet); JavaScript web panel |
| Su hardware reale | Ancora nessuno: nessuna scheda montata né inverter o batteria collegati (solo test su computer, la compilazione e il pannello in un browser) |

## 🎯 Panoramica

* **Due schede, un solo firmware:** una ESP32-S3-WROOM-1 N16R8 via Wi-Fi (predefinita) e la Waveshare ESP32-S3-ETH sul suo cavo Ethernet (DHCP o indirizzo fisso; tiene la propria rete Wi-Fi per raggiungerla da un telefono). L'immagine si sceglie in compilazione (`tools/build_node.sh generic s3-wifi` o `generic s3-eth`); la tabella dei pin, i pin predefiniti delle porte e l'accesso seguono la scheda, e un'immagine vale solo per la sua scheda. Entrambe compilano e sono testate sul computer; nessuna ha girato su una scheda.
* **Il firmware del nodo** (ESP32-S3-WROOM-1 N16R8 via Wi-Fi, o Waveshare ESP32-S3-ETH via cavo): dieci porte seriali, tre UART hardware e sette emulate (fino a 19200 baud), ognuna indipendente e ognuna legge un inverter, una batteria o un monitor grezzo, così un nodo può leggere solo inverter, solo batterie o un misto.
* **Il pannello web del nodo,** lo stesso dei nodi radar: configurazione con un codice mostrato sulla console USB, accesso e utenti, Wi-Fi (stazione e punto di accesso), broker, aggiornamento via radio con ripristino, registro, HTTPS, e le pagine delle porte (con ciò che ognuna sente, per protocolli non ancora decodificati) e delle letture, in otto lingue.
* **Inverter Voltronic / MPP Solar** (Axpert, PIP, InfiniSolar e cloni), RS232 a 2400 baud: i frame e il loro CRC, e le letture `QPIGS` (rete, uscita, batteria, FV, bit di stato), `QMOD` (modo), `QPIWS` (avvisi e guasti per nome) e `QPIRI` (valori nominali). Si possono costruire solo comandi di lettura: un'impostazione cambia come viene alimentata la casa. Tre dialetti, scelti sulla porta o trovati da essa (*auto*): PI30, REVO (un altro `QPIGS`, risposte con checksum) e PI18 (`^P005GS`: InfiniSolar V, LV5048, SunGoldPower), sola lettura.
* **Batterie Pylontech** (US2000, US3000, US5000): la tabella `pwr` della console (tensione, corrente, temperature e stato di carica di ogni modulo), `bat <n>` (tensione e temperatura di ogni cella) e `info <n>` (modello, capacità residua e totale, cicli), riassunte come un unico pacco con capacità ed energia totali, e il frame RS485 con i suoi due controlli. I formati sono scritti a memoria dalla console pubblica e possono variare tra i firmware. Le colonne si trovano dai nomi dell'intestazione (anche il formato della US5000 V2.3 con le colonne Id, MosTempr e SysAlarm.St), la carica residua e il bilanciamento vengono da `bat`, e modello, capacità nominale e cicli da `info` e `stat`, richiesti una volta e conservati mezz'ora.
* **Cosa dice di sé un modulo Pylontech:** il suo modello e le versioni della sua scheda e del suo software, le sue celle e la corrente che ammette, i contatori della sua vita (`stat`), la salute di ogni cella (`soh`, con quella anomala segnata) e la voce più recente del suo registro eventi, con i nomi delle protezioni come li stampa la console. La console si limita a leggere, e un amministratore può scaricare come file di testo ciò che una porta ha sentito.
* **I messaggi del nodo** (`armor/solar/<nodo>/<dispositivo>/state`, uno per inverter o pacco di batterie), definiti in ARMOR-COMMON con schemi e vettori di conformità; ciò che producono le porte è verificato contro di essi.
* **Batterie ANT-BMS** (le schede nere dei pacchi autocostruiti, da 7S a 32S), UART a 3,3 V a 19200 baud: entrambi i protocolli del suo firmware (il nodo chiede nell'uno e poi nell'altro e resta con quello che risponde), con celle, temperature, stato di carica, corrente, capacità e stati dei MOSFET e del bilanciatore, verificati su frame catturati su quattro modelli reali. Solo lettura: i suoi comandi di scrittura possono scollegare una batteria sotto carico. Un pulsante della pagina delle porte legge anche modello, versione e le 56 impostazioni di protezione e bilanciamento del BMS (protocollo nuovo), in sola lettura.
* **Configurazione dal telefono via Bluetooth,** lo stesso canale del nodo radar: l'app ARMOR trova il nodo come `ARMOR-XXXXXX` e ne imposta nome, Wi-Fi, indirizzo, broker e modo Bluetooth con gli utenti e il codice di messa in servizio del pannello ([il protocollo](docs/BLE_PROVISIONING.md)). Ascolta solo finché il nodo non ha utenti, salvo diversa impostazione. La parte radio non ha mai girato su una scheda.
* **La scheda base con multiplexer (il profilo *mux*):** tre UART hardware, ciascuna dietro un 74HC4052, servono fino a otto porte in gruppi da 4, 2 e 2 che si alternano sulla loro linea, ogni porta con la propria velocità e polarità, e un LED per porta tramite un 74HC595; le celle di una pila Pylontech alta si possono leggere a rotazione. Per un inverter nel dialetto standard, il **secondo ingresso fotovoltaico** (`QPIGS2`) e le **unità di un sistema in parallelo** (`QPGS`) sono letture facoltative. Nulla di tutto ciò ha mai girato su una scheda.
* **Aggiornamenti via etere firmati:** il nodo installa da GitHub solo un'immagine il cui SHA-256 coincide e che porta una firma Ed25519 valida del responsabile (la chiave pubblica è nel firmware, quella privata non lascia mai il responsabile); un file caricato a mano da un amministratore nel pannello resta accettato. Le pagine e i messaggi del pannello sono in otto lingue.
* **Abbinamento Bluetooth con PIN:** il PIN a sei cifre deriva dal codice di configurazione, è stampato sulla console USB e mostrato a un amministratore nel pannello; il telefono deve inserirlo prima di poter scrivere le impostazioni del nodo.
* **Non ancora:** il collegamento Bluetooth dell'ANT-BMS (il nodo usa il cavo, il più stabile dei due) e una prova su una scheda reale con dispositivi reali.
* **Solo la casa può usare il pannello:** il pannello risponde per impostazione predefinita alla rete privata (o alle reti indicate dal proprietario, come una VPN), rifiuta un elenco che escluderebbe chi lo salva, conta le password errate per persona e per indirizzo e funziona dietro il proxy del server per essere raggiunto da fuori casa senza pubblicare il nodo.**Sconosciuto non è nessuno:** un inverter REVO non ha una richiesta degli avvisi, quindi il suo messaggio porta `warnings_known: false` e Studio mostra gli avvisi come sconosciuti invece di "nessuno".
* **Collegamento cifrato facoltativo con il broker (TLS):** disattivato per impostazione predefinita, quindi il nodo parla MQTT non cifrato come prima. Un interruttore e una casella per il certificato dell'autorità propria dell'installazione (PEM, al massimo 2 KB) nella pagina Broker lo fanno collegare con `mqtts://`, fidarsi solo di quell'autorità e verificare che il certificato del broker sia stato fatto per l'indirizzo raggiunto; ciò che non è un solo certificato PEM viene rifiutato. Coperto da test sul computer; l'handshake non è ancora stato eseguito su una scheda.
* **Rapporto di salute:** circa una volta al minuto il nodo pubblica da quanto è acceso e perché si è riavviato l'ultima volta, la memoria libera, il collegamento e il segnale Wi-Fi, quante volte si è connesso al broker e l'ultimo errore registrato (`armor/health/<nodo>`, il contratto `node_health` di ARMOR-COMMON). Il codice che lo costruisce è condiviso da tutti i nodi e testato su un computer; non è mai girato su una scheda.
* **Scrittura sulle batterie Pylontech (disattivata di default):** solo operazioni tipizzate - orologio, contatori e registri, riavvio, capacità, il ripristino del livello di carica, limiti e protezioni, uscite, numero di serie, memoria esterna, firmware via XMODEM-1K e una console esperto con lista di blocco - in due passi (prepara, poi esegui con un token monouso), leggendo la batteria prima e dopo, con la password di servizio conservata solo nel nodo e una console simulata per i test. Non provato su una batteria reale: vedi il [piano di prove al banco](docs/BATTERY_BENCH_TEST_PLAN.md).
* **Controllo dal server:** Studio può riavviare il nodo in modo pulito, metterlo in pausa fino a un'ora (riprende da solo), mettere il suo lavoro in standby (senza fine, conservato ai riavvii, il nodo resta connesso e continua a rispondere), farlo mostrare, chiedere un'istantanea diagnostica e segnarlo in manutenzione (`armor/control/<nodo>/...`, il contratto `node_control` di ARMOR-COMMON). Ogni ordine è in due passi con un token monouso legato a quell'operazione, solo il server può scriverli sul broker e non esistono ripristino di fabbrica, cancellazione né sonno profondo. Finché il nodo è in pausa o in standby non scrive su nessuna batteria (un'operazione in corso si può ancora annullare) e continua a leggere inverter e batterie. Il codice è condiviso da tutti i nodi e provato su un computer; non ha ancora girato su una scheda.
* **I valori assurdi sono nominati, non nascosti:** quando un inverter invia un numero impossibile, il nodo lo trattiene al limite del contratto e lo nomina in `clamped`, così il server lo mostra come non reale; e il dump grezzo di una porta viene prodotto solo mentre il pannello lo sta guardando.
* **Diagnostica senza cavo:** dopo un avvio non pulito (un blocco, un watchdog, un calo di alimentazione, un'allocazione fallita) il rapporto di salute ne indica la causa per dieci minuti; un arresto anomalo lascia il suo core dump da vedere, scaricare o cancellare dal pannello; le impostazioni salvate vengono copiate prima di ogni installazione del firmware e recuperate se un aggiornamento le danneggia; e un amministratore può chiedere al nodo, tramite il server, un pacchetto di diagnostica o il suo registro con un token monouso.

## 🖼️ Schermate

Catturate a 1920×1080 su un banco di prova, con dati di esempio e senza alcuna installazione reale dietro.

<table>
<tr><td width="50%" valign="top"><img src="images/screenshots/overview.png" alt="Riepilogo: lo stato del nodo in breve - rete, broker, firmware e ciò che riporta." width="100%"><br><sub>Riepilogo: lo stato del nodo in breve - rete, broker, firmware e ciò che riporta.</sub></td><td width="50%" valign="top"><img src="images/screenshots/ports.png" alt="Porte: le porte seriali del nodo e l'apparecchiatura su ciascuna." width="100%"><br><sub>Porte: le porte seriali del nodo e l'apparecchiatura su ciascuna.</sub></td></tr>
<tr><td width="50%" valign="top"><img src="images/screenshots/readings.png" alt="Letture: ciò che il nodo legge in questo momento, con l'età di ogni valore." width="100%"><br><sub>Letture: ciò che il nodo legge in questo momento, con l'età di ogni valore.</sub></td><td width="50%" valign="top"><img src="images/screenshots/network.png" alt="Rete: l'indirizzamento Ethernet o Wi-Fi del nodo, impostato dal suo pannello web." width="100%"><br><sub>Rete: l'indirizzamento Ethernet o Wi-Fi del nodo, impostato dal suo pannello web.</sub></td></tr>
</table>

## 🧩 Ruolo in A.R.M.O.R.

ARMOR-SOLAR è il gateway fra gli apparecchi solari della casa e la famiglia. Legge inverter e batterie attraverso fino a dieci porte seriali e pubblica ciò che legge per [ARMOR-SERVER](https://github.com/JuanenRac/ARMOR-SERVER), che conserva lo storico, i totali e gli allarmi; i menu *Inverter* e *Batterie* di [ARMOR-STUDIO](https://github.com/JuanenRac/ARMOR-STUDIO) e la schermata Solare di [ARMOR-ANDROID-CONTROL](https://github.com/JuanenRac/ARMOR-ANDROID-CONTROL) lo mostrano. Si limita a leggere: non scrive mai un'impostazione in nessun apparecchio.

## 📂 Struttura del repository

```text
ARMOR-SOLAR/
├── main/    the ESP-IDF component: app_main, solar_manager (one task per port, or per group in the mux profile), uart_ports, mux_board, port_leds_hw, network, web_server, api_shared, mqtt_link, node_store, tls_cert, ble_provision, board_ethernet
├── core/    voltronic, voltronic_pi18, pylontech, ant_bms, ant_registers, ant_settings, solar_json + solar_config, poller, soft_uart, netplan, auth, board_s3, ble_frame, ble_dispatch, mux_group, port_leds, console_probe, json (no hardware in them)
├── panel/   the web panel: index.html, app.js, text.js (8 languages), style.css
├── tools/   build_node.sh, pack_panel.py, panel_mock.mjs, panel_browser_test.mjs
├── tests/   test_solar.cpp, test_node.cpp, test_mqtt_tls.cpp, test_board_eth.cpp, test_ble.cpp, test_mux.cpp, test_parallel.cpp, test_console.cpp, emit_samples.cpp, emit_poller_samples.cpp, check_samples.py (+ the ANT-BMS frames)
└── docs/    NODE_FIRMWARE, NODE_HARDWARE, BLE_PROVISIONING, PROTOCOLS, SOLAR_MESSAGES, STUDIO_MENUS
```

## 🛠️ Ambiente di sviluppo

**Requisiti:** un compilatore C++17 per i test sul computer, il container ESP-IDF 5.5.5 per compilare l'immagine, un ESP32-S3-WROOM-1 N16R8 o un Waveshare ESP32-S3-ETH

```bash
cmake -S tests -B build/host && cmake --build build/host
build/host/test_solar && build/host/test_node && build/host/test_board_eth && build/host/test_ble && build/host/test_mux && build/host/test_parallel && build/host/test_console && build/host/test_release      # 9,319 checks, -Werror
build/host/emit_poller_samples | python tests/check_samples.py   # what the ports make is accepted by ARMOR-COMMON
tools/build_node.sh generic                       # the firmware image for the N16R8 board in the ESP-IDF container: dist/generic-s3-wifi.bin
tools/build_node.sh generic s3-eth               # the same firmware for the Waveshare ESP32-S3-ETH (Ethernet): dist/generic-s3-eth.bin
node tools/panel_mock.mjs --user admin:adminpass123   # the panel without a board
```

Vedi la [guida al firmware](docs/NODE_FIRMWARE.md) (la scheda, le porte, il primo avvio e l'elenco delle prove da banco) e le [note di cablaggio](docs/NODE_HARDWARE.md).

## 🔗 Progetti correlati

**A.R.M.O.R.** (Autonomous Radar & Multimodal Observation Range) è un sistema di sicurezza perimetrale fatto di repository indipendenti. Ognuno ha la propria versione, i propri test e il proprio README; ecco la famiglia:

* **[ARMOR-COMMON](https://github.com/JuanenRac/ARMOR-COMMON)** - Contratti dei messaggi, validatori, vettori di conformità e tipi generati
* **[ARMOR-RADAR](https://github.com/JuanenRac/ARMOR-RADAR)** - Firmware del nodo di campo per ESP32-S3 con tre radar e un proprio pannello web
* **ARMOR-SOLAR** (questo repository) - Protocolli di inverter e batterie solari e messaggi di un nodo gateway
* **[ARMOR-ELECTRICAL](https://github.com/JuanenRac/ARMOR-ELECTRICAL)** - Nodo elettrico: contatori, il messaggio delle letture della rete e le regole di manovra
* **[ARMOR-ALARM](https://github.com/JuanenRac/ARMOR-ALARM)** - Nodo e centrale d'allarme: zone, inserimento, ritardi, sirena e PIN, con il server o senza
* **[ARMOR-HMI](https://github.com/JuanenRac/ARMOR-HMI)** - Pannello touch: lo stato del sistema su uno schermo a parete, attivare e riconoscere gli allarmi, e la casa dell'assistente vocale
* **[ARMOR-NETWORK](https://github.com/JuanenRac/ARMOR-NETWORK)** - La rete locale: i suoi dispositivi, internet e ciò che cambia
* **[ARMOR-SERVER](https://github.com/JuanenRac/ARMOR-SERVER)** - Coordinatore centrale: telemetria, allarmi, dispositivi, letture solari e telecamere
* **[ARMOR-STUDIO](https://github.com/JuanenRac/ARMOR-STUDIO)** - Console web: telecamere, radar, allarmi, energia solare e progettista del sito 2D/3D
* **[ARMOR-ANDROID-CONTROL](https://github.com/JuanenRac/ARMOR-ANDROID-CONTROL)** - Client Android dell'operatore con radar 2D/3D in tempo reale
* **[ARMOR-SERVER-AI](https://github.com/JuanenRac/ARMOR-SERVER-AI)** - Politica di inferenza visiva che spiega le sue decisioni e non agisce mai
* **[ARMOR-VISION](https://github.com/JuanenRac/ARMOR-VISION)** - Zone delle telecamere, maschere e regole di rilevamento per la Jetson Orin NX
* **[ARMOR-VOICE-AI](https://github.com/JuanenRac/ARMOR-VOICE-AI)** - Intenti vocali offline con una conferma impossibile da falsificare
* **[ARMOR-HARDWARE](https://github.com/JuanenRac/ARMOR-HARDWARE)** - Contenitori, elettronica e matrice di accettazione da banco
* **[ARMOR-DEVOPS](https://github.com/JuanenRac/ARMOR-DEVOPS)** - Distribuzione, banco di prova CM5, backup e TLS
* **[ARMOR-SIMULATOR](https://github.com/JuanenRac/ARMOR-SIMULATOR)** - Simulatore di telemetria offline con guasti ripetibili
* **[ARMOR-UPDATER](https://github.com/JuanenRac/ARMOR-UPDATER)** - Rileva, installa e aggiorna i repository stessi dell'ecosistema
* **[ARMOR-DOCS](https://github.com/JuanenRac/ARMOR-DOCS)** - Architettura, base di sicurezza e matrice delle capacità

## 📚 Documentazione e comunità

Dove leggere di più:

* [Matrice delle capacità: cosa è provato e cosa no](https://github.com/JuanenRac/ARMOR-DOCS/blob/main/docs/CAPABILITY_MATRIX.md)
* [Catalogo dei progetti: versioni e dipendenze tra i repository](https://github.com/JuanenRac/ARMOR-DOCS/blob/main/docs/PROJECT_CATALOG.md)
* [Cronologia delle modifiche di questo repository](CHANGELOG.md)
* [Licenza (GPL-3.0-or-later)](LICENSE)
* Domande, idee e segnalazioni: electrohobby3d@gmail.com

## 👤 AUTORE

**JuanenRac (Electro Hobby 3D)** · electrohobby3d@gmail.com

## 📜 LICENZA

GPL-3.0-or-later - vedi [LICENSE](LICENSE).
