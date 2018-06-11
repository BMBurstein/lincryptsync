#include "DirWatcher.hpp"

#include <filesystem>
#include <map>
#include <string>
#include <set>

namespace fs = std::filesystem;

enum class EncType {
    None,
    z7,
    gpg,
};

enum class SyncType {
    Both,
    Clr2Enc,
    Enc2Clr,
};

class DirPair {
public:
    DirPair(fs::path clrPath,
            fs::path encPath,
            EncType encType = EncType::None,
            SyncType syncType = SyncType::Both
    );

    void sync();
    void fullSync();

private:
    fs::path dirs[2];
    EncType  encType;
    SyncType syncType;
    std::map<std::string, fs::path> files[2];
    std::string encExt;
    DirWatcher watcher[2];

    void handleEvents();

    void encryptFile(fs::path const& path);
    void decryptFile(fs::path const& path);

    fs::path makeClrPath(fs::path const& encPath);
    fs::path makeEncPath(fs::path const& clrPath);

    std::string normalizeClr(fs::path const&);
    std::string normalizeEnc(fs::path const&);
};
