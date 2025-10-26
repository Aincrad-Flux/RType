pipeline {
    agent any

    options {
        timeout(time: 2, unit: 'HOURS')
        buildDiscarder(logRotator(numToKeepStr: '20'))
        disableConcurrentBuilds()
    }

    environment {
        GITHUB_TOKEN = credentials('github-https-token')
        REPO_URL = 'https://github.com/Aincrad-Flux/RType.git'
    }

    stages {
        stage('Initialize') {
            steps {
                script {
                    // Récupérer les informations de la PR
                    env.PR_BRANCH = env.CHANGE_BRANCH ?: params.BRANCH_NAME ?: 'main'
                    env.PR_NUMBER = env.CHANGE_ID ?: 'manual'

                    echo "🚀 Starting CI/CD Pipeline"
                    echo "Branch: ${env.PR_BRANCH}"
                    echo "PR Number: ${env.PR_NUMBER}"

                    // Poster le message initial
                    if (env.CHANGE_ID) {
                        postGitHubComment("""## 🔄 Jenkins CI - Pipeline démarré

**Build:** [#${env.BUILD_NUMBER}](${env.BUILD_URL})
**Branch:** `${env.PR_BRANCH}`

### 📋 Étapes à exécuter :
- ⏳ Installation des dépendances
- ⏳ Build des binaires
- ⏳ Analyse statique du code

*Les tests sont en cours d'exécution, veuillez patienter...*""")

                        setGitHubStatus('pending', 'Pipeline en cours...')
                    }
                }
            }
        }

        stage('Run Pipelines') {
            parallel {
                stage('1️⃣ Install Dependencies') {
                    steps {
                        script {
                            updatePipelineStatus('dependencies', 'running')

                            try {
                                build job: 'RType-Install-Dependencies',
                                    parameters: [
                                        string(name: 'SOURCE_REPO', value: env.REPO_URL),
                                        string(name: 'BRANCH_NAME', value: env.PR_BRANCH)
                                    ],
                                    wait: true,
                                    propagate: true

                                env.DEPS_STATUS = 'SUCCESS'
                                updatePipelineStatus('dependencies', 'success')
                            } catch (Exception e) {
                                env.DEPS_STATUS = 'FAILURE'
                                updatePipelineStatus('dependencies', 'failure', e.getMessage())
                                error("Installation des dépendances échouée: ${e.getMessage()}")
                            }
                        }
                    }
                }

                stage('2️⃣ Build Binaries') {
                    steps {
                        script {
                            updatePipelineStatus('build', 'running')

                            try {
                                def buildResult = build job: 'RType-Build-All-Binaries',
                                    parameters: [
                                        string(name: 'SOURCE_REPO', value: env.REPO_URL),
                                        string(name: 'BRANCH_NAME', value: env.PR_BRANCH)
                                    ],
                                    wait: true,
                                    propagate: true

                                env.BUILD_STATUS = 'SUCCESS'
                                env.BUILD_NUMBER_ARTIFACT = buildResult.number
                                updatePipelineStatus('build', 'success')

                                // Copier les artefacts
                                copyArtifacts(
                                    projectName: 'RType-Build-All-Binaries',
                                    selector: specific("${buildResult.number}"),
                                    target: 'artifacts/'
                                )

                            } catch (Exception e) {
                                env.BUILD_STATUS = 'FAILURE'
                                updatePipelineStatus('build', 'failure', e.getMessage())
                                error("Build des binaires échoué: ${e.getMessage()}")
                            }
                        }
                    }
                }

                stage('3️⃣ Code Analysis') {
                    steps {
                        script {
                            updatePipelineStatus('analysis', 'running')

                            try {
                                def analysisResult = build job: 'RType-Verify-Code-Integrity',
                                    parameters: [
                                        string(name: 'SOURCE_REPO', value: env.REPO_URL),
                                        string(name: 'BRANCH_NAME', value: env.PR_BRANCH)
                                    ],
                                    wait: true,
                                    propagate: false // Ne pas échouer si UNSTABLE

                                env.ANALYSIS_STATUS = analysisResult.result

                                if (analysisResult.result == 'SUCCESS') {
                                    updatePipelineStatus('analysis', 'success')
                                } else if (analysisResult.result == 'UNSTABLE') {
                                    updatePipelineStatus('analysis', 'unstable')
                                    // Ne pas échouer le build principal
                                    echo "⚠️ Analyse statique UNSTABLE - des warnings ont été détectés"
                                } else {
                                    updatePipelineStatus('analysis', 'failure')
                                    error("Analyse statique échouée")
                                }

                            } catch (Exception e) {
                                env.ANALYSIS_STATUS = 'FAILURE'
                                updatePipelineStatus('analysis', 'failure', e.getMessage())
                                error("Analyse du code échouée: ${e.getMessage()}")
                            }
                        }
                    }
                }
            }
        }

        stage('Final Report') {
            steps {
                script {
                    def allSuccess = (env.DEPS_STATUS == 'SUCCESS' &&
                                     env.BUILD_STATUS == 'SUCCESS' &&
                                     env.ANALYSIS_STATUS == 'SUCCESS')

                    def hasUnstable = (env.ANALYSIS_STATUS == 'UNSTABLE')

                    if (allSuccess) {
                        currentBuild.result = 'SUCCESS'
                    } else if (hasUnstable && env.BUILD_STATUS == 'SUCCESS') {
                        currentBuild.result = 'UNSTABLE'
                    } else {
                        currentBuild.result = 'FAILURE'
                    }

                    generateFinalReport()
                }
            }
        }
    }

    post {
        success {
            script {
                if (env.CHANGE_ID) {
                    postGitHubComment("""## ✅ Jenkins CI - Tous les tests sont passés !

**Build:** [#${env.BUILD_NUMBER}](${env.BUILD_URL})
**Durée:** ${currentBuild.durationString.replace(' and counting', '')}

### 📊 Résultats détaillés :
| Étape | Statut |
|-------|--------|
| 📦 Installation des dépendances | ✅ Succès |
| 🔨 Build des binaires | ✅ Succès |
| 🔍 Analyse statique | ✅ Succès |

### 🎉 Cette pull request peut être mergée !

Les binaires compilés sont disponibles dans les artefacts du build.""")

                    setGitHubStatus('success', 'Tous les tests sont passés')
                }
            }
        }

        unstable {
            script {
                if (env.CHANGE_ID) {
                    postGitHubComment("""## ⚠️ Jenkins CI - Build terminé avec des warnings

**Build:** [#${env.BUILD_NUMBER}](${env.BUILD_URL})
**Durée:** ${currentBuild.durationString.replace(' and counting', '')}

### 📊 Résultats détaillés :
| Étape | Statut |
|-------|--------|
| 📦 Installation des dépendances | ${getStatusEmoji(env.DEPS_STATUS)} ${env.DEPS_STATUS} |
| 🔨 Build des binaires | ${getStatusEmoji(env.BUILD_STATUS)} ${env.BUILD_STATUS} |
| 🔍 Analyse statique | ${getStatusEmoji(env.ANALYSIS_STATUS)} ${env.ANALYSIS_STATUS} |

### 💡 Recommandations :
L'analyse statique a détecté des warnings (cppcheck ou clang-tidy). Ces warnings n'empêchent pas le merge mais devraient être corrigés pour améliorer la qualité du code.

Consultez les artefacts du job [Verify Code Integrity](${env.JENKINS_URL}job/RType-Verify-Code-Integrity/) pour plus de détails.""")

                    setGitHubStatus('success', 'Build réussi avec warnings')
                }
            }
        }

        failure {
            script {
                if (env.CHANGE_ID) {
                    postGitHubComment("""## ❌ Jenkins CI - Des tests ont échoué

**Build:** [#${env.BUILD_NUMBER}](${env.BUILD_URL})
**Durée:** ${currentBuild.durationString.replace(' and counting', '')}

### 📊 Résultats détaillés :
| Étape | Statut |
|-------|--------|
| 📦 Installation des dépendances | ${getStatusEmoji(env.DEPS_STATUS)} ${env.DEPS_STATUS ?: 'N/A'} |
| 🔨 Build des binaires | ${getStatusEmoji(env.BUILD_STATUS)} ${env.BUILD_STATUS ?: 'N/A'} |
| 🔍 Analyse statique | ${getStatusEmoji(env.ANALYSIS_STATUS)} ${env.ANALYSIS_STATUS ?: 'N/A'} |

### 🔧 Actions requises :
Veuillez consulter les [logs du build](${env.BUILD_URL}console) pour identifier et corriger les erreurs avant de merger cette pull request.""")

                    setGitHubStatus('failure', 'Des tests ont échoué')
                }
            }
        }

        always {
            script {
                echo '🧹 Nettoyage...'
                // Ne pas nettoyer le workspace pour garder les artefacts
            }
        }
    }
}

