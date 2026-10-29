<p align="center">
  <img src="images/ARMOR_BANNER.svg" alt="Banner di ARMOR-NODE-SOLAR" width="100%">
</p>

# ☀️ ARMOR-NODE-SOLAR

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
  <img src="https://img.shields.io/badge/License-GPL%203.0--or--later-blue.svg" alt="GPL 3.0 o successiva">
  <img src="https://img.shields.io/badge/Language-C%2B%2B17-00599c.svg" alt="Linguaggio">
  <img src="https://img.shields.io/badge/Devices-Voltronic%20%C2%B7%20Pylontech-ffb020.svg" alt="Devices">
  <img src="https://img.shields.io/badge/Tests-11824%20checks-2ea44f.svg" alt="Test">
  <img src="https://img.shields.io/badge/Version-0.6.0-0f6cbd.svg" alt="Versione">
  <img src="https://img.shields.io/badge/Maturity-functional-00E5FF.svg" alt="Maturità">
</p>

---

**Controllo di onestà - cosa funziona oggi:** Il firmware si compila nel contenitore ESP-IDF 5.5.5, il suo nucleo (le impostazioni, lo scambio con ogni tipo di apparecchio, l'aritmetica dell'UART emulata, il canale Bluetooth, i controlli di versione e firma: 11.824 controlli in 18 programmi di test) è provato su un computer con apparecchi sostitutivi, i messaggi che produce sono accettati da ARMOR-COMMON e il pannello è stato esercitato in un browser contro un nodo sostitutivo. **Non ha mai girato su una scheda e nessun inverter o batteria è stato collegato**: il Wi-Fi, il pannello su TLS, l'aggiornamento, le UART hardware ed emulate e i formati dei protocolli (scritti da documenti pubblici e a memoria) non sono provati. I frame ANT-BMS dei suoi test sono stati catturati da altre persone sulle loro unità: questo progetto non ne ha letto nessuno di persona.

---

## 📋 In breve

| | |
|---|---|
| Versione | `0.6.0` |
| Maturità | Funzionale |
| Ruolo | Nodo di campo solare per ESP32-S3: inverter e batterie (famiglia: Field Nodes, padre: ARMOR-COMMON) |
| Tecnologia | C++17, ESP-IDF 5.5.5, ESP32-S3 |
| Test | 11824 verifiche in 18 programmi host e 6 verifiche Python (`ctest`) |
| Apparecchiature | Inverter Voltronic / MPP Solar, batterie Pylontech, ANT-BMS; 10 porte seriali |
| Gira su | ESP32-S3-WROOM-1 N16R8 o Waveshare ESP32-S3-ETH; test su host su qualsiasi computer |
| Su hardware reale | Ancora nessuno: nessuna scheda montata né inverter o batteria collegati (solo test su computer, la compilazione e il pannello in un browser) |
| Licenza | GPL-3.0-or-later |

