<p align="center">
  <img src="images/ARMOR_BANNER.svg" alt="ARMOR-SOLAR banner" width="100%">
</p>

# ☀️ ARMOR-SOLAR

<p align="center">
  <a href="README.md">🇺🇸 English</a> |
  🇪🇸 <b>Español</b> |
  <a href="README_fra.md">🇫🇷 Français</a> |
  <a href="README_ita.md">🇮🇹 Italiano</a> |
  <a href="README_deu.md">🇩🇪 Deutsch</a> |
  <a href="README_zho.md">🇨🇳 简体中文</a> |
  <a href="README_jpn.md">🇯🇵 日本語</a> |
  <a href="README_por.md">🇵🇹 Português</a>
</p>

### Nodo pasarela solar: lee inversores y baterías por hasta diez puertos serie (firmware para ESP32-S3, su panel web y la biblioteca de protocolos)

<p align="center">
  <img src="https://img.shields.io/badge/License-GPL%203.0-blue.svg" alt="GPL 3.0">
  <img src="https://img.shields.io/badge/Language-C%2B%2B17-00599c.svg" alt="Language">
  <img src="https://img.shields.io/badge/Devices-Voltronic%20%C2%B7%20Pylontech-ffb020.svg" alt="Devices">
  <img src="https://img.shields.io/badge/Version-0.5.4-0f6cbd.svg" alt="Version">
  <img src="https://img.shields.io/badge/Maturity-functional-00E5FF.svg" alt="Maturity">
</p>

---

**Comprobación de honestidad - qué funciona hoy:** El firmware compila en el contenedor ESP-IDF 5.5.5, su núcleo (los ajustes, el intercambio con cada tipo de equipo, la aritmética de la UART emulada, el canal Bluetooth, las comprobaciones de versión y firma: 3.013 comprobaciones en 10 programas de test) se prueba en un ordenador con equipos de pega, los mensajes que hace los acepta ARMOR-COMMON y el panel se ejercitó en un navegador contra un nodo de pega. **Nunca ha corrido en una placa y no se ha conectado ningún inversor ni batería**: el Wi-Fi, el panel sobre TLS, la actualización, las UART de hardware y emuladas y los formatos de los protocolos (escritos a partir de documentos públicos y de memoria) están sin probar. Las tramas ANT-BMS de sus tests las capturaron otras personas en sus propias unidades: este proyecto no ha leído ninguna por sí mismo.

---

## 📋 De un vistazo

| | |
|---|---|
| Versión | `0.5.4` |
| Madurez | Funcional |
| Tests | 9.319 comprobaciones en 14 programas (`ctest`) |
| Tecnología | C++17 (host-tested core); ESP-IDF 5.5.5 firmware for the ESP32-S3-WROOM-1 N16R8 (Wi-Fi) and the Waveshare ESP32-S3-ETH (Ethernet); JavaScript web panel |
| En hardware real | Ninguno todavía: ninguna placa montada ni inversor o batería conectado (solo tests en ordenador, la compilación y el panel en un navegador) |

## 🎯 Descripción general

