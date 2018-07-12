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

    typedef unsigned int DirType;

    void handleFile(DirType dir, fs::path const& srcPath);

    fs::path makePath(DirType destDir, fs::path const& srcPath, bool* isDir = nullptr) const;
    std::string normalize(DirType dir, fs::path const&) const;
};
