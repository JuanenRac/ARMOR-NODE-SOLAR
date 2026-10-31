<p align="center">
  <img src="images/ARMOR_BANNER.svg" alt="Bannière d'ARMOR-NODE-SOLAR" width="100%">
</p>

# ☀️ ARMOR-NODE-SOLAR

<p align="center">
  <a href="README.md">🇺🇸 English</a> |
  <a href="README_spa.md">🇪🇸 Español</a> |
  🇫🇷 <b>Français</b> |
  <a href="README_ita.md">🇮🇹 Italiano</a> |
  <a href="README_deu.md">🇩🇪 Deutsch</a> |
  <a href="README_zho.md">🇨🇳 简体中文</a> |
  <a href="README_jpn.md">🇯🇵 日本語</a> |
  <a href="README_por.md">🇵🇹 Português</a>
</p>

### Nœud passerelle solaire : lit onduleurs et batteries par jusqu'à dix ports série (firmware ESP32-S3, son panneau web et la bibliothèque de protocoles)

<p align="center">
  <img src="https://img.shields.io/badge/License-GPL%203.0--or--later-blue.svg" alt="GPL 3.0 ou ultérieure">
  <img src="https://img.shields.io/badge/Language-C%2B%2B17-00599c.svg" alt="Langage">
  <img src="https://img.shields.io/badge/Devices-Voltronic%20%C2%B7%20Pylontech-ffb020.svg" alt="Devices">
  <img src="https://img.shields.io/badge/Tests-11824%20checks-2ea44f.svg" alt="Tests">
  <img src="https://img.shields.io/badge/Version-0.6.1-0f6cbd.svg" alt="Version">
  <img src="https://img.shields.io/badge/Maturity-functional-00E5FF.svg" alt="Maturité">
</p>

---

**Vérification d'honnêteté - ce qui fonctionne aujourd'hui :** Le firmware se compile dans le conteneur ESP-IDF 5.5.5, son cœur (les réglages, l'échange avec chaque type d'équipement, l'arithmétique de l'UART émulée, le canal Bluetooth, les contrôles de version et de signature : 3 013 vérifications dans 10 programmes de test) est testé sur un ordinateur avec des équipements factices, les messages qu'il produit sont acceptés par ARMOR-COMMON et le panneau a été exercé dans un navigateur contre un nœud factice. **Il n'a jamais tourné sur une carte et aucun onduleur ni batterie n'a été connecté** : le Wi-Fi, le panneau sur TLS, la mise à jour, les UART matérielles et émulées et les formats des protocoles (écrits d'après des documents publics et de mémoire) ne sont pas essayés. Les trames ANT-BMS de ses tests ont été capturées par d'autres personnes sur leurs propres unités : ce projet n'en a lu aucune lui-même.

---

## 📋 En un coup d'œil

| | |
|---|---|
| Version | `0.6.1` |
| Maturité | Fonctionnel |
| Rôle | Nœud de terrain solaire pour ESP32-S3 : onduleurs et batteries (famille : Field Nodes, parent : ARMOR-COMMON) |
| Technologie | C++17, ESP-IDF 5.5.5, ESP32-S3 |
| Tests | 11824 vérifications dans 18 programmes hôte et 6 contrôles Python (`ctest`) |
| Équipements | Onduleurs Voltronic / MPP Solar, batteries Pylontech, ANT-BMS ; 10 ports série |
| Fonctionne sur | ESP32-S3-WROOM-1 N16R8 ou Waveshare ESP32-S3-ETH ; tests sur hôte sur n'importe quel ordinateur |
| Sur matériel réel | Aucun pour l'instant : aucune carte montée ni onduleur ou batterie connecté (seulement des tests sur ordinateur, la compilation et le panneau dans un navigateur) |
| Licence | GPL-3.0-or-later |

