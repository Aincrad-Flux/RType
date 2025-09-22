# Contributing Guidelines

This document describes the standards for commit messages and branch naming conventions for this repository. Please follow these rules to ensure consistency and clarity in our development workflow.

## Commit Message Convention

We use the Conventional Commit format:

```
state(scope): description
```

- **state**: The type of change (see table below)
- **scope**: The part of the codebase affected (optional, but recommended)
- **description**: A short summary of the change

### Example
```
feat(server): add login endpoint
fix(client): resolve crash on startup
refactor(common): improve protocol parsing
```

### Allowed States

| State      | Meaning                                                      |
|------------|--------------------------------------------------------------|
| feat       | A new feature                                                |
| fix        | A bug fix                                                    |
| refactor   | Code change that neither fixes a bug nor adds a feature      |
| docs       | Documentation only changes                                   |
| style      | Changes that do not affect meaning (formatting, missing semi) |
| test       | Adding or correcting tests                                   |
| chore      | Maintenance tasks (build, dependencies, etc.)                |
| perf       | Performance improvements                                     |
| ci         | Changes to CI/CD configuration                               |
| build      | Changes that affect the build system or external dependencies |
| revert     | Reverts a previous commit                                    |

## Branch Naming Convention

Branches must follow the folder-style pattern:

```
Part/State/Description
```

- **Part**: The main module or area (e.g., Server, Client, Common, Engine)
- **State**: The type of change (see commit states above)
- **Description**: Short, descriptive summary (use dashes to separate words)

### Example Branch Names

- `Server/Feature/Create-login`
- `Client/Fix/Crash-on-startup`
- `Common/Refactor/Protocol-parsing`
- `Engine/Test/Add-registry-tests`

### Guidelines
- Use PascalCase for Part (e.g., Server, Client)
- Use a capitalized State (Feature, Fix, Refactor, etc.)
- Use a short, clear Description (capitalize first letter, use dashes)

## Summary

- Always use the specified commit and branch conventions
- Keep messages and branch names clear and concise
- Refer to the tables and examples above for guidance

Thank you for contributing!