// ========== Fonctions Helper ==========

def postGitHubComment(String message) {
    if (!env.CHANGE_ID || !env.CHANGE_URL) {
        echo "Pas de PR détectée, skip commentaire GitHub"
        return
    }

    try {
        def repoInfo = env.CHANGE_URL.tokenize('/')
        def owner = repoInfo[-4]
        def repo = repoInfo[-3]

        // Échapper les caractères spéciaux pour JSON
        def escapedMessage = message
            .replace('\\', '\\\\')
            .replace('"', '\\"')
            .replace('\n', '\\n')
            .replace('\r', '\\r')
            .replace('\t', '\\t')

        sh """
            curl -s -X POST \
                -H "Authorization: token \${GITHUB_TOKEN}" \
                -H "Accept: application/vnd.github.v3+json" \
                -H "Content-Type: application/json" \
                -d '{"body": "${escapedMessage}"}' \
                "https://api.github.com/repos/${owner}/${repo}/issues/${env.CHANGE_ID}/comments" \
                || echo "Erreur lors du post du commentaire GitHub"
        """
    } catch (Exception e) {
        echo "Erreur lors du post du commentaire: ${e.getMessage()}"
    }
}

def setGitHubStatus(String state, String description) {
    if (!env.GIT_COMMIT || !env.CHANGE_URL) {
        echo "Pas de commit détecté, skip status GitHub"
        return
    }

    try {
        def repoInfo = env.CHANGE_URL.tokenize('/')
        def owner = repoInfo[-4]
        def repo = repoInfo[-3]

        sh """
            curl -s -X POST \
                -H "Authorization: token \${GITHUB_TOKEN}" \
                -H "Accept: application/vnd.github.v3+json" \
                -H "Content-Type: application/json" \
                -d '{
                    "state": "${state}",
                    "target_url": "${env.BUILD_URL}",
                    "description": "${description}",
                    "context": "continuous-integration/jenkins/pr-merge"
                }' \
                "https://api.github.com/repos/${owner}/${repo}/statuses/${env.GIT_COMMIT}" \
                || echo "Erreur lors de la mise à jour du status GitHub"
        """
    } catch (Exception e) {
        echo "Erreur lors de la mise à jour du status: ${e.getMessage()}"
    }
}

