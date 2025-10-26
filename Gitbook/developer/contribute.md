---
icon: git-pull-request
---

# Contributing

We follow Conventional Commits and structured branch names.

## Commit messages

Format:
```
state(scope): description
```
Examples:
```
feat(server): add login endpoint
fix(client): resolve crash on startup
refactor(common): improve protocol parsing
```
States: feat, fix, refactor, docs, style, test, chore, perf, ci, build, revert

## Branch names

Format:
```
Part/State/Description
```
Examples:
- `Server/Feature/Implement-handshake`
- `Client/Fix/Crash-on-startup`
- `Common/Refactor/Protocol-parsing`
- `Engine/Test/Add-registry-tests`

Guidelines:
- Part in PascalCase (Server, Client, Common, Engine)
- State capitalized (Feature, Fix, Refactor, ...)
- Short Description (Title Case, dash-separated)

Thank you for contributing!
