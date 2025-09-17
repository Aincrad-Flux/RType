# R Type

## Preliminaries
**binary**: r-type_server, r-type_client
**language**: C++
**build**: CMake, package manager

This project of the Advanced C++ knowledge unit will introduce you to networked video game development, and will give you the opportunity to explore advanced development techniques as well as to learn
good software engineering practices.
The goal is to implement a multi-threaded server and a graphical client for a well-known legacy video game called R-Type, using a game engine of your own design.
First, you will develop the core architecture of the game and deliver a working prototype, and in a second
time, you will expand several aspects the

### R-Type, the Game
This game is informally called a Horizontal Shmup (or simply, a Shoot'em'up), and while R-Type is not the first one of its category, it has been a huge success amongst gamers in the 90's, and had several ports, spin-offs, and 3D remakes on modern systems. Other similar and well-known games are the Gradius series and Blazing Star on Neo Geo. In this project, you have to make your own version of R-Type, with additional requirements not featured in the original game:
 - It **MUST** be a networked game, where several players will be able to fight the evil Bydo.
 - Its internal design **MUST** demonstrate architectural features of a real game engine.

### Project Organization
This project is split in two parts, each part leading to a Delivery and evaluated in a dedicated Defense. Additionally, there is also a common part that will be evaluated at both defenses.

This document is structured according to the following plan :
- **Common part: Software Engineering, Documentation, and Accessibility Requirements**

This part defines the expectations in terms of Software Engineering, Documentation, and Accessibility your project must have. Topics such as technical documentation, build system tooling, 3rd-party dependencies handling, development workflow, and packaging will be addressed.

This part have to be a continuous effort, and not something done at the very end of the project. As such, each project defense will take into account the work that have been done on this topic.

 - Part 1: Software Architecture & First Game prototype

The goal of the first part is to develop the core foundations and software architecture of your networked game engine, allowing you to create and deliver a first working game prototype.

The deadline for this first delivery and defense is 4 weeks after the beginning of the project.

 - Part 2: Advanced Topics : from game prototype to infinity and beyond!

The goal of this second part is to enhance different aspects of your prototype, bringing your final delivery to a more mature level. There is 3 technical tracks you may want to work on: Advanced Software Architecture, Advanced Networking, and/or Advanced Gameplay and Game Design.

You will have opportunity choose which topics you want to work on, finally leading you to the final delivery
of the project.

The deadline for this final delivery and defense is 3 weeks after the first delivery, for a total of 7 full weeks for the whole project

## Software Engineering Requirements

> The project **MUST** use CMake as its build system.
> The project **MUST** use a Package Manager to handle 3rd-party dependencies.

The package manager can be one of:
- Conan
- Vcpkg
- CMake CPM

The goal is to have the project fully self-contained: it has to be built and run **without altering anything on the system. In other terms, the project must not rely on system-wide installed libraries or development headers , except for the standard C++ compilers and SDKs, and some low-level and platform-specific libraries (such as OpenGL or X11 libraries for example).

> [WARNING] Copying the full dependencies source code straight into your repository is NOT considered to be a proper method of handling dependencies !

- The project **MUST** run on Linux, for both the client and the server.
- The project SHOULD be Cross-Platform.

In addition to Linux, it has to run on Windows using Microsoft Visual C++ compiler.

> [WARNING] Neither MacOS nor WSL count as Cross-Platform as they are both UNIX-like systems or environments.

A true cross-platform game allows to run the server and the client on both Windows and Linux, and ”cross-play” between clients of different OSes.

It should be noted that deciding to implement cross-platform have major impacts on development, and this is something to be tackled early on to succeed.

- A well-defined software development workflow **MUST** be used

You are expected to use good development practices, and in particular, you have to adopt good Git usage and practices: feature branches, merge requests, issues, tags for important milestones, commits content and description, etc.

It is even better if a CI/CD workflow is used, to build, test, and even deploy the server.

> [WARNING] **DO NOT** put a full-time dedicated member on the CI/CD, as this is a very time-consuming task for this project.
> **DO NOT** fetch the full dependencies after each commit, otherwise you'll break the CI/CD quota very soon. There is methods to handle this in more clever ways (caches, preconfigured build-images, etc.).

Another aspect to consider is the usage of specific tools such as C++ linters or formatters (for example, clang-tidy and clang-format), as they help to spot common programming errors, and enforce a common style.

