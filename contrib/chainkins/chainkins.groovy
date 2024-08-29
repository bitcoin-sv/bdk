/*
    Groovy script define reusable method for jenkins
    See
       https://stackoverflow.com/questions/55486023/reusing-groovy-script-in-jenkins-declarative-pipeline-file
*/

// Variables defined here and load by master can be seen in slaves, i.e
// env.jSLAVE_FOO="SLAVE FOO"

def _dump_directories_info(groovy_file) {
    sh label : "Dump common env variables build", script : """
        echo env.CHAINKINS_BUILD_DIRNAME=\\\"build\\\">>${groovy_file}
        echo env.CHAINKINS_BUILD_DIRNAME_RELEASE=\\\"buildrelease\\\">>${groovy_file}
        echo env.CHAINKINS_BUILD_DIRNAME_DEBUG=\\\"builddebug\\\">>${groovy_file}
        echo env.CHAINKINS_POSTBUILD_DIRNAME=\\\"buildpost\\\">>${groovy_file}
    """
}

def _dump_post_email_info(groovy_file) {
    if (!env.BITBUCKET_PAYLOAD) {// If BITBUCKET_PAYLOAD is not defined, then the build is manually triggered by a user
        wrap([$class: 'BuildUser']) {
            sh "echo env.CHAINKINS_BUILD_TRIGGER=\\\"\$BUILD_USER\\\">>${groovy_file}"
                // The manual build kickoff will send email to the only user who has kickoff the build.
            sh "echo env.CHAINKINS_EMAIL_TO_SEND=\\\"\$(python contrib/chainkins/chainkins.py --fix_nchain_email --email=$BUILD_USER_EMAIL)\\\">>${groovy_file}"
        }
    }
    else{
        sh label : "Dump common env variables for email", script : """
            echo env.CHAINKINS_BUILD_TRIGGER=\\\"Bitbucket webhook\\\">>${groovy_file}
            echo env.CHAINKINS_EMAIL_TO_SEND=\\\"c.nguyen@nchain.com,cc:s.okane@nchain.com,cc:m.prakash@nchain.com,cc:r.balagourouche@nchain.com\\\">>${groovy_file}
        """
    }
}

