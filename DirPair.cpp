#include "DirPair.hpp"

#include <algorithm>
#include <cstdlib>

DirPair::DirPair(fs::path clrPath, fs::path encPath, EncType encType, SyncType syncType)
  : clrDir(std::move(clrPath)),
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

void DirPair::sync() {
    scan();
    for(auto const& clr : clrFiles) {
        auto clrTime = fs::last_write_time(clr.second);
        auto enc = encFiles.find(clr.first);
        if(enc != encFiles.end()) {
            if(!fs::is_directory(clr.second)) {
                auto encTime = fs::last_write_time(enc->second);
                if(encTime < clrTime) {
                    encryptFile(clr.first);
                }
                else if (clrTime < encTime) {
                    decryptFile(clr.first);
                }
            }
            encFiles.erase(enc);
        }
        else {
            if(fs::is_directory(clr.second)) {
                fs::create_directories(encDir / clr.first);
            }
            else {
                encryptFile(clr.first);
            }
        }
    }
    clrFiles.clear();
    for (auto const& enc : encFiles) {
        if(fs::is_directory(enc.second)) {
            fs::create_directories(clrDir / enc.first);
        }
        else {
            decryptFile(enc.first);
        }
    }
}

void DirPair::scan() {
    clrFiles.clear();
    encFiles.clear();
    for(auto const& f : fs::recursive_directory_iterator(clrDir)) {
        clrFiles[normalizeClr(f)] = f;
    }
    for(auto const& f : fs::recursive_directory_iterator(encDir)) {
        encFiles[normalizeEnc(f)] = f;
    }
}

void DirPair::encryptFile(std::string name) {
    auto const& path = clrFiles[name].path();
    auto encPath = encDir / (name + ".7z");
    auto cmd = "7zr a -y -t7z -ssw -mx9 -mhe=on -m0=lzma2 -mtc=on -w -stl "
               + encPath.string() + " " + path.string();
    std::system(cmd.c_str());
    fs::last_write_time(encPath, fs::last_write_time(path));
}

void DirPair::decryptFile(std::string name) {
    auto const& path = encFiles[name].path();
    auto cmd = "7zr e -y " + path.string() + " -o" + clrDir.string();
    std::system(cmd.c_str());
}

std::string DirPair::normalizeClr(fs::path path) {
    return path.string().erase(0, clrDir.string().length());
}

std::string DirPair::normalizeEnc(fs::path path) {
    auto name = path.string().erase(0, encDir.string().length());
    if(path.extension() == encExt) {
        name.erase(name.length() - encExt.length());
    }
    return name;
}
