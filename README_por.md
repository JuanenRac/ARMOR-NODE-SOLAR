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
  <a href="README_jpn.md">🇯🇵 日本語</a> |
  🇵🇹 <b>Português</b>
</p>

### Nó de ligação solar: lê inversores e baterias através de até dez portas série (firmware ESP32-S3, o seu painel web e a biblioteca de protocolos)

<p align="center">
  <img src="https://img.shields.io/badge/License-GPL%203.0--or--later-blue.svg" alt="GPL 3.0 or later">
  <img src="https://img.shields.io/badge/Language-C%2B%2B17-00599c.svg" alt="Language">
  <img src="https://img.shields.io/badge/Devices-Voltronic%20%C2%B7%20Pylontech-ffb020.svg" alt="Devices">
  <img src="https://img.shields.io/badge/Tests-11824%20checks-2ea44f.svg" alt="Tests">
  <img src="https://img.shields.io/badge/Version-0.5.9-0f6cbd.svg" alt="Version">
  <img src="https://img.shields.io/badge/Maturity-functional-00E5FF.svg" alt="Maturity">
</p>

---

**Verificação de honestidade - o que funciona hoje:** O firmware compila no contentor ESP-IDF 5.5.5, o seu núcleo (as definições, a troca com cada tipo de equipamento, a aritmética da UART emulada, o canal Bluetooth, as verificações de versão e assinatura: 2885 verificações em 10 programas de teste) é testado num computador com equipamentos de substituição, as mensagens que produz são aceites pelo ARMOR-COMMON e o painel foi exercitado num navegador contra um nó de substituição. **Nunca correu numa placa e nenhum inversor ou bateria foi ligado**: o Wi-Fi, o painel sobre TLS, a atualização, as UART de hardware e emuladas e os formatos dos protocolos (escritos a partir de documentos públicos e de memória) estão por experimentar. As tramas ANT-BMS dos seus testes foram capturadas por outras pessoas nas suas próprias unidades: este projeto ainda não leu nenhuma por si.

---

## 📋 Num relance

| | |
|---|---|
| Versão | `0.5.9` |
| Maturidade | Funcional |
| Função | Nó de campo solar para ESP32-S3: inversores e baterias (família: Field Nodes, pai: ARMOR-COMMON) |
| Tecnologia | C++17, ESP-IDF 5.5.5, ESP32-S3 |
| Testes | 11824 verificações em 18 programas de anfitrião e 6 em Python (`ctest`) |
| Equipamentos | Inversores Voltronic / MPP Solar, baterias Pylontech, ANT-BMS; 10 portas série |
| Funciona em | ESP32-S3-WROOM-1 N16R8 ou Waveshare ESP32-S3-ETH; testes de anfitrião em qualquer computador |
| Em hardware real | Nenhum ainda: nenhuma placa montada nem inversor ou bateria ligados (só testes no computador, a compilação e o painel num navegador) |
| Licença | GPL-3.0-or-later |