def dump_env_pr(pr_groovy_file) {

    _dump_directories_info(pr_groovy_file)
    _dump_post_email_info(pr_groovy_file)

    sh label : "Dump common env variables for scm", script : """
        export pr_destination_repo_ssh=\"\$(python contrib/chainkins/chainkins.py --get_pr_destination_repository)\"
        export pr_destination_branch=\"\$(python contrib/chainkins/chainkins.py --get_bbpayload_info --key_path=pullrequest:destination:branch:name)\"
        export pr_destination_commit=\"\$(python contrib/chainkins/chainkins.py --get_bbpayload_info --key_path=pullrequest:destination:commit:hash)\"
        export pr_source_repo_ssh=\"\$(python contrib/chainkins/chainkins.py --get_pr_source_repository)\"
        export pr_source_commit=\"\$(python contrib/chainkins/chainkins.py --get_bbpayload_info --key_path=pullrequest:source:commit:hash)\"
        export pr_source_branch=\"\$(python contrib/chainkins/chainkins.py --get_bbpayload_info --key_path=pullrequest:source:branch:name)\"
        export pr_actor=\"\$(python contrib/chainkins/chainkins.py --get_bbpayload_info --key_path=pullrequest:author:display_name)\"
        export pr_title=\"\$(python contrib/chainkins/chainkins.py --get_bbpayload_info --key_path=pullrequest:title)\"

        echo env.CHAINKINS_TARGET_REPO_SSH=\\\"\$pr_source_repo_ssh\\\">>${pr_groovy_file}
        echo env.CHAINKINS_TARGET_REPO_HTTP=\\\"\$(python contrib/chainkins/chainkins.py --get_http_repo --ssh_repo=\$pr_source_repo_ssh)\\\">>${pr_groovy_file}
        echo env.CHAINKINS_TARGET_BRANCH=\\\"\$pr_source_branch\\\">>${pr_groovy_file}
        echo env.CHAINKINS_TARGET_COMMIT=\\\"\$pr_source_commit\\\">>${pr_groovy_file}
        echo env.CHAINKINS_TARGET_COMMIT_SHORT=\\\"\$(python contrib/chainkins/chainkins.py --get_short_hash --git_hash=\$pr_source_commit)\\\">>${pr_groovy_file}

        echo env.CHAINKINS_PR_BITBUCKET_DESTINATION_REPO_SSH=\\\"\$(python contrib/chainkins/chainkins.py --get_pr_destination_repository)\\\">>${pr_groovy_file}
        echo env.CHAINKINS_PR_BITBUCKET_DESTINATION_REPO_HTTP=\\\"\$(python contrib/chainkins/chainkins.py --get_http_repo --ssh_repo=\$pr_destination_repo_ssh)\\\">>${pr_groovy_file}
        echo env.CHAINKINS_PR_BITBUCKET_DESTINATION_BRANCH=\\\"\$pr_destination_branch\\\">>${pr_groovy_file}
        echo env.CHAINKINS_PR_BITBUCKET_DESTINATION_COMMIT=\\\"\$pr_destination_commit\\\">>${pr_groovy_file}
        echo env.CHAINKINS_PR_BITBUCKET_SOURCE_REPO_SSH=\\\"\$pr_source_repo_ssh\\\">>${pr_groovy_file}
        echo env.CHAINKINS_PR_BITBUCKET_SOURCE_BRANCH=\\\"\$pr_source_branch\\\">>${pr_groovy_file}
        echo env.CHAINKINS_PR_BITBUCKET_ACTOR=\\\"\$pr_actor\\\">>${pr_groovy_file}
        echo env.CHAINKINS_PR_BITBUCKET_TITLE=\\\"\$pr_title\\\">>${pr_groovy_file}
    """
}

def dump_env_main_repo(master_groovy_file) {

    _dump_directories_info(master_groovy_file)
    _dump_post_email_info(master_groovy_file)

    sh label : "Dump common env variables for scm", script : """
        echo env.CHAINKINS_TARGET_REPO_HTTP=\\\"\$(python contrib/chainkins/chainkins.py --get_http_repo --ssh_repo=${env.GIT_URL})\\\">>${master_groovy_file}
        echo env.CHAINKINS_TARGET_BRANCH=\\\"\$(python contrib/chainkins/chainkins.py --get_local_branch --git_branch=${env.GIT_BRANCH})\\\">>${master_groovy_file}
        echo env.CHAINKINS_TARGET_COMMIT=\\\"${env.GIT_COMMIT}\\\">>${master_groovy_file}
        echo env.CHAINKINS_TARGET_COMMIT_SHORT=\\\"\$(python contrib/chainkins/chainkins.py --get_short_hash --git_hash=${env.GIT_COMMIT})\\\">>${master_groovy_file}
    """
}

def pr_checkout_and_rebase_windows(nb_log = "20"){
    checkout([
        label : "Clone pull request from fork",
        $class: "GitSCM",
        branches: [[name: "pr_$BITBUCKET_PULL_REQUEST_ID/$CHAINKINS_PR_BITBUCKET_SOURCE_BRANCH"]],
        doGenerateSubmoduleConfigurations: false,
        extensions: [[$class: "CloneOption", honorRefspec : true]],
        userRemoteConfigs: [
            [credentialsId: "sdklibraries_ssh", refspec: "+refs/heads/$CHAINKINS_PR_BITBUCKET_SOURCE_BRANCH:refs/remotes/origin/pr_$BITBUCKET_PULL_REQUEST_ID/$CHAINKINS_PR_BITBUCKET_SOURCE_BRANCH", url:"$CHAINKINS_PR_BITBUCKET_SOURCE_REPO_SSH"]
        ]
    ])

    bat label : "Checkout destination branch and rebase on pull request branch", script : """
        git checkout $CHAINKINS_PR_BITBUCKET_DESTINATION_BRANCH
        git log -2
        git checkout pr_$BITBUCKET_PULL_REQUEST_ID/$CHAINKINS_PR_BITBUCKET_SOURCE_BRANCH
        git branch -a
        git rebase pr_$BITBUCKET_PULL_REQUEST_ID/$CHAINKINS_PR_BITBUCKET_SOURCE_BRANCH $CHAINKINS_PR_BITBUCKET_DESTINATION_BRANCH
        git log -${nb_log}
    """
}

