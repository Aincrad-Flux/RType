node {
  stage('SCM') {
    checkout scm
  }
  stage('SonarQube Analysis') {
    def scannerHome = tool 'SonarScanner'
    withEnv(["JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64", "PATH+JAVA=${JAVA_HOME}/bin"]) {
      withSonarQubeEnv('SonarQubeServer') {
        sh "${scannerHome}/bin/sonar-scanner"
      }
    }
  }
}