* **Dos placas, un firmware:** una ESP32-S3-WROOM-1 N16R8 por Wi-Fi (la de serie) y la Waveshare ESP32-S3-ETH por su cable Ethernet (DHCP o una dirección fija; conserva su propia red Wi-Fi para llegar a ella desde un móvil). La imagen se elige al compilar (`tools/build_node.sh generic s3-wifi` o `generic s3-eth`); la tabla de pines, los pines por defecto de los puertos y la forma de acceso siguen a la placa, y una imagen es solo para su placa. Ambas compilan y tienen tests en el ordenador; ninguna ha funcionado en una placa.
* **El firmware del nodo** (ESP32-S3-WROOM-1 N16R8 por Wi-Fi, o Waveshare ESP32-S3-ETH por cable): diez puertos serie, tres UART de hardware y siete emuladas (hasta 19200 baudios), cada una independiente y cada una lee un inversor, una batería o un monitor en crudo, de modo que un nodo puede leer solo inversores, solo baterías o una mezcla.
* **El panel web del nodo,** el mismo de los nodos radar: configuración con un código que se muestra en la consola USB, acceso y usuarios, Wi-Fi (estación y punto de acceso), broker, actualización por aire con vuelta atrás, registro, HTTPS, y las páginas de los puertos (con lo que oye cada uno, para protocolos aún sin decodificar) y de las lecturas, en ocho idiomas.
* **Inversores Voltronic / MPP Solar** (Axpert, PIP, InfiniSolar y clones), RS232 a 2400 baudios: las tramas y su CRC, y las lecturas `QPIGS` (red, salida, batería, FV, bits de estado), `QMOD` (modo), `QPIWS` (avisos y averías por nombre) y `QPIRI` (valores nominales). Solo se pueden construir comandos de lectura: un ajuste cambia cómo se alimenta la casa. Tres dialectos, elegidos en el puerto o encontrados por él (*auto*): PI30, REVO (otro `QPIGS`, respuestas con suma de comprobación) y PI18 (`^P005GS`: InfiniSolar V, LV5048, SunGoldPower), solo lectura.
* **Baterías Pylontech** (US2000, US3000, US5000): la tabla `pwr` de la consola (tensión, corriente, temperaturas y estado de carga de cada módulo), `bat <n>` (tensión y temperatura de cada celda) e `info <n>` (modelo, capacidad restante y total, ciclos), resumidas como una pila con su capacidad y energía totales, y la trama RS485 con sus dos comprobaciones. Los formatos están escritos de memoria de la consola pública y pueden variar entre firmwares. Las columnas se encuentran por los nombres de la cabecera (incluido el formato de la US5000 V2.3 con sus columnas Id, MosTempr y SysAlarm.St), la carga restante y el equilibrado salen de `bat`, y el modelo, la capacidad nominal y los ciclos de `info` y `stat`, que se piden una vez y se guardan media hora.
* **Lo que dice de sí mismo un módulo Pylontech:** su modelo y las versiones de su placa y su software, sus celdas y la corriente que admite, los contadores de su vida (`stat`), la salud de cada celda (`soh`, marcando la anormal) y la entrada más reciente de su registro de eventos, con los nombres de las protecciones tal como los imprime la consola. La consola solo lee, y un administrador puede descargar como archivo de texto lo que oyó un puerto.
* **Los mensajes del nodo** (`armor/solar/<nodo>/<dispositivo>/state`, uno por inversor o pila de baterías), definidos en ARMOR-COMMON con esquemas y vectores de conformidad; lo que producen los puertos se comprueba contra ellos.
* **Baterías con ANT-BMS** (las placas negras de los packs caseros, de 7S a 32S), UART de 3,3 V a 19200 baudios: los dos protocolos de su firmware (el nodo pregunta en uno y luego en el otro y se queda con el que responde), con las celdas, temperaturas, estado de carga, corriente, capacidades y los estados de los MOSFET y del equilibrador, comprobados con tramas capturadas en cuatro modelos reales. Solo lectura: sus comandos de escritura pueden desconectar una batería en carga. Un botón de la página de puertos lee además el modelo, la versión y los 56 ajustes de protección y equilibrado de la BMS (protocolo nuevo), solo lectura.
* **Configuración desde el móvil por Bluetooth,** el mismo canal que el del nodo radar: la app ARMOR encuentra el nodo como `ARMOR-XXXXXX` y ajusta su nombre, Wi-Fi, dirección, broker y modo de Bluetooth con los usuarios y el código de puesta en marcha del panel ([el protocolo](docs/BLE_PROVISIONING.md)). Solo escucha mientras el nodo no tiene usuarios, salvo que se indique otra cosa. La parte de radio nunca ha corrido en una placa.
* **La placa base con multiplexores (perfil *mux*):** tres UART de hardware, cada una detrás de un 74HC4052, atienden hasta ocho puertos en grupos de 4, 2 y 2 que se turnan en su línea, cada puerto con su propia velocidad y polaridad, y un LED por puerto a través de un 74HC595; las celdas de una pila Pylontech alta pueden leerse por turnos. Para un inversor en el dialecto estándar, la **segunda entrada fotovoltaica** (`QPIGS2`) y las **unidades de un sistema en paralelo** (`QPGS`) son lecturas opcionales. Nada de esto ha corrido en una placa.
* **Actualizaciones por aire firmadas:** el nodo solo instala desde GitHub una imagen cuyo SHA-256 coincide y que lleva una firma Ed25519 válida del responsable (la clave pública está en el firmware, la privada nunca sale del responsable); un archivo subido a mano por un administrador en el panel se sigue aceptando. Las páginas y mensajes del panel están en ocho idiomas.
* **Emparejamiento Bluetooth con PIN:** el PIN de seis cifras se deriva del código de configuración, se imprime en la consola USB y se muestra a un administrador en el panel; el móvil tiene que escribirlo antes de poder cambiar los ajustes del nodo.
* **Todavía no:** el enlace Bluetooth del ANT-BMS (el nodo usa el cable, el más estable de los dos) y una prueba en una placa real con equipos reales.
* **Solo la casa puede usar el panel:** el panel responde por defecto a la red privada (o a las redes que su dueño indique, como una VPN), rechaza una lista que dejaría fuera a quien la guarda, cuenta las contraseñas erróneas por persona y por dirección, y funciona detrás del proxy del servidor para llegar a él desde fuera de casa sin publicar el nodo.**Desconocido no es ninguno:** un inversor REVO no tiene consulta de avisos, así que su mensaje lleva `warnings_known: false` y Studio muestra los avisos como desconocidos en vez de "ninguno".
* **Enlace cifrado opcional con el broker (TLS):** desactivado por defecto, así que el nodo habla MQTT sin cifrar como antes. Un interruptor y una caja para el certificado de la autoridad propia de la instalación (PEM, 2 KB como máximo) en la página Broker hacen que se conecte con `mqtts://`, confíe solo en esa autoridad y compruebe que el certificado del broker se hizo para la dirección a la que llegó; lo que no sea un único certificado PEM se rechaza. Cubierto con pruebas en el ordenador; el handshake aún no se ha ejecutado en una placa.
* **Informe de salud:** cada minuto más o menos el nodo publica cuánto lleva encendido y por qué se reinició la última vez, su memoria libre, su enlace y su señal Wi-Fi, cuántas veces se conectó al broker y el último error que anotó (`armor/health/<nodo>`, el contrato `node_health` de ARMOR-COMMON). El código que lo construye lo comparten todos los nodos y está probado en un ordenador; no se ha ejecutado en una placa.
* **Escritura en baterías Pylontech (desactivada por defecto):** solo operaciones tipadas - reloj, contadores y registros, reinicio, capacidad, el reinicio del nivel de carga, límites y protecciones, salidas, número de serie, memoria externa, firmware por XMODEM-1K y una consola de experto con lista de bloqueo - en dos pasos (preparar y ejecutar con un token de un solo uso), leyendo la batería antes y después, con la contraseña de servicio guardada solo en el nodo y una consola simulada para las pruebas. Sin probar en una batería real: mira el [plan de pruebas de banco](docs/BATTERY_BENCH_TEST_PLAN.md).
* **Control desde el servidor:** Studio puede reiniciar el nodo limpiamente, pausarlo hasta una hora (se reanuda solo), poner su trabajo en espera (sin fin, se conserva al reiniciar, el nodo sigue conectado y respondiendo), hacer que se muestre, pedirle una instantánea de diagnóstico y marcarlo en mantenimiento (`armor/control/<nodo>/...`, el contrato `node_control` de ARMOR-COMMON). Cada orden son dos pasos con un token de un solo uso que pertenece a esa operación, solo el servidor puede escribirlas en el broker, y no existe restablecimiento de fábrica, borrado ni sueño profundo. Mientras el nodo está en pausa o en espera no escribe ninguna batería (una operación en curso aún se puede cancelar) y sigue leyendo inversores y baterías. El código es compartido por todos los nodos y se prueba en un ordenador; no ha funcionado en una placa.
* **Los valores absurdos se nombran, no se esconden:** cuando un inversor envía un número imposible, el nodo lo mantiene en el límite del contrato y lo nombra en `clamped`, así que el servidor lo muestra como no real; y el volcado en bruto de un puerto solo se genera mientras el panel lo está mirando.
* **Diagnóstico sin cable:** tras un arranque que no fue limpio (un cuelgue, un watchdog, una caída de la alimentación, una reserva de memoria fallida) el informe de salud dice el motivo durante diez minutos; un fallo deja su volcado de núcleo (core dump) para verlo, descargarlo o borrarlo desde el panel; los ajustes guardados se copian antes de cada instalación de firmware y se recuperan si una actualización los daña; y un administrador puede pedir al nodo, a través del servidor, un paquete de diagnóstico o su registro con un token de un solo uso.