def dump_buildenv_windows(win_env_groovy_file) {
    bat label : "Dump env variables for windows build", script : """
        echo env.WINDOWS_CHAINKINS_FILE=/${env.WORKSPACE}\\contrib\\chainkins\\chainkins.py/      >>${win_env_groovy_file}
        echo env.WINDOWS_BUILD_DIR=/${env.WORKSPACE}\\${env.CHAINKINS_BUILD_DIRNAME}/             >>${win_env_groovy_file}
        echo env.WINDOWS_POSTBUILD_DIR=/${env.WORKSPACE}\\${env.CHAINKINS_POSTBUILD_DIRNAME}/     >>${win_env_groovy_file}
        echo env.WINDOWS_TESTRESULT_DIR_RELEASE=/${env.WORKSPACE}\\${env.CHAINKINS_BUILD_DIRNAME}\\x64\\release/ >>${win_env_groovy_file}
        echo env.WINDOWS_TESTRESULT_DIR_DEBUG=/${env.WORKSPACE}\\${env.CHAINKINS_BUILD_DIRNAME}\\x64\\debug/     >>${win_env_groovy_file}
    """
}

def build_on_windows() {
    bat label : "Compile Release and Debug on Windows", script : '''
        mkdir %WINDOWS_BUILD_DIR%'
        cmake -B%WINDOWS_BUILD_DIR% -H%WORKSPACE% -G"Visual Studio 16 2019" -A x64
        cmake --build %WINDOWS_BUILD_DIR% --target ALL_BUILD --config Release --parallel 2
        cmake --build %WINDOWS_BUILD_DIR% --target ALL_BUILD --config Debug --parallel 2
    '''
}

def runtest_on_windows() {
    bat label : "Run test Release and Debug on Windows", script : '''
        cd %WINDOWS_BUILD_DIR% && ctest -C Release -F --output-on-failure
        cd %WINDOWS_BUILD_DIR% && ctest -C Debug -F --output-on-failure
        cd %WORKSPACE%
    '''
}

def consolidate_test_on_windows() {
    bat label : "Consolidate tests results for post build", script : '''
        python %WINDOWS_CHAINKINS_FILE% --consolidate_junit --indir_debug=%WINDOWS_TESTRESULT_DIR_DEBUG% --indir_release=%WINDOWS_TESTRESULT_DIR_RELEASE% --outdir=%WINDOWS_POSTBUILD_DIR%
        python %WINDOWS_CHAINKINS_FILE% --consolidate_html --indir_debug=%WINDOWS_TESTRESULT_DIR_DEBUG% --indir_release=%WINDOWS_TESTRESULT_DIR_RELEASE% --outdir=%WINDOWS_POSTBUILD_DIR%
    '''

    if (!env.BITBUCKET_PULL_REQUEST_ID) {// If BITBUCKET_PULL_REQUEST_ID is not defined, then its not a pull request
        bat label : "Dump production build email to send", script : '''
            python %WINDOWS_CHAINKINS_FILE% --dump_mainrepo_email_html --indir_debug=%WINDOWS_TESTRESULT_DIR_DEBUG% --indir_release=%WINDOWS_TESTRESULT_DIR_RELEASE% --outdir=%WINDOWS_POSTBUILD_DIR%
        '''
    }
    else{
        bat label : "Dump pull request build email to send", script : '''
            python %WINDOWS_CHAINKINS_FILE% --dump_pr_email_html --indir_debug=%WINDOWS_TESTRESULT_DIR_DEBUG% --indir_release=%WINDOWS_TESTRESULT_DIR_RELEASE% --outdir=%WINDOWS_POSTBUILD_DIR%
        '''
    }
}

