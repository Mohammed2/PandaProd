//// DEFINE THESE PLEASE ////

def in_files = ['023E56A6-4FF3-E711-867C-68B59972C49E', '7C75991C-535C-E811-B931-FA163E3A8726', '7E2944AD-145E-E811-90D7-FA163E11797A']

def cmssw_version = 'CMSSW_10_1_4'
def scram_arch = 'slc6_amd64_gcc630'
def panda_tree_user = 'PandaPhysics'
def panda_tree_branch = 'master'

//// Hopefully you don't need to change much of what follows

def do_src = "set +x; export SCRAM_ARCH=${scram_arch}; source /cvmfs/cms.cern.ch/cmsset_default.sh; set -x"

def prComment(message) {
  withCredentials([usernameColonPassword(credentialsId: 'mitsidekick', variable: 'USERPASS')]) {
    sh '''
       set +x
       if [ ! -z $CHANGE_URL ]
       then
           # Craft the URL for the github comments API from the PR
           COMMENT_URL=$(echo $CHANGE_URL | perl -a -F/ -ne 'chomp @F[-1]; print "https://api.github.com/repos/@F[-4]/@F[-3]/issues/@F[-1]/comments"')
           # Lots of escape strings because Jenkins uses this sh string to generate a .sh file...
           curl -u $USERPASS -X POST --data-binary \"{\\\"body\\\": \\\"''' + message + '''\\\"}\" $COMMENT_URL
       fi
       '''
  }
}

def produce_panda(cmssw_version, do_src, base) {
  def in_files_dir = '/mnt/hadoop/scratch/jenkins/miniaod'
  return {
    try {
      dir ("${cmssw_version}/src/PandaProd/Producer/cfg") {
        sh do_src + '''
           set +x; eval `scramv1 runtime -sh`; set -x
           BASE=''' + base + '''
           INPUT=''' + in_files_dir + '''/$BASE.root
           # Get the number of events to run. Sort of based on the size of the event content.
           MAX=$(edmEventSize -v $INPUT | perl -ne '/\\. [\\d\\.]+ [\\d\\.]+$/ && print $_' | perl -ane '$sum += $F[1]} END { print int(1e4 * exp($sum/-2e5))')
           cmsRun $(perl -ne '/^\\/store\\/(data|mc)\\// && print $1' $HOME/miniaod/$BASE.txt).py inputFiles=file:$INPUT outputFile=$BASE.root maxEvents=$MAX
           '''
      }
    }
    catch (all) {
      prComment('Problem producing with ``$(head -n1 ${HOME}/miniaod/' + base + '.txt)``. See ${BUILD_URL}console')
      error("Failed to produce ${base}.root")
    }
  }
}

def run_test(cmssw_version, do_src, base) {
  return {
    try {
      dir ("${cmssw_version}/src") {
        sh do_src + '''
           set +x; eval `scramv1 runtime -sh`; set -x
           BASE=''' + base + '''
           testpanda PandaProd/Producer/cfg/$BASE.root ${JOB_NAME}/${BUILD_NUMBER}/$(tail -n1 $HOME/miniaod/$BASE.txt)
           '''
      }
    }
    catch (all) {
      prComment('Problem testing ``' + base + '.root``. See ${BUILD_URL}console')
      error("Failed on test of ${base.root}")
    }
  }
}

node {
  checkout scm

  stage('Getting Source') {
    try {
      prComment('Starting tests! You can follow along (if on T3).\\n\\n- Job: ${JOB_URL}\\n- Run steps: ${BUILD_URL}flowGraphTable')

      sh 'ls'
      // Clear CMSSW and get new one
      sh """
         if [ -d ${cmssw_version} ]
         then
             rm -rf ${cmssw_version}
         fi

         ${do_src}
         scramv1 project CMSSW ${cmssw_version}
         """
      dir ("${cmssw_version}/src") {
        sh 'ls'

        // Get PandaTree if not there or if has new commit hash
        sh "git clone -b ${panda_tree_branch} https://www.github.com/${panda_tree_user}/PandaTree.git"

        // Make PandaProd directory for us to recursively copy files into
        sh 'mkdir PandaProd'
      }
  
      // Copy all of the files, and the HEAD where we will get the git tag from
      sh "cp --parents `git ls-files` ${cmssw_version}/src/PandaProd"
      sh "cp --parents .git/HEAD ${cmssw_version}/src/PandaProd"

      // Install the other CMSSW packages
      dir ("${cmssw_version}/src") {
        sh do_src + '''
           set +x; eval `scramv1 runtime -sh`; set -x
           ./PandaProd/Producer/cfg/setuprel.sh
           '''
      }
    }
    catch (all) {
      prComment('Problem gathering source. See ${BUILD_URL}console')
      error('Failed gathering source files')
    }
  }

  stage('Compiling') {
    try {
      dir ("${cmssw_version}/src") {
        // Compile
        sh "${do_src}; set +x; eval `scramv1 runtime -sh`; set -x; scram b -j4"
      }
    }
    catch (all) {
      prComment('Problem compiling. See ${BUILD_URL}console')
      error('Failed to compile')
    }
  }

  stage('Producing Pandas') {
    def begin_line = '\\n- ``$(head -n1 ${HOME}/miniaod/'
    def end_line = '.txt)``'
    def file_string = in_files.join("${end_line}${begin_line}")
    def result = 'Finished compiling! Will try to run over:\\n' + begin_line + file_string + end_line
    prComment(result)

    parallel in_files.collectEntries {
      ["Produce ${it}" : produce_panda(cmssw_version, do_src, it)]
    }
  }

  stage('Checking Pandas') {
    parallel in_files.collectEntries {
      ["Test ${it}" : run_test(cmssw_version, do_src, it)]
    }
  }

  stage('Finishing') {

    try {
      // Copy root files to hadoop for other repository's tests
      dir ("${cmssw_version}/src/PandaProd/Producer/cfg") {
        sh '''
           # Copy over individual branch result
           if [ -z $CHANGE_URL ]
           then
               mkdir -p /mnt/hadoop/scratch/jenkins/panda/${JOB_NAME}
               cp *.root /mnt/hadoop/scratch/jenkins/panda/${JOB_NAME}
           fi
           '''
      }
    }
    catch (all) {
      prComment ('Problem finishing up. See ${BUILD_URL}console')
      error ('Final steps failed')
    }

    prComment('Tests passed! See plots at:\\n\\n- http://t3serv001.mit.edu/~${USER}/relval/?d=${JOB_NAME}/${BUILD_NUMBER}')
  }
}
