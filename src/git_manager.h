#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <functional>

// Callback for log output: (message)
using GitLogCallback = std::function<void(const std::string&)>;

class GitManager {
public:
    void SetWorkDir(const std::string& dir) { m_workDir = dir; }
    void SetRepoUrl(const std::string& url) { m_repoUrl = url; }
    void SetLogCallback(GitLogCallback cb) { m_logCb = std::move(cb); }

    // Check if git is available on PATH
    bool IsGitAvailable();

    // Check if .git exists in work dir
    bool IsRepoInitialized();

    // Initialize repo: git init, write .gitignore, add remote, initial commit+push
    bool InitRepo();

    // git pull origin main --rebase
    bool Pull();

    // git add builds, commit with timestamp, push
    bool Push();

    // Returns count of changed files, or -1 on error
    int GetChangedFileCount();

private:
    std::string m_workDir;
    std::string m_repoUrl;
    GitLogCallback m_logCb;

    void Log(const std::string& msg);

    // Run a git command, capture combined stdout+stderr output.
    // Returns the process exit code, or -1 on failure to launch.
    int RunGit(const std::string& args, std::string& output);

    // Write .gitignore for DDO Builder folder
    bool WriteGitIgnore();
};
