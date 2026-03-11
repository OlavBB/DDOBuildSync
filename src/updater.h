#pragma once
#include <string>
#include <functional>

using UpdateLogCallback = std::function<void(const std::string&)>;

struct UpdateInfo {
    std::string latestVersion;   // e.g. "2.0.0.75"
    std::string downloadUrl;     // direct zip URL
    std::string assetName;       // e.g. "DDOBuilderV2_2.0.0.75.zip"
};

class Updater {
public:
    void SetLogCallback(UpdateLogCallback cb) { m_logCb = std::move(cb); }

    // Extract version from a path containing "DDOBuilderV2_X.X.X.X"
    static std::string ExtractVersionFromPath(const std::string& path);

    // Fetch latest release info from GitHub API. Returns true on success.
    bool FetchLatestRelease(UpdateInfo& out);

    // Returns true if version string a > b (format "X.X.X.X")
    static bool IsNewer(const std::string& a, const std::string& b);

    // Download zip, extract to temp, merge into existing buildsFolder (overwrites exe/data,
    // leaves .DDOBuild and .git untouched). Returns DDOBuilder.exe path, or empty on error.
    std::string DownloadAndInstall(const UpdateInfo& info,
                                   const std::string& buildsFolder);

private:
    UpdateLogCallback m_logCb;
    void Log(const std::string& msg);
    std::string RunHidden(const std::string& cmd, int timeoutMs = 120000);
};