## Documentation Requirements

Documentation is not your preferred task, we all know it ! However, documentation is also the first thing you'll want to look for if you needed to dive into a new project.

The idea is to provide the essential documentation elements that you'd be happy to see if you wanted to contribute yourself to a project for a new team.

> [WARNING] You **MUST** write the documentation in English.

This includes:
- First and foremost, the **README** file !

This is the first thing every developer will see: this is your project public facade. So, better to have it done properly.

It has to be short, nice, practical, and useful, with typical information such as project purpose, dependencies/requirements/supported platforms, build and usage instructions, license, authors/contacts, useful links or quick-start information, and so on.

- The Developer Documentation

This is the part you don't like. But think about it: its main purpose is to help new developers to dive in the project and understand how it works in a broad way (and not in the tiny details, the code is here for this).

No need to be exhaustive or verbose, it has to be practical before anything else.

The following kind of information are typically what you'll need:
- Architectural diagrams (a typical ”layer/subsystem” view common in video games)
- Main systems overviews and description, and how this materializes in the code
- Tutorials and How-To's.

Contribution guidelines and coding conventions are very useful too.

They allow new developers to know about your team conventions, processes and expectations.

Having a good developer documentation will demonstrate to the evaluator that you have a good understanding of your project, as well as the capacity to communicate well with other developers.

> [WARNING] Note that generating documentation from source code comments like with the Doxygen tool, while it is a good practice, can't be considered **alone** to be a real project documentation. You **MUST** produce documentation at a **higher level** than only just classes/functions descriptions !

- The Technical and Comparative Study

You are expected to explain the relevance of using the various technologies you are employing (programming language, graphics library, algorithms, networking techniques, etc...).

A good approach is to conduct a comparative study of the technologies to justify each of your choices, inparticular based on the different axes below:

**Algorithms and Data Structures**: you should be able to explain your selection of existing algorithms, design patterns, and data structures for the project, as well as your selection of new and custom-designed algorithms, and why you needed to develop those.
**Storage**: a study of different storage techniques should be included in your comparative study, regarding persistence, reliability, and storage constraints.
**Security**: security and data integrity must be managed and secured effectively. In your comparative study, it might be relevant to consider the main vulnerabilities of each technology. Also, explain how you implemented the security monitoring of those technologies, in the long term.

- The documentation have to be available and accessible in a modern way.

Documents such as PDF or .docx are not really how documentation is delivered nowadays. It is more practical to read documentation by navigating online through a set of properly interlinked structured pages, with a quick-access outline somewhere, a useful search bar, and content indexed by search engines.

Documentation generator tools are designed for this, allowing to generate a static website from source documentation files. Online Wikis are also an interesting alternative geared toward collaborative work.

> [INFO] Examples: markdown, reStructuredText, Sphinx, Gitbook, Doxygen, Wikis, etc. There is many possibilities nowadays, making legacy documents definitively obsolete.

- Protocol documentation

This project is a network game: as such, the communication protocol is a critical part of the system. Documentation of the network protocol shall describe the various commands and packets sent over the network between the server and the client. Someone SHOULD be able to write a new client for your server, just by reading the protocol documentation.

> [INFO] Communication protocols are usually more formal than usual developer documentation, and classical documents are acceptable for this purpose. Writing an RFC is a good idea.

## Accessibility Requirements
The project, including its documentation, must be accessible to people with disabilities. Here are the usual disability categories you have to think about (non-exhaustive list):

- Physical and Motor Disabilities
- Audio and Visual Disabilities
- Mental and Cognitive Disabilities

For each of these categories, what solution are you providing ? Try to draw inspiration from existing features and settings in actual games, there is many ways to handle this. You may also integrate these aspects into your comparative study.

---

## Part 1: Software Architecture & First Game Prototype
The first part of the project focus on building the core foundations of your game engine, and develop your first R-Type prototype.

The general goals are the following:
- The game **MUST** be playable at the end of this part: with a nice star-field in background, players spaceships confront waves of enemy Bydos, everyone shooting missiles to try to get down the op-
ponent.
- The game **MUST** be a networked game: each player use a distinct Client program on the network, connecting to a central Server having final authority on what is happening in the game.
- The game **MUST** demonstrate the foundations of a game engine, with at least visible and decoupled subsystems/layers for Rendering, Networking, and Game Logic.

