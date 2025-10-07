# FAQ

## Comment construire le projet ?
Voir `docs/setup.md` et utilisez `make build`. Conan téléchargera raylib et asio automatiquement.

## Comment lancer le serveur et le client ?
`make run-server` (port 4242) puis `make run-client`. Dans le client, renseignez 127.0.0.1:4242 en Multijoueur.

## Le client ne reçoit pas d’état, que faire ?
- Vérifiez que la version du protocole est la même (voir `Protocol.hpp`, version=1)
- Désactivez le firewall local ou autorisez l’UDP sur le port 4242
- Assurez-vous que le client a bien envoyé Hello et reçu HelloAck (voir logs client)

## Pourquoi UDP ?
Faible latence et tolérance aux pertes légères. La fiabilité peut être ajoutée au-dessus si nécessaire.

## Le tir (Space) ne fait rien ?
Le bit `InputShoot` est envoyé, mais le serveur n’a pas encore de logique de projectile. À venir.

## Je vois des ennemis « sauter » ou disparaître
Le serveur supprime les ennemis hors écran et diffuse des instantanés d’état. De petites saccades sont normales sans interpolation.

## Où est le protocole détaillé ?
`docs/protocol.md` (messages, tailles, endianness, etc.).

## Où est l’ECS et comment l’utiliser ?
`src/engine/rt/ecs/*`. Le `Registry` permet de créer des entités et gérer des composants. Les systèmes exécutent une logique dans `update(dt)`.

## Comment changer le port serveur ?
Lancez `r-type_server <port>` ou modifiez la commande `run-server` dans le Makefile. Mettez à jour le port côté client.

## Windows/macOS sont-ils supportés ?
Le projet est portable avec Conan + CMake. Raylib et asio sont cross-platform. Tests surtout sur Linux/macOS.