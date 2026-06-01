#include "config_manager.hpp"
#include "logger.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <filesystem>
#include <algorithm>
#include <cstdlib>

namespace fs = std::filesystem;

namespace SDO {

// ─── Minimal JSON helpers (no external deps) ─────────────────────────────────
// We use a simple hand-rolled serializer/parser for config to avoid extra deps.

static std::string jsonStr(const std::string& s) {
    std::ostringstream oss;
    oss << '"';
    for (char c : s) {
        if (c == '"')  oss << "\\\"";
        else if (c == '\\') oss << "\\\\";
        else if (c == '\n') oss << "\\n";
        else if (c == '\r') oss << "\\r";
        else if (c == '\t') oss << "\\t";
        else oss << c;
    }
    oss << '"';
    return oss.str();
}

static std::string jsonBool(bool v)         { return v ? "true" : "false"; }
static std::string jsonInt(int64_t v)       { return std::to_string(v); }
static std::string jsonUint(uint64_t v)     { return std::to_string(v); }

// Simple extraction helpers
[[maybe_unused]] static std::string extractStr(const std::string& json, const std::string& key) {
    auto it = json.find("\"" + key + "\"");
    if (it == std::string::npos) return "";
    auto colon = json.find(':', it);
    if (colon == std::string::npos) return "";
    auto q1 = json.find('"', colon + 1);
    if (q1 == std::string::npos) return "";
    auto q2 = q1 + 1;
    while (q2 < json.size()) {
        if (json[q2] == '"' && json[q2-1] != '\\') break;
        ++q2;
    }
    std::string val = json.substr(q1+1, q2-q1-1);
    // Unescape
    std::string result;
    for (size_t i = 0; i < val.size(); ++i) {
        if (val[i] == '\\' && i+1 < val.size()) {
            ++i;
            if (val[i] == '"')  result += '"';
            else if (val[i] == '\\') result += '\\';
            else if (val[i] == 'n')  result += '\n';
            else if (val[i] == 'r')  result += '\r';
            else if (val[i] == 't')  result += '\t';
            else result += val[i];
        } else {
            result += val[i];
        }
    }
    return result;
}

static bool extractBool(const std::string& json, const std::string& key, bool def = false) {
    auto it = json.find("\"" + key + "\"");
    if (it == std::string::npos) return def;
    auto colon = json.find(':', it);
    if (colon == std::string::npos) return def;
    size_t pos = colon + 1;
    while (pos < json.size() && (json[pos]==' '||json[pos]=='\n'||json[pos]=='\t')) ++pos;
    if (json.substr(pos, 4) == "true")  return true;
    if (json.substr(pos, 5) == "false") return false;
    return def;
}

static int64_t extractInt(const std::string& json, const std::string& key, int64_t def = 0) {
    auto it = json.find("\"" + key + "\"");
    if (it == std::string::npos) return def;
    auto colon = json.find(':', it);
    if (colon == std::string::npos) return def;
    size_t pos = colon + 1;
    while (pos < json.size() && (json[pos]==' '||json[pos]=='\n'||json[pos]=='\t')) ++pos;
    try { return std::stoll(json.substr(pos)); }
    catch(...) { return def; }
}

// Extract a JSON string array: "key": ["val1", "val2", ...]
// Returns empty vector if the key is absent or the array is empty.
static std::vector<std::string> extractStringArray(const std::string& json,
                                                    const std::string& key) {
    std::vector<std::string> result;
    auto it = json.find("\"" + key + "\"");
    if (it == std::string::npos) return result;
    auto bracket = json.find('[', it);
    if (bracket == std::string::npos) return result;
    auto end = json.find(']', bracket);
    if (end == std::string::npos) return result;
    std::string arr = json.substr(bracket + 1, end - bracket - 1);
    // Iterate through quoted strings inside the array
    size_t pos = 0;
    while (pos < arr.size()) {
        auto q1 = arr.find('"', pos);
        if (q1 == std::string::npos) break;
        auto q2 = q1 + 1;
        while (q2 < arr.size()) {
            if (arr[q2] == '"' && arr[q2-1] != '\\') break;
            ++q2;
        }
        if (q2 >= arr.size()) break;
        // Unescape the extracted string
        std::string raw = arr.substr(q1 + 1, q2 - q1 - 1);
        std::string val;
        val.reserve(raw.size());
        for (size_t i = 0; i < raw.size(); ++i) {
            if (raw[i] == '\\' && i + 1 < raw.size()) {
                ++i;
                if      (raw[i] == '"')  val += '"';
                else if (raw[i] == '\\') val += '\\';
                else if (raw[i] == 'n')  val += '\n';
                else if (raw[i] == 'r')  val += '\r';
                else if (raw[i] == 't')  val += '\t';
                else                     val += raw[i];
            } else {
                val += raw[i];
            }
        }
        if (!val.empty()) result.push_back(val);
        pos = q2 + 1;
    }
    return result;
}

// ─── Singleton ───────────────────────────────────────────────────────────────
ConfigManager& ConfigManager::instance() {
    static ConfigManager inst;
    return inst;
}

// ─── Defaults ────────────────────────────────────────────────────────────────
std::string ConfigManager::defaultConfigPath() {
    const char* home = std::getenv("HOME");
    if (!home) home = "/tmp";
    return std::string(home) + "/.config/smart-downloads-organizer/config.json";
}

std::string ConfigManager::defaultDatabasePath() {
    const char* home = std::getenv("HOME");
    if (!home) home = "/tmp";
    return std::string(home) + "/.local/share/smart-downloads-organizer/sdo.db";
}

std::string ConfigManager::defaultLogPath() {
    const char* home = std::getenv("HOME");
    if (!home) home = "/tmp";
    return std::string(home) + "/.local/share/smart-downloads-organizer/sdo.log";
}

AppConfig ConfigManager::defaultConfig() {
    AppConfig cfg;
    const char* home = std::getenv("HOME");
    if (!home) home = "/tmp";
    cfg.watchPaths   = { std::string(home) + "/Downloads" };
    cfg.configPath   = defaultConfigPath();
    cfg.databasePath = defaultDatabasePath();
    cfg.logPath      = defaultLogPath();

    // Default rules
    OrganizerRule imagesRule;
    imagesRule.id          = "builtin-images";
    imagesRule.name        = "Images → Pictures";
    imagesRule.description = "Move image files to Pictures folder";
    imagesRule.enabled     = true;
    imagesRule.priority    = 10;
    imagesRule.conditionLogic = "OR";
    for (auto& ext : {"jpg","jpeg","png","gif","bmp","tiff","webp","heic","svg","raw","cr2","nef"}) {
        RuleCondition c;
        c.field = "extension";
        c.op    = "eq";
        c.value = ext;
        imagesRule.conditions.push_back(c);
    }
    imagesRule.action.type            = ActionType::Move;
    imagesRule.action.targetDirectory = std::string(home) + "/Pictures/Downloaded";
    imagesRule.action.createSubfolders= true;
    cfg.rules.push_back(imagesRule);

    OrganizerRule videosRule;
    videosRule.id          = "builtin-videos";
    videosRule.name        = "Videos → Movies";
    videosRule.description = "Move video files to Movies folder";
    videosRule.enabled     = true;
    videosRule.priority    = 10;
    videosRule.conditionLogic = "OR";
    for (auto& ext : {"mp4","mkv","avi","mov","wmv","flv","webm","m4v","ts","mpg","mpeg"}) {
        RuleCondition c;
        c.field = "extension";
        c.op    = "eq";
        c.value = ext;
        videosRule.conditions.push_back(c);
    }
    videosRule.action.type            = ActionType::Move;
    videosRule.action.targetDirectory = std::string(home) + "/Videos/Downloaded";
    cfg.rules.push_back(videosRule);

    OrganizerRule docsRule;
    docsRule.id          = "builtin-documents";
    docsRule.name        = "Documents → Documents";
    docsRule.description = "Move document files to Documents folder";
    docsRule.enabled     = true;
    docsRule.priority    = 10;
    docsRule.conditionLogic = "OR";
    for (auto& ext : {"pdf","doc","docx","xls","xlsx","ppt","pptx","odt","ods","txt","rtf","csv"}) {
        RuleCondition c;
        c.field = "extension";
        c.op    = "eq";
        c.value = ext;
        docsRule.conditions.push_back(c);
    }
    docsRule.action.type            = ActionType::Move;
    docsRule.action.targetDirectory = std::string(home) + "/Documents/Downloaded";
    cfg.rules.push_back(docsRule);

    OrganizerRule archivesRule;
    archivesRule.id          = "builtin-archives";
    archivesRule.name        = "Archives → Archives";
    archivesRule.description = "Move archive files";
    archivesRule.enabled     = true;
    archivesRule.priority    = 10;
    archivesRule.conditionLogic = "OR";
    for (auto& ext : {"zip","tar","gz","bz2","xz","7z","rar","tgz","zst"}) {
        RuleCondition c;
        c.field = "extension";
        c.op    = "eq";
        c.value = ext;
        archivesRule.conditions.push_back(c);
    }
    archivesRule.action.type            = ActionType::Move;
    archivesRule.action.targetDirectory = std::string(home) + "/Downloads/Archives";
    cfg.rules.push_back(archivesRule);

    return cfg;
}

std::vector<CategoryMeta> ConfigManager::defaultCategoryMeta() {
    return {
        { "Images",      "image-x-generic",       "Pictures",    {"jpg","jpeg","png","gif","bmp","tiff","webp","heic","svg","raw","cr2","nef","avif"}, "#4CAF50" },
        { "Videos",      "video-x-generic",        "Videos",      {"mp4","mkv","avi","mov","wmv","flv","webm","m4v","ts","mpg","mpeg","vob"},           "#2196F3" },
        { "Audio",       "audio-x-generic",        "Music",       {"mp3","flac","wav","aac","ogg","wma","m4a","opus","alac","aiff"},                    "#9C27B0" },
        { "Documents",   "x-office-document",      "Documents",   {"pdf","doc","docx","xls","xlsx","ppt","pptx","odt","ods","txt","rtf","csv","md"},    "#FF9800" },
        { "Archives",    "package-x-generic",      "Archives",    {"zip","tar","gz","bz2","xz","7z","rar","tgz","zst","cab","dmg","iso"},              "#795548" },
        { "Code",        "text-x-script",          "Code",        {"py","js","ts","cpp","c","h","java","go","rs","rb","php","cs","swift","kt","sh"},    "#00BCD4" },
        { "Executables", "application-x-executable","Apps",       {"exe","msi","deb","rpm","appimage","flatpak","sh","bat","cmd","apk"},               "#F44336" },
        { "Fonts",       "font-x-generic",         "Fonts",       {"ttf","otf","woff","woff2","eot","fon"},                                            "#607D8B" },
        { "Data",        "application-x-generic",  "Data",        {"json","xml","yaml","yml","sql","db","sqlite","parquet","csv"},                     "#009688" },
        { "Ebooks",      "x-office-document",      "Books",       {"epub","mobi","azw","azw3","fb2","djvu","cbz","cbr"},                               "#8BC34A" },
        { "Torrents",    "application-x-bittorrent","Torrents",   {"torrent","magnet"},                                                                "#FF5722" },
        { "Unknown",     "application-x-generic",  "Misc",        {},                                                                                  "#9E9E9E" },
    };
}

// ─── Load ─────────────────────────────────────────────────────────────────────
bool ConfigManager::load(const std::string& path) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_configPath  = path;
    m_config      = defaultConfig();
    m_config.configPath = path;