def pack_on_windows() {
    bat label : "Pack Release and Debug on Windows", script : '''
        if not exist %WINDOWS_POSTBUILD_DIR% mkdir %WINDOWS_POSTBUILD_DIR%
        cpack --config %WINDOWS_BUILD_DIR%\\CPackConfig.cmake -G NSIS -C Debug
        cpack --config %WINDOWS_BUILD_DIR%\\CPackConfig.cmake -G NSIS -C Release
        cpack --config %WINDOWS_BUILD_DIR%\\CPackSourceConfig.cmake -G ZIP
        move bdk-v*Windows*.exe %WINDOWS_POSTBUILD_DIR%
        dir %WINDOWS_POSTBUILD_DIR%\\*.exe
        move bdk-v*source*.zip %WINDOWS_POSTBUILD_DIR%
        dir %WINDOWS_POSTBUILD_DIR%\\*.zip
    '''
}


def pr_checkout_and_rebase_linux(nb_log = "20"){
    checkout([
        label : "Clone pull request from fork",
        $class: "GitSCM",
        branches: [[name: "pr_$BITBUCKET_PULL_REQUEST_ID/$CHAINKINS_PR_BITBUCKET_SOURCE_BRANCH"]],
        doGenerateSubmoduleConfigurations: false,
        extensions: [[$class: "CloneOption", honorRefspec : true]],
        userRemoteConfigs: [
            [credentialsId: "sdklibraries_ssh", refspec: "+refs/heads/$CHAINKINS_PR_BITBUCKET_SOURCE_BRANCH:refs/remotes/origin/pr_$BITBUCKET_PULL_REQUEST_ID/$CHAINKINS_PR_BITBUCKET_SOURCE_BRANCH", url:"$CHAINKINS_PR_BITBUCKET_SOURCE_REPO_SSH"]
        ]
    ])

    sh label : "Checkout destination branch and rebase on pull request branch", script : """
        git checkout $CHAINKINS_PR_BITBUCKET_DESTINATION_BRANCH
        git log -2
        git checkout pr_$BITBUCKET_PULL_REQUEST_ID/$CHAINKINS_PR_BITBUCKET_SOURCE_BRANCH
        git branch -a
        git rebase pr_$BITBUCKET_PULL_REQUEST_ID/$CHAINKINS_PR_BITBUCKET_SOURCE_BRANCH $CHAINKINS_PR_BITBUCKET_DESTINATION_BRANCH
        git log -${nb_log}
    """
}

def dump_buildenv_linux(linux_env_groovy_file) {
    sh label : "Dump env variables for linux build", script : """
        echo env.LINUX_CHAINKINS_FILE=\\\"${env.WORKSPACE}/contrib/chainkins/chainkins.py\\\"              >> ${linux_env_groovy_file}
        echo env.LINUX_BUILD_DIR_RELEASE=\\\"${env.WORKSPACE}/${env.CHAINKINS_BUILD_DIRNAME_RELEASE}\\\"  >> ${linux_env_groovy_file}
        echo env.LINUX_BUILD_DIR_DEBUG=\\\"${env.WORKSPACE}/${env.CHAINKINS_BUILD_DIRNAME_DEBUG}\\\"      >> ${linux_env_groovy_file}
        echo env.LINUX_POSTBUILD_DIR=\\\"${env.WORKSPACE}/${env.CHAINKINS_POSTBUILD_DIRNAME}\\\"          >> ${linux_env_groovy_file}
        echo env.LINUX_TESTRESULT_DIR_RELEASE=\\\"${env.WORKSPACE}/${env.CHAINKINS_BUILD_DIRNAME_RELEASE}/x64/release\\\" >> ${linux_env_groovy_file}
        echo env.LINUX_TESTRESULT_DIR_DEBUG=\\\"${env.WORKSPACE}/${env.CHAINKINS_BUILD_DIRNAME_DEBUG}/x64/debug\\\"       >> ${linux_env_groovy_file}
    """
}

