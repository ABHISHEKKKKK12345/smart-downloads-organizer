#include "file_analyzer.hpp"
#include "config_manager.hpp"
#include "logger.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <set>
#include <sys/stat.h>
// OpenSSL EVP API (non-deprecated, OpenSSL 3.x compatible)
#include <openssl/evp.h>

namespace fs = std::filesystem;

namespace SDO {

// ─── Singleton ────────────────────────────────────────────────────────────────
FileAnalyzer& FileAnalyzer::instance() {
    static FileAnalyzer inst;
    return inst;
}

FileAnalyzer::FileAnalyzer() {
    buildExtensionMap();
}

// ─── Extension map ────────────────────────────────────────────────────────────
void FileAnalyzer::buildExtensionMap() {
    auto add = [&](FileCategory cat, std::initializer_list<const char*> exts) {
        for (const char* e : exts) m_extMap[e] = cat;
    };

    add(FileCategory::Images, {
        "jpg","jpeg","png","gif","bmp","tiff","tif","webp","heic","heif",
        "svg","raw","cr2","cr3","nef","arw","dng","orf","rw2","pef","avif",
        "ico","psd","ai","xcf","jfif","jp2","jxl","qoi"
    });
    add(FileCategory::Videos, {
        "mp4","mkv","avi","mov","wmv","flv","webm","m4v","ts","mpg","mpeg",
        "vob","3gp","3g2","mxf","f4v","rm","rmvb","asf","m2ts","divx","ogv"
    });
    add(FileCategory::Audio, {
        "mp3","flac","wav","aac","ogg","wma","m4a","opus","alac","aiff",
        "ape","mka","mid","midi","amr","ac3","dts","ra","au","caf","snd"
    });
    add(FileCategory::Documents, {
        "pdf","doc","docx","xls","xlsx","ppt","pptx","odt","ods","odp",
        "txt","rtf","md","markdown","tex","pages","numbers","key","ps","eps",
        "djvu","fodt","fods","fodp"
    });
    add(FileCategory::Archives, {
        "zip","tar","gz","bz2","xz","7z","rar","tgz","zst","lz4","lzma",
        "cab","deb","rpm","pkg","dmg","iso","img","bin","cue","apk","ipa",
        "whl","egg","jar","war","ear"
    });
    add(FileCategory::Code, {
        "py","js","ts","cpp","c","h","hpp","java","go","rs","rb","php",
        "cs","swift","kt","sh","bash","zsh","fish","pl","lua","r","m","mm",
        "html","htm","css","scss","sass","less","jsx","tsx","vue","svelte",
        "json","xml","yaml","yml","toml","ini","conf","cfg","env",
        "sql","graphql","proto","thrift","avsc","wasm","wat"
    });
    add(FileCategory::Executables, {
        "exe","msi","appimage","flatpak","run","elf","com","out","a","so","dll"
    });
    add(FileCategory::Fonts, {
        "ttf","otf","woff","woff2","eot","fon","pfb","pfm","bdf","pcf"
    });
    add(FileCategory::Data, {
        "sqlite","sqlite3","db","parquet","csv","tsv","ndjson","jsonl",
        "avro","msgpack","cbor","arrow","feather","hdf5","h5","nc","mat"
    });
    add(FileCategory::Ebooks, {
        "epub","mobi","azw","azw3","fb2","cbz","cbr","lrf","lit","pdb"
    });
    add(FileCategory::Torrents, { "torrent" });
}

// ─── Helpers ──────────────────────────────────────────────────────────────────
std::string FileAnalyzer::toLower(const std::string& s) const {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c){ return std::tolower(c); });
    return r;
}

FileCategory FileAnalyzer::categorize(const std::string& extension, const std::string&) {
    std::string ext = toLower(extension);
    if (!ext.empty() && ext[0] == '.') ext = ext.substr(1);
    auto it = m_extMap.find(ext);
    return (it != m_extMap.end()) ? it->second : FileCategory::Unknown;
}