    if (!fs::exists(path)) {
        LOG_INFO("Config file not found, using defaults", path);
        return true; // Use defaults
    }

    try {
        std::ifstream ifs(path);
        if (!ifs.is_open()) {
            LOG_ERROR("Cannot open config file", path);
            return false;
        }
        std::ostringstream oss;
        oss << ifs.rdbuf();
        std::string json = oss.str();
        return parseConfig(json);
    } catch (const std::exception& ex) {
        LOG_ERROR("Config load failed", ex.what());
        return false;
    }
}

bool ConfigManager::parseConfig(const std::string& json) {
    m_config.autoOrganize        = extractBool(json, "autoOrganize", false);
    m_config.watchRecursive      = extractBool(json, "watchRecursive", false);
    m_config.enableDuplicateDetect = extractBool(json, "enableDuplicateDetect", true);
    m_config.enableNotifications = extractBool(json, "enableNotifications", true);
    m_config.moveToTrash         = extractBool(json, "moveToTrash", true);
    m_config.simulateMode        = extractBool(json, "simulateMode", false);
    m_config.startMinimized      = extractBool(json, "startMinimized", false);
    m_config.darkMode            = extractBool(json, "darkMode", true);
    m_config.suggestCleanup      = extractBool(json, "suggestCleanup", true);
    m_config.largeSizeThreshold  = static_cast<uint64_t>(extractInt(json, "largeSizeThreshold", 500*1024*1024));
    m_config.oldFileAgeDays      = static_cast<int>(extractInt(json, "oldFileAgeDays", 30));
    m_config.scanIntervalSecs    = static_cast<int>(extractInt(json, "scanIntervalSecs", 5));
    m_config.windowWidth         = static_cast<int>(extractInt(json, "windowWidth", 1280));
    m_config.windowHeight        = static_cast<int>(extractInt(json, "windowHeight", 800));
    // Restore watched paths from JSON; keep defaults only when the key is absent or empty
    auto paths = extractStringArray(json, "watchPaths");
    if (!paths.empty()) {
        m_config.watchPaths = std::move(paths);
    }
    return true;
}

