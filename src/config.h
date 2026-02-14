#pragma once
#include <string>

struct SyncConfig {
    std::string buildsFolder;
    std::string ddoBuilderExe;
    std::string gitRepoUrl;
    bool autoPushOnClose = true;
    bool autoPullOnLaunch = true;
};

class ConfigManager {
public:
    // Load config from JSON file. Returns true on success.
    bool Load(const std::string& path);

    // Save config to JSON file. Returns true on success.
    bool Save(const std::string& path) const;

    // Load default_config.json from same directory as exe
    bool LoadDefault();

    // Save to the standard config path next to exe
    bool SaveDefault() const;

    SyncConfig& Get() { return m_config; }
    const SyncConfig& Get() const { return m_config; }

    // Path to user config file (next to exe)
    static std::string GetConfigPath();

private:
    SyncConfig m_config;
};