// ─── MIME detection via magic bytes ──────────────────────────────────────────
std::string FileAnalyzer::detectMimeType(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return "application/octet-stream";

    uint8_t magic[16] = {};
    f.read(reinterpret_cast<char*>(magic), sizeof(magic));
    size_t n = static_cast<size_t>(f.gcount());

    if (n >= 4 && magic[0]=='%' && magic[1]=='P' && magic[2]=='D' && magic[3]=='F')
        return "application/pdf";
    if (n >= 8 && magic[0]==0x89 && magic[1]=='P' && magic[2]=='N' && magic[3]=='G')
        return "image/png";
    if (n >= 2 && magic[0]==0xFF && magic[1]==0xD8)
        return "image/jpeg";
    if (n >= 4 && magic[0]=='P' && magic[1]=='K' && magic[2]==0x03 && magic[3]==0x04)
        return "application/zip";
    if (n >= 6 && magic[0]=='G' && magic[1]=='I' && magic[2]=='F')
        return "image/gif";
    if (n >= 3 && ((magic[0]==0xFF && (magic[1]&0xE0)==0xE0)
                || (magic[0]=='I' && magic[1]=='D' && magic[2]=='3')))
        return "audio/mpeg";
    if (n >= 8 && magic[4]=='f' && magic[5]=='t' && magic[6]=='y' && magic[7]=='p')
        return "video/mp4";
    if (n >= 4 && magic[0]==0x1A && magic[1]==0x45 && magic[2]==0xDF && magic[3]==0xA3)
        return "video/x-matroska";
    if (n >= 4 && magic[0]=='7' && magic[1]=='z' && magic[2]==0xBC && magic[3]==0xAF)
        return "application/x-7z-compressed";
    if (n >= 4 && magic[0]=='R' && magic[1]=='a' && magic[2]=='r' && magic[3]=='!')
        return "application/x-rar-compressed";
    if (n >= 12 && magic[0]=='R' && magic[1]=='I' && magic[2]=='F' && magic[3]=='F'
               && magic[8]=='W' && magic[9]=='E' && magic[10]=='B' && magic[11]=='P')
        return "image/webp";
    if (n >= 4 && magic[0]==0x7F && magic[1]=='E' && magic[2]=='L' && magic[3]=='F')
        return "application/x-elf";
    // fLaC
    if (n >= 4 && magic[0]=='f' && magic[1]=='L' && magic[2]=='a' && magic[3]=='C')
        return "audio/flac";
    // OGG
    if (n >= 4 && magic[0]=='O' && magic[1]=='g' && magic[2]=='g' && magic[3]=='S')
        return "audio/ogg";
    // BMP
    if (n >= 2 && magic[0]=='B' && magic[1]=='M')
        return "image/bmp";
    // TIFF
    if (n >= 4 && ((magic[0]=='I' && magic[1]=='I' && magic[2]==42 && magic[3]==0)
                || (magic[0]=='M' && magic[1]=='M' && magic[2]==0  && magic[3]==42)))
        return "image/tiff";

    // Fallback: text-based extension check
    fs::path p(path);
    std::string ext = toLower(p.extension().string());
    if (!ext.empty() && ext[0] == '.') ext = ext.substr(1);

    static const std::set<std::string> textExts = {
        "txt","md","csv","json","xml","yaml","yml","html","htm","css",
        "js","ts","py","c","h","cpp","hpp","java","sh","bash","rs","go",
        "toml","ini","conf","cfg","env","sql","graphql","proto","rb","pl"
    };
    if (textExts.count(ext)) return "text/plain";

    return "application/octet-stream";
}

// ─── Quick analysis (no hash) ─────────────────────────────────────────────────
FileInfo FileAnalyzer::analyzeQuick(const std::string& path) {
    FileInfo fi;
    fi.path       = path;
    fi.detectedAt = std::chrono::system_clock::now();

    try {
        fs::path p(path);
        std::error_code ec;
        if (!fs::exists(p, ec) || ec) return fi;

        fi.filename  = p.filename().string();
        fi.extension = toLower(p.extension().string());
        if (!fi.extension.empty() && fi.extension[0] == '.') fi.extension = fi.extension.substr(1);

        fi.sizeBytes = fs::file_size(p, ec);
        if (ec) fi.sizeBytes = 0;

        fi.category = categorize(fi.extension);

        struct stat st{};
        if (::stat(path.c_str(), &st) == 0) {
            fi.modifiedAt = std::chrono::system_clock::from_time_t(st.st_mtime);
            fi.createdAt  = std::chrono::system_clock::from_time_t(st.st_ctime);
            fi.accessedAt = std::chrono::system_clock::from_time_t(st.st_atime);
        } else {
            fi.modifiedAt = fi.createdAt = fi.accessedAt = fi.detectedAt;
        }

        // Basic status flags
        const auto& cfg = ConfigManager::instance().config();
        auto ageDays = fi.ageInDays();
        if (fi.sizeBytes >= cfg.largeSizeThreshold)  fi.status = FileStatus::Large;
        else if (ageDays  >= cfg.oldFileAgeDays)      fi.status = FileStatus::Old;
        else                                          fi.status = FileStatus::Normal;

    } catch (const std::exception& ex) {
        LOG_WARN("analyzeQuick failed", ex.what(), path);
    }
    return fi;
}

