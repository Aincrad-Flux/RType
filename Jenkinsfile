node {
  stage('SCM') {
    checkout scm
  }
  stage('SonarQube Analysis') {
    def scannerHome = tool 'SonarScanner'
    def javaHome = '/usr/lib/jvm/java-21-openjdk-amd64'

    withEnv(["JAVA_HOME=${javaHome}", "PATH+JAVA=${javaHome}/bin"]) {
      withSonarQubeEnv('SonarQubeServer') {
        sh "java -version"
        sh "${scannerHome}/bin/sonar-scanner"
      }
    }
  }
}