### Server :
The server implements all the game logic. It acts as the authoritative source of game logic events in the game: whatever the server decides, the clients have to comply with it.

> [WARNING] On a typical and simplified client/server video game architecture, the Clients sends local user inputs to the Server, which processes them and update the game world, and send back regular game updates to all Clients. In turn the Clients renders the updated game world on the screen.
> There is however many variations around this basic principle, and you have to design your solution.

- The server **MUST** notify each client when a monster spawns, moves, is destroyed, fires, kills a player, and so on..., as well as notifies others clients' actions (a player moves, shoots, etc.).
- The server **MUST** be multithreaded, so that it does not block or wait for clients messages, as the game must run frame after frame on the server regardless of clients' actions.
- If a client crashes for any reason, the server **MUST** continue to work and **MUST** notify other clients in the same game that a client crashed. More generally, the server must be robust and be able to run regardless of what's wrong might happen.
- You MAY use the Asio library for networking, or rely on OS-specific network API (eg. Unix BSD Sockets on Linux for example, or Windows Sockets on Windows).

> [WARNING] In case you decide to use low level sockets provided by the system, you **MUST** encapsulate them with proper abstractions !

### Client :
The client is the graphical display of the game.

It **MUST** contain anything necessary to display the game and handle player input, while everything related to gameplay logic shall occur on the server.

The client **MAY** nevertheless run parts of the game logic code, such as local player movement or missile movements, but in any case the server **MUST** have authority on what happens in the end. This is particularly true for any kind of effect that have a great impact on gameplay (death of an enemy/player, pickup of an item, etc.): all players have to play the same game, whose rules are driven by the server.

You may use the *SFML* for rendering/audio/input/network, but other libraries can be used (such as SDL or Raylib for example). However, libraries with a too broad scope, or existing game engines (UE, Unity, Godot, etc.) are strictly forbidden.

### Protocol :
You **MUST** design a **binary protocol** for client/server communications.

A binary protocol, in contrast with a text protocol, is a protocol where all data is transmitted in binary format, either as-is from memory (raw data) or with some specific encoding optimized for data transmission.

> [WARNING] The protocol **MUST** be binary. Please read on the internet for differences between binary and text protocols.

You **MUST** use UDP for communications between the server and the clients. A second connection using TCP can be tolerated for specific cases, but you must provide a strong justification. In any case, ALL in-game communications (entities, movements, events) **MUST** use UDP.

> [WARNING] UDP works differently than TCP, be sure to understand well the difference between datagram oriented communication VS stream-oriented communication.

Think about your protocol completeness, and in particular, the handling of erroneous messages, or buffer overflows. Such malformed messages or packets **MUST NOT** lead the client or server to crash, consume excessive memory, or compromise server security.

You **MUST** document your protocol. See the section on documentation for more information about what is expected for the protocol documentation.

### Game Engine :
You've now been experimenting with C++ and Object-Oriented Design for a year. That experience means you should be able to create **abstractions** and write **re-usable code**.

Therefore, before you begin work on your game, it is important that you start by creating a prototype **game engine !**

The game engine is the core foundation of any video game: it determines how you represent an object in-game, how the coordinate system works, and how the various systems of your game (graphics, physics, network...) communicate.

When designing your game engine, **decoupling** is the most important thing you should focus on. The graphics system of your game only needs an entity appearance and position to render it: it doesn't need to know about how much damage it can deal or the speed at which it can move ! Think of the best ways to decouple the various systems in your engine.

> [INFO] We recommend taking a look at the Entity-Component-System architectural pattern, as well as the Mediator design pattern. But there are many other ways to implement a game engine, from common Object-Oriented paradigms to full Data Driven ones. Be sure to look for articles on the Internet.

### Gameplay :
The client **MUST** display a slow horizontal scrolling background representing space with stars, planets... This is the star-field.

The star-field scrolling, entities behavior, and overall time flow must NOT be tied to the CPU speed. Instead, you **MUST** use timers.

Players **MUST** be able to move using the arrow keys.

There **MUST** be Bydos slaves in your game, as well as missiles.

Monsters **MUST** be able to spawn randomly on the right of the screen.

The four players in a game **MUST** be distinctly identifiable (via color, sprite, etc.)

**R-Type** sprites are freely available on the Internet, but a set of sprites is available with this subject.

Finally, think about basic sound design in your game. This is important for a good gameplay experience.

This is the minimum, you can add anything you feel will get your game closer to the original.