// ─── Full analysis (with hash) ────────────────────────────────────────────────
FileInfo FileAnalyzer::analyze(const std::string& path) {
    FileInfo fi = analyzeQuick(path);
    if (fi.filename.empty()) return fi;
    try {
        fi.mimeType   = detectMimeType(path);
        fi.sha256Hash = computeSHA256(path);
    } catch (const std::exception& ex) {
        LOG_WARN("analyze (hash) failed", ex.what(), path);
    }
    return fi;
}

// ─── Hashing — OpenSSL EVP (non-deprecated) ───────────────────────────────────
static std::string evpDigest(const std::string& path, const EVP_MD* md) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return "";

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return "";

    if (EVP_DigestInit_ex(ctx, md, nullptr) != 1) {
        EVP_MD_CTX_free(ctx);
        return "";
    }

    constexpr size_t CHUNK = 65536;
    std::vector<char> buf(CHUNK);
    while (file.read(buf.data(), CHUNK) || file.gcount() > 0) {
        EVP_DigestUpdate(ctx, buf.data(), static_cast<size_t>(file.gcount()));
    }

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int  digestLen = 0;
    EVP_DigestFinal_ex(ctx, digest, &digestLen);
    EVP_MD_CTX_free(ctx);

    std::ostringstream oss;
    for (unsigned int i = 0; i < digestLen; ++i)
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
    return oss.str();
}

std::string FileAnalyzer::computeSHA256(const std::string& path) {
    return evpDigest(path, EVP_sha256());
}

std::string FileAnalyzer::computeMD5(const std::string& path) {
    return evpDigest(path, EVP_md5());
}

std::string FileAnalyzer::sha256(const std::string& path) { return computeSHA256(path); }
std::string FileAnalyzer::md5(const std::string& path)    { return computeMD5(path);    }

std::string FileAnalyzer::hashKey(const FileInfo& fi) const {
    return fi.sha256Hash.empty() ? fi.md5Hash : fi.sha256Hash;
}

std::string FileAnalyzer::formatSize(uint64_t bytes) {
    const char* units[] = {"B","KB","MB","GB","TB"};
    double s = static_cast<double>(bytes);
    int i = 0;
    while (s >= 1024.0 && i < 4) { s /= 1024.0; ++i; }
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(i > 0 ? 1 : 0) << s << " " << units[i];
    return oss.str();
}

const std::map<std::string, FileCategory>& FileAnalyzer::extensionMap() const {
    return m_extMap;
}

// ─── FileInfo method implementations ─────────────────────────────────────────
std::string FileInfo::sizeHuman() const {
    return FileAnalyzer::instance().formatSize(sizeBytes);
}

std::string FileInfo::categoryName() const {
    static const char* names[] = {
        "Images","Videos","Audio","Documents","Archives",
        "Code","Executables","Fonts","Data","Ebooks","Torrents","Unknown"
    };
    int idx = static_cast<int>(category);
    if (idx >= 0 && idx < static_cast<int>(FileCategory::COUNT))
        return names[idx];
    return "Unknown";
}

std::string FileInfo::statusName() const {
    switch (status) {
        case FileStatus::Normal:    return "Normal";
        case FileStatus::Duplicate: return "Duplicate";
        case FileStatus::Large:     return "Large";
        case FileStatus::Old:       return "Old";
        case FileStatus::Orphaned:  return "Orphaned";
        case FileStatus::Temporary: return "Temporary";
        default:                    return "Unknown";
    }
}

int64_t FileInfo::ageInDays() const {
    using namespace std::chrono;
    auto diff = system_clock::now() - modifiedAt;
    return duration_cast<hours>(diff).count() / 24;
}

} // namespace SDO