**Vai a:** [Panoramica](#-panoramica) · [Architettura](#-architettura) · [Sicurezza](#-sicurezza-e-protezione) · [Interfacce](#-interfacce) · [Configurazione](#-configurazione) · [Avvio rapido](#-avvio-rapido) · [Test](#-test) · [Problemi](#-risoluzione-dei-problemi) · [Struttura](#-struttura-del-repository) · [Correlati](#-progetti-correlati)

## 🚦 Stato del progetto

Ogni riga è una parte di questo repository e il semaforo dice a che punto è oggi. È scritta in base a ciò che il repository può mostrare (test, compilazioni, esecuzioni), non a ciò che è previsto.

🟢 Fatto e verificato · 🟡 Funziona, con limiti noti · 🟠 Fatto a metà o non ancora verificato · 🔴 Non fatto o non funzionante

| Area | Stato | Cosa significa qui |
|---|---|---|
| Software | 🟢 | Il firmware compila nel contenitore ESP-IDF 5.5.5 per entrambe le schede (Wi-Fi ed Ethernet). |
| Test | 🟢 | Superano 11.824 controlli in 18 programmi host (`ctest`): le impostazioni, lo scambio con ogni tipo di apparecchiatura, la UART emulata, il canale Bluetooth e i controlli di versione. |
| Protocolli e dati | 🟡 | I frame, i checksum e le letture di Voltronic, Pylontech e ANT-BMS sono verificati contro i frame documentati e campioni registrati; nessuno ha ancora parlato con un inverter o una batteria reali. |
| Interfaccia | 🟡 | Il pannello web è stato provato in un browser contro una simulazione del nodo, non su una scheda. |
| Hardware | 🔴 | Nessuna scheda costruita e nessun inverter o batteria collegati: nulla ha ancora funzionato su apparecchiature reali. |
| Sicurezza | 🟡 | Il codice di configurazione, l'accesso con limite di password errate, il PIN Bluetooth e gli aggiornamenti firmati sono testati sull'host; non c'è stata una revisione indipendente. |
| Aggiornamenti | 🟠 | I controlli di hash e firma Ed25519 e la logica di ripristino passano sull'host; un aggiornamento via etere non è mai stato eseguito su una scheda. |
| Scrittura sulle apparecchiature | 🟠 | La scrittura sulle batterie Pylontech è disattivata per impostazione predefinita ed è stata provata solo contro un simulatore. |
| Integrazione | 🟢 | I messaggi del nodo sono validati contro gli schemi condivisi e i vettori di conformità di ARMOR-COMMON. |
| Documentazione | 🟢 | Otto lingue, la guida del firmware, le note di cablaggio e il registro delle modifiche sono aggiornati. |

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
* **Solo la casa può usare il pannello:** il pannello risponde per impostazione predefinita alla rete privata (o alle reti indicate dal proprietario, come una VPN), rifiuta un elenco che escluderebbe chi lo salva, conta le password errate per persona e per indirizzo e funziona dietro il proxy del server per essere raggiunto da fuori casa senza pubblicare il nodo.
* **Sconosciuto non è nessuno:** un inverter REVO non ha una richiesta degli avvisi, quindi il suo messaggio porta `warnings_known: false` e Studio mostra gli avvisi come sconosciuti invece di "nessuno".
* **Collegamento cifrato facoltativo con il broker (TLS):** disattivato per impostazione predefinita, quindi il nodo parla MQTT non cifrato come prima. Un interruttore e una casella per il certificato dell'autorità propria dell'installazione (PEM, al massimo 2 KB) nella pagina Broker lo fanno collegare con `mqtts://`, fidarsi solo di quell'autorità e verificare che il certificato del broker sia stato fatto per l'indirizzo raggiunto; ciò che non è un solo certificato PEM viene rifiutato. Coperto da test sul computer; l'handshake non è ancora stato eseguito su una scheda.
* **Rapporto di salute:** circa una volta al minuto il nodo pubblica da quanto è acceso e perché si è riavviato l'ultima volta, la memoria libera, il collegamento e il segnale Wi-Fi, quante volte si è connesso al broker e l'ultimo errore registrato (`armor/health/<nodo>`, il contratto `node_health` di ARMOR-COMMON). Il codice che lo costruisce è condiviso da tutti i nodi e testato su un computer; non è mai girato su una scheda.
* **Scrittura sulle batterie Pylontech (disattivata di default):** solo operazioni tipizzate - orologio, contatori e registri, riavvio, capacità, il ripristino del livello di carica, limiti e protezioni, uscite, numero di serie, memoria esterna, firmware via XMODEM-1K e una console esperto con lista di blocco - in due passi (prepara, poi esegui con un token monouso), leggendo la batteria prima e dopo, con la password di servizio conservata solo nel nodo e una console simulata per i test. Non provato su una batteria reale: vedi il [piano di prove al banco](docs/BATTERY_BENCH_TEST_PLAN.md).
* **Controllo dal server:** Studio può riavviare il nodo in modo pulito, metterlo in pausa fino a un'ora (riprende da solo), mettere il suo lavoro in standby (senza fine, conservato ai riavvii, il nodo resta connesso e continua a rispondere), farlo mostrare, chiedere un'istantanea diagnostica e segnarlo in manutenzione (`armor/control/<nodo>/...`, il contratto `node_control` di ARMOR-COMMON). Ogni ordine è in due passi con un token monouso legato a quell'operazione, solo il server può scriverli sul broker e non esistono ripristino di fabbrica, cancellazione né sonno profondo. Finché il nodo è in pausa o in standby non scrive su nessuna batteria (un'operazione in corso si può ancora annullare) e continua a leggere inverter e batterie. Il codice è condiviso da tutti i nodi e provato su un computer; non ha ancora girato su una scheda.
* **I valori assurdi sono nominati, non nascosti:** quando un inverter invia un numero impossibile, il nodo lo trattiene al limite del contratto e lo nomina in `clamped`, così il server lo mostra come non reale; e il dump grezzo di una porta viene prodotto solo mentre il pannello lo sta guardando.
* **Diagnostica senza cavo:** dopo un avvio non pulito (un blocco, un watchdog, un calo di alimentazione, un'allocazione fallita) il rapporto di salute ne indica la causa per dieci minuti; un arresto anomalo lascia il suo core dump da vedere, scaricare o cancellare dal pannello; le impostazioni salvate vengono copiate prima di ogni installazione del firmware e recuperate se un aggiornamento le danneggia; e un amministratore può chiedere al nodo, tramite il server, un pacchetto di diagnostica o il suo registro con un token monouso.
* **Sigillatura facoltativa dei segreti salvati:** disattivata per impostazione predefinita (`security.seal_secrets`, o `nodes.nvs_encryption` del server). Attiva, le password, i token, le chiavi, i PIN e gli hash delle password che il nodo salva restano sigillati con ChaCha20-Poly1305 e tutto il resto resta leggibile; attivarla o disattivarla li riscrive senza perdere nulla, anche se il nodo si ferma a metà. Nessun eFuse viene bruciato e la flash non è cifrata: la chiave si trova in una partizione dello stesso chip, quindi **non** protegge da chi ha la scheda, e un nodo installato prima di quella partizione mantiene i suoi segreti com'erano finché non viene flashato con la nuova tabella delle partizioni.

## 🧩 Ruolo in A.R.M.O.R.

ARMOR-NODE-SOLAR è il gateway fra gli apparecchi solari della casa e la famiglia. Legge inverter e batterie attraverso fino a dieci porte seriali e pubblica ciò che legge per [ARMOR-SERVER](https://github.com/JuanenRac/ARMOR-SERVER), che conserva lo storico, i totali e gli allarmi; i menu *Inverter* e *Batterie* di [ARMOR-STUDIO](https://github.com/JuanenRac/ARMOR-STUDIO) e la schermata Solare di [ARMOR-ANDROID-CONTROL](https://github.com/JuanenRac/ARMOR-ANDROID-CONTROL) lo mostrano. Si limita a leggere: non scrive mai un'impostazione in nessun apparecchio.

| Nome | Come | A cosa serve |
|---|---|---|
| ARMOR-SERVER | MQTT `armor/solar/<node>/<device>/state` | Riceve un messaggio per inverter o pila di batterie; ordina la manutenzione di una batteria |
| ARMOR-STUDIO | menu, collegamento al pannello | Mostra inverter e batterie e apre il pannello del nodo |
| ARMOR-ANDROID-CONTROL | Bluetooth | Configura il nodo sul campo e mostra le schermate dell'energia |
| ARMOR-COMMON | contratto solare | I messaggi sono verificati con i suoi schemi; il firmware comune è copiato da lì |

## 🏛️ Architettura

<p align="center">
  <img src="images/architecture.svg" alt="Architettura di ARMOR-NODE-SOLAR: chi lo chiama, cosa contiene e da cosa dipende" width="100%">
</p>

## 🖼️ Schermate

Catturate a 1920×1080 su un banco di prova, con dati di esempio e senza alcuna installazione reale dietro.

<table>
<tr><td width="50%" valign="top"><img src="images/screenshots/overview.png" alt="Riepilogo: lo stato del nodo in breve - rete, broker, firmware e ciò che riporta." width="100%"><br><sub>Riepilogo: lo stato del nodo in breve - rete, broker, firmware e ciò che riporta.</sub></td><td width="50%" valign="top"><img src="images/screenshots/ports.png" alt="Porte: le porte seriali del nodo e l'apparecchiatura su ciascuna." width="100%"><br><sub>Porte: le porte seriali del nodo e l'apparecchiatura su ciascuna.</sub></td></tr>
<tr><td width="50%" valign="top"><img src="images/screenshots/readings.png" alt="Letture: ciò che il nodo legge in questo momento, con l'età di ogni valore." width="100%"><br><sub>Letture: ciò che il nodo legge in questo momento, con l'età di ogni valore.</sub></td><td width="50%" valign="top"><img src="images/screenshots/network.png" alt="Rete: l'indirizzamento Ethernet o Wi-Fi del nodo, impostato dal suo pannello web." width="100%"><br><sub>Rete: l'indirizzamento Ethernet o Wi-Fi del nodo, impostato dal suo pannello web.</sub></td></tr>
</table>

## 🔒 Sicurezza e protezione

| Minaccia | Difesa |
|---|---|
| Password in chiaro o compilate | PBKDF2-HMAC-SHA256 con 10 000 round; nessuna password nel firmware (un nodo senza utenti entra in configurazione); al massimo 4 utenti (admin o visualizzatore) e da 8 a 64 caratteri; al massimo 6 sessioni, 30 minuti di inattività |
| Prendere un nodo appena programmato | La configurazione richiede un codice di avvio: quello della build (`CONFIG_ARMOR_SETUP_CODE`) o uno casuale tenuto in flash e mostrato sulla console USB; con un segreto di flotta è `HMAC-SHA256(segreto, MAC)`; è anche la chiave del Wi-Fi di configurazione (WPA2, mai in bridge) |
| Il pannello raggiunto da Internet | Risponde di default alle reti private di una casa (10/8, 172.16/12, 192.168/16, 169.254/16, 127/8 e 100.64/10 dove vivono alcune VPN); l'impostazione `web.admin_sources` elenca altri indirizzi e reti IPv4 (8 voci, 120 caratteri); tutto il resto è rifiutato tranne l'identità pubblica `GET /api/v1/session`; una lista che escluderebbe chi la salva è rifiutata |
| Il pannello in chiaro su HTTP | HTTPS sulla 443 con un certificato che il nodo crea da sé (autofirmato, in flash, impronta SHA-256 visibile per confrontarla); il pannello può essere http, https o entrambi; il cookie di sessione è sempre HttpOnly e SameSite=Strict (e Secure su TLS) |
| Un firmware falso da GitHub | Un'immagine si installa solo con uno SHA-256 coincidente e una firma Ed25519 valida fatta con la chiave del manutentore (`no_checksum`, `no_signature`, `bad_signature`, `checksum_mismatch`, `wrong_firmware`, `not_newer`, `version_mismatch` la rifiutano); un caricamento manuale da un amministratore non è verificato: un limite esplicito |
| Dirottamento della configurazione Bluetooth | Un PIN a 6 cifre derivato dal codice di configurazione, visibile sulla console USB e a un amministratore; il telefono deve digitarlo prima di scrivere le impostazioni; la finestra dura `CONFIG_ARMOR_BLE_SETUP_MINUTES` (10 di default, da 1 a 120) |
| Un ordine remoto che fa danni | Due passi con un token monouso legato a quell'operazione; solo il server può scrivere quei topic nel broker; una pausa dura al massimo un'ora; non ci sono ripristino di fabbrica, cancellazione né sonno profondo da ordine remoto |
| Segreti memorizzati letti da chi ha la scheda | Opzionale (spento di default, interruttore `security.seal_secrets` o `nodes.nvs_encryption` del server): ChaCha20-Poly1305 per password, token, chiavi, PIN e hash. Non cifra la flash né brucia un eFuse: la chiave è in una partizione dello stesso chip, quindi non protegge da chi ha la scheda |
| Il collegamento al broker in chiaro | TLS opzionale (`mqtts://`) con l'autorità propria del proprietario (PEM fino a 2 KB) e verifica del nome; spento di default. L'handshake è girato solo nei test su host, non su una scheda |
| Una batteria scritta per errore o da un attaccante | Tre serrature: l'impostazione del nodo `battery.write_enabled` (amministratori) con la password di servizio della batteria, `ARMOR_BATTERY_WRITE=1` del server, e la lista di accesso del broker per `armor/solar/<nodo>/maintenance/command` e `.../result` |
| Un ordine di scrittura ripetuto o rubato | Due passi (armare, poi eseguire con un token monouso), finestra di armamento di 120 s, 30 s di raffreddamento tra scritture su una porta, 3 errori in 10 minuti la bloccano per 5; il numero di serie della batteria si digita di nuovo, la batteria deve essere a riposo e senza allarme critico, e l'immagine del firmware è tenuta 30 minuti e verificata |
| Scrivere su un inverter o un ANT-BMS | I comandi di scrittura di un inverter (un'impostazione cambia come è alimentata la casa) e di un ANT-BMS (può scollegare una batteria sotto carico) non si possono nemmeno costruire |
| Numeri assurdi mostrati come reali | Un valore impossibile viene trattenuto al limite del contratto e nominato in `clamped`, così il server lo mostra come non reale; un inverter REVO non ha una richiesta di avvisi, quindi il suo messaggio dice `warnings_known: false` invece di affermare che non ci sono avvisi |

Nessun frame è stato letto da un vero inverter o da una vera batteria; i formati sono scritti da documenti pubblici e a memoria, e i frame ANT-BMS dei test sono stati catturati da altre persone. Leggi [docs/BATTERY_MAINTENANCE.md](docs/BATTERY_MAINTENANCE.md) e [SECURITY.md](SECURITY.md).

## 🔌 Interfacce

| Parte | Cosa imposta |
|---|---|
| MQTT | `armor/solar/<nodo>/<dispositivo>/state` (un messaggio per inverter o pila di batterie, tipo `inverter` o `battery` del contratto di ARMOR-COMMON), `armor/solar/<nodo>/maintenance/{command,result}`, `armor/health/<nodo>` (last will), `armor/control/<id>/{command,result,maintenance}` e `armor/control/all/command` |
| Porte | 10 porte seriali (3 UART hardware e 7 emulate, fino a 19200 baud se emulate); per porta un tipo: `voltronic` (2400 baud, ogni 5 s), `pylontech` (115200, ogni 10 s), `ant` (19200, ogni 5 s) o `raw` (9600, solo ascolto, non pubblica nulla); dialetti dell'inverter `auto`, `pi30`, `revo`, `pi18`; il profilo mux ha 3 gruppi di porte |
| Richieste | Voltronic: `QPIGS`, `QMOD`, `QPIWS`, `QPIRI` (e `QPIGS2`, `QPGS` su un inverter standard con il mux); console Pylontech: `pwr`, `bat <n>`, `info <n>`, `stat`, `soh`, registro eventi, fino a 8 moduli richiesti cella per cella |
| Pannello | I percorsi della famiglia (`tests/check_routes.py` verifica che ogni percorso chiamato dal pannello esista) più le pagine di porte e letture; 51 codici di errore con testo in otto lingue (772 righe in `panel/text.js`); i menu di Studio sono in [docs/STUDIO_MENUS.md](docs/STUDIO_MENUS.md) |

## ⚙️ Configurazione

| Variabile | Predefinito | Cosa imposta |
|---|---|---|
| `enabled`, `kind`, `name`, `baud`, `rx`, `tx`, `de`, `poll_s`, `modules`, `dialect` | - | Le impostazioni di ogni porta: attiva o no, il tipo, il nome del dispositivo nel topic, la velocità (0 = quella del tipo), i pin (e il pin DE/RE di un RS485), il periodo di interrogazione (0 = quello del tipo), il numero di moduli e il dialetto dell'inverter; al massimo 3 reti di riserva e 2 broker di riserva |
| `battery.write_enabled` | off | La serratura del nodo per scrivere su una batteria Pylontech (amministratori, con la password di servizio della batteria, che non lascia mai il nodo) |
| `CONFIG_ARMOR_BOARD_S3_WIFI` / `_S3_ETH` | wifi | La scheda: ESP32-S3-WROOM-1 N16R8 via Wi-Fi (predefinita) o la Waveshare ESP32-S3-ETH |
| `CONFIG_ARMOR_NODE_ID`, `_SETUP_CODE`, `_FLEET_SECRET`, `_MQTT_*`, `_HEARTBEAT_S`, `_BLE_SETUP_MINUTES`, `_BOOT_FACTORY_RESET` | - | Le impostazioni che tutti i nodi condividono: il nome (vuoto dà `armor-` e gli ultimi byte del MAC), il codice di configurazione (vuoto: casuale), il segreto di flotta (16+ caratteri, mai nel repository), le prime impostazioni del broker, il battito (da 2 a 300 s), i minuti di configurazione Bluetooth (da 1 a 120) e il ripristino con BOOT (8 s nei primi 30 s) |

## 🚀 Avvio rapido

**Requisiti:** un compilatore C++17 per i test su host; il container ESP-IDF 5.5.5 per costruire l'immagine; una ESP32-S3-WROOM-1 N16R8 o una Waveshare ESP32-S3-ETH.

```bash
cmake -S tests -B build/host && cmake --build build/host && ctest --test-dir build/host   # host tests, -Werror
build/host/emit_poller_samples | python tests/check_samples.py   # what the ports make is accepted by ARMOR-COMMON
tools/build_node.sh generic                       # the image for the N16R8 board in the ESP-IDF container: dist/generic-s3-wifi.bin
tools/build_node.sh generic s3-eth               # the same firmware for the Waveshare ESP32-S3-ETH (Ethernet): dist/generic-s3-eth.bin
node tools/panel_mock.mjs --user admin:adminpass123   # the panel without a board
```

Vedi la [guida al firmware](docs/NODE_FIRMWARE.md) (la scheda, le porte, il primo avvio e l'elenco delle prove da banco) e le [note di cablaggio](docs/NODE_HARDWARE.md).

## 🧪 Test

```bash
cmake -S tests -B build/host && cmake --build build/host && ctest --test-dir build/host
python -I tools/armor_ci_validate.py
```

18 programmi di test su host e 6 verifiche Python girano con `ctest`; 11824 verifiche contate dai programmi host, compilati con `-Werror`.

Verifiche Python: i testi del pannello in otto lingue, i task, i percorsi di salvataggio, la sessione Bluetooth sotto il suo lock e i percorsi chiamati dal pannello (e, in alcuni nodi, i codici HTTP o i glifi dei caratteri)

I test su host richiedono un qualsiasi compilatore C++17 (Linux, WSL, MSYS2); il pannello web è anche provato in un browser reale contro un nodo sostitutivo (`tools/panel_mock.mjs`, `tools/panel_browser_test.mjs`).

`test_clock` stampa solo `ok` e non ha un contatore; ogni altro programma riporta le sue verifiche. I frame reali dei test sono in `tests/pylontech_real.hpp`, `pylontech_us2000_real.hpp`, `pylontech_logs_real.hpp`, `ant_frames.hpp` e `battery_bench.hpp`.

## 🏗️ Compilazione e rilascio

`tools/build_node.sh` costruisce l'immagine nel container ESP-IDF 5.5.5 e la versione porta l'immagine, il suo SHA-256 e la sua firma Ed25519, una per scheda; l'immagine si scrive via USB-C la prima volta e poi si aggiorna dal pannello.

## 🩺 Risoluzione dei problemi

| Vedi | Perché | Fai |
|---|---|---|
| Hai perso la password o il nodo non entra più in rete | Un nodo in configurazione senza indirizzo dopo 90 s apre un proprio Wi-Fi `ARMOR-SETUP-xxxxxx` (apri `http://192.168.4.1/`) | Per ricominciare tieni premuto BOOT per 8 secondi nei primi 30 secondi dopo l'accensione (cancella impostazioni e utenti), o usa *Ripristino di fabbrica* nel pannello |
| Un aggiornamento da GitHub è rifiutato | La versione non ha checksum o firma, non corrispondono (`e_no_checksum`, `e_no_signature`, `e_checksum_mismatch`, `e_bad_signature`), non ha un'immagine per questa scheda (`e_no_asset_for_board`) o è di un altro progetto (`e_wrong_firmware`) | Il nodo non è cambiato; riprova o lascia che un amministratore carichi il file a mano |
| Una scrittura su una batteria è rifiutata con un codice | `serial_mismatch`, `not_pylontech`, `no_battery`, `unknown_model`, `not_idle`, `critical_alarm`, `busy`, `cooldown`, `locked`, `not_armed`, `bad_token`, `expired`, `disabled`, `image_missing`, `image_mismatch`...: il nodo rifiuta ciò che non è esattamente l'operazione armata | Leggi il codice nel risultato, correggi la causa e arma di nuovo l'operazione (vedi [docs/BATTERY_MAINTENANCE.md](docs/BATTERY_MAINTENANCE.md)) |
| Una porta non pubblica nulla | La porta è spenta, il suo tipo è `raw` (solo ascolto), la velocità o i pin non corrispondono all'apparecchio, o l'apparecchio non risponde | Controlla le impostazioni della porta e il cablaggio in [docs/NODE_HARDWARE.md](docs/NODE_HARDWARE.md); il pannello mostra il dump grezzo di una porta mentre la guarda |

## 📂 Struttura del repository

<p align="center">
  <img src="images/repository-structure.svg" alt="Struttura del repository di ARMOR-NODE-SOLAR" width="100%">
</p>

<details>
<summary>Versione testuale dell'albero</summary>

```text
ARMOR-NODE-SOLAR/
├── main/                                il firmware: un componente ESP-IDF
├── core/                                la logica senza ESP-IDF, testata su un computer
├── panel/                               il pannello web incorporato nel firmware
├── tests/                               test su host e verifiche Python
├── docs/                                10 guide: protocolli, messaggi, manutenzione, hardware
├── tools/                               strumenti di build, impacchettamento e verifica
├── .github/                             workflow CI e modelli per issue e pull request
├── images/                              banner, icona e le due immagini di questo README
├── build.sh build.bat                   build tramite il flusso di progetto condiviso
├── test.sh test.bat                     eseguire i test tramite il flusso condiviso
├── run.sh run.bat                       avvio tramite il flusso di progetto condiviso
├── armor.project.json                   manifesto: versione, ruolo, maturità, comando di build
├── CHANGELOG.md                         cosa è cambiato in ogni versione
├── SECURITY.md SUPPORT.md LICENSE       politica di sicurezza, supporto e licenza
├── CONTRIBUTING.md CODE_OF_CONDUCT.md   come contribuire e il codice di condotta
├── CMakeLists.txt                       il progetto ESP-IDF
├── partitions.csv                       la tabella della flash
├── sdkconfig.defaults                   le impostazioni del firmware
├── sdkconfig.board.s3-eth               le impostazioni di una scheda
├── sdkconfig.board.s3-wifi              le impostazioni di una scheda
├── secrets/                             esempio del file del nodo (quelli veri non sono versionati)
├── build-test.sh build-test.bat         eseguire i test tramite il flusso condiviso
└── .gitattributes .gitignore            impostazioni di Git
```

</details>

## 🗺️ Roadmap e limiti noti

**Limiti noti oggi**

* I frame, i checksum e le letture di Voltronic, Pylontech e ANT-BMS sono verificati contro i frame documentati e campioni registrati; nessuno ha ancora parlato con un inverter o una batteria reali.
* Il pannello web è stato provato in un browser contro una simulazione del nodo, non su una scheda.
* Nessuna scheda costruita e nessun inverter o batteria collegati: nulla ha ancora funzionato su apparecchiature reali.
* Il codice di configurazione, l'accesso con limite di password errate, il PIN Bluetooth e gli aggiornamenti firmati sono testati sull'host; non c'è stata una revisione indipendente.
* I controlli di hash e firma Ed25519 e la logica di ripristino passano sull'host; un aggiornamento via etere non è mai stato eseguito su una scheda.
* La scrittura sulle batterie Pylontech è disattivata per impostazione predefinita ed è stata provata solo contro un simulatore.

**Prossimi passi**

* Far girare il nodo su una scheda reale con un vero inverter e vere batterie, poi il collegamento Bluetooth dell'ANT-BMS (oggi si usa il cavo), l'aggiornamento via etere e le scritture su una batteria.

## 🌍 Lingue

Il README e i testi esistono in inglese, spagnolo, francese, italiano, tedesco, cinese, giapponese e portoghese europeo.

## 🔗 Progetti correlati

**A.R.M.O.R.** (Autonomous Radar & Multimodal Observation Range) è un sistema di sicurezza perimetrale fatto di repository indipendenti. Ognuno ha la propria versione, i propri test e il proprio README; ecco la famiglia:

* **[ARMOR-COMMON](https://github.com/JuanenRac/ARMOR-COMMON)** - Contratti dei messaggi, validatori, vettori di conformità e tipi generati
* **[ARMOR-NODE-RADAR](https://github.com/JuanenRac/ARMOR-NODE-RADAR)** - Firmware del nodo di campo per ESP32-S3 con tre radar e un proprio pannello web
* **ARMOR-NODE-SOLAR** (questo repository) - Protocolli di inverter e batterie solari e messaggi di un nodo gateway
* **[ARMOR-NODE-ELECTRICAL](https://github.com/JuanenRac/ARMOR-NODE-ELECTRICAL)** - Nodo elettrico: contatori, il messaggio delle letture della rete e le regole di manovra
* **[ARMOR-NODE-ALARM](https://github.com/JuanenRac/ARMOR-NODE-ALARM)** - Nodo e centrale d'allarme: zone, inserimento, ritardi, sirena e PIN, con il server o senza
* **[ARMOR-NODE-HMI](https://github.com/JuanenRac/ARMOR-NODE-HMI)** - Pannello touch: lo stato del sistema su uno schermo a parete, attivare e riconoscere gli allarmi, e la casa dell'assistente vocale
* **[ARMOR-NETWORK](https://github.com/JuanenRac/ARMOR-NETWORK)** - La rete locale: i suoi dispositivi, internet e ciò che cambia
* **[ARMOR-SERVER](https://github.com/JuanenRac/ARMOR-SERVER)** - Coordinatore centrale: telemetria, allarmi, dispositivi, letture solari e telecamere
* **[ARMOR-STUDIO](https://github.com/JuanenRac/ARMOR-STUDIO)** - Console web: telecamere, radar, allarmi, energia solare e progettista del sito 2D/3D
* **[ARMOR-ANDROID-CONTROL](https://github.com/JuanenRac/ARMOR-ANDROID-CONTROL)** - Client Android dell'operatore con radar 2D/3D in tempo reale
* **[ARMOR-IOS-CONTROL](https://github.com/JuanenRac/ARMOR-IOS-CONTROL)** - Client operatore per iPhone
* **[ARMOR-SERVER-AI](https://github.com/JuanenRac/ARMOR-SERVER-AI)** - Politica di inferenza visiva che spiega le sue decisioni e non agisce mai
* **[ARMOR-VISION](https://github.com/JuanenRac/ARMOR-VISION)** - Zone delle telecamere, maschere e regole di rilevamento per la Jetson Orin NX
* **[ARMOR-VOICE-AI](https://github.com/JuanenRac/ARMOR-VOICE-AI)** - Intenti vocali offline con una conferma impossibile da falsificare
* **[ARMOR-HARDWARE](https://github.com/JuanenRac/ARMOR-HARDWARE)** - Contenitori, elettronica e matrice di accettazione da banco
* **[ARMOR-DEVOPS](https://github.com/JuanenRac/ARMOR-DEVOPS)** - Distribuzione, banco di prova CM5, backup e TLS
* **[ARMOR-SIMULATOR](https://github.com/JuanenRac/ARMOR-SIMULATOR)** - Simulatore di telemetria offline con guasti ripetibili
* **[ARMOR-UPDATER](https://github.com/JuanenRac/ARMOR-UPDATER)** - Rileva, installa e aggiorna i repository stessi dell'ecosistema
* **[ARMOR-DOCS](https://github.com/JuanenRac/ARMOR-DOCS)** - Architettura, base di sicurezza e matrice delle capacità

## 📚 Documentazione

* [Firmware del nodo](docs/NODE_FIRMWARE.md): la scheda, le porte, il primo avvio e la lista di controllo del banco
* [Cablaggio](docs/NODE_HARDWARE.md), [protocolli](docs/PROTOCOLS.md) e [messaggi](docs/SOLAR_MESSAGES.md)
* [Manutenzione delle batterie](docs/BATTERY_MAINTENANCE.md) e il suo [piano di prove al banco](docs/BATTERY_BENCH_TEST_PLAN.md)
* [Menu di Studio](docs/STUDIO_MENUS.md), [prove al banco](docs/BENCH_TESTS.md), [codice di configurazione](docs/SETUP_CODE.md) e [provisioning Bluetooth](docs/BLE_PROVISIONING.md)
* [Matrice delle capacità: cosa è provato e cosa no](https://github.com/JuanenRac/ARMOR-DOCS/blob/main/docs/CAPABILITY_MATRIX.md)
* [Catalogo dei progetti: versioni e dipendenze tra i repository](https://github.com/JuanenRac/ARMOR-DOCS/blob/main/docs/PROJECT_CATALOG.md)
* [Cronologia delle modifiche di questo repository](CHANGELOG.md)

## 🤝 Contribuire e comunità

* Prima di una pull request esegui `ctest --test-dir build/host` and `python -I tools/armor_ci_validate.py`: deve passare. Vedi [CONTRIBUTING.md](CONTRIBUTING.md) e il [codice di condotta](CODE_OF_CONDUCT.md).
* Il README esiste in otto lingue ed è generato dalle sorgenti in ARMOR-DOCS: una modifica al testo si fa lì.
* Una modifica a un messaggio tra componenti parte da ARMOR-COMMON.
* Non descrivere come funzionante su una scheda ciò che è stato solo compilato.
* Segnala una vulnerabilità in privato: [SECURITY.md](SECURITY.md). Domande, idee e segnalazioni: electrohobby3d@gmail.com ([SUPPORT.md](SUPPORT.md)).

## 👤 AUTORE

**JuanenRac (Electro Hobby 3D)** · electrohobby3d@gmail.com

## 📜 LICENZA

GPL-3.0-or-later - vedi [LICENSE](LICENSE).
