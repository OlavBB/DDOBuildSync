#include "config.h"
#include <nlohmann/json.hpp>
#include <fstream>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

using json = nlohmann::json;

static std::string GetExeDir() {
    char buf[MAX_PATH];
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string path(buf);
    auto pos = path.find_last_of("\\/");
    return (pos != std::string::npos) ? path.substr(0, pos) : ".";
}

std::string ConfigManager::GetConfigPath() {
    return GetExeDir() + "\\ddobuildsync_config.json";
}

bool ConfigManager::Load(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return false;

    try {
        json j = json::parse(f);
        if (j.contains("buildsFolder"))    m_config.buildsFolder    = j["buildsFolder"].get<std::string>();
        if (j.contains("ddoBuilderExe"))   m_config.ddoBuilderExe   = j["ddoBuilderExe"].get<std::string>();
        if (j.contains("gitRepoUrl"))      m_config.gitRepoUrl      = j["gitRepoUrl"].get<std::string>();
        if (j.contains("autoPushOnClose")) m_config.autoPushOnClose = j["autoPushOnClose"].get<bool>();
        if (j.contains("autoPullOnLaunch"))m_config.autoPullOnLaunch= j["autoPullOnLaunch"].get<bool>();
        return true;
    } catch (...) {
        return false;
    }
}

bool ConfigManager::Save(const std::string& path) const {
    json j;
    j["buildsFolder"]     = m_config.buildsFolder;
    j["ddoBuilderExe"]    = m_config.ddoBuilderExe;
    j["gitRepoUrl"]       = m_config.gitRepoUrl;
    j["autoPushOnClose"]  = m_config.autoPushOnClose;
    j["autoPullOnLaunch"] = m_config.autoPullOnLaunch;

    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << j.dump(2);
    return f.good();
}

bool ConfigManager::LoadDefault() {
    std::string userConfig = GetConfigPath();
    if (Load(userConfig)) return true;

    // Fall back to default_config.json
    std::string defaultConfig = GetExeDir() + "\\default_config.json";
    return Load(defaultConfig);
}

bool ConfigManager::SaveDefault() const {
    return Save(GetConfigPath());
}
