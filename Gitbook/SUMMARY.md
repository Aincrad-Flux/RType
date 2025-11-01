# Table of contents

* [Welcome](README.md)

## Get started
* [Overview](get-started/overview.md)
* [Setup](get-started/setup.md)
* [Usage](get-started/usage.md)
* [FAQ](get-started/faq.md)
* [Troubleshooting](get-started/troubleshooting.md)
* [Changelog](get-started/changelog.md)
* [API Documentation](get-started/api.md)

## Engine
* [Overview](engine/overview.md)
* [ECS Graph](engine/ecs-graph.md)
* [Components](engine/components.md)
* [Systems](engine/systems.md)
* [Registry](engine/registry.md)
* [Runtime integration](engine/runtime-integration.md)

## Protocol
* [Overview](protocol/00-overview.md)
* [Message Header](protocol/01-header.md)
* [Transport Layer](protocol/02-transport.md)
* [Data Structures](protocol/03-data-structures.md)
* [TCP Messages](protocol/tcp-messages.md)
  * [TcpWelcome](protocol/tcp-100-welcome.md)
  * [StartGame](protocol/tcp-101-start-game.md)
* [UDP Messages - Connection](protocol/udp-connection.md)
  * [Hello](protocol/udp-01-hello.md)
  * [HelloAck](protocol/udp-02-hello-ack.md)
* [UDP Messages - Gameplay](protocol/udp-gameplay.md)
  * [Input](protocol/udp-03-input.md)
  * [State](protocol/udp-04-state.md)
* [UDP Messages - Entity Management](protocol/udp-entities.md)
  * [Spawn](protocol/udp-05-spawn.md)
  * [Despawn](protocol/udp-06-despawn.md)
* [UDP Messages - Player Management](protocol/udp-players.md)
  * [Roster](protocol/udp-09-roster.md)
  * [LivesUpdate](protocol/udp-10-lives-update.md)
  * [ScoreUpdate](protocol/udp-11-score-update.md)
  * [Disconnect](protocol/udp-12-disconnect.md)
* [UDP Messages - Session Control](protocol/udp-session.md)
  * [ReturnToMenu](protocol/udp-13-return-to-menu.md)

## Technical and Comparative Study
* [Overview](technical-study/README.md)
* [Algorithms and Data Structures](technical-study/algorithms-data-structures.md)
* [Storage](technical-study/storage.md)
* [Security](technical-study/security.md)
* [Accessibility](technical-study/accessibility.md)

## Developer
* [Contributing](developer/contribute.md)
* [Pull requests](developer/pull-requests.md)
* [Architecture](developer/architecture.md)
* [Client internals](developer/client-internals.md)
* [Server internals](developer/server-internals.md)
* [CI/CD](developer/ci-cd.md)
* [Quality & tooling](developer/quality-tooling.md)
* [Build & dependencies](developer/build-dependencies.md)