**Aller à :** [Présentation](#-présentation) · [Architecture](#-architecture) · [Sécurité](#-sécurité-et-protection) · [Interfaces](#-interfaces) · [Configuration](#-configuration) · [Démarrage rapide](#-démarrage-rapide) · [Tests](#-tests) · [Dépannage](#-dépannage) · [Structure](#-structure-du-dépôt) · [Liés](#-projets-liés)

## 🚦 État du projet

Chaque ligne est une partie de ce dépôt et le feu dit où elle en est aujourd'hui. Elle est écrite d'après ce que le dépôt peut montrer (tests, compilations, exécutions), pas d'après ce qui est prévu.

🟢 Fait et vérifié · 🟡 Fonctionne, avec des limites connues · 🟠 À moitié fait ou pas encore vérifié · 🔴 Pas fait ou ne fonctionne pas

| Domaine | État | Ce que cela veut dire ici |
|---|---|---|
| Logiciel | 🟢 | Le firmware compile dans le conteneur ESP-IDF 5.5.5 pour les deux cartes (Wi-Fi et Ethernet). |
| Tests | 🟢 | 11 824 vérifications dans 18 programmes d'hôte réussissent (`ctest`) : les réglages, l'échange avec chaque type d'équipement, l'UART émulée, le canal Bluetooth et les contrôles de version. |
| Protocoles et données | 🟡 | Les trames, sommes de contrôle et lectures de Voltronic, Pylontech et ANT-BMS sont vérifiées par rapport aux trames documentées et à des échantillons enregistrés ; aucun n'a encore parlé à un vrai onduleur ou à une vraie batterie. |
| Interface | 🟡 | Le panneau web a été essayé dans un navigateur contre une simulation du nœud, pas sur une carte. |
| Matériel | 🔴 | Aucune carte construite ni onduleur ou batterie connectés : rien n'a encore tourné sur de vrais équipements. |
| Sécurité | 🟡 | Le code de configuration, la connexion avec une limite de mots de passe erronés, le PIN Bluetooth et les mises à jour signées sont testés sur l'hôte ; il n'y a pas eu de revue indépendante. |
| Mises à jour | 🟠 | Les contrôles de hash et de signature Ed25519 et la logique de retour arrière réussissent sur l'hôte ; une mise à jour par voie hertzienne n'a jamais été exécutée sur une carte. |
| Écriture sur les équipements | 🟠 | L'écriture sur les batteries Pylontech est désactivée par défaut et n'a été testée que contre un simulateur. |
| Intégration | 🟠 | Les messages du nœud sont validés par rapport aux schémas partagés et aux vecteurs de conformité d'ARMOR-COMMON. Son client MQTT est joué contre un vrai Mosquitto dans les tests ; aucune carte ne les a encore envoyés à un serveur. |
| Documentation | 🟢 | Huit langues, le guide du firmware, les notes de câblage et le journal des modifications sont à jour. |

## 🎯 Présentation

* **Deux cartes, un firmware :** une ESP32-S3-WROOM-1 N16R8 en Wi-Fi (par défaut) et la Waveshare ESP32-S3-ETH sur son câble Ethernet (DHCP ou adresse fixe ; son propre réseau Wi-Fi est conservé pour la joindre depuis un téléphone). L'image se choisit à la compilation (`tools/build_node.sh generic s3-wifi` ou `generic s3-eth`) ; la table des broches, les broches par défaut des ports et l'accès suivent la carte, et une image ne vaut que pour sa carte. Les deux compilent et sont testées sur ordinateur ; aucune n'a tourné sur une carte.
* **Le firmware du nœud** (ESP32-S3-WROOM-1 N16R8 en Wi-Fi, ou Waveshare ESP32-S3-ETH sur câble) : dix ports série, trois UART matériels et sept émulés (jusqu'à 19200 bauds), chacun indépendant et lisant un onduleur, une batterie ou un moniteur brut, si bien qu'un nœud peut lire seulement des onduleurs, seulement des batteries ou un mélange.
* **Le panneau web du nœud,** celui des nœuds radar aussi : configuration avec un code affiché sur la console USB, connexion et utilisateurs, Wi-Fi (station et point d'accès), broker, mise à jour par radio avec retour arrière, journal, HTTPS, et les pages des ports (avec ce que chacun entend, pour les protocoles pas encore décodés) et des relevés, en huit langues.
* **Onduleurs Voltronic / MPP Solar** (Axpert, PIP, InfiniSolar et clones), RS232 à 2400 bauds : les trames et leur CRC, et les relevés `QPIGS` (réseau, sortie, batterie, PV, bits d'état), `QMOD` (mode), `QPIWS` (avertissements et pannes par nom) et `QPIRI` (valeurs nominales). Seules les commandes de lecture peuvent être construites : un réglage change la façon dont la maison est alimentée. Trois dialectes, choisis sur le port ou trouvés par lui (*auto*) : PI30, REVO (un autre `QPIGS`, réponses avec somme de contrôle) et PI18 (`^P005GS` : InfiniSolar V, LV5048, SunGoldPower), en lecture seule.
* **Batteries Pylontech** (US2000, US3000, US5000) : le tableau `pwr` de la console (tension, courant, températures et état de charge de chaque module), `bat <n>` (tension et température de chaque cellule) et `info <n>` (modèle, capacité restante et totale, cycles), résumés en une pile avec sa capacité et son énergie totales, et la trame RS485 avec ses deux contrôles. Les formats sont écrits de mémoire de la console publique et peuvent varier selon les firmwares. Les colonnes sont trouvées par les noms de l'en-tête (y compris le format de l'US5000 V2.3 avec ses colonnes Id, MosTempr et SysAlarm.St), la charge restante et l'équilibrage viennent de `bat`, et le modèle, la capacité nominale et les cycles de `info` et `stat`, demandés une fois et gardés une demi-heure.
* **Ce qu'un module Pylontech dit de lui-même :** son modèle et les versions de sa carte et de son logiciel, ses cellules et le courant qu'il admet, les compteurs de sa vie (`stat`), la santé de chaque cellule (`soh`, l'anormale marquée) et l'entrée la plus récente de son journal d'événements, avec les noms des protections tels que la console les imprime. La console ne fait que lire, et un administrateur peut télécharger sous forme de fichier texte ce qu'un port a entendu.
* **Les messages du nœud** (`armor/solar/<nœud>/<appareil>/state`, un par onduleur ou pile de batteries), définis dans ARMOR-COMMON avec schémas et vecteurs de conformité ; ce que produisent les ports est vérifié contre eux.
* **Batteries ANT-BMS** (les cartes noires des packs faits maison, 7S à 32S), UART 3,3 V à 19200 bauds : les deux protocoles de son firmware (le nœud demande dans l'un puis dans l'autre et garde celui qui répond), avec les cellules, les températures, l'état de charge, le courant, les capacités et les états des MOSFET et de l'équilibreur, vérifiés sur des trames capturées sur quatre modèles réels. Lecture seule : ses commandes d'écriture peuvent déconnecter une batterie en charge. Un bouton de la page des ports lit aussi le modèle, la version et les 56 réglages de protection et d'équilibrage du BMS (protocole récent), en lecture seule.
* **Configuration depuis le téléphone en Bluetooth,** le même canal que celui du nœud radar : l'app ARMOR trouve le nœud sous le nom `ARMOR-XXXXXX` et règle son nom, son Wi-Fi, son adresse, son broker et son mode Bluetooth avec les utilisateurs et le code de mise en service du panneau ([le protocole](docs/BLE_PROVISIONING.md)). Il n'écoute que tant que le nœud n'a pas d'utilisateur, sauf réglage contraire. La partie radio n'a jamais tourné sur une carte.
* **La carte de base avec multiplexeurs (le profil *mux*) :** trois UART matériels, chacun derrière un 74HC4052, desservent jusqu'à huit ports en groupes de 4, 2 et 2 qui se relaient sur leur ligne, chaque port avec sa propre vitesse et sa propre polarité, et une LED par port via un 74HC595 ; les cellules d'une grande pile Pylontech peuvent être lues à tour de rôle. Pour un onduleur en dialecte standard, la **seconde entrée photovoltaïque** (`QPIGS2`) et les **unités d'un système en parallèle** (`QPGS`) sont des lectures facultatives. Rien de tout cela n'a tourné sur une carte.
* **Mises à jour par radio signées :** le nœud n'installe depuis GitHub qu'une image dont le SHA-256 correspond et qui porte une signature Ed25519 valide du responsable (la clé publique est dans le firmware, la privée ne quitte jamais le responsable) ; un fichier envoyé à la main par un administrateur dans le panneau reste accepté. Les pages et messages du panneau sont en huit langues.
* **Appairage Bluetooth avec un PIN :** le PIN à six chiffres est dérivé du code de configuration, imprimé sur la console USB et montré à un administrateur dans le panneau ; le téléphone doit le saisir avant de pouvoir écrire les réglages du nœud.
* **Pas encore :** la liaison Bluetooth de l'ANT-BMS (le nœud utilise le câble, le plus stable des deux) et un essai sur une vraie carte avec de vrais équipements.
* **Seule la maison peut utiliser le panneau :** le panneau répond par défaut au réseau privé (ou aux réseaux que son propriétaire indique, comme un VPN), refuse une liste qui exclurait celui qui l'enregistre, compte les mauvais mots de passe par personne et par adresse, et fonctionne derrière le proxy du serveur pour être atteint depuis l'extérieur sans publier le nœud.
* **Inconnu ne veut pas dire aucun :** un onduleur REVO n'a pas de requête d'avertissements, son message porte donc `warnings_known: false` et Studio affiche les avertissements comme inconnus et non comme "aucun".
* **Liaison chiffrée facultative avec le broker (TLS) :** désactivée par défaut, le nœud parle donc MQTT non chiffré comme avant. Un interrupteur et une zone pour le certificat de l'autorité propre à l'installation (PEM, 2 Ko au plus) dans la page Broker lui font se connecter en `mqtts://`, ne faire confiance qu'à cette autorité et vérifier que le certificat du broker a été établi pour l'adresse atteinte ; ce qui n'est pas un seul certificat PEM est refusé. Couvert par des tests sur ordinateur ; la poignée de main n'a pas encore été exécutée sur une carte.
* **Rapport de santé :** environ une fois par minute le nœud publie depuis combien de temps il tourne et pourquoi il a redémarré la dernière fois, sa mémoire libre, sa liaison et son signal Wi-Fi, combien de fois il s'est connecté au broker et la dernière erreur notée (`armor/health/<nœud>`, le contrat `node_health` d'ARMOR-COMMON). Le code qui le construit est partagé par tous les nœuds et testé sur un ordinateur ; il n'a pas tourné sur une carte.
* **Écriture sur les batteries Pylontech (désactivée par défaut) :** uniquement des opérations typées - horloge, compteurs et journaux, redémarrage, capacité, la réinitialisation du niveau de charge, limites et protections, sorties, numéro de série, mémoire externe, firmware par XMODEM-1K et une console expert avec liste de blocage - en deux étapes (préparer, puis exécuter avec un jeton à usage unique), la batterie étant lue avant et après, le mot de passe de service gardé seulement dans le nœud et une console simulée pour les tests. Non essayé sur une vraie batterie : voir le [plan d'essais sur banc](docs/BATTERY_BENCH_TEST_PLAN.md).
* **Commande depuis le serveur :** Studio peut redémarrer le nœud proprement, le mettre en pause jusqu'à une heure (il reprend seul), mettre son travail en veille (sans fin, conservée aux redémarrages, le nœud reste connecté et continue de répondre), le faire se signaler, demander un instantané de diagnostic et le marquer en maintenance (`armor/control/<nœud>/...`, le contrat `node_control` d'ARMOR-COMMON). Chaque ordre se fait en deux temps avec un jeton à usage unique propre à cette opération, seul le serveur peut les écrire sur le broker, et il n'existe ni réinitialisation d'usine, ni effacement, ni sommeil profond. Tant que le nœud est en pause ou en veille, il n'écrit dans aucune batterie (une opération en cours peut encore être annulée) et continue de lire onduleurs et batteries. Le code est partagé par tous les nœuds et testé sur un ordinateur ; il n'a pas tourné sur une carte.
* **Les valeurs absurdes sont nommées, pas cachées :** quand un onduleur envoie un nombre impossible, le nœud le retient à la limite du contrat et le nomme dans `clamped`, si bien que le serveur l'affiche comme non réel ; et le vidage brut d'un port n'est produit que pendant que le panneau le regarde.
* **Diagnostic sans câble :** après un démarrage qui n'était pas propre (plantage, chien de garde, chute d'alimentation, allocation échouée), le rapport de santé en donne la raison pendant dix minutes ; un plantage laisse son vidage mémoire (core dump) à consulter, télécharger ou effacer depuis le panneau ; les réglages enregistrés sont copiés avant chaque installation de micrologiciel et récupérés si une mise à jour les abîme ; et un administrateur peut demander au nœud, via le serveur, un paquet de diagnostic ou son journal avec un jeton à usage unique.
* **Scellement facultatif des secrets enregistrés :** désactivé par défaut (`security.seal_secrets`, ou `nodes.nvs_encryption` du serveur). Activé, les mots de passe, jetons, clés, codes PIN et empreintes de mots de passe que le nœud enregistre sont scellés avec ChaCha20-Poly1305 et tout le reste reste lisible ; l'activer ou le désactiver les réécrit sans rien perdre, même si le nœud s'arrête en route. Aucun eFuse n'est brûlé et la flash n'est pas chiffrée : la clé est dans une partition de la même puce, donc cela ne protège **pas** contre quelqu'un qui tient la carte, et un nœud installé avant cette partition garde ses secrets tels quels jusqu'à ce qu'il soit flashé avec la nouvelle table de partitions.

## 🧩 Rôle dans A.R.M.O.R.

ARMOR-NODE-SOLAR est la passerelle entre le matériel solaire de la maison et la famille. Il lit onduleurs et batteries par jusqu'à dix ports série et publie ce qu'il lit pour [ARMOR-SERVER](https://github.com/JuanenRac/ARMOR-SERVER), qui garde l'historique, les totaux et les alarmes ; les menus *Onduleurs* et *Batteries* d'[ARMOR-STUDIO](https://github.com/JuanenRac/ARMOR-STUDIO) et l'écran Solaire d'[ARMOR-ANDROID-CONTROL](https://github.com/JuanenRac/ARMOR-ANDROID-CONTROL) le montrent. Il ne fait que lire : il n'écrit jamais un réglage dans un équipement.

| Nom | Comment | À quoi ça sert |
|---|---|---|
| ARMOR-SERVER | MQTT `armor/solar/<node>/<device>/state` | Reçoit un message par onduleur ou pile de batteries ; commande la maintenance d'une batterie |
| ARMOR-STUDIO | menus, lien vers le panneau | Montre onduleurs et batteries et ouvre le panneau du nœud |
| ARMOR-ANDROID-CONTROL | Bluetooth | Configure le nœud sur le terrain et montre les écrans d'énergie |
| ARMOR-COMMON | contrat solaire | Les messages sont vérifiés avec ses schémas ; le firmware commun en est copié |

## 🏛️ Architecture

<p align="center">
  <img src="images/architecture.svg" alt="Architecture de ARMOR-NODE-SOLAR : qui l'appelle, ce qu'il contient et de quoi il dépend" width="100%">
</p>

## 🖼️ Captures d'écran

Capturées en 1920×1080 sur un banc d'essai, avec des données d'exemple et sans installation réelle derrière.

<table>
<tr><td width="50%" valign="top"><img src="images/screenshots/overview.png" alt="Résumé : l'état du nœud en un coup d'œil - réseau, broker, firmware et ce qu'il remonte." width="100%"><br><sub>Résumé : l'état du nœud en un coup d'œil - réseau, broker, firmware et ce qu'il remonte.</sub></td><td width="50%" valign="top"><img src="images/screenshots/ports.png" alt="Ports : les ports série du nœud et l'équipement de chacun." width="100%"><br><sub>Ports : les ports série du nœud et l'équipement de chacun.</sub></td></tr>
<tr><td width="50%" valign="top"><img src="images/screenshots/readings.png" alt="Mesures : ce que le nœud lit en ce moment, avec l'âge de chaque valeur." width="100%"><br><sub>Mesures : ce que le nœud lit en ce moment, avec l'âge de chaque valeur.</sub></td><td width="50%" valign="top"><img src="images/screenshots/network.png" alt="Réseau : l'adressage Ethernet ou Wi-Fi du nœud, réglé depuis son propre panneau web." width="100%"><br><sub>Réseau : l'adressage Ethernet ou Wi-Fi du nœud, réglé depuis son propre panneau web.</sub></td></tr>
</table>

## 🔒 Sécurité et protection

| Menace | Défense |
|---|---|
| Mots de passe en clair ou compilés | PBKDF2-HMAC-SHA256 avec 10 000 tours ; aucun mot de passe dans le firmware (un nœud sans utilisateur passe en configuration) ; au plus 4 utilisateurs (admin ou lecteur) et de 8 à 64 caractères ; au plus 6 sessions, 30 minutes d'inactivité |
| Prendre un nœud tout juste flashé | La configuration exige un code de démarrage : celui du build (`CONFIG_ARMOR_SETUP_CODE`) ou un code aléatoire gardé en flash et affiché sur la console USB ; avec un secret de flotte c'est `HMAC-SHA256(secret, MAC)` ; c'est aussi la clé du Wi-Fi de configuration (WPA2, jamais ponté) |
| Le panneau atteint depuis Internet | Il répond par défaut aux réseaux privés d'une maison (10/8, 172.16/12, 192.168/16, 169.254/16, 127/8 et 100.64/10 où vivent certains VPN) ; le réglage `web.admin_sources` liste d'autres adresses et réseaux IPv4 (8 entrées, 120 caractères) ; tout le reste est refusé sauf l'identité publique `GET /api/v1/session` ; une liste qui exclurait celui qui l'enregistre est rejetée |
| Le panneau en clair en HTTP | HTTPS sur le 443 avec un certificat que le nœud fabrique lui-même (autosigné, en flash, empreinte SHA-256 visible pour comparer) ; le panneau peut être http, https ou les deux ; le cookie de session est toujours HttpOnly et SameSite=Strict (et Secure en TLS) |
| Un faux firmware depuis GitHub | Une image n'est installée qu'avec un SHA-256 concordant et une signature Ed25519 valide faite avec la clé du mainteneur (`no_checksum`, `no_signature`, `bad_signature`, `checksum_mismatch`, `wrong_firmware`, `not_newer`, `version_mismatch` la refusent) ; un envoi manuel par un administrateur n'est pas vérifié : une limite explicite |
| Détournement de la configuration Bluetooth | Un PIN à 6 chiffres dérivé du code de configuration, visible sur la console USB et pour un administrateur ; le téléphone doit le saisir avant d'écrire des réglages ; la fenêtre dure `CONFIG_ARMOR_BLE_SETUP_MINUTES` (10 par défaut, de 1 à 120) |
| Un ordre distant qui fait du mal | Deux étapes avec un jeton à usage unique lié à cette opération ; seul le serveur peut écrire ces topics dans le broker ; une pause dure une heure au plus ; il n'y a ni réinitialisation d'usine, ni effacement, ni sommeil profond par ordre distant |
| Secrets stockés lus par qui a la carte | Optionnel (désactivé par défaut, interrupteur `security.seal_secrets` ou `nodes.nvs_encryption` du serveur) : ChaCha20-Poly1305 pour mots de passe, jetons, clés, PIN et hachages. Il ne chiffre pas la flash et ne grille aucun eFuse : la clé est dans une partition de la même puce, il ne protège donc pas contre qui tient la carte |
| Le lien avec le broker en clair | TLS optionnel (`mqtts://`) avec l'autorité propre du propriétaire (PEM jusqu'à 2 Ko) et vérification du nom ; désactivé par défaut. Le handshake n'a tourné qu'en tests sur hôte, pas sur une carte |
| Une batterie écrite par erreur ou par un attaquant | Trois verrous : le réglage du nœud `battery.write_enabled` (administrateurs) avec le mot de passe de service de la batterie, `ARMOR_BATTERY_WRITE=1` du serveur, et la liste d'accès du broker pour `armor/solar/<nœud>/maintenance/command` et `.../result` |
| Un ordre d'écriture répété ou volé | Deux étapes (armer, puis exécuter avec un jeton à usage unique), fenêtre d'armement de 120 s, 30 s de refroidissement entre écritures sur un port, 3 échecs en 10 minutes le bloquent 5 ; le numéro de série de la batterie est ressaisi, la batterie doit être au repos et sans alarme critique, et l'image de firmware est gardée 30 minutes et vérifiée |
| Écrire sur un onduleur ou un ANT-BMS | Les commandes d'écriture d'un onduleur (un réglage change l'alimentation de la maison) et d'un ANT-BMS (il peut déconnecter une batterie sous charge) ne peuvent même pas être construites |
| Des nombres absurdes présentés comme réels | Une valeur impossible est retenue à la limite du contrat et nommée dans `clamped`, pour que le serveur l'affiche comme non réelle ; un onduleur REVO n'a pas de requête d'avertissements, son message dit donc `warnings_known: false` au lieu d'affirmer l'absence d'avertissements |

Aucune trame n'a été lue d'un vrai onduleur ou d'une vraie batterie ; les formats sont écrits d'après des documents publics et de mémoire, et les trames ANT-BMS des tests ont été capturées par d'autres personnes. Lisez [docs/BATTERY_MAINTENANCE.md](docs/BATTERY_MAINTENANCE.md) et [SECURITY.md](SECURITY.md).

## 🔌 Interfaces

| Partie | Ce qu'elle règle |
|---|---|
| MQTT | `armor/solar/<nœud>/<appareil>/state` (un message par onduleur ou pile de batteries, type `inverter` ou `battery` du contrat d'ARMOR-COMMON), `armor/solar/<nœud>/maintenance/{command,result}`, `armor/health/<nœud>` (testament), `armor/control/<id>/{command,result,maintenance}` et `armor/control/all/command` |
| Ports | 10 ports série (3 UART matérielles et 7 émulées, jusqu'à 19200 bauds en émulation) ; par port un type : `voltronic` (2400 bauds, toutes les 5 s), `pylontech` (115200, toutes les 10 s), `ant` (19200, toutes les 5 s) ou `raw` (9600, écoute seule, ne publie rien) ; dialectes d'onduleur `auto`, `pi30`, `revo`, `pi18` ; le profil mux a 3 groupes de ports |
| Requêtes | Voltronic : `QPIGS`, `QMOD`, `QPIWS`, `QPIRI` (et `QPIGS2`, `QPGS` sur un onduleur standard avec le mux) ; console Pylontech : `pwr`, `bat <n>`, `info <n>`, `stat`, `soh`, journal d'événements, jusqu'à 8 modules interrogés cellule par cellule |
| Panneau | Les routes de la famille (`tests/check_routes.py` vérifie que toute route appelée par le panneau existe) plus les pages des ports et des mesures ; 51 codes d'erreur avec texte en huit langues (772 lignes dans `panel/text.js`) ; les menus de Studio sont dans [docs/STUDIO_MENUS.md](docs/STUDIO_MENUS.md) |

## ⚙️ Configuration

| Variable | Par défaut | Ce qu'elle règle |
|---|---|---|
| `enabled`, `kind`, `name`, `baud`, `rx`, `tx`, `de`, `poll_s`, `modules`, `dialect` | - | Les réglages de chaque port : activé ou non, le type, le nom de l'appareil dans le topic, la vitesse (0 = celle du type), les broches (et la broche DE/RE d'un RS485), la période d'interrogation (0 = celle du type), le nombre de modules et le dialecte de l'onduleur ; au plus 3 réseaux de secours et 2 brokers de secours |
| `battery.write_enabled` | off | Le verrou du nœud pour écrire sur une batterie Pylontech (administrateurs, avec le mot de passe de service de la batterie, qui ne quitte jamais le nœud) |
| `CONFIG_ARMOR_BOARD_S3_WIFI` / `_S3_ETH` | wifi | La carte : ESP32-S3-WROOM-1 N16R8 en Wi-Fi (par défaut) ou la Waveshare ESP32-S3-ETH |
| `CONFIG_ARMOR_NODE_ID`, `_SETUP_CODE`, `_FLEET_SECRET`, `_MQTT_*`, `_HEARTBEAT_S`, `_BLE_SETUP_MINUTES`, `_BOOT_FACTORY_RESET` | - | Les réglages que tous les nœuds partagent : le nom (vide donne `armor-` et les derniers octets de la MAC), le code de configuration (vide : aléatoire), le secret de flotte (16+ caractères, jamais dans le dépôt), les premiers réglages du broker, le battement (de 2 à 300 s), les minutes de configuration Bluetooth (de 1 à 120) et la réinitialisation avec BOOT (8 s dans les 30 premières s) |

## 🚀 Démarrage rapide

**Prérequis:** un compilateur C++17 pour les tests sur hôte ; le conteneur ESP-IDF 5.5.5 pour construire l'image ; une ESP32-S3-WROOM-1 N16R8 ou une Waveshare ESP32-S3-ETH.

```bash
cmake -S tests -B build/host && cmake --build build/host && ctest --test-dir build/host   # host tests, -Werror
build/host/emit_poller_samples | python tests/check_samples.py   # what the ports make is accepted by ARMOR-COMMON
tools/build_node.sh generic                       # the image for the N16R8 board in the ESP-IDF container: dist/generic-s3-wifi.bin
tools/build_node.sh generic s3-eth               # the same firmware for the Waveshare ESP32-S3-ETH (Ethernet): dist/generic-s3-eth.bin
node tools/panel_mock.mjs --user admin:adminpass123   # the panel without a board
```

Voir le [guide du firmware](docs/NODE_FIRMWARE.md) (la carte, les ports, le premier démarrage et la liste d'essais sur banc) et les [notes de câblage](docs/NODE_HARDWARE.md).

## 🧪 Tests

```bash
cmake -S tests -B build/host && cmake --build build/host && ctest --test-dir build/host
python -I tools/armor_ci_validate.py
```

18 programmes de test sur hôte et 6 contrôles Python tournent sous `ctest` ; 11824 vérifications comptées par les programmes hôte, compilés avec `-Werror`.

Contrôles Python : les textes du panneau en huit langues, les tâches, les chemins de sauvegarde, la session Bluetooth sous son verrou et les routes appelées par le panneau (et, dans certains nœuds, les codes HTTP ou les glyphes des polices)

Les tests sur hôte demandent un compilateur C++17 quelconque (Linux, WSL, MSYS2) ; le panneau web est aussi exercé dans un vrai navigateur contre un nœud de substitution (`tools/panel_mock.mjs`, `tools/panel_browser_test.mjs`).

`test_clock` n'imprime que `ok` et n'a pas de compteur ; chaque autre programme rapporte ses vérifications. Les vraies trames des tests sont dans `tests/pylontech_real.hpp`, `pylontech_us2000_real.hpp`, `pylontech_logs_real.hpp`, `ant_frames.hpp` et `battery_bench.hpp`.

## 🏗️ Compilation et publication

`tools/build_node.sh` construit l'image dans le conteneur ESP-IDF 5.5.5 et la version porte l'image, son SHA-256 et sa signature Ed25519, une par carte ; l'image est écrite par USB-C la première fois, puis mise à jour depuis le panneau.

## 🩺 Dépannage

| Vous voyez | Parce que | Faites |
|---|---|---|
| Vous avez perdu le mot de passe ou le nœud ne rejoint plus le réseau | Un nœud en configuration sans adresse après 90 s ouvre son propre Wi-Fi `ARMOR-SETUP-xxxxxx` (ouvrez `http://192.168.4.1/`) | Pour recommencer, maintenez BOOT 8 secondes dans les 30 premières secondes après la mise sous tension (efface réglages et utilisateurs), ou utilisez *Réinitialisation d'usine* dans le panneau |
| Une mise à jour depuis GitHub est refusée | La version n'a ni somme de contrôle ni signature, elles ne concordent pas (`e_no_checksum`, `e_no_signature`, `e_checksum_mismatch`, `e_bad_signature`), n'a pas d'image pour cette carte (`e_no_asset_for_board`) ou est d'un autre projet (`e_wrong_firmware`) | Le nœud n'a pas changé ; réessayez ou laissez un administrateur envoyer le fichier à la main |
| Une écriture vers une batterie est refusée avec un code | `serial_mismatch`, `not_pylontech`, `no_battery`, `unknown_model`, `not_idle`, `critical_alarm`, `busy`, `cooldown`, `locked`, `not_armed`, `bad_token`, `expired`, `disabled`, `image_missing`, `image_mismatch`... : le nœud refuse ce qui n'est pas exactement l'opération armée | Lisez le code du résultat, corrigez la cause et armez de nouveau l'opération (voir [docs/BATTERY_MAINTENANCE.md](docs/BATTERY_MAINTENANCE.md)) |
| Un port ne publie rien | Le port est désactivé, son type est `raw` (écoute seule), la vitesse ou les broches ne correspondent pas à l'équipement, ou l'équipement ne répond pas | Vérifiez les réglages du port et le câblage dans [docs/NODE_HARDWARE.md](docs/NODE_HARDWARE.md) ; le panneau montre le vidage brut d'un port pendant qu'il le regarde |

## 📂 Structure du dépôt

<p align="center">
  <img src="images/repository-structure.svg" alt="Structure du dépôt de ARMOR-NODE-SOLAR" width="100%">
</p>

<details>
<summary>Version texte de l'arborescence</summary>

```text
ARMOR-NODE-SOLAR/
├── main/                                le firmware : un composant ESP-IDF
├── core/                                la logique sans ESP-IDF, testée sur ordinateur
├── panel/                               le panneau web intégré au firmware
├── tests/                               tests sur hôte et contrôles Python
├── docs/                                10 guides : protocoles, messages, maintenance, matériel
├── tools/                               outils de build, d'empaquetage et de contrôle
├── .github/                             workflow CI et modèles d'issues et de pull requests
├── images/                              bannière, icône et les deux images de ce README
├── build.sh build.bat                   build par le flux de projet partagé
├── test.sh test.bat                     lancer les tests par le flux partagé
├── run.sh run.bat                       démarrer par le flux de projet partagé
├── armor.project.json                   manifeste : version, rôle, maturité, commande de build
├── CHANGELOG.md                         ce qui a changé à chaque version
├── SECURITY.md SUPPORT.md LICENSE       politique de sécurité, support et licence
├── CONTRIBUTING.md CODE_OF_CONDUCT.md   comment contribuer et le code de conduite
├── CMakeLists.txt                       le projet ESP-IDF
├── partitions.csv                       la table de la flash
├── sdkconfig.defaults                   les réglages du firmware
├── sdkconfig.board.s3-eth               les réglages d'une carte
├── sdkconfig.board.s3-wifi              les réglages d'une carte
├── secrets/                             exemple du fichier de nœud (les vrais ne sont pas versionnés)
├── build-test.sh build-test.bat         lancer les tests par le flux partagé
└── .gitattributes .gitignore            réglages Git
```

</details>

## 🗺️ Feuille de route et limites connues

**Limites connues aujourd'hui**

* Les trames, sommes de contrôle et lectures de Voltronic, Pylontech et ANT-BMS sont vérifiées par rapport aux trames documentées et à des échantillons enregistrés ; aucun n'a encore parlé à un vrai onduleur ou à une vraie batterie.
* Le panneau web a été essayé dans un navigateur contre une simulation du nœud, pas sur une carte.
* Aucune carte construite ni onduleur ou batterie connectés : rien n'a encore tourné sur de vrais équipements.
* Le code de configuration, la connexion avec une limite de mots de passe erronés, le PIN Bluetooth et les mises à jour signées sont testés sur l'hôte ; il n'y a pas eu de revue indépendante.
* Les contrôles de hash et de signature Ed25519 et la logique de retour arrière réussissent sur l'hôte ; une mise à jour par voie hertzienne n'a jamais été exécutée sur une carte.
* L'écriture sur les batteries Pylontech est désactivée par défaut et n'a été testée que contre un simulateur.

**Ensuite**

* Faire tourner le nœud sur une vraie carte avec un vrai onduleur et de vraies batteries, puis le lien Bluetooth de l'ANT-BMS (le câble est utilisé aujourd'hui), la mise à jour par voie aérienne et les écritures vers une batterie.

## 🌍 Langues

Le README et les textes existent en anglais, espagnol, français, italien, allemand, chinois, japonais et portugais européen.

## 🔗 Projets liés

**A.R.M.O.R.** (Autonomous Radar & Multimodal Observation Range) est un système de sécurité périmétrique composé de dépôts indépendants. Chacun a sa propre version, ses propres tests et son propre README ; voici la famille :

* **[ARMOR-COMMON](https://github.com/JuanenRac/ARMOR-COMMON)** - Contrats de messages, validateurs, vecteurs de conformité et types générés
* **[ARMOR-NODE-RADAR](https://github.com/JuanenRac/ARMOR-NODE-RADAR)** - Firmware du nœud de terrain pour ESP32-S3 avec trois radars et son propre panneau web
* **ARMOR-NODE-SOLAR** (ce dépôt) - Protocoles des onduleurs et batteries solaires et messages d'un nœud passerelle
* **[ARMOR-NODE-ELECTRICAL](https://github.com/JuanenRac/ARMOR-NODE-ELECTRICAL)** - Nœud électrique : compteurs, le message des mesures du réseau et les règles de commutation
* **[ARMOR-NODE-ALARM](https://github.com/JuanenRac/ARMOR-NODE-ALARM)** - Nœud et centrale d'alarme : zones, armement, temporisations, sirène et PIN, avec ou sans le serveur
* **[ARMOR-NODE-HMI](https://github.com/JuanenRac/ARMOR-NODE-HMI)** - Panneau tactile : l'état du système sur un écran mural, armer et acquitter, et la maison de l'assistant vocal
* **[ARMOR-NETWORK](https://github.com/JuanenRac/ARMOR-NETWORK)** - Le réseau local : ses appareils, internet et ce qui change
* **[ARMOR-SERVER](https://github.com/JuanenRac/ARMOR-SERVER)** - Coordinateur central : télémétrie, alarmes, appareils, relevés solaires et caméras
* **[ARMOR-STUDIO](https://github.com/JuanenRac/ARMOR-STUDIO)** - Console web : caméras, radar, alarmes, énergie solaire et concepteur de site 2D/3D
* **[ARMOR-ANDROID-CONTROL](https://github.com/JuanenRac/ARMOR-ANDROID-CONTROL)** - Client Android de l'opérateur avec radar 2D/3D en direct
* **[ARMOR-IOS-CONTROL](https://github.com/JuanenRac/ARMOR-IOS-CONTROL)** - Client opérateur iPhone
* **[ARMOR-SERVER-AI](https://github.com/JuanenRac/ARMOR-SERVER-AI)** - Politique d'inférence visuelle qui explique ses décisions et n'agit jamais
* **[ARMOR-VISION](https://github.com/JuanenRac/ARMOR-VISION)** - Zones de caméra, masques et règles de détection pour la Jetson Orin NX
* **[ARMOR-VOICE-AI](https://github.com/JuanenRac/ARMOR-VOICE-AI)** - Intentions vocales hors ligne avec une confirmation impossible à falsifier
* **[ARMOR-HARDWARE](https://github.com/JuanenRac/ARMOR-HARDWARE)** - Boîtiers, électronique et matrice d'acceptation sur banc
* **[ARMOR-DEVOPS](https://github.com/JuanenRac/ARMOR-DEVOPS)** - Déploiement, banc d'essai CM5, sauvegarde et TLS
* **[ARMOR-SIMULATOR](https://github.com/JuanenRac/ARMOR-SIMULATOR)** - Simulateur de télémétrie hors ligne avec des pannes reproductibles
* **[ARMOR-UPDATER](https://github.com/JuanenRac/ARMOR-UPDATER)** - Détecte, installe et met à jour les propres dépôts de l'écosystème
* **[ARMOR-DOCS](https://github.com/JuanenRac/ARMOR-DOCS)** - Architecture, base de sécurité et matrice des capacités

## 📚 Documentation

* [Firmware du nœud](docs/NODE_FIRMWARE.md) : la carte, les ports, le premier démarrage et la liste de contrôle du banc
* [Câblage](docs/NODE_HARDWARE.md), [protocoles](docs/PROTOCOLS.md) et [messages](docs/SOLAR_MESSAGES.md)
* [Maintenance des batteries](docs/BATTERY_MAINTENANCE.md) et son [plan d'essais de banc](docs/BATTERY_BENCH_TEST_PLAN.md)
* [Menus de Studio](docs/STUDIO_MENUS.md), [essais de banc](docs/BENCH_TESTS.md), [code de configuration](docs/SETUP_CODE.md) et [mise en service Bluetooth](docs/BLE_PROVISIONING.md)
* [Matrice des capacités : ce qui est prouvé et ce qui ne l'est pas](https://github.com/JuanenRac/ARMOR-DOCS/blob/main/docs/CAPABILITY_MATRIX.md)
* [Catalogue des projets : versions et dépendances entre les dépôts](https://github.com/JuanenRac/ARMOR-DOCS/blob/main/docs/PROJECT_CATALOG.md)
* [Historique des modifications de ce dépôt](CHANGELOG.md)

## 🤝 Contribuer et communauté

* Avant une pull request, lancez `ctest --test-dir build/host` and `python -I tools/armor_ci_validate.py` : il doit réussir. Voir [CONTRIBUTING.md](CONTRIBUTING.md) et le [code de conduite](CODE_OF_CONDUCT.md).
* Le README existe en huit langues et est généré à partir des sources d'ARMOR-DOCS : une modification du texte se fait là-bas.
* Une modification d'un message entre composants commence dans ARMOR-COMMON.
* Ne décrivez pas comme fonctionnant sur une carte ce qui a seulement été compilé.
* Signalez une vulnérabilité en privé : [SECURITY.md](SECURITY.md). Questions, idées et rapports : electrohobby3d@gmail.com ([SUPPORT.md](SUPPORT.md)).

## 👤 AUTEUR

**JuanenRac (Electro Hobby 3D)** · electrohobby3d@gmail.com

## 📜 LICENCE

GPL-3.0-or-later - voir [LICENSE](LICENSE).
