# Protocole Réseau R-Type (Ultra précis)

Ce document décrit précisément le protocole binaire utilisé entre le client et le serveur pour R-Type.
Il couvre: transport, versioning, en-têtes, types de messages, formats binaires, fréquence, et points d’attention.

## Vue d’ensemble

- Transport: UDP (asio, IPv4)
- Port par défaut serveur: 4242 (modifiable via argument CLI)
- Protocole: binaire minimal, non chiffré, non fiable (pas de retransmission, pas d’ACK applicatifs, latence variable)
- Endianness: little-endian natif (aucune conversion en « network byte order » n’est effectuée)
- Version du protocole: 1 (les paquets avec une autre version sont ignorés côté serveur)
- MTU/fragmentation: pas géré explicitement. Les gros états peuvent être fragmentés par l’IP; pas de réassemblage applicatif.

## Cadence et flux

- Boucle serveur: 60 Hz (tick ~16,66 ms)
- Diffusion des états (MsgType::State): environ à chaque tick à tous les clients connus
- Entrées client (MsgType::Input): libre fréquence; le serveur applique la dernière entrée reçue par joueur

## En-tête commun des messages

Structure C++ (non packée):

- size: uint16, taille du payload (hors en-tête)
- type: MsgType (uint8)
- version: uint8

Taille attendue: 4 octets sur les plateformes visées (aucun padding observé avec l’ABI actuelle). Il n’y a pas de pragma pack ici – serveur et client doivent compiler avec le même ABI pour garantir la même taille/alignement.

Extrait: `src/common/include/common/Protocol.hpp`

```
struct Header {
    std::uint16_t size;   // payload size excluding header
    MsgType type;         // uint8
    std::uint8_t version; // uint8
};
```

- Constantes utiles: `ProtocolVersion = 1`, `HeaderSize = sizeof(Header)`

Note: Le serveur ne vérifie pas que `header->size` correspond exactement à la taille réelle du payload reçu, sauf pour `Input` où il vérifie `payloadSize >= sizeof(InputPacket)`.

## Types de messages (MsgType)

- 1: Hello — handshake d’initialisation du client (payload vide)
- 2: HelloAck — réponse serveur au Hello (payload vide)
- 3: Input — entrées client (payload: `InputPacket`)
- 4: State — état du monde (payload: `StateHeader` + N × `PackedEntity`)
- 5: Spawn — réservé (non utilisé actuellement)
- 6: Despawn — réservé (non utilisé actuellement)
- 7: Ping — réservé (non utilisé actuellement)
- 8: Pong — réservé (non utilisé actuellement)

## Handshake (Hello / HelloAck)

- Client -> Serveur: `Header{ size=0, type=Hello(1), version=1 }`
- Serveur -> Client: `Header{ size=0, type=HelloAck(2), version=1 }`

Effets serveur:
- Associe l’endpoint UDP à un `playerId` unique si nouveau
- Crée une entité `Player` côté serveur (position initiale x=50, y=100+offset, couleur RGBA 0x55AAFFFF)
- Initialise les bits d’entrée à 0 pour ce joueur

Exemple hex (little-endian) pour Hello: `00 00 01 01` (size=0x0000, type=0x01, version=0x01)

## Entrées client (Input)

### Bitmask d’entrées

- Up:   1 << 0 (0x01)
- Down: 1 << 1 (0x02)
- Left: 1 << 2 (0x04)
- Right:1 << 3 (0x08)
- Shoot:1 << 4 (0x10) — non traité actuellement par le serveur

### Payload: InputPacket (packé 1 octet)

```
#pragma pack(push, 1)
struct InputPacket {
    std::uint32_t sequence; // id croissant côté client (actuellement ignoré par le serveur)
    std::uint8_t bits;      // combinaison des bits d’entrée
};
#pragma pack(pop)
```

- Taille: 5 octets
- Endianness: `sequence` en little-endian (natif)
- Usage serveur: ignore `sequence`, lit `bits` et met à jour `playerInputBits_[playerId]`

Exemple de trame Input (sequence=42, Right+Shoot):
- Header: size=5 -> `05 00`, type=Input(3) -> `03`, version=1 -> `01`
- Payload: sequence=42 -> `2A 00 00 00`, bits=0x18 -> `18`
- Hex total: `05 00 03 01 2A 00 00 00 18`

### Application côté serveur

À chaque tick:
- Pour chaque entité `Player`, lit les bits courants
- Vitesse: 150 px/s; calcule vx/vy selon bits; met à jour x/y avec dt=1/60

## État du monde (State)

### Types d’entités

- Player (1)
- Enemy (2)
- Bullet (3) — non émis actuellement

### Payload: StateHeader + N × PackedEntity (packé 1 octet)