**Ir para:** [Visão geral](#-visão-geral) · [Arquitetura](#-arquitetura) · [Segurança](#-segurança-e-proteção) · [Interfaces](#-interfaces) · [Configuração](#-configuração) · [Início rápido](#-início-rápido) · [Testes](#-testes) · [Problemas](#-resolução-de-problemas) · [Estrutura](#-estrutura-do-repositório) · [Relacionados](#-projetos-relacionados)

## 🚦 Estado do projeto

Cada linha é uma parte deste repositório e a luz diz até onde chega hoje. É escrita com o que o repositório consegue demonstrar (testes, compilações, execuções), não com o que está previsto.

🟢 Feito e verificado · 🟡 Funciona, com limites conhecidos · 🟠 Feito a meio ou ainda por verificar · 🔴 Por fazer ou a não funcionar

| Área | Estado | O que significa aqui |
|---|---|---|
| Software | 🟢 | O firmware compila no contentor ESP-IDF 5.5.5 para as duas placas (Wi-Fi e Ethernet). |
| Testes | 🟢 | Passam 11.824 verificações em 18 programas de anfitrião (`ctest`): as definições, a troca com cada tipo de equipamento, a UART emulada, o canal Bluetooth e as verificações de versão. |
| Protocolos e dados | 🟡 | As tramas, somas de verificação e leituras de Voltronic, Pylontech e ANT-BMS são verificadas contra as tramas documentadas e amostras gravadas; nenhum falou ainda com um inversor ou uma bateria reais. |
| Interface | 🟡 | O painel web foi exercitado num navegador contra uma simulação do nó, não numa placa. |
| Hardware | 🔴 | Nenhuma placa construída nem inversor ou bateria ligados: nada funcionou ainda em equipamentos reais. |
| Segurança | 🟡 | O código de configuração, o acesso com limite de palavras-passe erradas, o PIN Bluetooth e as atualizações assinadas estão testados no anfitrião; não houve revisão independente. |
| Atualizações | 🟠 | As verificações de hash e de assinatura Ed25519 e a lógica de retrocesso passam no anfitrião; uma atualização pelo ar nunca foi executada numa placa. |
| Escrita nos equipamentos | 🟠 | A escrita nas baterias Pylontech está desativada por predefinição e só foi testada contra um simulador. |
| Integração | 🟢 | As mensagens do nó são validadas contra os esquemas partilhados e os vetores de conformidade do ARMOR-COMMON. |
| Documentação | 🟢 | Oito idiomas, o guia do firmware, as notas de cablagem e o registo de alterações estão atualizados. |

## 🎯 Visão geral

* **Duas placas, um só firmware:** um ESP32-S3-WROOM-1 N16R8 em Wi-Fi (o predefinido) e a Waveshare ESP32-S3-ETH no seu cabo Ethernet (DHCP ou um endereço fixo; a sua própria rede Wi-Fi mantém-se para lhe chegar a partir de um telemóvel). A imagem escolhe-se ao compilar (`tools/build_node.sh generic s3-wifi` ou `generic s3-eth`); a tabela de pinos, os pinos predefinidos das portas e a forma de entrada seguem a placa, e uma imagem só serve para a sua própria placa. Ambas compilam e estão testadas em anfitrião; nenhuma correu numa placa.
* **O firmware do nó** (ESP32-S3-WROOM-1 N16R8 em Wi-Fi, ou a Waveshare ESP32-S3-ETH no seu cabo): dez portas série, três UART de hardware e sete emuladas (até 19200 baud), cada uma independente e cada uma a ler um inversor, uma bateria ou um monitor em bruto, de modo que um nó pode ler só inversores, só baterias ou uma mistura.
* **O painel web do nó,** também o dos nós de radar: configuração com um código mostrado na consola USB, início de sessão e utilizadores, Wi-Fi (estação e ponto de acesso), broker, atualização pelo ar com retrocesso, registo, HTTPS, e as páginas das portas (com o que cada uma ouve, para protocolos ainda não descodificados) e das leituras, em oito idiomas.
* **Inversores Voltronic / MPP Solar** (Axpert, PIP, InfiniSolar e clones), RS232 a 2400 baud: as tramas e o seu CRC, e as leituras `QPIGS` (rede, saída, bateria, FV, bits de estado), `QMOD` (modo), `QPIWS` (avisos e avarias por nome) e `QPIRI` (valores nominais). Só se podem construir comandos de leitura: uma definição muda a forma como a casa é alimentada. Três dialetos, escolhidos na porta ou encontrados por ela (*auto*): PI30, REVO (outro `QPIGS`, respostas com soma de controlo) e PI18 (`^P005GS`: InfiniSolar V, LV5048, SunGoldPower), só de leitura.
* **Baterias Pylontech** (US2000, US3000, US5000): a tabela `pwr` da consola (tensão, corrente, temperaturas e estado de carga de cada módulo), `bat <n>` (a tensão e temperatura de cada célula) e `info <n>` (modelo, capacidade restante e total, ciclos), resumidas como um só conjunto com a sua capacidade e energia totais, e a trama RS485 com as suas duas verificações. Os formatos estão escritos de memória da consola pública e podem diferir entre firmwares. As colunas encontram-se pelos nomes do cabeçalho (o formato do US5000 V2.3 com as suas colunas Id, MosTempr e SysAlarm.St incluídas), a carga restante e o equilíbrio vêm de `bat`, e o modelo, a capacidade nominal e os ciclos de `info` e `stat`, pedidos uma vez e guardados durante meia hora.
* **O que um módulo Pylontech diz de si mesmo:** o seu modelo e as versões da sua placa e do seu software, as suas células e a corrente que admite, os contadores da sua vida (`stat`), a saúde de cada célula (`soh`, marcando a anormal) e a entrada mais recente do seu registo de eventos, com os nomes das proteções tal como a consola os imprime. A consola só lê, e um administrador pode descarregar como ficheiro de texto o que uma porta ouviu.
* **As mensagens do nó** (`armor/solar/<node>/<device>/state`, uma por inversor ou conjunto de baterias), definidas no ARMOR-COMMON com esquemas e vetores de conformidade; o que as portas produzem verifica-se contra elas.
* **Baterias ANT-BMS** (as placas pretas de baterias caseiras, 7S a 32S), UART de 3,3 V a 19200 baud: os dois protocolos do seu firmware (o nó pergunta num e depois no outro e fica com o que responde), com as células, temperaturas, estado de carga, corrente, capacidades e os estados dos MOSFET e do equilibrador, verificados contra tramas capturadas em quatro modelos reais. Só leitura: os seus comandos de escrita podem desligar uma bateria em carga. Um botão na página das portas lê também o modelo, a versão e as 56 definições de proteção e equilíbrio do BMS (protocolo mais recente), só de leitura.
* **Configuração a partir de um telemóvel por Bluetooth,** o mesmo canal que o do nó de radar: a aplicação ARMOR encontra o nó como `ARMOR-XXXXXX` e define o seu nome, Wi-Fi, endereço, broker e modo Bluetooth com os utilizadores do painel e o código de configuração ([o protocolo](docs/BLE_PROVISIONING.md)). Só escuta enquanto o nó não tem utilizador, a menos que se diga o contrário. O lado de rádio nunca correu numa placa.
* **A placa base com multiplexadores (o perfil *mux*):** três UART de hardware, cada uma atrás de um 74HC4052, servem até oito portas em grupos de 4, 2 e 2 que se revezam na sua linha, cada porta com a sua própria velocidade e polaridade, e um LED por porta através de um 74HC595; as células de um conjunto Pylontech alto podem ler-se em rotação. Para um inversor no dialeto padrão, a **segunda entrada FV** (`QPIGS2`) e as **unidades de um sistema em paralelo** (`QPGS`) são leituras opcionais. Nada disto correu numa placa.
* **Atualizações pelo ar assinadas:** o nó só instala do GitHub uma imagem cujo SHA-256 coincide e que leva uma assinatura Ed25519 válida do responsável (a chave pública está no firmware, a privada nunca sai do responsável); um ficheiro carregado à mão por um administrador no painel continua a ser aceite. As páginas e mensagens do painel estão em oito idiomas.
* **Emparelhamento Bluetooth com PIN:** o PIN de seis dígitos deriva do código de configuração, é impresso na consola USB e mostrado a um administrador no painel; o telemóvel tem de o introduzir antes de poder escrever as definições do nó.
* **Ainda não:** a ligação Bluetooth do ANT-BMS (o nó usa o cabo, o mais estável dos dois) e uma execução numa placa real com equipamento real.
* **Só a casa pode usar o painel:** o painel responde por omissão à rede privada (ou às redes que o dono indicar, como uma VPN), recusa uma lista que deixaria de fora quem a guarda, conta as palavras-passe erradas por pessoa e por endereço e funciona atrás do proxy do servidor para ser alcançado de fora de casa sem publicar o nó.
* **Desconhecido não é nenhum:** um inversor REVO não tem consulta de avisos, por isso a sua mensagem leva `warnings_known: false` e o Studio mostra os avisos como desconhecidos em vez de "nenhum".
* **Ligação cifrada opcional ao broker (TLS):** desativada por predefinição, por isso o nó fala MQTT sem cifra como antes. Um interruptor e uma caixa para o certificado da autoridade própria da instalação (PEM, no máximo 2 KB) na página Broker fazem-no ligar-se com `mqtts://`, confiar só nessa autoridade e verificar que o certificado do broker foi feito para o endereço alcançado; o que não for um único certificado PEM é recusado. Coberto por testes no computador; o handshake ainda não foi executado numa placa.
* **Relatório de saúde:** cerca de uma vez por minuto o nó publica há quanto tempo está ligado e porque reiniciou da última vez, a memória livre, a ligação e o sinal Wi-Fi, quantas vezes se ligou ao broker e o último erro que registou (`armor/health/<nó>`, o contrato `node_health` do ARMOR-COMMON). O código que o constrói é partilhado por todos os nós e testado num computador; nunca correu numa placa.
* **Escrita em baterias Pylontech (desligada por omissão):** só operações tipificadas - relógio, contadores e registos, reinício, capacidade, a reposição do nível de carga, limites e proteções, saídas, número de série, memória externa, firmware por XMODEM-1K e uma consola de especialista com lista de bloqueio - em dois passos (preparar e executar com um token de uso único), lendo a bateria antes e depois, com a palavra-passe de serviço guardada só no nó e uma consola simulada para os testes. Não experimentado numa bateria real: veja o [plano de testes de bancada](docs/BATTERY_BENCH_TEST_PLAN.md).
* **Controlo a partir do servidor:** o Studio pode reiniciar o nó de forma limpa, pô-lo em pausa até uma hora (retoma sozinho), pôr o seu trabalho em espera (sem fim, mantida nos reinícios, o nó continua ligado e a responder), fazê-lo mostrar-se, pedir um instantâneo de diagnóstico e marcá-lo em manutenção (`armor/control/<nó>/...`, o contrato `node_control` do ARMOR-COMMON). Cada ordem tem dois passos com um token de uso único que pertence a essa operação, só o servidor as pode escrever no broker, e não existe reposição de fábrica, apagamento nem sono profundo. Enquanto o nó está em pausa ou em espera, não escreve em nenhuma bateria (uma operação em curso ainda pode ser cancelada) e continua a ler inversores e baterias. O código é partilhado por todos os nós e testado num computador; ainda não correu numa placa.
* **Os valores absurdos são nomeados, não escondidos:** quando um inversor envia um número impossível, o nó mantém-no no limite do contrato e nomeia-o em `clamped`, pelo que o servidor o mostra como não real; e o despejo em bruto de uma porta só é feito enquanto o painel o está a ver.
* **Diagnóstico sem cabo:** depois de um arranque que não foi limpo (um bloqueio, um watchdog, uma quebra de alimentação, uma reserva de memória falhada), o relatório de saúde diz o motivo durante dez minutos; uma falha deixa o seu core dump para ver, descarregar ou apagar a partir do painel; as definições guardadas são copiadas antes de cada instalação de firmware e recuperadas se uma atualização as danificar; e um administrador pode pedir ao nó, através do servidor, um pacote de diagnóstico ou o seu registo com um token de utilização única.
* **Selagem opcional dos segredos guardados:** desativada por predefinição (`security.seal_secrets`, ou `nodes.nvs_encryption` do servidor). Ativada, as palavras-passe, os tokens, as chaves, os PIN e os hashes de palavras-passe que o nó guarda ficam selados com ChaCha20-Poly1305 e tudo o resto continua legível; ativá-la ou desativá-la reescreve-os sem perder nada, mesmo que o nó pare a meio. Não se queima nenhum eFuse nem se cifra a flash: a chave está numa partição do mesmo chip, pelo que **não** protege contra quem tenha a placa, e um nó instalado antes dessa partição mantém os segredos como estavam até ser gravado com a nova tabela de partições.

## 🧩 Papel no A.R.M.O.R.

O ARMOR-NODE-SOLAR é a porta de ligação entre o equipamento solar da casa e a família. Lê inversores e baterias por até dez portas série e publica o que lê para o [ARMOR-SERVER](https://github.com/JuanenRac/ARMOR-SERVER), que guarda o histórico, os totais e os alarmes; os menus *Inversores* e *Baterias* do [ARMOR-STUDIO](https://github.com/JuanenRac/ARMOR-STUDIO) e o ecrã Solar do [ARMOR-ANDROID-CONTROL](https://github.com/JuanenRac/ARMOR-ANDROID-CONTROL) mostram-no. Só lê: nunca escreve uma definição em nenhum equipamento.

| Nome | Como | Para quê |
|---|---|---|
| ARMOR-SERVER | MQTT `armor/solar/<node>/<device>/state` | Recebe uma mensagem por inversor ou pilha de baterias; ordena a manutenção de uma bateria |
| ARMOR-STUDIO | menus, ligação ao painel | Mostra inversores e baterias e abre o painel do nó |
| ARMOR-ANDROID-CONTROL | Bluetooth | Configura o nó no terreno e mostra os ecrãs de energia |
| ARMOR-COMMON | contrato solar | As mensagens são verificadas com os seus esquemas; o firmware comum é copiado dele |

## 🏛️ Arquitetura

<p align="center">
  <img src="images/architecture.svg" alt="Arquitetura de ARMOR-NODE-SOLAR: quem o chama, o que contém e de que depende" width="100%">
</p>

## 🖼️ Capturas de ecrã

Capturadas a 1920×1080 numa bancada de testes, com dados de exemplo e sem qualquer instalação real por trás.

<table>
<tr><td width="50%" valign="top"><img src="images/screenshots/overview.png" alt="Resumo: o estado do nó num relance - rede, broker, firmware e o que reporta." width="100%"><br><sub>Resumo: o estado do nó num relance - rede, broker, firmware e o que reporta.</sub></td><td width="50%" valign="top"><img src="images/screenshots/ports.png" alt="Portas: as portas série do nó e o equipamento de cada uma." width="100%"><br><sub>Portas: as portas série do nó e o equipamento de cada uma.</sub></td></tr>
<tr><td width="50%" valign="top"><img src="images/screenshots/readings.png" alt="Leituras: o que o nó está a ler neste momento, com a idade de cada valor." width="100%"><br><sub>Leituras: o que o nó está a ler neste momento, com a idade de cada valor.</sub></td><td width="50%" valign="top"><img src="images/screenshots/network.png" alt="Rede: o endereçamento Ethernet ou Wi-Fi do nó, definido no seu próprio painel web." width="100%"><br><sub>Rede: o endereçamento Ethernet ou Wi-Fi do nó, definido no seu próprio painel web.</sub></td></tr>
</table>

## 🔒 Segurança e proteção

| Ameaça | Defesa |
|---|---|
| Palavras-passe em claro ou compiladas | PBKDF2-HMAC-SHA256 com 10 000 rondas; nenhuma palavra-passe no firmware (um nó sem utilizador entra em configuração); no máximo 4 utilizadores (admin ou visualizador) e de 8 a 64 caracteres; no máximo 6 sessões, 30 minutos de inatividade |
| Tomar um nó acabado de gravar | A configuração exige um código de arranque: o da compilação (`CONFIG_ARMOR_SETUP_CODE`) ou um aleatório guardado na flash e mostrado na consola USB; com segredo de frota é `HMAC-SHA256(segredo, MAC)`; é também a chave do Wi-Fi de configuração (WPA2, nunca em ponte) |
| O painel alcançado a partir da internet | Responde por omissão às redes privadas de uma casa (10/8, 172.16/12, 192.168/16, 169.254/16, 127/8 e 100.64/10 onde vivem algumas VPN); a definição `web.admin_sources` lista outros endereços e redes IPv4 (8 entradas, 120 caracteres); tudo o que está fora é recusado exceto a identidade pública `GET /api/v1/session`; uma lista que deixaria de fora quem a guarda é rejeitada |
| O painel em claro por HTTP | HTTPS no 443 com um certificado que o nó cria sozinho (autoassinado, na flash, impressão digital SHA-256 visível para comparar); o painel pode ser http, https ou ambos; o cookie de sessão é sempre HttpOnly e SameSite=Strict (e Secure por TLS) |
| Um firmware falso do GitHub | Uma imagem só se instala com um SHA-256 coincidente e uma assinatura Ed25519 válida feita com a chave do mantenedor (`no_checksum`, `no_signature`, `bad_signature`, `checksum_mismatch`, `wrong_firmware`, `not_newer`, `version_mismatch` recusam-na); um envio manual por um administrador não é verificado: um limite explícito |
| Sequestro da configuração Bluetooth | Um PIN de 6 dígitos derivado do código de configuração, visível na consola USB e ao administrador; o telemóvel tem de o introduzir antes de escrever definições; a janela dura `CONFIG_ARMOR_BLE_SETUP_MINUTES` (10 por omissão, de 1 a 120) |
| Uma ordem remota que faz mal | Dois passos com um token de uso único ligado a essa operação; só o servidor pode escrever esses tópicos no broker; uma pausa dura uma hora no máximo; não há reposição de fábrica, apagamento nem sono profundo por ordem remota |
| Segredos guardados lidos por quem tem a placa | Opcional (desligado por omissão, interruptor `security.seal_secrets` ou `nodes.nvs_encryption` do servidor): ChaCha20-Poly1305 para palavras-passe, tokens, chaves, PIN e hashes. Não cifra a flash nem queima um eFuse: a chave está numa partição do mesmo chip, por isso não protege contra quem tem a placa |
| A ligação ao broker em claro | TLS opcional (`mqtts://`) com a autoridade própria do dono (PEM até 2 KB) e verificação do nome; desligado por omissão. O handshake só correu em testes de anfitrião, não numa placa |
| Uma bateria escrita por engano ou por um atacante | Três trancas: a definição do nó `battery.write_enabled` (administradores) com a palavra-passe de serviço da bateria, `ARMOR_BATTERY_WRITE=1` do servidor, e a lista de acesso do broker para `armor/solar/<nó>/maintenance/command` e `.../result` |
| Uma ordem de escrita repetida ou roubada | Dois passos (armar, depois executar com um token de uso único), janela de armação de 120 s, 30 s de arrefecimento entre escritas numa porta, 3 falhas em 10 minutos bloqueiam-na 5; o número de série da bateria digita-se de novo, a bateria tem de estar em repouso e sem alarme crítico, e a imagem de firmware guarda-se 30 minutos e verifica-se |
| Escrever num inversor ou num ANT-BMS | Os comandos de escrita de um inversor (uma definição muda como a casa é alimentada) e de um ANT-BMS (pode desligar uma bateria sob carga) nem sequer se podem construir |
| Números absurdos mostrados como reais | Um valor impossível é retido no limite do contrato e nomeado em `clamped`, para o servidor o mostrar como não real; um inversor REVO não tem consulta de avisos, por isso a sua mensagem diz `warnings_known: false` em vez de afirmar que não há avisos |

Nenhuma trama foi lida de um inversor ou bateria reais; os formatos estão escritos a partir de documentos públicos e de memória, e as tramas ANT-BMS dos testes foram capturadas por outras pessoas. Leia [docs/BATTERY_MAINTENANCE.md](docs/BATTERY_MAINTENANCE.md) e [SECURITY.md](SECURITY.md).

## 🔌 Interfaces

| Parte | O que define |
|---|---|
| MQTT | `armor/solar/<nó>/<dispositivo>/state` (uma mensagem por inversor ou pilha de baterias, tipo `inverter` ou `battery` do contrato do ARMOR-COMMON), `armor/solar/<nó>/maintenance/{command,result}`, `armor/health/<nó>` (testamento), `armor/control/<id>/{command,result,maintenance}` e `armor/control/all/command` |
| Portas | 10 portas série (3 UART de hardware e 7 emuladas, até 19200 baud se emuladas); por porta um tipo: `voltronic` (2400 baud, a cada 5 s), `pylontech` (115200, a cada 10 s), `ant` (19200, a cada 5 s) ou `raw` (9600, só escuta, não publica nada); dialetos de inversor `auto`, `pi30`, `revo`, `pi18`; o perfil mux tem 3 grupos de portas |
| Consultas | Voltronic: `QPIGS`, `QMOD`, `QPIWS`, `QPIRI` (e `QPIGS2`, `QPGS` num inversor padrão com o mux); consola Pylontech: `pwr`, `bat <n>`, `info <n>`, `stat`, `soh`, registo de eventos, até 8 módulos pedidos célula a célula |
| Painel | As rotas da família (`tests/check_routes.py` verifica que toda a rota que o painel chama existe) mais as páginas de portas e leituras; 51 códigos de erro com texto em oito línguas (772 linhas em `panel/text.js`); os menus do Studio estão em [docs/STUDIO_MENUS.md](docs/STUDIO_MENUS.md) |

## ⚙️ Configuração

| Variável | Predefinição | O que define |
|---|---|---|
| `enabled`, `kind`, `name`, `baud`, `rx`, `tx`, `de`, `poll_s`, `modules`, `dialect` | - | As definições de cada porta: ativada ou não, o tipo, o nome do dispositivo no tópico, a velocidade (0 = a do tipo), os pinos (e o pino DE/RE de um RS485), o período de consulta (0 = o do tipo), o número de módulos e o dialeto do inversor; no máximo 3 redes de reserva e 2 brokers de reserva |
| `battery.write_enabled` | off | A tranca do nó para escrever numa bateria Pylontech (administradores, com a palavra-passe de serviço da bateria, que nunca sai do nó) |
| `CONFIG_ARMOR_BOARD_S3_WIFI` / `_S3_ETH` | wifi | A placa: ESP32-S3-WROOM-1 N16R8 por Wi-Fi (predefinição) ou a Waveshare ESP32-S3-ETH |
| `CONFIG_ARMOR_NODE_ID`, `_SETUP_CODE`, `_FLEET_SECRET`, `_MQTT_*`, `_HEARTBEAT_S`, `_BLE_SETUP_MINUTES`, `_BOOT_FACTORY_RESET` | - | As definições que todos os nós partilham: o nome (vazio dá `armor-` e os últimos bytes do MAC), o código de configuração (vazio: aleatório), o segredo de frota (16+ caracteres, nunca no repositório), as primeiras definições do broker, o batimento (de 2 a 300 s), os minutos de configuração Bluetooth (de 1 a 120) e a reposição com BOOT (8 s nos primeiros 30 s) |

## 🚀 Início rápido

**Requisitos:** um compilador C++17 para os testes de anfitrião; o contentor ESP-IDF 5.5.5 para compilar a imagem; uma ESP32-S3-WROOM-1 N16R8 ou uma Waveshare ESP32-S3-ETH.

```bash
cmake -S tests -B build/host && cmake --build build/host && ctest --test-dir build/host   # host tests, -Werror
build/host/emit_poller_samples | python tests/check_samples.py   # what the ports make is accepted by ARMOR-COMMON
tools/build_node.sh generic                       # the image for the N16R8 board in the ESP-IDF container: dist/generic-s3-wifi.bin
tools/build_node.sh generic s3-eth               # the same firmware for the Waveshare ESP32-S3-ETH (Ethernet): dist/generic-s3-eth.bin
node tools/panel_mock.mjs --user admin:adminpass123   # the panel without a board
```

Veja o [guia do firmware](docs/NODE_FIRMWARE.md) (a placa, as portas, o primeiro arranque e a lista de verificação de bancada) e as [notas de ligação](docs/NODE_HARDWARE.md).

## 🧪 Testes

```bash
cmake -S tests -B build/host && cmake --build build/host && ctest --test-dir build/host
python -I tools/armor_ci_validate.py
```

18 programas de teste de anfitrião e 6 verificações Python correm com `ctest`; 11824 verificações contadas pelos programas de anfitrião, compiladas com `-Werror`.

Verificações Python: os textos do painel em oito línguas, as tarefas, os caminhos de gravação, a sessão Bluetooth sob o seu bloqueio e as rotas que o painel chama (e, em alguns nós, os códigos HTTP ou os glifos dos tipos de letra)

Os testes de anfitrião precisam de qualquer compilador C++17 (Linux, WSL, MSYS2); o painel web também é exercitado num navegador real contra um nó de substituição (`tools/panel_mock.mjs`, `tools/panel_browser_test.mjs`).

O `test_clock` só imprime `ok` e não tem contador; cada um dos outros programas comunica as suas verificações. As tramas reais dos testes estão em `tests/pylontech_real.hpp`, `pylontech_us2000_real.hpp`, `pylontech_logs_real.hpp`, `ant_frames.hpp` e `battery_bench.hpp`.

## 🏗️ Compilação e publicação

`tools/build_node.sh` compila a imagem no contentor ESP-IDF 5.5.5 e a versão leva a imagem, o seu SHA-256 e a sua assinatura Ed25519, uma por placa; a imagem grava-se por USB-C da primeira vez e depois atualiza-se a partir do painel.

## 🩺 Resolução de problemas

| Vê | Porque | Faça |
|---|---|---|
| Perdeu a palavra-passe ou o nó já não se liga à rede | Um nó em configuração e sem endereço após 90 s abre o seu próprio Wi-Fi `ARMOR-SETUP-xxxxxx` (abra `http://192.168.4.1/`) | Para recomeçar mantenha BOOT 8 segundos nos primeiros 30 segundos após ligar (apaga definições e utilizadores), ou use *Reposição de fábrica* no painel |
| Uma atualização do GitHub é recusada | A versão não traz soma de verificação nem assinatura, não coincidem (`e_no_checksum`, `e_no_signature`, `e_checksum_mismatch`, `e_bad_signature`), não traz imagem para esta placa (`e_no_asset_for_board`) ou é de outro projeto (`e_wrong_firmware`) | O nó não mudou; tente de novo ou deixe um administrador enviar o ficheiro à mão |
| Uma escrita numa bateria é recusada com um código | `serial_mismatch`, `not_pylontech`, `no_battery`, `unknown_model`, `not_idle`, `critical_alarm`, `busy`, `cooldown`, `locked`, `not_armed`, `bad_token`, `expired`, `disabled`, `image_missing`, `image_mismatch`...: o nó recusa o que não é exatamente a operação armada | Leia o código no resultado, corrija a causa e arme a operação de novo (veja [docs/BATTERY_MAINTENANCE.md](docs/BATTERY_MAINTENANCE.md)) |
| Uma porta não publica nada | A porta está desligada, o seu tipo é `raw` (só escuta), a velocidade ou os pinos não coincidem com o equipamento, ou o equipamento não responde | Verifique as definições da porta e a cablagem em [docs/NODE_HARDWARE.md](docs/NODE_HARDWARE.md); o painel mostra o despejo bruto de uma porta enquanto a observa |

## 📂 Estrutura do repositório

<p align="center">
  <img src="images/repository-structure.svg" alt="Estrutura do repositório de ARMOR-NODE-SOLAR" width="100%">
</p>

<details>
<summary>Versão em texto da árvore</summary>

```text
ARMOR-NODE-SOLAR/
├── main/                                o firmware: um componente ESP-IDF
├── core/                                a lógica sem ESP-IDF, testada num computador
├── panel/                               o painel web embebido no firmware
├── tests/                               testes de anfitrião e verificações Python
├── docs/                                10 guias: protocolos, mensagens, manutenção, hardware
├── tools/                               ferramentas de compilação, empacotamento e verificação
├── .github/                             fluxo de CI e modelos de issues e pull requests
├── images/                              faixa, ícone e as duas imagens deste README
├── build.sh build.bat                   compilar pelo fluxo de projeto partilhado
├── test.sh test.bat                     executar os testes pelo fluxo partilhado
├── run.sh run.bat                       arrancar pelo fluxo de projeto partilhado
├── armor.project.json                   manifesto: versão, função, maturidade, comando de build
├── CHANGELOG.md                         o que mudou em cada versão
├── SECURITY.md SUPPORT.md LICENSE       política de segurança, apoio e licença
├── CONTRIBUTING.md CODE_OF_CONDUCT.md   como contribuir e o código de conduta
├── CMakeLists.txt                       o projeto ESP-IDF
├── partitions.csv                       a tabela da flash
├── sdkconfig.defaults                   as definições do firmware
├── sdkconfig.board.s3-eth               as definições de uma placa
├── sdkconfig.board.s3-wifi              as definições de uma placa
├── secrets/                             exemplo do ficheiro do nó (os verdadeiros não são versionados)
├── build-test.sh build-test.bat         executar os testes pelo fluxo partilhado
└── .gitattributes .gitignore            definições do Git
```

</details>

## 🗺️ Roteiro e limites conhecidos

**Limites conhecidos hoje**

* As tramas, somas de verificação e leituras de Voltronic, Pylontech e ANT-BMS são verificadas contra as tramas documentadas e amostras gravadas; nenhum falou ainda com um inversor ou uma bateria reais.
* O painel web foi exercitado num navegador contra uma simulação do nó, não numa placa.
* Nenhuma placa construída nem inversor ou bateria ligados: nada funcionou ainda em equipamentos reais.
* O código de configuração, o acesso com limite de palavras-passe erradas, o PIN Bluetooth e as atualizações assinadas estão testados no anfitrião; não houve revisão independente.
* As verificações de hash e de assinatura Ed25519 e a lógica de retrocesso passam no anfitrião; uma atualização pelo ar nunca foi executada numa placa.
* A escrita nas baterias Pylontech está desativada por predefinição e só foi testada contra um simulador.

**A seguir**

* Correr o nó numa placa real com um inversor e baterias reais, e depois a ligação Bluetooth do ANT-BMS (hoje usa-se o cabo), a atualização pelo ar e as escritas numa bateria.

## 🌍 Idiomas

O README e os textos existem em inglês, espanhol, francês, italiano, alemão, chinês, japonês e português europeu.

## 🔗 Projetos relacionados

O **A.R.M.O.R.** (Autonomous Radar & Multimodal Observation Range) é um sistema de segurança perimetral feito de repositórios independentes. Cada um tem a sua própria versão, os seus próprios testes e o seu próprio README; esta é a família:

* **[ARMOR-COMMON](https://github.com/JuanenRac/ARMOR-COMMON)** - Contratos de mensagens, validadores, vetores de conformidade e tipos gerados
* **[ARMOR-NODE-RADAR](https://github.com/JuanenRac/ARMOR-NODE-RADAR)** - Firmware de nó de campo para ESP32-S3 com três radares e o seu próprio painel web
* **ARMOR-NODE-SOLAR** (este repositório) - Protocolos de inversores e baterias solares e as mensagens de um nó porta de ligação
* **[ARMOR-NODE-ELECTRICAL](https://github.com/JuanenRac/ARMOR-NODE-ELECTRICAL)** - Nó elétrico: contadores, a mensagem das leituras da rede e as regras de comutação
* **[ARMOR-NODE-ALARM](https://github.com/JuanenRac/ARMOR-NODE-ALARM)** - Nó e painel de alarme: zonas, armar, atrasos, sirene e PIN, com o servidor ou sem ele
* **[ARMOR-NODE-HMI](https://github.com/JuanenRac/ARMOR-NODE-HMI)** - Painel tátil: o estado do sistema num ecrã de parede, armar e reconhecer, e a casa do assistente de voz
* **[ARMOR-NETWORK](https://github.com/JuanenRac/ARMOR-NETWORK)** - A rede local: os seus dispositivos, a internet e o que muda
* **[ARMOR-SERVER](https://github.com/JuanenRac/ARMOR-SERVER)** - Coordenador central: telemetria, alarmes, dispositivos, leituras solares e câmaras
* **[ARMOR-STUDIO](https://github.com/JuanenRac/ARMOR-STUDIO)** - Consola web: câmaras, radar, alarmes, energia solar e o desenhador 2D/3D do local
* **[ARMOR-ANDROID-CONTROL](https://github.com/JuanenRac/ARMOR-ANDROID-CONTROL)** - Cliente de operador Android com radar 2D/3D em direto
* **[ARMOR-IOS-CONTROL](https://github.com/JuanenRac/ARMOR-IOS-CONTROL)** - Cliente de operador para iPhone
* **[ARMOR-SERVER-AI](https://github.com/JuanenRac/ARMOR-SERVER-AI)** - Política de inferência visual que explica as suas decisões e nunca atua
* **[ARMOR-VISION](https://github.com/JuanenRac/ARMOR-VISION)** - Zonas de câmara, máscaras e regras de deteção para o Jetson Orin NX
* **[ARMOR-VOICE-AI](https://github.com/JuanenRac/ARMOR-VOICE-AI)** - Intenções de voz offline com uma confirmação que não pode ser falsificada
* **[ARMOR-HARDWARE](https://github.com/JuanenRac/ARMOR-HARDWARE)** - Caixas, eletrónica e a matriz de aceitação em bancada
* **[ARMOR-DEVOPS](https://github.com/JuanenRac/ARMOR-DEVOPS)** - Implantação, a bancada de testes do CM5, cópias de segurança e TLS
* **[ARMOR-SIMULATOR](https://github.com/JuanenRac/ARMOR-SIMULATOR)** - Simulador de telemetria offline com falhas repetíveis
* **[ARMOR-UPDATER](https://github.com/JuanenRac/ARMOR-UPDATER)** - Deteta, instala e atualiza os repositórios do próprio ecossistema
* **[ARMOR-DOCS](https://github.com/JuanenRac/ARMOR-DOCS)** - Arquitetura, base de segurança e a matriz de capacidades

## 📚 Documentação

* [Firmware do nó](docs/NODE_FIRMWARE.md): a placa, as portas, o primeiro arranque e a lista de verificação da bancada
* [Cablagem](docs/NODE_HARDWARE.md), [protocolos](docs/PROTOCOLS.md) e [mensagens](docs/SOLAR_MESSAGES.md)
* [Manutenção de baterias](docs/BATTERY_MAINTENANCE.md) e o seu [plano de testes de bancada](docs/BATTERY_BENCH_TEST_PLAN.md)
* [Menus do Studio](docs/STUDIO_MENUS.md), [testes de bancada](docs/BENCH_TESTS.md), [código de configuração](docs/SETUP_CODE.md) e [provisionamento Bluetooth](docs/BLE_PROVISIONING.md)
* [Matriz de capacidades: o que está demonstrado e o que não está](https://github.com/JuanenRac/ARMOR-DOCS/blob/main/docs/CAPABILITY_MATRIX.md)
* [Catálogo de projetos: versões e como os repositórios dependem uns dos outros](https://github.com/JuanenRac/ARMOR-DOCS/blob/main/docs/PROJECT_CATALOG.md)
* [Registo de alterações deste repositório](CHANGELOG.md)

## 🤝 Contribuir e comunidade

* Antes de um pull request, execute `ctest --test-dir build/host` and `python -I tools/armor_ci_validate.py`: tem de passar. Consulte [CONTRIBUTING.md](CONTRIBUTING.md) e o [código de conduta](CODE_OF_CONDUCT.md).
* O README existe em oito idiomas e é gerado a partir das fontes do ARMOR-DOCS: uma alteração ao texto faz-se lá.
* Uma alteração a uma mensagem entre componentes começa no ARMOR-COMMON.
* Não descreva como a funcionar numa placa aquilo que só foi compilado.
* Comunique uma vulnerabilidade em privado: [SECURITY.md](SECURITY.md). Perguntas, ideias e relatórios: electrohobby3d@gmail.com ([SUPPORT.md](SUPPORT.md)).

## 👤 AUTOR

**JuanenRac (Electro Hobby 3D)** · electrohobby3d@gmail.com

## 📜 LICENÇA

GPL-3.0-or-later - consulte [LICENSE](LICENSE).