def updatePipelineStatus(String stage, String status, String error = '') {
    if (!env.CHANGE_ID) {
        return
    }

    def emoji = [
        'running': '⏳',
        'success': '✅',
        'failure': '❌',
        'unstable': '⚠️'
    ]

    def statusText = [
        'running': 'En cours...',
        'success': 'Succès',
        'failure': 'Échec',
        'unstable': 'Unstable (warnings)'
    ]

    def stageNames = [
        'dependencies': 'Installation des dépendances',
        'build': 'Build des binaires',
        'analysis': 'Analyse statique'
    ]

    echo "${emoji[status]} ${stageNames[stage]}: ${statusText[status]}"
    if (error) {
        echo "   Erreur: ${error}"
    }
}

def getStatusEmoji(String status) {
    def emojis = [
        'SUCCESS': '✅',
        'FAILURE': '❌',
        'UNSTABLE': '⚠️',
        'ABORTED': '🚫',
        'NOT_BUILT': '⏭️'
    ]
    return emojis[status] ?: '❓'
}

def generateFinalReport() {
    echo """
    ============================================
    📊 RAPPORT FINAL DU PIPELINE
    ============================================
    Build Number: ${env.BUILD_NUMBER}
    Durée: ${currentBuild.durationString}
    Résultat: ${currentBuild.result}

    📦 Dépendances:  ${env.DEPS_STATUS ?: 'N/A'}
    🔨 Build:        ${env.BUILD_STATUS ?: 'N/A'}
    🔍 Analyse:      ${env.ANALYSIS_STATUS ?: 'N/A'}
    ============================================
    """
}