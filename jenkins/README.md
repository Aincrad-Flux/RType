# Pipelines Jenkins RType

Ce dossier contient plusieurs pipelines Jenkins spécialisés pour le projet RType.
Chaque Jenkinsfile est autonome. Le pipeline `Jenkinsfile.full` enchaîne toutes les étapes clés.

## Pré-requis agents Jenkins
- Outils installés sur l'agent: `git`, `cmake`, `conan`, `jq` (pour analyse), `tar`, `bash`.
- (Optionnel) Outils d'analyse: `cppcheck`, `clang-tidy`.
- Credentials Jenkins: `github-https-token` (type: Username + Personal Access Token) utilisés pour le checkout.

## Paramètres communs
- `SOURCE_REPO`: URL du repository Git (défaut: repo actuel).
- `BRANCH`: Branche à utiliser (défaut: `main`).
- `BUILD_TYPE`: `Debug` ou `Release` selon le besoin.

## Fichiers

### 1. `Jenkinsfile.dependencies`
Installe uniquement les dépendances Conan et prépare un cache local (`build/`).
Artefacts: fichiers conan générés.

### 2. `Jenkinsfile.build`
Chaîne complète: Conan install + CMake configure + Build. Paramètre optionnel pour lancer des tests (si un CTest existe plus tard).
Artefacts: dossier `build/`.

### 3. `Jenkinsfile.analysis`
Génère une base de compilation (compile_commands.json) puis exécute:
- `cppcheck` (warnings, performance, style...)
- `clang-tidy` (limité aux ~50 premiers fichiers trouvés pour éviter une surcharge)
Rapports dans `analysis-reports/`.

### 4. `Jenkinsfile.package`
Build + empaquetage binaire léger dans une archive `*.tar.gz` contenant:
- `bin/` (binaries détectés exécutables)
- `docs/` (README, subject)
- `BUILD_INFO`

### 5. `Jenkinsfile.full`
Pipeline complet CI:
1. Dépendances
2. Configuration
3. Build
4. Tests (optionnel)
5. Analyse statique (optionnelle)
6. Packaging
Artefacts: build, rapports, archive.

## Exemple de multibranch
Dans un job Multibranch, vous pouvez placer un Jenkinsfile à la racine si vous voulez centraliser. Ici la granularité a été déplacée dans `jenkins/`.

## Bonnes pratiques
- Activer un cache Conan global via un volume ou un workspace persistant pour accélérer.
- Ajouter plus tard un job nightly sur `Jenkinsfile.full` en `Release` + analyse.
- Intégrer éventuellement un upload (Artifactory, Nexus) dans `Jenkinsfile.package`.

## Étapes futures suggérées
- Ajout d'une étape de tests unitaires (quand tests disponibles).
- Ajout d'un scan sécurité (ex: `trivy fs .` ou `grype`) si conteneurisation.
- Publication d'images Docker (ajouter un `Jenkinsfile.docker`).

---
Mainteneur CI: mettre à jour cette documentation à chaque ajout/modification significative.
