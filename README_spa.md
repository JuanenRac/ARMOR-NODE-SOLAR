<p align="center">
  <img src="images/ARMOR_BANNER.svg" alt="Banner de ARMOR-NODE-SOLAR" width="100%">
</p>

# ☀️ ARMOR-NODE-SOLAR

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
  <img src="https://img.shields.io/badge/License-GPL%203.0--or--later-blue.svg" alt="GPL 3.0 o posterior">
  <img src="https://img.shields.io/badge/Language-C%2B%2B17-00599c.svg" alt="Lenguaje">
  <img src="https://img.shields.io/badge/Devices-Voltronic%20%C2%B7%20Pylontech-ffb020.svg" alt="Devices">
  <img src="https://img.shields.io/badge/Tests-11824%20checks-2ea44f.svg" alt="Pruebas">
  <img src="https://img.shields.io/badge/Version-0.6.1-0f6cbd.svg" alt="Versión">
  <img src="https://img.shields.io/badge/Maturity-functional-00E5FF.svg" alt="Madurez">
</p>

---

**Comprobación de honestidad - qué funciona hoy:** El firmware compila en el contenedor ESP-IDF 5.5.5, su núcleo (los ajustes, el intercambio con cada tipo de equipo, la aritmética de la UART emulada, el canal Bluetooth, las comprobaciones de versión y firma: 11.824 comprobaciones en 18 programas de test) se prueba en un ordenador con equipos de pega, los mensajes que hace los acepta ARMOR-COMMON y el panel se ejercitó en un navegador contra un nodo de pega. **Nunca ha corrido en una placa y no se ha conectado ningún inversor ni batería**: el Wi-Fi, el panel sobre TLS, la actualización, las UART de hardware y emuladas y los formatos de los protocolos (escritos a partir de documentos públicos y de memoria) están sin probar. Las tramas ANT-BMS de sus tests las capturaron otras personas en sus propias unidades: este proyecto no ha leído ninguna por sí mismo.

---

## 📋 De un vistazo

| | |
|---|---|
| Versión | `0.6.1` |
| Madurez | Funcional |
| Función | Nodo de campo solar para ESP32-S3: inversores y baterías (familia: Field Nodes, padre: ARMOR-COMMON) |
| Tecnología | C++17, ESP-IDF 5.5.5, ESP32-S3 |
| Pruebas | 11824 comprobaciones en 18 programas de host y 6 de Python (`ctest`) |
| Equipos | Inversores Voltronic / MPP Solar, baterías Pylontech, ANT-BMS; 10 puertos serie |
| Funciona en | ESP32-S3-WROOM-1 N16R8 o Waveshare ESP32-S3-ETH; pruebas de host en cualquier ordenador |
| En hardware real | Ninguno todavía: ninguna placa montada ni inversor o batería conectado (solo tests en ordenador, la compilación y el panel en un navegador) |
| Licencia | GPL-3.0-or-later |