// ─── Save ─────────────────────────────────────────────────────────────────────
bool ConfigManager::save() const {
    if (m_configPath.empty()) return false;
    return saveAs(m_configPath);
}

bool ConfigManager::saveAs(const std::string& path) const {
    try {
        fs::path p(path);
        fs::create_directories(p.parent_path());

        std::ofstream ofs(path);
        if (!ofs.is_open()) {
            LOG_ERROR("Cannot write config file", path);
            return false;
        }
        ofs << serializeConfig();
        LOG_INFO("Config saved", path);
        return true;
    } catch (const std::exception& ex) {
        LOG_ERROR("Config save failed", ex.what());
        return false;
    }
}

std::string ConfigManager::serializeConfig() const {
    std::ostringstream oss;
    const auto& c = m_config;
    oss << "{\n";
    oss << "  \"autoOrganize\": "          << jsonBool(c.autoOrganize)          << ",\n";
    oss << "  \"watchRecursive\": "        << jsonBool(c.watchRecursive)        << ",\n";
    oss << "  \"enableDuplicateDetect\": " << jsonBool(c.enableDuplicateDetect) << ",\n";
    oss << "  \"enableNotifications\": "  << jsonBool(c.enableNotifications)   << ",\n";
    oss << "  \"moveToTrash\": "          << jsonBool(c.moveToTrash)           << ",\n";
    oss << "  \"simulateMode\": "         << jsonBool(c.simulateMode)          << ",\n";
    oss << "  \"startMinimized\": "       << jsonBool(c.startMinimized)        << ",\n";
    oss << "  \"darkMode\": "             << jsonBool(c.darkMode)              << ",\n";
    oss << "  \"suggestCleanup\": "       << jsonBool(c.suggestCleanup)        << ",\n";
    oss << "  \"largeSizeThreshold\": "   << jsonUint(c.largeSizeThreshold)    << ",\n";
    oss << "  \"oldFileAgeDays\": "       << jsonInt(c.oldFileAgeDays)         << ",\n";
    oss << "  \"scanIntervalSecs\": "     << jsonInt(c.scanIntervalSecs)       << ",\n";
    oss << "  \"windowWidth\": "          << jsonInt(c.windowWidth)            << ",\n";
    oss << "  \"windowHeight\": "         << jsonInt(c.windowHeight)           << ",\n";
    oss << "  \"watchPaths\": [";
    for (size_t i = 0; i < c.watchPaths.size(); ++i) {
        if (i) oss << ", ";
        oss << jsonStr(c.watchPaths[i]);
    }
    oss << "]\n";
    oss << "}\n";
    return oss.str();
}