## 🖼️ Capturas de pantalla

Capturadas a 1920×1080 en un banco de pruebas, con datos de ejemplo y sin ninguna instalación real detrás.

<table>
<tr><td width="50%" valign="top"><img src="images/screenshots/overview.png" alt="Resumen: el estado del nodo de un vistazo - red, broker, firmware y lo que reporta." width="100%"><br><sub>Resumen: el estado del nodo de un vistazo - red, broker, firmware y lo que reporta.</sub></td><td width="50%" valign="top"><img src="images/screenshots/ports.png" alt="Puertos: los puertos serie del nodo y el equipo de cada uno." width="100%"><br><sub>Puertos: los puertos serie del nodo y el equipo de cada uno.</sub></td></tr>
<tr><td width="50%" valign="top"><img src="images/screenshots/readings.png" alt="Lecturas: lo que el nodo lee ahora mismo, con la antigüedad de cada valor." width="100%"><br><sub>Lecturas: lo que el nodo lee ahora mismo, con la antigüedad de cada valor.</sub></td><td width="50%" valign="top"><img src="images/screenshots/network.png" alt="Red: el direccionamiento Ethernet o Wi-Fi del nodo, ajustado desde su propio panel web." width="100%"><br><sub>Red: el direccionamiento Ethernet o Wi-Fi del nodo, ajustado desde su propio panel web.</sub></td></tr>
</table>