```
#pragma pack(push, 1)
struct StateHeader { std::uint16_t count; };

struct PackedEntity {
    std::uint32_t id;      // identifiant unique
    EntityType type;       // uint8 (1=Player,2=Enemy,3=Bullet)
    float x; float y;      // position
    float vx; float vy;    // vitesse
    std::uint32_t rgba;    // 0xRRGGBBAA
};
#pragma pack(pop)
```

- Taille `StateHeader`: 2 octets
- Taille `PackedEntity`: 25 octets (avec pack(1))
- Le serveur calcule: `payloadSize = 2 + count * 25`
- `count` limité à `min(entities.size, 512)` côté serveur
- Diffusion: à tous les endpoints connus, à ~60 Hz

Remarque MTU: Avec 512 entités, le payload ~12 802 octets; `Header`=4; total ~12 806 octets — susceptible d’être fragmenté UDP.

## Spawns ennemis et logique minimale serveur

- Spawn d’un ennemi toutes ~2 s à x=900 (y aléatoire 20..500), vx=-60
- Suppression des ennemis quand x < -50
- Les joueurs se déplacent via entrées; pas de tirs ni collisions implémentés dans ce protocole

## Représentation binaire et endianness

- Aucun `hton*/ntoh*` n’est utilisé: tous les entiers et floats sont envoyés en représentation machine (little-endian sur les plateformes cibles)
- Floats: IEEE-754 32 bits en little-endian
- Implication: le client doit être compilé/éxécuté sur une archi little-endian compatible; pour portabilité réseau stricte, il faudrait introduire une sérialisation déterministe

## Réception et validations serveur

Dans `UdpServer::handlePacket`:
- Vérifie: taille >= HeaderSize
- Vérifie: `header->version == ProtocolVersion`
- Route selon `header->type`
- Hello: enregistre endpoint, crée player si nécessaire, renvoie HelloAck (Header seul, size=0)
- Input: si `payloadSize >= sizeof(InputPacket)` alors met à jour `playerInputBits_`
- Autres types: ignorés (pas de log d’erreur)

Points à noter:
- `header->size` n’est pas strictement validé vs `payloadSize`
- Aucun anti-flood/ratelimit
- Aucune authentification ni chiffrement

## Exemples de séquences

1) Connexion client
- Client envoie: Hello
- Serveur répond: HelloAck
- Serveur commence à inclure le joueur dans les `State`

2) Jeu en cours
- Client envoie régulièrement `Input` (par exemple à 60 Hz ou à chaque changement)
- Serveur diffuse `State` (~60 Hz) à tous les clients

## Contrat de messages (résumé)

- Entrée -> Serveur
  - Header(type=Input, version=1, size=5) + InputPacket
  - Erreurs: version != 1 ignorée; payload < 5 ignoré

- État <- Serveur
  - Header(type=State, version=1, size=2+25*N) + StateHeader(count=N) + N*PackedEntity
  - N ≤ 512 (côté serveur)

- Handshake
  - Hello/HelloAck: Header seul, size=0

## Recommandations client (impl.)

- Toujours vérifier:
  - Taille datagramme >= HeaderSize
  - `header.version == 1`
  - Optionnel: `header.size == payloadReçu`
- Parser `State` avec attention aux tailles: `payload >= 2 + 25*N`
- Gérer perte/duplication de paquets (UDP): appliquer « dernier état reçu »; éventuellement interpoler/extrapoler côté rendu
- Limiter la fréquence d’envoi `Input` (ex. 30–60 Hz ou sur changement)

## Extension et champs réservés

- MsgType::Spawn / ::Despawn: prévus pour delta d’état; non implémentés
- MsgType::Ping / ::Pong: prévus pour RTT/keep-alive; non implémentés
- `InputPacket.sequence`: prévu pour désambigüiser l’ordre; non utilisé actuellement

## Sécurité et robustesse

- Aucun contrôle d’origine: tout endpoint pouvant envoyer Hello reçoit l’état
- Risques: spoofing, flood, payload malformé
- Durcissements suggérés:
  - Vérifier `header.size`
  - Ignorer/blacklister endpoints bruyants
  - Heartbeat (Ping/Pong) et timeout d’inactivité
  - Sérialisation explicite (endianness fixe) et checksums optionnels

## Annexes: tailles et constantes

- Header: 4 octets
- InputPacket: 5 octets
- StateHeader: 2 octets
- PackedEntity: 25 octets
- Max entités par `State`: 512
- Buffer réception serveur: 2048 octets (pour la réception d’Input/Hello)

## Références code

- Définition protocole: `src/common/include/common/Protocol.hpp`
- Serveur (envoi/réception): `src/server/UdpServer.cpp`, `src/server/UdpServer.hpp`
- Lancement serveur (port par défaut 4242): `src/server/main.cpp`
