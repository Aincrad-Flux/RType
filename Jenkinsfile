node {
  stage('SCM') {
    checkout scm
  }
  stage('SonarQube Analysis') {
    def scannerHome = tool 'SonarScanner';
    withSonarQubeEnv('SonarQubeServer') {
      sh "${scannerHome}/bin/sonar-scanner"
    }
  }
}
