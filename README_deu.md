<p align="center">
  <img src="images/ARMOR_BANNER.svg" alt="ARMOR-NODE-SOLAR-Banner" width="100%">
</p>

# ☀️ ARMOR-NODE-SOLAR

<p align="center">
  <a href="README.md">🇺🇸 English</a> |
  <a href="README_spa.md">🇪🇸 Español</a> |
  <a href="README_fra.md">🇫🇷 Français</a> |
  <a href="README_ita.md">🇮🇹 Italiano</a> |
  🇩🇪 <b>Deutsch</b> |
  <a href="README_zho.md">🇨🇳 简体中文</a> |
  <a href="README_jpn.md">🇯🇵 日本語</a> |
  <a href="README_por.md">🇵🇹 Português</a>
</p>

### Solar-Gateway-Knoten: liest Wechselrichter und Batterien über bis zu zehn serielle Ports (ESP32-S3-Firmware, sein Web-Panel und die Protokollbibliothek)

<p align="center">
  <img src="https://img.shields.io/badge/License-GPL%203.0--or--later-blue.svg" alt="GPL 3.0 oder neuer">
  <img src="https://img.shields.io/badge/Language-C%2B%2B17-00599c.svg" alt="Sprache">
  <img src="https://img.shields.io/badge/Devices-Voltronic%20%C2%B7%20Pylontech-ffb020.svg" alt="Devices">
  <img src="https://img.shields.io/badge/Tests-11824%20checks-2ea44f.svg" alt="Tests">
  <img src="https://img.shields.io/badge/Version-0.6.1-0f6cbd.svg" alt="Version">
  <img src="https://img.shields.io/badge/Maturity-functional-00E5FF.svg" alt="Reifegrad">
</p>

---

**Ehrlichkeitsprüfung - was heute läuft:** Die Firmware baut im ESP-IDF-5.5.5-Container, ihr Kern (die Einstellungen, der Austausch mit jeder Geräteart, die Arithmetik der emulierten UART, der Bluetooth-Kanal, die Versions- und Signaturprüfungen: 11.824 Prüfungen in 18 Testprogrammen) wird auf einem Computer mit Ersatzgeräten getestet, die von ihr erzeugten Nachrichten akzeptiert ARMOR-COMMON, und das Panel wurde in einem Browser gegen einen Ersatzknoten geprüft. **Sie lief nie auf einer Platine, und es wurde kein Wechselrichter und keine Batterie angeschlossen**: das WLAN, das Panel über TLS, das Update, die Hardware- und die emulierten UARTs und die Formate der Protokolle (nach öffentlichen Dokumenten und aus dem Gedächtnis geschrieben) sind unerprobt. Die ANT-BMS-Rahmen in den Tests wurden von anderen Personen an ihren eigenen Geräten aufgezeichnet: dieses Projekt hat selbst noch keinen gelesen.

---

## 📋 Auf einen Blick

| | |
|---|---|
| Version | `0.6.1` |
| Reifegrad | Funktionsfähig |
| Rolle | Solar-Feldknoten für ESP32-S3: Wechselrichter und Batterien (Familie: Field Nodes, übergeordnet: ARMOR-COMMON) |
| Technik | C++17, ESP-IDF 5.5.5, ESP32-S3 |
| Tests | 11824 Prüfungen in 18 Host-Programmen und 6 Python-Prüfungen (`ctest`) |
| Geräte | Voltronic- / MPP-Solar-Wechselrichter, Pylontech-Batterien, ANT-BMS; 10 serielle Ports |
| Läuft auf | ESP32-S3-WROOM-1 N16R8 oder Waveshare ESP32-S3-ETH; Host-Tests auf jedem Computer |
| Auf echter Hardware | Noch keiner: keine Platine aufgebaut und kein Wechselrichter oder keine Batterie angeschlossen (nur Computertests, der Build und das Panel im Browser) |
| Lizenz | GPL-3.0-or-later |

