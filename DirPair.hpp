#include <experimental/filesystem>
#include <map>
#include <string>

namespace fs = std::experimental::filesystem;

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

private:
    fs::path clrDir;
    fs::path encDir;
    EncType  encType;
    SyncType syncType;
    std::map<std::string, fs::directory_entry> clrFiles;
    std::map<std::string, fs::directory_entry> encFiles;
    std::string encExt = ".7z";

    void scan();

    void encryptFile(std::string name);
    void decryptFile(std::string name);

    std::string normalizeClr(fs::path path);
    std::string normalizeEnc(fs::path path);
};
