#include "updater.h"
#include <nlohmann/json.hpp>
#include <windows.h>
#include <regex>
#include <sstream>
#include <vector>

void Updater::Log(const std::string& msg) {
    if (m_logCb) m_logCb(msg);
}

std::string Updater::RunHidden(const std::string& cmd, int timeoutMs) {
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hRead = nullptr, hWrite = nullptr;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) return "";
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = hWrite;
    si.hStdError  = hWrite;
    si.hStdInput  = INVALID_HANDLE_VALUE;

    PROCESS_INFORMATION pi = {};
    std::string cmdLine = "cmd.exe /C " + cmd;
    BOOL ok = CreateProcessA(nullptr, &cmdLine[0], nullptr, nullptr,
                             TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(hWrite);
    if (!ok) { CloseHandle(hRead); return ""; }

    std::string result;
    char buf[4096];
    DWORD bytesRead;
    while (ReadFile(hRead, buf, sizeof(buf) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        buf[bytesRead] = '\0';
        result += buf;
    }
    WaitForSingleObject(pi.hProcess, static_cast<DWORD>(timeoutMs));
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hRead);

    while (!result.empty() && (result.back() == '\n' || result.back() == '\r' || result.back() == ' '))
        result.pop_back();
    return result;
}

std::string Updater::ExtractVersionFromPath(const std::string& path) {
    std::regex re(R"(DDOBuilderV2_(\d+\.\d+\.\d+\.\d+))");
    std::smatch m;
    if (std::regex_search(path, m, re))
        return m[1].str();
    return "";
}

bool Updater::IsNewer(const std::string& a, const std::string& b) {
    auto parse = [](const std::string& v) -> std::vector<int> {
        std::vector<int> parts;
        std::stringstream ss(v);
        std::string tok;
        while (std::getline(ss, tok, '.')) {
            try { parts.push_back(std::stoi(tok)); } catch (...) { parts.push_back(0); }
        }
        return parts;
    };
    auto va = parse(a), vb = parse(b);
    size_t n = (std::max)(va.size(), vb.size());
    for (size_t i = 0; i < n; ++i) {
        int ai = (i < va.size()) ? va[i] : 0;
        int bi = (i < vb.size()) ? vb[i] : 0;
        if (ai > bi) return true;
        if (ai < bi) return false;
    }
    return false;
}

bool Updater::FetchLatestRelease(UpdateInfo& out) {
    Log("Checking for DDO Builder V2 updates...");

    std::string json = RunHidden(
        "curl -s -L -A \"DDOBuildSync/1.0\" "
        "\"https://api.github.com/repos/Maetrim/DDOBuilderV2/releases/latest\"",
        30000
    );

    if (json.empty()) {
        Log("Update check failed: no response from GitHub");
        return false;
    }

    try {
        auto j = nlohmann::json::parse(json);

        std::string tag = j.value("tag_name", "");
        if (tag.empty()) {
            Log("Update check failed: could not read release tag");
            return false;
        }
        out.latestVersion = tag;

        for (auto& asset : j["assets"]) {
            std::string name = asset.value("name", "");
            if (name.size() > 4 && name.substr(name.size() - 4) == ".zip") {
                out.assetName   = name;
                out.downloadUrl = asset.value("browser_download_url", "");
                break;
            }
        }

        if (out.downloadUrl.empty()) {
            Log("Update check failed: no zip asset in release");
            return false;
        }
        return true;
    } catch (...) {
        Log("Update check failed: could not parse GitHub response");
        return false;
    }
}

std::string Updater::DownloadAndInstall(const UpdateInfo& info,
                                        const std::string& buildsFolder) {
    // --- Download ---
    char tempBuf[MAX_PATH];
    GetTempPathA(MAX_PATH, tempBuf);
    std::string tempDir  = std::string(tempBuf) + "DDOBuildSync_update\\";
    std::string zipPath  = tempDir + info.assetName;

    CreateDirectoryA(tempDir.c_str(), nullptr);

    Log("Downloading " + info.assetName + " (~45 MB, please wait)...");
    RunHidden("curl -L -o \"" + zipPath + "\" \"" + info.downloadUrl + "\"", 300000);

    if (GetFileAttributesA(zipPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        Log("Download failed: zip not found at " + zipPath);
        return "";
    }

    // --- Extract to temp subfolder ---
    std::string extractDir = tempDir + "extracted\\";
    Log("Extracting...");
    RunHidden(
        "powershell -NoProfile -NonInteractive -Command "
        "\"Expand-Archive -Path '" + zipPath + "' -DestinationPath '" + extractDir + "' -Force\"",
        120000
    );
    DeleteFileA(zipPath.c_str());

    // The zip extracts to a subfolder: extracted/DDOBuilderV2_X.X.X.X/
    std::string extractedFolder = extractDir + "DDOBuilderV2_" + info.latestVersion + "\\";
    if (GetFileAttributesA(extractedFolder.c_str()) == INVALID_FILE_ATTRIBUTES) {
        Log("Extraction failed: expected folder not found: " + extractedFolder);
        return "";
    }

    // --- Merge into existing buildsFolder (overwrite exe/data, keep .DDOBuild + .git) ---
    Log("Installing into " + buildsFolder + "...");
    RunHidden(
        "robocopy \"" + extractedFolder + "\" \"" + buildsFolder +
        "\" /E /IS /IT /NFL /NDL /NJH /NJS /NC /NS",
        60000
    );

    // --- Cleanup temp ---
    RunHidden("rmdir /S /Q \"" + tempDir + "\"", 15000);

    // Verify
    std::string exePath = buildsFolder + "\\DDOBuilder.exe";
    if (GetFileAttributesA(exePath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        Log("Install failed: DDOBuilder.exe not found after update");
        return "";
    }

    Log("DDO Builder V2 " + info.latestVersion + " installed successfully.");
    return exePath;
}
