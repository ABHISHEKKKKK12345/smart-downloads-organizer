#pragma once

#include "types.hpp"
#include <string>
#include <memory>
#include <mutex>
#include <functional>

namespace SDO {

class ConfigManager {
public:
    static ConfigManager& instance();

    bool load(const std::string& path);
    bool save() const;
    bool saveAs(const std::string& path) const;

    AppConfig&       config()       { return m_config; }
    const AppConfig& config() const { return m_config; }

    void setChangeCallback(std::function<void()> cb);
    void notifyChanged();

    // Convenience accessors
    bool    darkMode()      const { return m_config.darkMode; }
    bool    autoOrganize()  const { return m_config.autoOrganize; }
    bool    simulateMode()  const { return m_config.simulateMode; }

    // Rule management
    bool addRule(OrganizerRule rule);
    bool updateRule(const OrganizerRule& rule);
    bool deleteRule(const std::string& id);
    bool reorderRules(const std::vector<std::string>& orderedIds);

    // Path management
    bool addWatchPath(const std::string& path);
    bool removeWatchPath(const std::string& path);

    static std::string defaultConfigPath();
    static std::string defaultDatabasePath();
    static std::string defaultLogPath();
    static AppConfig   defaultConfig();
    static std::vector<CategoryMeta> defaultCategoryMeta();

private:
    ConfigManager() = default;
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    std::string   serializeConfig() const;
    bool          parseConfig(const std::string& json);
    std::string   serializeRule(const OrganizerRule& r) const;
    OrganizerRule parseRule(const std::string& json) const;

    mutable std::mutex       m_mutex;
    AppConfig                m_config;
    std::string              m_configPath;
    std::function<void()>    m_changeCb;
};

} // namespace SDO
