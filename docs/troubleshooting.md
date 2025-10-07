# Troubleshooting

## Conan/CMake errors

Symptom: `conan_toolchain.cmake not found`.
- Cause: Setup not executed.
- Fix: `make setup` (ou `sudo make setup-sudo`). Ensuite `make build`.

Symptom: Échec d’installation de paquets systèmes.
- Cause: Permissions requises.
- Fix: `sudo make setup-sudo`.

## Build C++ errors

Symptom: erreurs C++20 ou headers manquants.
- Vérifiez la version du compilateur (`g++ --version`/`clang --version`).
- Nettoyez et reconstruisez: `make re`.

## Raylib/display

Symptôme: fenêtre n’apparaît pas ou crash.
- Linux: assurez une session graphique (X11/Wayland). Installez X11 dev si nécessaire (Conan peut tenter de l’installer).
- SSH: utilisez X11 forwarding ou lancez en local.

## Réseau UDP

Symptôme: pas de HelloAck / pas d’entités.
- Vérifiez que le serveur tourne et écoute sur le bon port.
- Désactivez temporairement le firewall local ou ouvrez le port UDP 4242.
- Vérifiez l’adresse saisie dans le client (127.0.0.1 pour local).

Symptôme: saccades/retards.
- Nature de l’UDP; la latence et les pertes peuvent provoquer des sauts. Implémenter interpolation client ou réduction de payload pour améliorer.

## Version du protocole

Symptôme: paquets ignorés.
- Serveur et client doivent utiliser `ProtocolVersion = 1`. Si vous modifiez `Protocol.hpp`, rebuild des deux.

## Logs utiles

- Serveur: affiche port/IP au démarrage; ajoutez des logs dans `UdpServer::handlePacket` pour diagnostiquer.
- Client: `Screens::logMessage` imprime les étapes de connexion et erreurs receive.