// ─── Rule management ─────────────────────────────────────────────────────────
bool ConfigManager::addRule(OrganizerRule rule) {
    std::lock_guard<std::mutex> lk(m_mutex);
    // Check no duplicate id
    auto it = std::find_if(m_config.rules.begin(), m_config.rules.end(),
                           [&](const OrganizerRule& r){ return r.id == rule.id; });
    if (it != m_config.rules.end()) return false;
    m_config.rules.push_back(std::move(rule));
    return true;
}

bool ConfigManager::updateRule(const OrganizerRule& rule) {
    std::lock_guard<std::mutex> lk(m_mutex);
    for (auto& r : m_config.rules) {
        if (r.id == rule.id) { r = rule; return true; }
    }
    return false;
}

bool ConfigManager::deleteRule(const std::string& id) {
    std::lock_guard<std::mutex> lk(m_mutex);
    auto it = std::remove_if(m_config.rules.begin(), m_config.rules.end(),
                             [&](const OrganizerRule& r){ return r.id == id; });
    if (it == m_config.rules.end()) return false;
    m_config.rules.erase(it, m_config.rules.end());
    return true;
}

bool ConfigManager::reorderRules(const std::vector<std::string>& orderedIds) {
    std::lock_guard<std::mutex> lk(m_mutex);
    std::vector<OrganizerRule> reordered;
    for (const auto& id : orderedIds) {
        auto it = std::find_if(m_config.rules.begin(), m_config.rules.end(),
                               [&](const OrganizerRule& r){ return r.id == id; });
        if (it != m_config.rules.end()) reordered.push_back(*it);
    }
    if (reordered.size() != m_config.rules.size()) return false;
    m_config.rules = std::move(reordered);
    return true;
}

bool ConfigManager::addWatchPath(const std::string& path) {
    std::lock_guard<std::mutex> lk(m_mutex);
    auto& wp = m_config.watchPaths;
    if (std::find(wp.begin(), wp.end(), path) != wp.end()) return false;
    wp.push_back(path);
    return true;
}

bool ConfigManager::removeWatchPath(const std::string& path) {
    std::lock_guard<std::mutex> lk(m_mutex);
    auto& wp = m_config.watchPaths;
    auto it  = std::remove(wp.begin(), wp.end(), path);
    if (it == wp.end()) return false;
    wp.erase(it, wp.end());
    return true;
}

void ConfigManager::setChangeCallback(std::function<void()> cb) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_changeCb = std::move(cb);
}

void ConfigManager::notifyChanged() {
    if (m_changeCb) m_changeCb();
}

} // namespace SDO
