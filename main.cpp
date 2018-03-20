#include <algorithm>
#include <cstdlib>
#include <iostream>
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

struct DirPair {
    fs::path clrDir;
    fs::path encDir;
    EncType  encType;
    SyncType syncType;
    std::map<std::string, fs::directory_entry> clrFiles;
    std::map<std::string, fs::directory_entry> encFiles;
    
    DirPair(fs::path clrPath,
            fs::path encPath,
            EncType encType = EncType::None,
            SyncType syncType = SyncType::Both
    ) : clrDir(std::move(clrPath)),
        encDir(std::move(encPath)),
        encType(encType),
        syncType(syncType)
    {
        if(!fs::is_directory(clrDir)) {
            fs::create_directories(clrDir);
        }
        if(!fs::is_directory(encDir)) {
            fs::create_directories(encDir);
        }
    }

    void scan() {
        clrFiles.clear();
        encFiles.clear();
        for(auto const& f : fs::recursive_directory_iterator(clrDir)) {
            clrFiles[normalizeClr(f)] = f;
        }
        for(auto const& f : fs::recursive_directory_iterator(encDir)) {
            encFiles[normalizeEnc(f)] = f;
        }
    }

    void sync() {
        scan();
        for(auto const& clr : clrFiles) {
            auto clrTime = fs::last_write_time(clr.second);
            auto enc = encFiles.find(clr.first);
            if(enc != encFiles.end()) {
                auto encTime = fs::last_write_time(enc->second);
                if(encTime < clrTime) {
                    encryptFile(clr.first);
                }
                else if (clrTime < encTime) {
                    decryptFile(clr.first);
                }
            }
            else {
                encryptFile(clr.first);
            }
        }
    }

    void encryptFile(std::string name) {
        auto const& path = clrFiles[name].path();
        if(fs::is_directory(path)) {
            fs::create_directories(encDir / name);
        }
        else {
            std::system(("7zr a -y " + (encDir / name).string() + ".7z " + path.string()).c_str());
        }
    }

    void decryptFile(std::string name) {
        auto const& path = encFiles[name].path();
        if(fs::is_directory(path)) {
            fs::create_directories(clrDir / name);
        }
        else {
            std::system(("7zr x -y " + (clrDir / name).string() + ".7z " + path.string()).c_str());
        }
    }

    std::string normalizeClr(fs::path path) {
        return path.string().erase(0, clrDir.string().length());
    }
    std::string normalizeEnc(fs::path path) {
        auto name = path.string().erase(0, encDir.string().length());
        name.erase(name.length() - 3);
        return name;
    }
};

int main() {
    DirPair pair{"~/test/clrsync", "~/test/cryptsync", EncType::z7};
    pair.sync();
}