**Ir a:** [Resumen](#-resumen) · [Arquitectura](#-arquitectura) · [Seguridad](#-seguridad-y-protección) · [Interfaces](#-interfaces) · [Configuración](#-configuración) · [Inicio rápido](#-inicio-rápido) · [Pruebas](#-pruebas) · [Problemas](#-resolución-de-problemas) · [Estructura](#-estructura-del-repositorio) · [Relacionados](#-proyectos-relacionados)

## 🚦 Estado del proyecto

Cada línea es una parte de este repositorio y la luz dice hasta dónde llega hoy. Está escrito con lo que el repositorio puede demostrar (pruebas, compilaciones, ejecuciones), no con lo que está planeado.

🟢 Hecho y verificado · 🟡 Funciona, con límites conocidos · 🟠 Parcial o sin verificar todavía · 🔴 No hecho o no funciona

| Área | Estado | Qué significa aquí |
|---|---|---|
| Software | 🟢 | El firmware compila en el contenedor ESP-IDF 5.5.5 para las dos placas (Wi-Fi y Ethernet). |
| Pruebas | 🟢 | Pasan 11.824 comprobaciones en 18 programas de host (`ctest`): los ajustes, el intercambio con cada tipo de equipo, la UART emulada, el canal Bluetooth y las comprobaciones de versión. |
| Protocolos y datos | 🟡 | Las tramas, sumas de control y lecturas de Voltronic, Pylontech y ANT-BMS se comprueban contra las tramas documentadas y muestras grabadas; ninguno ha hablado todavía con un inversor o una batería reales. |
| Interfaz | 🟡 | El panel web se ha ejercitado en un navegador contra una simulación del nodo, no en una placa. |
| Hardware | 🔴 | Ninguna placa construida ni inversor o batería conectados: nada ha funcionado todavía con equipos reales. |
| Seguridad | 🟡 | El código de configuración, el acceso con límite de contraseñas erróneas, el PIN Bluetooth y las actualizaciones firmadas están probados en el host; no ha habido una revisión independiente. |
| Actualizaciones | 🟠 | Las comprobaciones de hash y de firma Ed25519 y la lógica de vuelta atrás pasan en el host; una actualización por aire nunca se ha ejecutado en una placa. |
| Escritura en los equipos | 🟠 | La escritura en baterías Pylontech está desactivada por defecto y solo se ha probado contra un simulador. |
| Integración | 🟠 | Los mensajes del nodo se validan contra los esquemas compartidos y los vectores de conformidad de ARMOR-COMMON. Su cliente MQTT se prueba contra un Mosquitto real en los tests; ninguna placa los ha enviado aún a un servidor. |
| Documentación | 🟢 | Ocho idiomas, la guía del firmware, las notas de cableado y el historial de cambios están al día. |

## 🎯 Resumen

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
* **Solo la casa puede usar el panel:** el panel responde por defecto a la red privada (o a las redes que su dueño indique, como una VPN), rechaza una lista que dejaría fuera a quien la guarda, cuenta las contraseñas erróneas por persona y por dirección, y funciona detrás del proxy del servidor para llegar a él desde fuera de casa sin publicar el nodo.
* **Desconocido no es ninguno:** un inversor REVO no tiene consulta de avisos, así que su mensaje lleva `warnings_known: false` y Studio muestra los avisos como desconocidos en vez de "ninguno".
* **Enlace cifrado opcional con el broker (TLS):** desactivado por defecto, así que el nodo habla MQTT sin cifrar como antes. Un interruptor y una caja para el certificado de la autoridad propia de la instalación (PEM, 2 KB como máximo) en la página Broker hacen que se conecte con `mqtts://`, confíe solo en esa autoridad y compruebe que el certificado del broker se hizo para la dirección a la que llegó; lo que no sea un único certificado PEM se rechaza. Cubierto con pruebas en el ordenador; el handshake aún no se ha ejecutado en una placa.
* **Informe de salud:** cada minuto más o menos el nodo publica cuánto lleva encendido y por qué se reinició la última vez, su memoria libre, su enlace y su señal Wi-Fi, cuántas veces se conectó al broker y el último error que anotó (`armor/health/<nodo>`, el contrato `node_health` de ARMOR-COMMON). El código que lo construye lo comparten todos los nodos y está probado en un ordenador; no se ha ejecutado en una placa.
* **Escritura en baterías Pylontech (desactivada por defecto):** solo operaciones tipadas - reloj, contadores y registros, reinicio, capacidad, el reinicio del nivel de carga, límites y protecciones, salidas, número de serie, memoria externa, firmware por XMODEM-1K y una consola de experto con lista de bloqueo - en dos pasos (preparar y ejecutar con un token de un solo uso), leyendo la batería antes y después, con la contraseña de servicio guardada solo en el nodo y una consola simulada para las pruebas. Sin probar en una batería real: mira el [plan de pruebas de banco](docs/BATTERY_BENCH_TEST_PLAN.md).
* **Control desde el servidor:** Studio puede reiniciar el nodo limpiamente, pausarlo hasta una hora (se reanuda solo), poner su trabajo en espera (sin fin, se conserva al reiniciar, el nodo sigue conectado y respondiendo), hacer que se muestre, pedirle una instantánea de diagnóstico y marcarlo en mantenimiento (`armor/control/<nodo>/...`, el contrato `node_control` de ARMOR-COMMON). Cada orden son dos pasos con un token de un solo uso que pertenece a esa operación, solo el servidor puede escribirlas en el broker, y no existe restablecimiento de fábrica, borrado ni sueño profundo. Mientras el nodo está en pausa o en espera no escribe ninguna batería (una operación en curso aún se puede cancelar) y sigue leyendo inversores y baterías. El código es compartido por todos los nodos y se prueba en un ordenador; no ha funcionado en una placa.
* **Los valores absurdos se nombran, no se esconden:** cuando un inversor envía un número imposible, el nodo lo mantiene en el límite del contrato y lo nombra en `clamped`, así que el servidor lo muestra como no real; y el volcado en bruto de un puerto solo se genera mientras el panel lo está mirando.
* **Diagnóstico sin cable:** tras un arranque que no fue limpio (un cuelgue, un watchdog, una caída de la alimentación, una reserva de memoria fallida) el informe de salud dice el motivo durante diez minutos; un fallo deja su volcado de núcleo (core dump) para verlo, descargarlo o borrarlo desde el panel; los ajustes guardados se copian antes de cada instalación de firmware y se recuperan si una actualización los daña; y un administrador puede pedir al nodo, a través del servidor, un paquete de diagnóstico o su registro con un token de un solo uso.
* **Sellado opcional de los secretos guardados:** desactivado por defecto (`security.seal_secrets`, o `nodes.nvs_encryption` del servidor). Activado, las contraseñas, tokens, claves, PIN y hashes de contraseña que guarda el nodo quedan sellados con ChaCha20-Poly1305 y todo lo demás sigue legible; activarlo o desactivarlo los reescribe sin perder nada, aunque el nodo se pare a medias. No se quema ningún eFuse ni se cifra la flash: la clave está en una partición del mismo chip, así que **no** protege frente a quien tenga la placa, y un nodo instalado antes de esa partición conserva sus secretos como estaban hasta que se grabe con la nueva tabla de particiones.

## 🧩 Papel en A.R.M.O.R.

ARMOR-NODE-SOLAR es la pasarela entre el equipo solar de la casa y la familia. Lee inversores y baterías por hasta diez puertos serie y publica lo que lee para [ARMOR-SERVER](https://github.com/JuanenRac/ARMOR-SERVER), que guarda el historial, los totales y las alarmas; los menús *Inversores* y *Baterías* de [ARMOR-STUDIO](https://github.com/JuanenRac/ARMOR-STUDIO) y la pantalla Solar de [ARMOR-ANDROID-CONTROL](https://github.com/JuanenRac/ARMOR-ANDROID-CONTROL) lo muestran. Solo lee: nunca escribe un ajuste en ningún equipo.

| Nombre | Cómo | Para qué |
|---|---|---|
| ARMOR-SERVER | MQTT `armor/solar/<node>/<device>/state` | Recibe un mensaje por inversor o pila de baterías; ordena el mantenimiento de una batería |
| ARMOR-STUDIO | menús, enlace al panel | Muestra inversores y baterías y abre el panel del nodo |
| ARMOR-ANDROID-CONTROL | Bluetooth | Configura el nodo en el campo y muestra las pantallas de energía |
| ARMOR-COMMON | contrato solar | Los mensajes se comprueban con sus esquemas; el firmware común se copia de él |

## 🏛️ Arquitectura

<p align="center">
  <img src="images/architecture.svg" alt="Arquitectura de ARMOR-NODE-SOLAR: quién lo llama, qué contiene y de qué depende" width="100%">
</p>

## 🖼️ Capturas de pantalla

Capturadas a 1920×1080 en un banco de pruebas, con datos de ejemplo y sin ninguna instalación real detrás.

<table>
<tr><td width="50%" valign="top"><img src="images/screenshots/overview.png" alt="Resumen: el estado del nodo de un vistazo - red, broker, firmware y lo que reporta." width="100%"><br><sub>Resumen: el estado del nodo de un vistazo - red, broker, firmware y lo que reporta.</sub></td><td width="50%" valign="top"><img src="images/screenshots/ports.png" alt="Puertos: los puertos serie del nodo y el equipo de cada uno." width="100%"><br><sub>Puertos: los puertos serie del nodo y el equipo de cada uno.</sub></td></tr>
<tr><td width="50%" valign="top"><img src="images/screenshots/readings.png" alt="Lecturas: lo que el nodo lee ahora mismo, con la antigüedad de cada valor." width="100%"><br><sub>Lecturas: lo que el nodo lee ahora mismo, con la antigüedad de cada valor.</sub></td><td width="50%" valign="top"><img src="images/screenshots/network.png" alt="Red: el direccionamiento Ethernet o Wi-Fi del nodo, ajustado desde su propio panel web." width="100%"><br><sub>Red: el direccionamiento Ethernet o Wi-Fi del nodo, ajustado desde su propio panel web.</sub></td></tr>
</table>

## 🔒 Seguridad y protección

| Amenaza | Defensa |
|---|---|
| Contraseñas en claro o compiladas | PBKDF2-HMAC-SHA256 con 10 000 rondas; ninguna contraseña en el firmware (un nodo sin usuario entra en puesta en marcha); máximo 4 usuarios (admin o visor) y de 8 a 64 caracteres; máximo 6 sesiones, 30 minutos de inactividad |
| Tomar un nodo recién grabado | La puesta en marcha exige un código: el de la compilación (`CONFIG_ARMOR_SETUP_CODE`) o uno aleatorio guardado en flash y mostrado en la consola USB; con secreto de flota es `HMAC-SHA256(secreto, MAC)`; también es la clave del Wi-Fi de configuración (WPA2, nunca puenteado) |
| El panel alcanzado desde internet | Responde por defecto a las redes privadas de una casa (10/8, 172.16/12, 192.168/16, 169.254/16, 127/8 y 100.64/10 donde viven algunas VPN); el ajuste `web.admin_sources` lista otras direcciones y redes IPv4 (8 entradas, 120 caracteres); lo de fuera se rechaza salvo la identidad pública `GET /api/v1/session`; se rechaza una lista que dejaría fuera a quien la guarda |
| El panel en claro por HTTP | HTTPS en el 443 con un certificado que el nodo hace él mismo (autofirmado, en flash, huella SHA-256 visible para comparar); el panel puede ser http, https o ambos; la cookie de sesión es siempre HttpOnly y SameSite=Strict (y Secure por TLS) |
| Un firmware falso desde GitHub | Una imagen solo se instala con un SHA-256 que coincide y una firma Ed25519 válida hecha con la clave del mantenedor (`no_checksum`, `no_signature`, `bad_signature`, `checksum_mismatch`, `wrong_firmware`, `not_newer`, `version_mismatch` la rechazan); una subida manual de un administrador no se comprueba: es un límite explícito |
| Secuestro de la puesta en marcha por Bluetooth | Un PIN de 6 dígitos derivado del código de puesta en marcha, visible en la consola USB y para el administrador; el móvil debe introducirlo antes de escribir ajustes; la ventana dura `CONFIG_ARMOR_BLE_SETUP_MINUTES` (10 por defecto, de 1 a 120) |
| Una orden remota que hace daño | Dos pasos con un token de un solo uso ligado a esa operación; solo el servidor puede escribir esos topics en el broker; la pausa dura una hora como máximo; no hay restablecimiento de fábrica, borrado ni sueño profundo por orden remota |
| Secretos guardados leídos por quien tiene la placa | Opcional (apagado por defecto, interruptor `security.seal_secrets` o `nodes.nvs_encryption` del servidor): ChaCha20-Poly1305 para contraseñas, tokens, claves, PIN y hashes. No cifra la flash ni quema un eFuse: la clave está en una partición del mismo chip, así que no protege frente a quien tenga la placa |
| El enlace con el broker en claro | TLS opcional (`mqtts://`) con la autoridad propia del dueño (PEM de hasta 2 KB) y comprobación del nombre; apagado por defecto. El handshake solo se ha ejecutado en pruebas de host, no en una placa |
| Una batería escrita por error o por un atacante | Tres cerrojos: el ajuste del nodo `battery.write_enabled` (administradores) con la contraseña de servicio de la batería, `ARMOR_BATTERY_WRITE=1` del servidor, y la lista de acceso del broker para `armor/solar/<nodo>/maintenance/command` y `.../result` |
| Una orden de escritura repetida o robada | Dos pasos (armar y ejecutar con un token de un solo uso), ventana de armado de 120 s, 30 s de enfriamiento entre escrituras a un puerto, 3 fallos en 10 minutos lo bloquean 5; se vuelve a teclear el número de serie de la batería, la batería debe estar en reposo y sin alarma crítica, y la imagen de firmware se conserva 30 minutos y se comprueba |
| Escribir en un inversor o un ANT-BMS | Los comandos de escritura de un inversor (un ajuste cambia cómo se alimenta la casa) y de un ANT-BMS (puede desconectar una batería bajo carga) ni siquiera se pueden construir |
| Números absurdos mostrados como reales | Un valor imposible se retiene en el límite del contrato y se nombra en `clamped`, para que el servidor lo muestre como no real; un inversor REVO no tiene consulta de avisos, así que su mensaje dice `warnings_known: false` en vez de afirmar que no hay avisos |

Ninguna trama se ha leído de un inversor o una batería reales; los formatos están escritos de documentos públicos y de memoria, y las tramas ANT-BMS de las pruebas las capturaron otras personas. Lee [docs/BATTERY_MAINTENANCE.md](docs/BATTERY_MAINTENANCE.md) y [SECURITY.md](SECURITY.md).

## 🔌 Interfaces

| Parte | Qué define |
|---|---|
| MQTT | `armor/solar/<nodo>/<dispositivo>/state` (un mensaje por inversor o pila de baterías, tipo `inverter` o `battery` del contrato de ARMOR-COMMON), `armor/solar/<nodo>/maintenance/{command,result}`, `armor/health/<nodo>` (última voluntad), `armor/control/<id>/{command,result,maintenance}` y `armor/control/all/command` |
| Puertos | 10 puertos serie (3 UART de hardware y 7 emuladas, hasta 19200 baudios si son emuladas); por puerto un tipo: `voltronic` (2400 baudios, cada 5 s), `pylontech` (115200, cada 10 s), `ant` (19200, cada 5 s) o `raw` (9600, solo escucha, no publica); dialectos de inversor `auto`, `pi30`, `revo`, `pi18`; el perfil mux tiene 3 grupos de puertos |
| Consultas | Voltronic: `QPIGS`, `QMOD`, `QPIWS`, `QPIRI` (y `QPIGS2`, `QPGS` en un inversor estándar con el mux); consola Pylontech: `pwr`, `bat <n>`, `info <n>`, `stat`, `soh`, registro de eventos, hasta 8 módulos pedidos celda a celda |
| Panel | Las rutas de la familia (`tests/check_routes.py` comprueba que toda ruta que llama el panel existe) más las páginas de puertos y lecturas; 51 códigos de error con texto en ocho idiomas (772 filas en `panel/text.js`); los menús de Studio, en [docs/STUDIO_MENUS.md](docs/STUDIO_MENUS.md) |

## ⚙️ Configuración

| Variable | Por defecto | Qué define |
|---|---|---|
| `enabled`, `kind`, `name`, `baud`, `rx`, `tx`, `de`, `poll_s`, `modules`, `dialect` | - | Los ajustes de cada puerto: activado o no, el tipo, el nombre del dispositivo en el topic, la velocidad (0 = la del tipo), los pines (y el pin DE/RE de un RS485), el periodo de sondeo (0 = el del tipo), el número de módulos y el dialecto del inversor; como máximo 3 redes de reserva y 2 brokers de reserva |
| `battery.write_enabled` | off | El cerrojo del nodo para escribir en una batería Pylontech (administradores, con la contraseña de servicio de la batería, que nunca sale del nodo) |
| `CONFIG_ARMOR_BOARD_S3_WIFI` / `_S3_ETH` | wifi | La placa: ESP32-S3-WROOM-1 N16R8 por Wi-Fi (por defecto) o la Waveshare ESP32-S3-ETH |
| `CONFIG_ARMOR_NODE_ID`, `_SETUP_CODE`, `_FLEET_SECRET`, `_MQTT_*`, `_HEARTBEAT_S`, `_BLE_SETUP_MINUTES`, `_BOOT_FACTORY_RESET` | - | Los ajustes que comparten todos los nodos: el nombre (vacío da `armor-` y los últimos bytes de la MAC), el código de puesta en marcha (vacío: aleatorio), el secreto de flota (16+ caracteres, nunca en el repositorio), los primeros ajustes del broker, el latido (de 2 a 300 s), los minutos de puesta en marcha por Bluetooth (de 1 a 120) y el reinicio con BOOT (8 s en los primeros 30 s) |
| `system.login_limit_per_minute` | 30 (0-120) | Contraseñas erróneas por minuto antes de que el panel bloquee el acceso a todos (0 = sin límite global); se lee y se cambia desde Studio (un botón en cada nodo) o con `POST /api/v1/admin/nodes/settings`, donde el acceso al panel se usa una vez y nunca se guarda |

## 🚀 Inicio rápido

**Requisitos:** un compilador C++17 para las pruebas de host; el contenedor ESP-IDF 5.5.5 para compilar la imagen; una ESP32-S3-WROOM-1 N16R8 o una Waveshare ESP32-S3-ETH.

```bash
cmake -S tests -B build/host && cmake --build build/host && ctest --test-dir build/host   # host tests, -Werror
build/host/emit_poller_samples | python tests/check_samples.py   # what the ports make is accepted by ARMOR-COMMON
tools/build_node.sh generic                       # the image for the N16R8 board in the ESP-IDF container: dist/generic-s3-wifi.bin
tools/build_node.sh generic s3-eth               # the same firmware for the Waveshare ESP32-S3-ETH (Ethernet): dist/generic-s3-eth.bin
node tools/panel_mock.mjs --user admin:adminpass123   # the panel without a board
```

Véase la [guía del firmware](docs/NODE_FIRMWARE.md) (la placa, los puertos, el primer arranque y la lista de pruebas en banco) y las [notas de cableado](docs/NODE_HARDWARE.md).

## 🧪 Pruebas

```bash
cmake -S tests -B build/host && cmake --build build/host && ctest --test-dir build/host
python -I tools/armor_ci_validate.py
```

18 programas de prueba de host y 6 comprobaciones en Python se ejecutan con `ctest`; 11824 comprobaciones contadas por los programas de host, compilados con `-Werror`.

Comprobaciones Python: los textos del panel en ocho idiomas, las tareas, las rutas de guardado, la sesión Bluetooth bajo su bloqueo y las rutas que llama el panel (y, en algunos nodos, los códigos HTTP o los glifos de las fuentes)

Las pruebas de host necesitan cualquier compilador C++17 (Linux, WSL, MSYS2); el panel web también se ejercita en un navegador real contra un nodo de pruebas (`tools/panel_mock.mjs`, `tools/panel_browser_test.mjs`).

`test_clock` solo imprime `ok` y no tiene contador; los demás programas informan de sus comprobaciones. Las tramas reales de las pruebas están en `tests/pylontech_real.hpp`, `pylontech_us2000_real.hpp`, `pylontech_logs_real.hpp`, `ant_frames.hpp` y `battery_bench.hpp`.

## 🏗️ Compilación y publicación

`tools/build_node.sh` compila la imagen en el contenedor ESP-IDF 5.5.5 y la versión lleva la imagen, su SHA-256 y su firma Ed25519, una por placa; la imagen se graba por USB-C la primera vez y después se actualiza desde el panel.

## 🩺 Resolución de problemas

| Ves | Porque | Haz |
|---|---|---|
| Perdiste la contraseña o el nodo ya no se une a la red | Un nodo con puesta en marcha y sin dirección tras 90 s abre su propio Wi-Fi `ARMOR-SETUP-xxxxxx` (abre `http://192.168.4.1/`) | Para empezar de cero mantén BOOT 8 segundos en los primeros 30 segundos tras encender (borra ajustes y usuarios), o usa *Restablecer de fábrica* en el panel |
| Se rechaza una actualización desde GitHub | La versión no trae suma de comprobación ni firma, no coinciden (`e_no_checksum`, `e_no_signature`, `e_checksum_mismatch`, `e_bad_signature`), no trae imagen para esta placa (`e_no_asset_for_board`) o es de otro proyecto (`e_wrong_firmware`) | El nodo no cambió; reintenta o deja que un administrador suba el fichero a mano |
| Se rechaza una escritura a una batería con un código | `serial_mismatch`, `not_pylontech`, `no_battery`, `unknown_model`, `not_idle`, `critical_alarm`, `busy`, `cooldown`, `locked`, `not_armed`, `bad_token`, `expired`, `disabled`, `image_missing`, `image_mismatch`...: el nodo rechaza lo que no es exactamente la operación armada | Lee el código del resultado, corrige la causa y arma la operación otra vez (mira [docs/BATTERY_MAINTENANCE.md](docs/BATTERY_MAINTENANCE.md)) |
| Un puerto no publica nada | El puerto está apagado, su tipo es `raw` (solo escucha), la velocidad o los pines no coinciden con el equipo, o el equipo no contesta | Revisa los ajustes del puerto y el cableado en [docs/NODE_HARDWARE.md](docs/NODE_HARDWARE.md); el panel muestra el volcado crudo de un puerto mientras lo mira |

## 📂 Estructura del repositorio

<p align="center">
  <img src="images/repository-structure.svg" alt="Estructura del repositorio de ARMOR-NODE-SOLAR" width="100%">
</p>

<details>
<summary>Versión en texto del árbol</summary>

```text
ARMOR-NODE-SOLAR/
├── main/                                el firmware: un componente de ESP-IDF
├── core/                                la lógica sin ESP-IDF, probada en un ordenador
├── panel/                               el panel web embebido en el firmware
├── tests/                               pruebas de host y comprobaciones Python
├── docs/                                10 guías: protocolos, mensajes, mantenimiento, hardware
├── tools/                               herramientas de compilación, empaquetado y comprobación
├── .github/                             flujo de CI y plantillas de issues y pull requests
├── images/                              banner, icono y las dos imágenes de este README
├── build.sh build.bat                   compilar con el flujo de proyecto compartido
├── test.sh test.bat                     ejecutar las pruebas con el flujo compartido
├── run.sh run.bat                       arrancar con el flujo de proyecto compartido
├── armor.project.json                   manifiesto: versión, rol, madurez, comando de build
├── CHANGELOG.md                         qué cambió en cada versión
├── SECURITY.md SUPPORT.md LICENSE       política de seguridad, soporte y licencia
├── CONTRIBUTING.md CODE_OF_CONDUCT.md   cómo contribuir y el código de conducta
├── CMakeLists.txt                       el proyecto ESP-IDF
├── partitions.csv                       la tabla de la flash
├── sdkconfig.defaults                   los ajustes del firmware
├── sdkconfig.board.s3-eth               los ajustes de una placa
├── sdkconfig.board.s3-wifi              los ajustes de una placa
├── secrets/                             ejemplo del fichero del nodo (los reales no se versionan)
├── build-test.sh build-test.bat         ejecutar las pruebas con el flujo compartido
└── .gitattributes .gitignore            ajustes de Git
```

</details>

## 🗺️ Hoja de ruta y límites conocidos

**Límites conocidos hoy**

* Las tramas, sumas de control y lecturas de Voltronic, Pylontech y ANT-BMS se comprueban contra las tramas documentadas y muestras grabadas; ninguno ha hablado todavía con un inversor o una batería reales.
* El panel web se ha ejercitado en un navegador contra una simulación del nodo, no en una placa.
* Ninguna placa construida ni inversor o batería conectados: nada ha funcionado todavía con equipos reales.
* El código de configuración, el acceso con límite de contraseñas erróneas, el PIN Bluetooth y las actualizaciones firmadas están probados en el host; no ha habido una revisión independiente.
* Las comprobaciones de hash y de firma Ed25519 y la lógica de vuelta atrás pasan en el host; una actualización por aire nunca se ha ejecutado en una placa.
* La escritura en baterías Pylontech está desactivada por defecto y solo se ha probado contra un simulador.

**Lo siguiente**

* Ejecutar el nodo en una placa real con un inversor y baterías reales, y luego el enlace Bluetooth del ANT-BMS (hoy se usa el cable), la actualización por aire y las escrituras a una batería.

## 🌍 Idiomas

El README y los textos existen en inglés, español, francés, italiano, alemán, chino, japonés y portugués de Portugal.

## 🔗 Proyectos relacionados

**A.R.M.O.R.** (Autonomous Radar & Multimodal Observation Range) es un sistema de seguridad perimetral hecho de repositorios independientes. Cada uno tiene su propia versión, sus propias pruebas y su propio README; esta es la familia:

* **[ARMOR-COMMON](https://github.com/JuanenRac/ARMOR-COMMON)** - Contratos de mensajes, validadores, vectores de conformidad y tipos generados
* **[ARMOR-NODE-RADAR](https://github.com/JuanenRac/ARMOR-NODE-RADAR)** - Firmware de nodo de campo para ESP32-S3 con tres radares y su propio panel web
* **ARMOR-NODE-SOLAR** (este repositorio) - Protocolos de inversores y baterías solares y los mensajes de un nodo pasarela
* **[ARMOR-NODE-ELECTRICAL](https://github.com/JuanenRac/ARMOR-NODE-ELECTRICAL)** - Nodo eléctrico: medidores, el mensaje de las lecturas de la red y las reglas de maniobra
* **[ARMOR-NODE-ALARM](https://github.com/JuanenRac/ARMOR-NODE-ALARM)** - Nodo y panel de alarma: zonas, armado, retardos, sirena y PIN, con el servidor o sin él
* **[ARMOR-NODE-HMI](https://github.com/JuanenRac/ARMOR-NODE-HMI)** - Panel táctil: el estado del sistema en una pantalla de pared, armar y reconocer, y el hogar del asistente de voz
* **[ARMOR-NETWORK](https://github.com/JuanenRac/ARMOR-NETWORK)** - La red local: sus dispositivos, internet y lo que cambia
* **[ARMOR-SERVER](https://github.com/JuanenRac/ARMOR-SERVER)** - Coordinador central: telemetría, alarmas, dispositivos, lecturas solares y cámaras
* **[ARMOR-STUDIO](https://github.com/JuanenRac/ARMOR-STUDIO)** - Consola web: cámaras, radar, alarmas, energía solar y el diseñador 2D/3D del emplazamiento
* **[ARMOR-ANDROID-CONTROL](https://github.com/JuanenRac/ARMOR-ANDROID-CONTROL)** - Cliente de operador Android con radar 2D/3D en vivo
* **[ARMOR-IOS-CONTROL](https://github.com/JuanenRac/ARMOR-IOS-CONTROL)** - Cliente de operador para iPhone
* **[ARMOR-SERVER-AI](https://github.com/JuanenRac/ARMOR-SERVER-AI)** - Política de inferencia visual que explica sus decisiones y nunca actúa
* **[ARMOR-VISION](https://github.com/JuanenRac/ARMOR-VISION)** - Zonas de cámara, máscaras y reglas de detección para la Jetson Orin NX
* **[ARMOR-VOICE-AI](https://github.com/JuanenRac/ARMOR-VOICE-AI)** - Intenciones de voz sin conexión con una confirmación que no se puede falsificar
* **[ARMOR-HARDWARE](https://github.com/JuanenRac/ARMOR-HARDWARE)** - Cajas, electrónica y la matriz de aceptación en banco
* **[ARMOR-DEVOPS](https://github.com/JuanenRac/ARMOR-DEVOPS)** - Despliegue, el banco de pruebas de la CM5, copias de seguridad y TLS
* **[ARMOR-SIMULATOR](https://github.com/JuanenRac/ARMOR-SIMULATOR)** - Simulador de telemetría sin conexión con fallos repetibles
* **[ARMOR-UPDATER](https://github.com/JuanenRac/ARMOR-UPDATER)** - Detecta, instala y actualiza los repositorios del propio ecosistema
* **[ARMOR-DOCS](https://github.com/JuanenRac/ARMOR-DOCS)** - Arquitectura, base de seguridad y la matriz de capacidades

## 📚 Documentación

* [Firmware del nodo](docs/NODE_FIRMWARE.md): la placa, los puertos, el primer arranque y la lista de banco
* [Cableado](docs/NODE_HARDWARE.md), [protocolos](docs/PROTOCOLS.md) y [mensajes](docs/SOLAR_MESSAGES.md)
* [Mantenimiento de baterías](docs/BATTERY_MAINTENANCE.md) y su [plan de pruebas de banco](docs/BATTERY_BENCH_TEST_PLAN.md)
* [Menús de Studio](docs/STUDIO_MENUS.md), [pruebas de banco](docs/BENCH_TESTS.md), [código de puesta en marcha](docs/SETUP_CODE.md) y [alta por Bluetooth](docs/BLE_PROVISIONING.md)
* [Matriz de capacidades: qué está demostrado y qué no](https://github.com/JuanenRac/ARMOR-DOCS/blob/main/docs/CAPABILITY_MATRIX.md)
* [Catálogo de proyectos: versiones y cómo dependen unos repositorios de otros](https://github.com/JuanenRac/ARMOR-DOCS/blob/main/docs/PROJECT_CATALOG.md)
* [Registro de cambios de este repositorio](CHANGELOG.md)

## 🤝 Contribuir y comunidad

* Antes de un pull request ejecuta `ctest --test-dir build/host` and `python -I tools/armor_ci_validate.py`: debe pasar. Consulta [CONTRIBUTING.md](CONTRIBUTING.md) y el [código de conducta](CODE_OF_CONDUCT.md).
* El README existe en ocho idiomas y se genera desde las fuentes de ARMOR-DOCS: un cambio de texto se hace allí.
* Un cambio en un mensaje entre componentes empieza en ARMOR-COMMON.
* No describas como funcionando en una placa lo que solo se ha compilado.
* Informa de una vulnerabilidad en privado: [SECURITY.md](SECURITY.md). Preguntas, ideas e informes: electrohobby3d@gmail.com ([SUPPORT.md](SUPPORT.md)).

## 👤 AUTOR

**JuanenRac (Electro Hobby 3D)** · electrohobby3d@gmail.com

## 📜 LICENCIA

GPL-3.0-or-later - véase [LICENSE](LICENSE).
