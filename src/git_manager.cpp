#include "git_manager.h"
#include "utils.h"
#include <fstream>
#include <sstream>

void GitManager::Log(const std::string& msg) {
    if (m_logCb) m_logCb(msg);
}

int GitManager::RunGit(const std::string& args, std::string& output) {
    output.clear();

    std::string cmdLine = "git " + args;
    Log("> " + cmdLine);

    // Create pipes for stdout+stderr
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hReadPipe = nullptr, hWritePipe = nullptr;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        Log("Failed to create pipe");
        return -1;
    }

    // Don't inherit the read end
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.hStdInput = nullptr;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {};

    // Need a mutable buffer for CreateProcessA
    std::string cmdBuf = cmdLine;

    BOOL ok = CreateProcessA(
        nullptr,
        cmdBuf.data(),
        nullptr, nullptr,
        TRUE,           // inherit handles
        CREATE_NO_WINDOW,
        nullptr,
        m_workDir.empty() ? nullptr : m_workDir.c_str(),
        &si, &pi
    );

    // Close write end in parent so ReadFile will return when child exits
    CloseHandle(hWritePipe);

    if (!ok) {
        CloseHandle(hReadPipe);
        Log("Failed to launch git (is git on PATH?)");
        return -1;
    }

    // Read all output
    char buf[4096];
    DWORD bytesRead;
    while (ReadFile(hReadPipe, buf, sizeof(buf) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        buf[bytesRead] = '\0';
        output += buf;
    }
    CloseHandle(hReadPipe);

    // Wait for process to finish
    WaitForSingleObject(pi.hProcess, 30000); // 30s timeout

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    // Log output lines
    if (!output.empty()) {
        std::istringstream iss(output);
        std::string line;
        while (std::getline(iss, line)) {
            // Trim trailing \r
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (!line.empty()) Log("  " + line);
        }
    }

    return static_cast<int>(exitCode);
}

bool GitManager::IsGitAvailable() {
    std::string output;
    int rc = RunGit("--version", output);
    return rc == 0;
}

bool GitManager::IsRepoInitialized() {
    return Utils::DirExists(m_workDir + "\\.git");
}

bool GitManager::WriteGitIgnore() {
    std::string path = m_workDir + "\\.gitignore";
    std::ofstream f(path);
    if (!f.is_open()) {
        Log("Failed to write .gitignore");
        return false;
    }
    f << "# DDO Builder V2 - ignore everything except build files\n";
    f << "*\n";
    f << "\n";
    f << "# Allow build files\n";
    f << "!.gitignore\n";
    f << "!*.DDOBuild\n";
    f << "!*.DDOBuild.backup\n";
    f.close();
    Log("Wrote .gitignore");
    return true;
}

bool GitManager::InitRepo() {
    if (m_workDir.empty()) {
        Log("Error: builds folder not set");
        return false;
    }

    Log("Initializing git repo in: " + m_workDir);

    std::string output;

    // git init
    if (RunGit("init", output) != 0) {
        Log("git init failed");
        return false;
    }

    // Write .gitignore
    if (!WriteGitIgnore()) return false;

    // Set default branch to main
    RunGit("branch -M main", output);

    // Add remote
    if (!m_repoUrl.empty()) {
        // Remove existing remote if any
        RunGit("remote remove origin", output);
        if (RunGit("remote add origin " + m_repoUrl, output) != 0) {
            Log("Failed to add remote");
            return false;
        }
    }

    // Add all build files (.gitignore whitelist handles filtering)
    RunGit("add -A", output);

    // Initial commit
    if (RunGit("commit -m \"Initial commit - DDO Builder builds\"", output) != 0) {
        Log("Initial commit failed (maybe no build files yet?)");
        // Not fatal - might be empty repo
    }

    // Push
    if (!m_repoUrl.empty()) {
        if (RunGit("push -u origin main", output) != 0) {
            Log("Initial push failed - you may need to push manually");
            return false;
        }
    }

    Log("Repository initialized successfully");
    return true;
}

bool GitManager::Pull() {
    if (!IsRepoInitialized()) {
        Log("Error: repository not initialized");
        return false;
    }

    Log("Pulling latest builds...");
    std::string output;

    int rc = RunGit("pull origin main --rebase", output);
    if (rc != 0) {
        // Try without --rebase in case of issues
        Log("Pull with rebase failed, trying regular pull...");
        rc = RunGit("pull origin main", output);
        if (rc != 0) {
            Log("Pull failed");
            return false;
        }
    }

    Log("Pull complete");
    return true;
}

bool GitManager::Push() {
    if (!IsRepoInitialized()) {
        Log("Error: repository not initialized");
        return false;
    }

    Log("Pushing builds...");
    std::string output;

    // Stage all changes (additions, modifications, and deletions)
    // .gitignore whitelist ensures only build files are tracked
    RunGit("add -A", output);

    // Check if there are changes
    int changedCount = GetChangedFileCount();
    if (changedCount == 0) {
        Log("No changes to push");
        return true;
    }

    // Commit
    std::string timestamp = Utils::GetTimestamp();
    std::string commitMsg = "Update builds - " + timestamp;
    if (RunGit("commit -m \"" + commitMsg + "\"", output) != 0) {
        Log("Commit failed");
        return false;
    }

    // Push
    if (RunGit("push origin main", output) != 0) {
        Log("Push failed");
        return false;
    }

    Log("Push complete");
    return true;
}

int GitManager::GetChangedFileCount() {
    std::string output;
    if (RunGit("status --porcelain", output) != 0) {
        return -1;
    }

    if (output.empty()) return 0;

    int count = 0;
    std::istringstream iss(output);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty() && line != "\r") count++;
    }
    return count;
}