## 🧩 Papel en A.R.M.O.R.

ARMOR-SOLAR es la pasarela entre el equipo solar de la casa y la familia. Lee inversores y baterías por hasta diez puertos serie y publica lo que lee para [ARMOR-SERVER](https://github.com/JuanenRac/ARMOR-SERVER), que guarda el historial, los totales y las alarmas; los menús *Inversores* y *Baterías* de [ARMOR-STUDIO](https://github.com/JuanenRac/ARMOR-STUDIO) y la pantalla Solar de [ARMOR-ANDROID-CONTROL](https://github.com/JuanenRac/ARMOR-ANDROID-CONTROL) lo muestran. Solo lee: nunca escribe un ajuste en ningún equipo.

## 📂 Estructura del repositorio

```text
ARMOR-SOLAR/
├── main/    the ESP-IDF component: app_main, solar_manager (one task per port, or per group in the mux profile), uart_ports, mux_board, port_leds_hw, network, web_server, api_shared, mqtt_link, node_store, tls_cert, ble_provision, board_ethernet
├── core/    voltronic, voltronic_pi18, pylontech, ant_bms, ant_registers, ant_settings, solar_json + solar_config, poller, soft_uart, netplan, auth, board_s3, ble_frame, ble_dispatch, mux_group, port_leds, console_probe, json (no hardware in them)
├── panel/   the web panel: index.html, app.js, text.js (8 languages), style.css
├── tools/   build_node.sh, pack_panel.py, panel_mock.mjs, panel_browser_test.mjs
├── tests/   test_solar.cpp, test_node.cpp, test_mqtt_tls.cpp, test_board_eth.cpp, test_ble.cpp, test_mux.cpp, test_parallel.cpp, test_console.cpp, emit_samples.cpp, emit_poller_samples.cpp, check_samples.py (+ the ANT-BMS frames)
└── docs/    NODE_FIRMWARE, NODE_HARDWARE, BLE_PROVISIONING, PROTOCOLS, SOLAR_MESSAGES, STUDIO_MENUS
```

## 🛠️ Entorno de desarrollo

**Requisitos:** un compilador de C++17 para las pruebas en el PC, el contenedor de ESP-IDF 5.5.5 para compilar la imagen, un ESP32-S3-WROOM-1 N16R8 o un Waveshare ESP32-S3-ETH

```bash
cmake -S tests -B build/host && cmake --build build/host
build/host/test_solar && build/host/test_node && build/host/test_board_eth && build/host/test_ble && build/host/test_mux && build/host/test_parallel && build/host/test_console && build/host/test_release      # 9,319 checks, -Werror
build/host/emit_poller_samples | python tests/check_samples.py   # what the ports make is accepted by ARMOR-COMMON
tools/build_node.sh generic                       # the firmware image for the N16R8 board in the ESP-IDF container: dist/generic-s3-wifi.bin
tools/build_node.sh generic s3-eth               # the same firmware for the Waveshare ESP32-S3-ETH (Ethernet): dist/generic-s3-eth.bin
node tools/panel_mock.mjs --user admin:adminpass123   # the panel without a board
```

Véase la [guía del firmware](docs/NODE_FIRMWARE.md) (la placa, los puertos, el primer arranque y la lista de pruebas en banco) y las [notas de cableado](docs/NODE_HARDWARE.md).

## 🔗 Proyectos relacionados

**A.R.M.O.R.** (Autonomous Radar & Multimodal Observation Range) es un sistema de seguridad perimetral hecho de repositorios independientes. Cada uno tiene su propia versión, sus propias pruebas y su propio README; esta es la familia:

* **[ARMOR-COMMON](https://github.com/JuanenRac/ARMOR-COMMON)** - Contratos de mensajes, validadores, vectores de conformidad y tipos generados
* **[ARMOR-RADAR](https://github.com/JuanenRac/ARMOR-RADAR)** - Firmware del nodo de campo para ESP32-S3 con tres radares y su propio panel web
* **ARMOR-SOLAR** (este repositorio) - Protocolos de inversores y baterías solares y los mensajes de un nodo pasarela
* **[ARMOR-ELECTRICAL](https://github.com/JuanenRac/ARMOR-ELECTRICAL)** - Nodo eléctrico: contadores, el mensaje de las lecturas de la red y las reglas para maniobrar
* **[ARMOR-ALARM](https://github.com/JuanenRac/ARMOR-ALARM)** - Nodo y central de alarma: zonas, armado, retardos, sirena y PIN, con el servidor o sin él
* **[ARMOR-HMI](https://github.com/JuanenRac/ARMOR-HMI)** - Panel táctil: el estado del sistema en una pantalla de pared, armar y reconocer alarmas, y el hogar del asistente de voz
* **[ARMOR-NETWORK](https://github.com/JuanenRac/ARMOR-NETWORK)** - La red local: sus dispositivos, internet y lo que cambia
* **[ARMOR-SERVER](https://github.com/JuanenRac/ARMOR-SERVER)** - Coordinador central: telemetría, alarmas, dispositivos, lecturas solares y cámaras
* **[ARMOR-STUDIO](https://github.com/JuanenRac/ARMOR-STUDIO)** - Consola web: cámaras, radar, alarmas, energía solar y el diseñador de sitio 2D/3D
* **[ARMOR-ANDROID-CONTROL](https://github.com/JuanenRac/ARMOR-ANDROID-CONTROL)** - Cliente Android del operador con radar 2D/3D en vivo
* **[ARMOR-SERVER-AI](https://github.com/JuanenRac/ARMOR-SERVER-AI)** - Política de inferencia visual que explica sus decisiones y nunca actúa
* **[ARMOR-VISION](https://github.com/JuanenRac/ARMOR-VISION)** - Zonas de cámara, máscaras y reglas de detección para la Jetson Orin NX
* **[ARMOR-VOICE-AI](https://github.com/JuanenRac/ARMOR-VOICE-AI)** - Intenciones de voz sin conexión con una confirmación imposible de falsificar
* **[ARMOR-HARDWARE](https://github.com/JuanenRac/ARMOR-HARDWARE)** - Cajas, electrónica y la matriz de aceptación en banco
* **[ARMOR-DEVOPS](https://github.com/JuanenRac/ARMOR-DEVOPS)** - Despliegue, el banco de pruebas de la CM5, copias de seguridad y TLS
* **[ARMOR-SIMULATOR](https://github.com/JuanenRac/ARMOR-SIMULATOR)** - Simulador de telemetría sin conexión con fallos repetibles
* **[ARMOR-UPDATER](https://github.com/JuanenRac/ARMOR-UPDATER)** - Detecta, instala y actualiza los propios repositorios del ecosistema
* **[ARMOR-DOCS](https://github.com/JuanenRac/ARMOR-DOCS)** - Arquitectura, base de seguridad y la matriz de capacidades

## 📚 Documentación y comunidad

Dónde leer más:

* [Matriz de capacidades: qué está probado y qué no](https://github.com/JuanenRac/ARMOR-DOCS/blob/main/docs/CAPABILITY_MATRIX.md)
* [Catálogo de proyectos: versiones y cómo dependen unos de otros](https://github.com/JuanenRac/ARMOR-DOCS/blob/main/docs/PROJECT_CATALOG.md)
* [Historial de cambios de este repositorio](CHANGELOG.md)
* [Licencia (GPL-3.0-or-later)](LICENSE)
* Preguntas, ideas e informes: electrohobby3d@gmail.com

## 👤 AUTOR

**JuanenRac (Electro Hobby 3D)** · electrohobby3d@gmail.com

## 📜 LICENCIA

GPL-3.0-or-later - véase [LICENSE](LICENSE).