**Springe zu:** [Überblick](#-überblick) · [Architektur](#-architektur) · [Sicherheit](#-sicherheit-und-schutz) · [Schnittstellen](#-schnittstellen) · [Konfiguration](#-konfiguration) · [Schnellstart](#-schnellstart) · [Tests](#-tests) · [Probleme](#-fehlerbehebung) · [Struktur](#-struktur-des-repositorys) · [Verwandte](#-verwandte-projekte)

## 🚦 Projektstatus

Jede Zeile ist ein Teil dieses Repositorys, und die Ampel sagt, wie weit er heute ist. Sie beruht auf dem, was das Repository zeigen kann (Tests, Builds, Läufe), nicht auf dem, was geplant ist.

🟢 Fertig und überprüft · 🟡 Funktioniert, mit bekannten Grenzen · 🟠 Teilweise fertig oder noch nicht überprüft · 🔴 Nicht gemacht oder funktioniert nicht

| Bereich | Stand | Was das hier bedeutet |
|---|---|---|
| Software | 🟢 | Die Firmware wird im ESP-IDF-5.5.5-Container für beide Platinen (WLAN und Ethernet) gebaut. |
| Tests | 🟢 | 11.824 Prüfungen in 18 Host-Programmen bestehen (`ctest`): die Einstellungen, der Austausch mit jeder Geräteart, die emulierte UART, der Bluetooth-Kanal und die Versionsprüfungen. |
| Protokolle und Daten | 🟡 | Die Telegramme, Prüfsummen und Messwerte von Voltronic, Pylontech und ANT-BMS werden gegen die dokumentierten Telegramme und aufgezeichnete Beispiele geprüft; keines hat bisher mit einem echten Wechselrichter oder einer echten Batterie gesprochen. |
| Oberfläche | 🟡 | Das Web-Panel wurde in einem Browser gegen eine Nachbildung des Knotens erprobt, nicht auf einer Platine. |
| Hardware | 🔴 | Keine Platine gebaut und kein Wechselrichter oder keine Batterie angeschlossen: Bisher lief nichts auf echten Geräten. |
| Sicherheit | 🟡 | Der Einrichtungscode, die Anmeldung mit Begrenzung falscher Passwörter, die Bluetooth-PIN und die signierten Aktualisierungen sind auf dem Host getestet; es gab keine unabhängige Prüfung. |
| Aktualisierungen | 🟠 | Die Hash- und Ed25519-Signaturprüfungen und die Rückfalllogik bestehen auf dem Host; eine Aktualisierung über die Luft wurde nie auf einer Platine ausgeführt. |
| Schreiben auf Geräte | 🟠 | Das Schreiben auf Pylontech-Batterien ist standardmäßig aus und wurde nur gegen einen Simulator getestet. |
| Integration | 🟠 | Die Nachrichten des Knotens werden gegen die gemeinsamen Schemas und Konformitätsvektoren von ARMOR-COMMON geprüft. Sein MQTT-Client wird in den Tests gegen einen echten Mosquitto gespielt; noch keine Platine hat sie an einen Server geschickt. |
| Dokumentation | 🟢 | Acht Sprachen, die Firmware-Anleitung, die Verdrahtungshinweise und das Änderungsprotokoll sind aktuell. |

## 🎯 Überblick

* **Zwei Platinen, eine Firmware:** ein ESP32-S3-WROOM-1 N16R8 per WLAN (Standard) und die Waveshare ESP32-S3-ETH am Ethernet-Kabel (DHCP oder feste Adresse; ihr eigenes WLAN bleibt, um sie vom Handy aus zu erreichen). Das Image wird beim Bauen gewählt (`tools/build_node.sh generic s3-wifi` oder `generic s3-eth`); Pin-Tabelle, Standard-Pins der Ports und Zugangsweg folgen der Platine, und ein Image gilt nur für seine Platine. Beide bauen und sind am Rechner getestet; keine lief je auf einer Platine.
* **Die Firmware des Knotens** (ESP32-S3-WROOM-1 N16R8 per WLAN oder Waveshare ESP32-S3-ETH am Kabel): zehn serielle Ports, drei Hardware-UARTs und sieben emulierte (bis 19200 Baud), jeder unabhängig und jeder liest einen Wechselrichter, eine Batterie oder einen Rohmonitor, sodass ein Knoten nur Wechselrichter, nur Batterien oder eine Mischung lesen kann.
* **Das Web-Panel des Knotens,** auch das der Radarknoten: Einrichtung mit einem Code, der auf der USB-Konsole erscheint, Anmeldung und Benutzer, WLAN (Station und Access Point), Broker, Update per Funk mit Rollback, Protokoll, HTTPS und die Seiten der Ports (mit dem, was jeder hört, für noch nicht dekodierte Protokolle) und der Messwerte, in acht Sprachen.
* **Voltronic- / MPP-Solar-Wechselrichter** (Axpert, PIP, InfiniSolar und Klone), RS232 mit 2400 Baud: die Rahmen und ihre CRC sowie die Messwerte `QPIGS` (Netz, Ausgang, Batterie, PV, Statusbits), `QMOD` (Modus), `QPIWS` (Warnungen und Störungen mit Namen) und `QPIRI` (Nennwerte). Nur lesende Befehle lassen sich bauen: eine Einstellung ändert, wie das Haus versorgt wird. Drei Dialekte, am Port gewählt oder von ihm gefunden (*auto*): PI30, REVO (ein anderes `QPIGS`, Antworten mit Prüfsumme) und PI18 (`^P005GS`: InfiniSolar V, LV5048, SunGoldPower), nur lesend.
* **Pylontech-Batterien** (US2000, US3000, US5000): die `pwr`-Tabelle der Konsole (Spannung, Strom, Temperaturen und Ladezustand jedes Moduls), `bat <n>` (Spannung und Temperatur jeder Zelle) und `info <n>` (Modell, Rest- und Gesamtkapazität, Zyklen), zusammengefasst als ein Stapel mit Gesamtkapazität und -energie, und der RS485-Rahmen mit seinen zwei Prüfungen. Die Formate sind aus dem Gedächtnis der öffentlichen Konsole geschrieben und können sich je nach Firmware unterscheiden. Die Spalten werden über die Namen der Kopfzeile gefunden (auch das Format der US5000 V2.3 mit Id-Spalten, MosTempr und SysAlarm.St), die Restladung und das Balancing kommen aus `bat`, Modell, Nennkapazität und Zyklen aus `info` und `stat`, die einmal abgefragt und eine halbe Stunde behalten werden.
* **Was ein Pylontech-Modul über sich sagt:** sein Modell und die Versionen von Platine und Software, seine Zellen und der zulässige Strom, die Zähler seines Lebens (`stat`), der Zustand jeder Zelle (`soh`, die auffällige markiert) und der neueste Eintrag seines Ereignisprotokolls, mit den Schutznamen genau so, wie die Konsole sie ausgibt. Die Konsole liest nur, und ein Administrator kann herunterladen, was ein Anschluss gehört hat, als Textdatei.
* **Die Nachrichten des Knotens** (`armor/solar/<Knoten>/<Gerät>/state`, eine je Wechselrichter oder Batteriestapel), in ARMOR-COMMON mit Schemas und Konformitätsvektoren definiert; was die Ports erzeugen, wird dagegen geprüft.
* **ANT-BMS-Batterien** (die schwarzen Platinen selbstgebauter Akkus, 7S bis 32S), 3,3-V-UART mit 19200 Baud: beide Protokolle seiner Firmware (der Knoten fragt im einen und dann im anderen und bleibt bei dem, das antwortet), mit Zellen, Temperaturen, Ladezustand, Strom, Kapazitäten sowie den Zuständen der MOSFETs und des Balancers, geprüft an Frames, die an vier echten Modellen aufgezeichnet wurden. Nur Lesen: seine Schreibbefehle können eine Batterie unter Last trennen. Eine Schaltfläche auf der Port-Seite liest außerdem Modell, Version und 56 Schutz- und Balancing-Einstellungen des BMS (neueres Protokoll), nur lesend.
* **Konfiguration vom Handy über Bluetooth,** derselbe Kanal wie beim Radarknoten: Die ARMOR-App findet den Knoten als `ARMOR-XXXXXX` und stellt Name, WLAN, Adresse, Broker und Bluetooth-Modus mit den Benutzern und dem Einrichtungscode des Panels ein ([das Protokoll](docs/BLE_PROVISIONING.md)). Er lauscht nur, solange der Knoten keinen Benutzer hat, sofern nichts anderes eingestellt ist. Der Funkteil lief noch nie auf einer Platine.
* **Die Basisplatine mit Multiplexern (das *mux*-Profil):** drei Hardware-UARTs, je eines hinter einem 74HC4052, bedienen bis zu acht Ports in Gruppen zu 4, 2 und 2, die sich auf ihrer Leitung abwechseln, jeder Port mit eigener Geschwindigkeit und Polarität, und eine LED pro Port über ein 74HC595; die Zellen eines hohen Pylontech-Stapels lassen sich im Wechsel lesen. Für einen Wechselrichter im Standarddialekt sind der **zweite PV-Eingang** (`QPIGS2`) und die **Einheiten eines Parallelsystems** (`QPGS`) optionale Messungen. Nichts davon lief bisher auf einer Platine.
* **Signierte Over-the-Air-Updates:** der Knoten installiert von GitHub nur ein Abbild, dessen SHA-256 stimmt und das eine gültige Ed25519-Signatur des Betreuers trägt (der öffentliche Schlüssel steckt in der Firmware, der private verlässt den Betreuer nie); eine von einem Administrator im Panel von Hand hochgeladene Datei wird weiterhin akzeptiert. Seiten und Meldungen des Panels sind in acht Sprachen.
* **Bluetooth-Kopplung mit PIN:** die sechsstellige PIN wird aus dem Einrichtungscode abgeleitet, auf der USB-Konsole ausgegeben und einem Administrator im Panel angezeigt; das Telefon muss sie eingeben, bevor es die Einstellungen des Knotens schreiben darf.
* **Noch nicht:** die Bluetooth-Verbindung des ANT-BMS (der Knoten nutzt das Kabel, das stabilere von beiden) und ein Lauf auf einer echten Platine mit echten Geräten.
* **Nur das Haus darf das Panel nutzen:** Das Panel antwortet standardmäßig dem privaten Netz (oder den Netzen, die der Besitzer nennt, etwa einem VPN), lehnt eine Liste ab, die den Speichernden aussperren würde, zählt falsche Passwörter je Person und je Adresse und funktioniert hinter dem Proxy des Servers, sodass man es von außerhalb des Hauses erreicht, ohne den Knoten zu veröffentlichen.
* **Unbekannt ist nicht keine:** ein REVO-Wechselrichter hat keine Warnungsabfrage, deshalb trägt seine Nachricht `warnings_known: false` und Studio zeigt die Warnungen als unbekannt statt als "keine".
* **Optionale verschlüsselte Verbindung zum Broker (TLS):** standardmäßig aus, der Knoten spricht also wie bisher unverschlüsseltes MQTT. Ein Schalter und ein Feld für das Zertifikat der eigenen Zertifizierungsstelle der Anlage (PEM, höchstens 2 KB) auf der Broker-Seite sorgen dafür, dass er sich mit `mqtts://` verbindet, nur dieser Stelle vertraut und prüft, ob das Zertifikat des Brokers für die erreichte Adresse ausgestellt wurde; alles, was nicht genau ein PEM-Zertifikat ist, wird abgelehnt. Durch Host-Tests abgedeckt; der Handshake wurde auf einer Platine noch nicht ausgeführt.
* **Zustandsbericht:** etwa einmal pro Minute veröffentlicht der Knoten, wie lange er läuft und warum er zuletzt neu gestartet ist, seinen freien Speicher, seine Verbindung und sein WLAN-Signal, wie oft er sich mit dem Broker verbunden hat und den letzten protokollierten Fehler (`armor/health/<Knoten>`, der Vertrag `node_health` von ARMOR-COMMON). Der Code, der ihn erzeugt, wird von allen Knoten geteilt und auf einem Computer getestet; auf einer Platine ist er noch nicht gelaufen.
* **Schreiben auf Pylontech-Batterien (standardmäßig aus):** nur typisierte Operationen - Uhr, Zähler und Protokolle, Neustart, Kapazität, das Zurücksetzen des Ladezustands, Grenzwerte und Schutzfunktionen, Ausgänge, Seriennummer, externer Speicher, Firmware per XMODEM-1K und eine Expertenkonsole mit Sperrliste - in zwei Schritten (vorbereiten, dann mit einem Einmal-Token ausführen), die Batterie wird davor und danach gelesen, das Service-Passwort liegt nur im Knoten, und die Tests laufen gegen eine simulierte Konsole. An einer echten Batterie nicht erprobt: siehe den [Prüfstandsplan](docs/BATTERY_BENCH_TEST_PLAN.md).
* **Steuerung vom Server aus:** Studio kann den Knoten sauber neu starten, bis zu einer Stunde pausieren (er läuft von selbst weiter), seine Arbeit in den Standby setzen (ohne Ende, über Neustarts hinweg gespeichert, der Knoten bleibt verbunden und antwortet weiter), ihn sich zeigen lassen, einen Diagnose-Schnappschuss anfordern und ihn als in Wartung markieren (`armor/control/<Knoten>/...`, der Vertrag `node_control` von ARMOR-COMMON). Jeder Befehl besteht aus zwei Schritten mit einem Einmal-Token, das zu dieser Operation gehört; nur der Server darf sie am Broker schreiben, und es gibt kein Zurücksetzen auf Werkseinstellungen, kein Löschen und keinen Tiefschlaf. Solange der Knoten pausiert oder im Standby ist, beschreibt er keine Batterie (eine laufende Operation lässt sich noch abbrechen) und liest weiter Wechselrichter und Batterien. Der Code wird von allen Knoten geteilt und auf einem Rechner getestet; auf einer Platine ist er noch nicht gelaufen.
* **Absurde Werte werden benannt, nicht versteckt:** Sendet ein Wechselrichter eine unmögliche Zahl, hält der Knoten sie am Grenzwert des Vertrags fest und nennt sie in `clamped`, sodass der Server sie als nicht echt anzeigt; und der Rohdaten-Dump eines Ports wird nur erzeugt, solange das Panel ihn ansieht.
* **Diagnose ohne Kabel:** Nach einem unsauberen Start (Absturz, Watchdog, Spannungseinbruch, fehlgeschlagene Speicherreservierung) nennt der Gesundheitsbericht zehn Minuten lang den Grund; ein Absturz hinterlässt sein Core-Dump, das man im Panel ansehen, herunterladen oder löschen kann; die gespeicherten Einstellungen werden vor jeder Firmware-Installation kopiert und wiederhergestellt, falls ein Update sie beschädigt; und ein Administrator kann den Knoten über den Server mit einem Einmal-Token nach einem Diagnosepaket oder seinem Protokoll fragen.
* **Optionales Versiegeln gespeicherter Geheimnisse:** standardmäßig aus (`security.seal_secrets` oder `nodes.nvs_encryption` des Servers). Eingeschaltet werden die Passwörter, Token, Schlüssel, PINs und Passwort-Hashes, die der Knoten speichert, mit ChaCha20-Poly1305 versiegelt, alles andere bleibt lesbar; Ein- und Ausschalten schreibt sie ohne Verlust um, auch wenn der Knoten mittendrin stehen bleibt. Es wird kein eFuse gebrannt und der Flash nicht verschlüsselt: Der Schlüssel liegt in einer Partition desselben Chips und schützt daher **nicht** vor jemandem, der die Platine hat; ein Knoten, der vor dieser Partition installiert wurde, behält seine Geheimnisse unverändert, bis er mit der neuen Partitionstabelle geflasht wird.

## 🧩 Rolle in A.R.M.O.R.

ARMOR-NODE-SOLAR ist das Gateway zwischen den Solargeräten des Hauses und der Familie. Es liest Wechselrichter und Batterien über bis zu zehn serielle Ports und veröffentlicht, was es liest, für [ARMOR-SERVER](https://github.com/JuanenRac/ARMOR-SERVER), der Verlauf, Summen und Alarme führt; die Menüs *Wechselrichter* und *Batterien* von [ARMOR-STUDIO](https://github.com/JuanenRac/ARMOR-STUDIO) und der Solar-Bildschirm von [ARMOR-ANDROID-CONTROL](https://github.com/JuanenRac/ARMOR-ANDROID-CONTROL) zeigen es. Es liest nur: Es schreibt nie eine Einstellung in ein Gerät.

| Name | Wie | Wozu |
|---|---|---|
| ARMOR-SERVER | MQTT `armor/solar/<node>/<device>/state` | Empfängt eine Nachricht je Wechselrichter oder Batteriestapel; ordnet die Wartung einer Batterie an |
| ARMOR-STUDIO | Menüs, Link zum Panel | Zeigt Wechselrichter und Batterien und öffnet das Panel des Knotens |
| ARMOR-ANDROID-CONTROL | Bluetooth | Richtet den Knoten vor Ort ein und zeigt die Energie-Bildschirme |
| ARMOR-COMMON | Solar-Vertrag | Die Nachrichten werden gegen seine Schemas geprüft; die gemeinsame Firmware stammt daraus |

## 🏛️ Architektur

<p align="center">
  <img src="images/architecture.svg" alt="Architektur von ARMOR-NODE-SOLAR: wer es aufruft, was es enthält und wovon es abhängt" width="100%">
</p>

## 🖼️ Screenshots

Aufgenommen mit 1920×1080 auf einem Prüfstand, mit Beispieldaten und ohne eine echte Anlage dahinter.

<table>
<tr><td width="50%" valign="top"><img src="images/screenshots/overview.png" alt="Übersicht: der Zustand des Knotens auf einen Blick - Netzwerk, Broker, Firmware und was er meldet." width="100%"><br><sub>Übersicht: der Zustand des Knotens auf einen Blick - Netzwerk, Broker, Firmware und was er meldet.</sub></td><td width="50%" valign="top"><img src="images/screenshots/ports.png" alt="Anschlüsse: die seriellen Anschlüsse des Knotens und das Gerät an jedem." width="100%"><br><sub>Anschlüsse: die seriellen Anschlüsse des Knotens und das Gerät an jedem.</sub></td></tr>
<tr><td width="50%" valign="top"><img src="images/screenshots/readings.png" alt="Messwerte: was der Knoten gerade liest, mit dem Alter jedes Wertes." width="100%"><br><sub>Messwerte: was der Knoten gerade liest, mit dem Alter jedes Wertes.</sub></td><td width="50%" valign="top"><img src="images/screenshots/network.png" alt="Netzwerk: Ethernet- oder WLAN-Adressierung des Knotens, im eigenen Web-Panel eingestellt." width="100%"><br><sub>Netzwerk: Ethernet- oder WLAN-Adressierung des Knotens, im eigenen Web-Panel eingestellt.</sub></td></tr>
</table>

## 🔒 Sicherheit und Schutz

| Bedrohung | Abwehr |
|---|---|
| Passwörter im Klartext oder einkompiliert | PBKDF2-HMAC-SHA256 mit 10 000 Runden; kein Passwort in der Firmware (ein Knoten ohne Benutzer geht in die Einrichtung); höchstens 4 Benutzer (Admin oder Betrachter) und 8 bis 64 Zeichen; höchstens 6 Sitzungen, 30 Minuten Leerlauf |
| Einen frisch geflashten Knoten übernehmen | Die Einrichtung braucht einen Startcode: den des Builds (`CONFIG_ARMOR_SETUP_CODE`) oder einen zufälligen im Flash, der auf der USB-Konsole erscheint; mit Flottengeheimnis ist er `HMAC-SHA256(Geheimnis, MAC)`; er ist auch der Schlüssel des Einrichtungs-WLANs (WPA2, nie überbrückt) |
| Das Panel aus dem Internet erreicht | Es antwortet standardmäßig den privaten Netzen eines Hauses (10/8, 172.16/12, 192.168/16, 169.254/16, 127/8 und 100.64/10, wo manche VPNs liegen); die Einstellung `web.admin_sources` listet weitere IPv4-Adressen und Netze (8 Einträge, 120 Zeichen); alles außerhalb wird abgewiesen außer der öffentlichen Identität `GET /api/v1/session`; eine Liste, die den Speichernden aussperren würde, wird abgelehnt |
| Das Panel im Klartext über HTTP | HTTPS auf 443 mit einem vom Knoten selbst erzeugten Zertifikat (selbstsigniert, im Flash, SHA-256-Fingerabdruck zum Vergleichen sichtbar); das Panel kann http, https oder beides sein; das Sitzungscookie ist immer HttpOnly und SameSite=Strict (und Secure über TLS) |
| Eine gefälschte Firmware von GitHub | Ein Image wird nur mit passendem SHA-256 und gültiger Ed25519-Signatur mit dem Schlüssel des Maintainers installiert (`no_checksum`, `no_signature`, `bad_signature`, `checksum_mismatch`, `wrong_firmware`, `not_newer`, `version_mismatch` lehnen es ab); ein manueller Upload durch einen Administrator wird nicht geprüft: eine ausdrückliche Grenze |
| Kapern der Bluetooth-Einrichtung | Eine 6-stellige PIN, abgeleitet vom Einrichtungscode, auf der USB-Konsole und für einen Administrator sichtbar; das Telefon muss sie vor dem Schreiben von Einstellungen eingeben; das Einrichtungsfenster dauert `CONFIG_ARMOR_BLE_SETUP_MINUTES` (standardmäßig 10, 1 bis 120) |
| Ein ferner Befehl, der Schaden anrichtet | Zwei Schritte mit einem Einmal-Token, das an diese Operation gebunden ist; nur der Server darf diese Topics im Broker schreiben; eine Pause dauert höchstens eine Stunde; es gibt weder Werksreset noch Löschen noch Tiefschlaf per Fernbefehl |
| Gespeicherte Geheimnisse, gelesen von jemandem mit der Platine | Optional (standardmäßig aus, Schalter `security.seal_secrets` oder `nodes.nvs_encryption` des Servers): ChaCha20-Poly1305 für Passwörter, Token, Schlüssel, PINs und Hashes. Es verschlüsselt nicht den Flash und brennt kein eFuse: Der Schlüssel liegt in einer Partition desselben Chips und schützt daher nicht vor dem, der die Platine hat |
| Die Broker-Verbindung im Klartext | Optionales TLS (`mqtts://`) mit der eigenen Zertifizierungsstelle des Besitzers (PEM bis 2 KB) und Namensprüfung; standardmäßig aus. Der Handshake lief nur in Host-Tests, nicht auf einer Platine |
| Eine Batterie, die versehentlich oder von einem Angreifer beschrieben wird | Drei Riegel: die Knoteneinstellung `battery.write_enabled` (Administratoren) mit dem Servicepasswort der Batterie, `ARMOR_BATTERY_WRITE=1` des Servers und die Zugriffsliste des Brokers für `armor/solar/<Knoten>/maintenance/command` und `.../result` |
| Ein wiederholter oder gestohlener Schreibbefehl | Zwei Schritte (Scharfmachen, dann Ausführen mit einem Einmal-Token), ein Scharfmachfenster von 120 s, 30 s Abkühlzeit zwischen Schreibzugriffen auf einen Port, 3 Fehler in 10 Minuten sperren ihn für 5; die Seriennummer der Batterie wird erneut eingetippt, die Batterie muss im Leerlauf und ohne kritischen Alarm sein, und ein Firmware-Image wird 30 Minuten behalten und geprüft |
| Schreiben auf einen Wechselrichter oder ein ANT-BMS | Die Schreibbefehle eines Wechselrichters (eine Einstellung ändert, wie das Haus versorgt wird) und eines ANT-BMS (es kann eine Batterie unter Last trennen) lassen sich nicht einmal bauen |
| Absurde Zahlen als echt angezeigt | Ein unmöglicher Wert wird an der Grenze des Vertrags gehalten und in `clamped` benannt, damit der Server ihn als nicht echt zeigt; ein REVO-Wechselrichter hat keine Warnungsabfrage, daher sagt seine Nachricht `warnings_known: false`, statt keine Warnungen zu behaupten |

Es wurde noch kein Frame von einem echten Wechselrichter oder einer echten Batterie gelesen; die Formate sind aus öffentlichen Dokumenten und aus dem Gedächtnis geschrieben, und die ANT-BMS-Frames der Tests wurden von anderen aufgezeichnet. Lies [docs/BATTERY_MAINTENANCE.md](docs/BATTERY_MAINTENANCE.md) und [SECURITY.md](SECURITY.md).

## 🔌 Schnittstellen

| Teil | Was sie festlegt |
|---|---|
| MQTT | `armor/solar/<Knoten>/<Gerät>/state` (eine Nachricht je Wechselrichter oder Batteriestapel, Art `inverter` oder `battery` des ARMOR-COMMON-Vertrags), `armor/solar/<Knoten>/maintenance/{command,result}`, `armor/health/<Knoten>` (Last Will), `armor/control/<id>/{command,result,maintenance}` und `armor/control/all/command` |
| Ports | 10 serielle Ports (3 Hardware-UARTs und 7 emulierte, emuliert bis 19200 Baud); je Port ein Typ: `voltronic` (2400 Baud, alle 5 s), `pylontech` (115200, alle 10 s), `ant` (19200, alle 5 s) oder `raw` (9600, nur hören, veröffentlicht nichts); Wechselrichter-Dialekte `auto`, `pi30`, `revo`, `pi18`; das Mux-Profil hat 3 Portgruppen |
| Abfragen | Voltronic: `QPIGS`, `QMOD`, `QPIWS`, `QPIRI` (und `QPIGS2`, `QPGS` bei einem Standard-Wechselrichter mit dem Mux); Pylontech-Konsole: `pwr`, `bat <n>`, `info <n>`, `stat`, `soh`, Ereignisprotokoll, bis zu 8 Module zellenweise abgefragt |
| Panel | Die Familien-Routen (`tests/check_routes.py` prüft, dass jede vom Panel aufgerufene Route existiert) plus die Seiten für Ports und Messwerte; 51 Fehlercodes mit Text in acht Sprachen (772 Zeilen in `panel/text.js`); die Menüs von Studio stehen in [docs/STUDIO_MENUS.md](docs/STUDIO_MENUS.md) |

## ⚙️ Konfiguration

| Variable | Standard | Was sie festlegt |
|---|---|---|
| `enabled`, `kind`, `name`, `baud`, `rx`, `tx`, `de`, `poll_s`, `modules`, `dialect` | - | Die Einstellungen jedes Ports: ein oder aus, der Typ, der Gerätename im Topic, die Geschwindigkeit (0 = die des Typs), die Pins (und der DE/RE-Pin eines RS485), die Abfrageperiode (0 = die des Typs), die Modulzahl und der Wechselrichter-Dialekt; höchstens 3 Ersatznetze und 2 Ersatz-Broker |
| `battery.write_enabled` | off | Der Riegel des Knotens für das Schreiben auf eine Pylontech-Batterie (Administratoren, mit dem Servicepasswort der Batterie, das den Knoten nie verlässt) |
| `CONFIG_ARMOR_BOARD_S3_WIFI` / `_S3_ETH` | wifi | Die Platine: ESP32-S3-WROOM-1 N16R8 per WLAN (Standard) oder die Waveshare ESP32-S3-ETH |
| `CONFIG_ARMOR_NODE_ID`, `_SETUP_CODE`, `_FLEET_SECRET`, `_MQTT_*`, `_HEARTBEAT_S`, `_BLE_SETUP_MINUTES`, `_BOOT_FACTORY_RESET` | - | Die Einstellungen, die alle Knoten teilen: der Name (leer ergibt `armor-` und die letzten Bytes der MAC), der Einrichtungscode (leer: zufällig), das Flottengeheimnis (16+ Zeichen, nie im Repository), die ersten Broker-Einstellungen, der Herzschlag (2 bis 300 s), die Minuten der Bluetooth-Einrichtung (1 bis 120) und der Reset mit BOOT (8 s in den ersten 30 s) |
| `system.login_limit_per_minute` | 30 (0-120) | Falsche Passwörter pro Minute, bevor das Panel die Anmeldung für alle sperrt (0 = kein globales Limit); in Studio (eine Schaltfläche an jedem Knoten) oder mit `POST /api/v1/admin/nodes/settings` lesen und ändern, wobei die Panel-Anmeldung einmal benutzt und nie gespeichert wird |

## 🚀 Schnellstart

**Voraussetzungen:** ein C++17-Compiler für die Host-Tests; der ESP-IDF-5.5.5-Container zum Bauen des Images; ein ESP32-S3-WROOM-1 N16R8 oder ein Waveshare ESP32-S3-ETH.

```bash
cmake -S tests -B build/host && cmake --build build/host && ctest --test-dir build/host   # host tests, -Werror
build/host/emit_poller_samples | python tests/check_samples.py   # what the ports make is accepted by ARMOR-COMMON
tools/build_node.sh generic                       # the image for the N16R8 board in the ESP-IDF container: dist/generic-s3-wifi.bin
tools/build_node.sh generic s3-eth               # the same firmware for the Waveshare ESP32-S3-ETH (Ethernet): dist/generic-s3-eth.bin
node tools/panel_mock.mjs --user admin:adminpass123   # the panel without a board
```

Siehe die [Firmware-Anleitung](docs/NODE_FIRMWARE.md) (die Platine, die Ports, der erste Start und die Prüfliste am Prüfstand) und die [Verdrahtungshinweise](docs/NODE_HARDWARE.md).

## 🧪 Tests

```bash
cmake -S tests -B build/host && cmake --build build/host && ctest --test-dir build/host
python -I tools/armor_ci_validate.py
```

18 Host-Testprogramme und 6 Python-Prüfungen laufen unter `ctest`; 11824 Prüfungen, gezählt von den Host-Programmen, mit `-Werror` gebaut.

Python-Prüfungen: die Panel-Texte in acht Sprachen, die Tasks, die Speicherpfade, die Bluetooth-Sitzung unter ihrer Sperre und die vom Panel aufgerufenen Routen (und in manchen Knoten die HTTP-Codes oder die Glyphen der Schriften)

Die Host-Tests brauchen einen beliebigen C++17-Compiler (Linux, WSL, MSYS2); das Web-Panel wird außerdem in einem echten Browser gegen einen Ersatzknoten geprüft (`tools/panel_mock.mjs`, `tools/panel_browser_test.mjs`).

`test_clock` gibt nur `ok` aus und hat keinen Zähler; jedes andere Programm meldet seine Prüfungen. Die echten Frames der Tests liegen in `tests/pylontech_real.hpp`, `pylontech_us2000_real.hpp`, `pylontech_logs_real.hpp`, `ant_frames.hpp` und `battery_bench.hpp`.

## 🏗️ Build und Veröffentlichung

`tools/build_node.sh` baut das Image im ESP-IDF-5.5.5-Container, und die Version trägt das Image, sein SHA-256 und seine Ed25519-Signatur, je eines pro Platine; das Image wird beim ersten Mal per USB-C geschrieben und danach vom Panel aktualisiert.

## 🩺 Fehlerbehebung

| Sie sehen | Weil | Tun Sie |
|---|---|---|
| Du hast das Passwort verloren oder der Knoten tritt dem Netz nicht mehr bei | Ein Knoten in der Einrichtung ohne Adresse nach 90 s öffnet ein eigenes WLAN `ARMOR-SETUP-xxxxxx` (öffne `http://192.168.4.1/`) | Zum Neustart BOOT 8 Sekunden in den ersten 30 Sekunden nach dem Einschalten halten (löscht Einstellungen und Benutzer) oder im Panel *Werksreset* nutzen |
| Ein Update von GitHub wird abgelehnt | Die Version enthält keine Prüfsumme oder Signatur, sie passen nicht (`e_no_checksum`, `e_no_signature`, `e_checksum_mismatch`, `e_bad_signature`), hat kein Image für diese Platine (`e_no_asset_for_board`) oder gehört zu einem anderen Projekt (`e_wrong_firmware`) | Der Knoten blieb unverändert; erneut versuchen oder einen Administrator die Datei von Hand hochladen lassen |
| Ein Schreibzugriff auf eine Batterie wird mit einem Code abgelehnt | `serial_mismatch`, `not_pylontech`, `no_battery`, `unknown_model`, `not_idle`, `critical_alarm`, `busy`, `cooldown`, `locked`, `not_armed`, `bad_token`, `expired`, `disabled`, `image_missing`, `image_mismatch`...: Der Knoten lehnt ab, was nicht genau die scharf gemachte Operation ist | Lies den Code im Ergebnis, behebe die Ursache und mache die Operation erneut scharf (siehe [docs/BATTERY_MAINTENANCE.md](docs/BATTERY_MAINTENANCE.md)) |
| Ein Port veröffentlicht nichts | Der Port ist aus, sein Typ ist `raw` (nur hören), Geschwindigkeit oder Pins passen nicht zum Gerät, oder das Gerät antwortet nicht | Prüfe die Port-Einstellungen und die Verdrahtung in [docs/NODE_HARDWARE.md](docs/NODE_HARDWARE.md); das Panel zeigt den Rohdump eines Ports, solange es ihn ansieht |

## 📂 Struktur des Repositorys

<p align="center">
  <img src="images/repository-structure.svg" alt="Repository-Struktur von ARMOR-NODE-SOLAR" width="100%">
</p>

<details>
<summary>Textversion des Baums</summary>

```text
ARMOR-NODE-SOLAR/
├── main/                                die Firmware: eine ESP-IDF-Komponente
├── core/                                die Logik ohne ESP-IDF, am Rechner getestet
├── panel/                               das in die Firmware eingebettete Web-Panel
├── tests/                               Host-Tests und Python-Prüfungen
├── docs/                                10 Anleitungen: Protokolle, Nachrichten, Wartung, Hardware
├── tools/                               Werkzeuge für Build, Packen und Prüfung
├── .github/                             CI-Workflow sowie Vorlagen für Issues und Pull Requests
├── images/                              Banner, Symbol und die zwei Bilder dieses READMEs
├── build.sh build.bat                   Build über den gemeinsamen Projekt-Workflow
├── test.sh test.bat                     Tests über den gemeinsamen Workflow ausführen
├── run.sh run.bat                       Start über den gemeinsamen Projekt-Workflow
├── armor.project.json                   Manifest: Version, Rolle, Reifegrad, Build-Befehl
├── CHANGELOG.md                         was sich in jeder Version geändert hat
├── SECURITY.md SUPPORT.md LICENSE       Sicherheitsrichtlinie, Support und Lizenz
├── CONTRIBUTING.md CODE_OF_CONDUCT.md   Mitwirkung und Verhaltenskodex
├── CMakeLists.txt                       das ESP-IDF-Projekt
├── partitions.csv                       die Flash-Tabelle
├── sdkconfig.defaults                   die Firmware-Einstellungen
├── sdkconfig.board.s3-eth               die Einstellungen einer Platine
├── sdkconfig.board.s3-wifi              die Einstellungen einer Platine
├── secrets/                             Beispiel der Knotendatei (die echten sind nicht versioniert)
├── build-test.sh build-test.bat         Tests über den gemeinsamen Workflow ausführen
└── .gitattributes .gitignore            Git-Einstellungen
```

</details>

## 🗺️ Roadmap und bekannte Grenzen

**Bekannte Grenzen heute**

* Die Telegramme, Prüfsummen und Messwerte von Voltronic, Pylontech und ANT-BMS werden gegen die dokumentierten Telegramme und aufgezeichnete Beispiele geprüft; keines hat bisher mit einem echten Wechselrichter oder einer echten Batterie gesprochen.
* Das Web-Panel wurde in einem Browser gegen eine Nachbildung des Knotens erprobt, nicht auf einer Platine.
* Keine Platine gebaut und kein Wechselrichter oder keine Batterie angeschlossen: Bisher lief nichts auf echten Geräten.
* Der Einrichtungscode, die Anmeldung mit Begrenzung falscher Passwörter, die Bluetooth-PIN und die signierten Aktualisierungen sind auf dem Host getestet; es gab keine unabhängige Prüfung.
* Die Hash- und Ed25519-Signaturprüfungen und die Rückfalllogik bestehen auf dem Host; eine Aktualisierung über die Luft wurde nie auf einer Platine ausgeführt.
* Das Schreiben auf Pylontech-Batterien ist standardmäßig aus und wurde nur gegen einen Simulator getestet.

**Als Nächstes**

* Den Knoten auf einer echten Platine mit echtem Wechselrichter und Batterien betreiben, danach die Bluetooth-Verbindung des ANT-BMS (heute wird das Kabel genutzt), das Update über die Luft und die Schreibzugriffe auf eine Batterie.

## 🌍 Sprachen

README und Texte gibt es auf Englisch, Spanisch, Französisch, Italienisch, Deutsch, Chinesisch, Japanisch und europäischem Portugiesisch.

## 🔗 Verwandte Projekte

**A.R.M.O.R.** (Autonomous Radar & Multimodal Observation Range) ist ein Perimeter-Sicherheitssystem aus unabhängigen Repositorys. Jedes hat eine eigene Version, eigene Tests und ein eigenes README; hier ist die Familie:

* **[ARMOR-COMMON](https://github.com/JuanenRac/ARMOR-COMMON)** - Nachrichtenverträge, Validierer, Konformitätsvektoren und generierte Typen
* **[ARMOR-NODE-RADAR](https://github.com/JuanenRac/ARMOR-NODE-RADAR)** - Feldknoten-Firmware für ESP32-S3 mit drei Radaren und eigenem Web-Panel
* **ARMOR-NODE-SOLAR** (dieses Repository) - Protokolle für Solar-Wechselrichter und -Batterien und die Nachrichten eines Gateway-Knotens
* **[ARMOR-NODE-ELECTRICAL](https://github.com/JuanenRac/ARMOR-NODE-ELECTRICAL)** - Elektroknoten: Zähler, die Nachricht der Netzmesswerte und die Regeln fürs Schalten
* **[ARMOR-NODE-ALARM](https://github.com/JuanenRac/ARMOR-NODE-ALARM)** - Alarmknoten und Alarmzentrale: Zonen, Scharfschalten, Verzögerungen, Sirene und PIN, mit dem Server oder ohne ihn
* **[ARMOR-NODE-HMI](https://github.com/JuanenRac/ARMOR-NODE-HMI)** - Touch-Panel: der Systemzustand auf einem Wandbildschirm, Scharf- und Quittieren sowie das Zuhause des Sprachassistenten
* **[ARMOR-NETWORK](https://github.com/JuanenRac/ARMOR-NETWORK)** - Das lokale Netzwerk: seine Geräte, das Internet und was sich ändert
* **[ARMOR-SERVER](https://github.com/JuanenRac/ARMOR-SERVER)** - Zentraler Koordinator: Telemetrie, Alarme, Geräte, Solarmesswerte und Kameras
* **[ARMOR-STUDIO](https://github.com/JuanenRac/ARMOR-STUDIO)** - Web-Konsole: Kameras, Radar, Alarme, Solarenergie und 2D/3D-Standortdesigner
* **[ARMOR-ANDROID-CONTROL](https://github.com/JuanenRac/ARMOR-ANDROID-CONTROL)** - Android-Bedienclient mit Live-Radar in 2D/3D
* **[ARMOR-IOS-CONTROL](https://github.com/JuanenRac/ARMOR-IOS-CONTROL)** - iPhone-Bedienclient
* **[ARMOR-SERVER-AI](https://github.com/JuanenRac/ARMOR-SERVER-AI)** - Visuelle Inferenzrichtlinie, die ihre Entscheidungen erklärt und nie handelt
* **[ARMOR-VISION](https://github.com/JuanenRac/ARMOR-VISION)** - Kamerazonen, Masken und Erkennungsregeln für den Jetson Orin NX
* **[ARMOR-VOICE-AI](https://github.com/JuanenRac/ARMOR-VOICE-AI)** - Offline-Sprachabsichten mit einer nicht fälschbaren Bestätigung
* **[ARMOR-HARDWARE](https://github.com/JuanenRac/ARMOR-HARDWARE)** - Gehäuse, Elektronik und die Abnahmematrix am Prüfstand
* **[ARMOR-DEVOPS](https://github.com/JuanenRac/ARMOR-DEVOPS)** - Bereitstellung, CM5-Prüfstand, Backup und TLS
* **[ARMOR-SIMULATOR](https://github.com/JuanenRac/ARMOR-SIMULATOR)** - Offline-Telemetriesimulator mit wiederholbaren Fehlern
* **[ARMOR-UPDATER](https://github.com/JuanenRac/ARMOR-UPDATER)** - Erkennt, installiert und aktualisiert die eigenen Repositorys des Ökosystems
* **[ARMOR-DOCS](https://github.com/JuanenRac/ARMOR-DOCS)** - Architektur, Sicherheitsgrundlage und die Fähigkeitsmatrix

## 📚 Dokumentation

* [Knoten-Firmware](docs/NODE_FIRMWARE.md): die Platine, die Ports, der erste Start und die Prüfstand-Checkliste
* [Verdrahtung](docs/NODE_HARDWARE.md), [Protokolle](docs/PROTOCOLS.md) und [Nachrichten](docs/SOLAR_MESSAGES.md)
* [Batteriewartung](docs/BATTERY_MAINTENANCE.md) und ihr [Prüfstandsplan](docs/BATTERY_BENCH_TEST_PLAN.md)
* [Studio-Menüs](docs/STUDIO_MENUS.md), [Prüfstandtests](docs/BENCH_TESTS.md), [Einrichtungscode](docs/SETUP_CODE.md) und [Bluetooth-Einrichtung](docs/BLE_PROVISIONING.md)
* [Fähigkeitsmatrix: was belegt ist und was nicht](https://github.com/JuanenRac/ARMOR-DOCS/blob/main/docs/CAPABILITY_MATRIX.md)
* [Projektkatalog: Versionen und wie die Repositorys voneinander abhängen](https://github.com/JuanenRac/ARMOR-DOCS/blob/main/docs/PROJECT_CATALOG.md)
* [Änderungsverlauf dieses Repositorys](CHANGELOG.md)

## 🤝 Mitwirken und Community

* Führe vor einem Pull Request `ctest --test-dir build/host` and `python -I tools/armor_ci_validate.py` aus: Es muss bestehen. Siehe [CONTRIBUTING.md](CONTRIBUTING.md) und den [Verhaltenskodex](CODE_OF_CONDUCT.md).
* Das README gibt es in acht Sprachen, und es wird aus den Quellen in ARMOR-DOCS erzeugt: Eine Textänderung erfolgt dort.
* Eine Änderung an einer Nachricht zwischen Komponenten beginnt in ARMOR-COMMON.
* Beschreibe nichts als auf einer Platine funktionierend, was nur gebaut wurde.
* Melde eine Schwachstelle privat: [SECURITY.md](SECURITY.md). Fragen, Ideen und Meldungen: electrohobby3d@gmail.com ([SUPPORT.md](SUPPORT.md)).

## 👤 AUTOR

**JuanenRac (Electro Hobby 3D)** · electrohobby3d@gmail.com

## 📜 LIZENZ

GPL-3.0-or-later - siehe [LICENSE](LICENSE).