def build_on_linux() {
    sh label : "Compile Release and Debug on Linux", script : '''
        mkdir $LINUX_BUILD_DIR_RELEASE
        mkdir $LINUX_BUILD_DIR_DEBUG
        cmake -B$LINUX_BUILD_DIR_RELEASE -H$WORKSPACE -DCUSTOM_SYSTEM_OS_NAME=Ubuntu -DCMAKE_BUILD_TYPE=Release
        cmake --build $LINUX_BUILD_DIR_RELEASE --target all --parallel 2
        cmake -B$LINUX_BUILD_DIR_DEBUG -H$WORKSPACE -DCUSTOM_SYSTEM_OS_NAME=Ubuntu -DCMAKE_BUILD_TYPE=Debug
        cmake --build $LINUX_BUILD_DIR_DEBUG --target all --parallel 2
    '''
}

def runtest_on_linux() {
    sh label : "Run test Release and Debug on Linux", script : '''
       cd $LINUX_BUILD_DIR_RELEASE ; ctest -F --output-on-failure
       cd $LINUX_BUILD_DIR_DEBUG ; ctest -F --output-on-failure
       cd $WORKSPACE
    '''
}

def consolidate_test_on_linux() {
    sh label : "Consolidate tests results for post build", script : '''
        python $LINUX_CHAINKINS_FILE --consolidate_junit --indir_debug=$LINUX_TESTRESULT_DIR_DEBUG --indir_release=$LINUX_TESTRESULT_DIR_RELEASE --outdir=$LINUX_POSTBUILD_DIR
        python $LINUX_CHAINKINS_FILE --consolidate_html --indir_debug=$LINUX_TESTRESULT_DIR_DEBUG --indir_release=$LINUX_TESTRESULT_DIR_RELEASE --outdir=$LINUX_POSTBUILD_DIR
    '''

    if (!env.BITBUCKET_PULL_REQUEST_ID) {// If BITBUCKET_PULL_REQUEST_ID is not defined, then its not a pull request
        sh label : "Dump production build email to send", script : '''
            python $LINUX_CHAINKINS_FILE --dump_mainrepo_email_html --indir_debug=$LINUX_TESTRESULT_DIR_DEBUG --indir_release=$LINUX_TESTRESULT_DIR_RELEASE --outdir=$LINUX_POSTBUILD_DIR
        '''
    }
    else{
        sh label : "Dump pull request build email to send", script : '''
            python $LINUX_CHAINKINS_FILE --dump_pr_email_html --indir_debug=$LINUX_TESTRESULT_DIR_DEBUG --indir_release=$LINUX_TESTRESULT_DIR_RELEASE --outdir=$LINUX_POSTBUILD_DIR
        '''
    }
}

def pack_on_linux() {
    sh label : "Pack Release and Debug on Linux", script : '''
        mkdir -p $LINUX_POSTBUILD_DIR
        cpack --config $LINUX_BUILD_DIR_DEBUG/CPackConfig.cmake -G TGZ
        cpack --config $LINUX_BUILD_DIR_RELEASE/CPackConfig.cmake -G TGZ
        mv bdk-v*Ubuntu*.tar.gz $LINUX_POSTBUILD_DIR
        cpack --config $LINUX_BUILD_DIR_RELEASE/CPackSourceConfig.cmake -G TGZ
        mv bdk-v*source*.tar.gz $LINUX_POSTBUILD_DIR
        ls -Ss1pq --block-size=M $LINUX_POSTBUILD_DIR/*.tar.gz
    '''
}

return this
