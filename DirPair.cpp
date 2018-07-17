#include "DirPair.hpp"

#include <algorithm>
#include <cstdlib>
#include <chrono>
#include <iostream>

const unsigned int CLEAR = 0;
const unsigned int CRYPT = 1;
auto const OTHER  = [](unsigned int dir){ return 1 - dir; };

using namespace std::chrono_literals;

static std::string getExt(EncType t) {
    switch(t) {
        case EncType::z7:
            return ".7z";
        case EncType::None:break;
        case EncType::gpg:break;
    }
    return {};
}

DirPair::DirPair(fs::path clrPath, fs::path encPath, std::string password, EncType encType, SyncType syncType)
  : dirs{std::move(clrPath), std::move(encPath)},
    encType(encType),
    syncType(syncType),
    watcher{ dirs[CLEAR], dirs[CRYPT] }
{
    if(!password.empty()) {
        passwd = "-p\"" + password + "\" ";
    }

    fullSync();
}

void DirPair::fullSync() {
    sync();

    for(auto i = CLEAR; i <= CRYPT; ++i) {
        files[i].clear();
        for (auto const &f : fs::recursive_directory_iterator(dirs[i])) {
            files[i][normalize(i, f)] = f;
        }
    }

    for(auto const& clr : files[CLEAR]) {
        auto clrTime = fs::last_write_time(clr.second);
        auto enc = files[CRYPT].find(clr.first);
        if(enc != files[CRYPT].end()) {
            if(!fs::is_directory(clr.second)) {
                auto encTime = fs::last_write_time(enc->second);
                if(encTime + 1s < clrTime) {
                    handleFile(CLEAR, clr.second);
                }
                else if (clrTime + 1s < encTime) {
                    handleFile(CRYPT, enc->second);
                }
            }
            files[CRYPT].erase(enc);
        }
        else {
            handleFile(CLEAR, clr.second);
        }
    }
    files[CLEAR].clear();
    for (auto const& enc : files[1]) {
        handleFile(CRYPT, enc.second);
    }
    files[CRYPT].clear();
}

void DirPair::sync() {
    DirWatcher::DirEvent ev;
    for(auto i = CLEAR; i <= CRYPT; ++i) {
        while (watcher[i].getEvent(ev)) {
            switch (ev.evType) {
                case DirWatcher::DirEventTypes::CREATE:
                case DirWatcher::DirEventTypes::MODIFY:
                    handleFile(i, ev.path);
                    if (ev.isDir) {
                        for (auto const &f : fs::recursive_directory_iterator(ev.path)) {
                            handleFile(i, f);
                        }
                    }
                    break;
                case DirWatcher::DirEventTypes::DELETE:
                    auto path = makePath(OTHER(i), ev.path);
                    fs::remove_all(path);
                    watcher[OTHER(i)].ignore(path);
                    break;
            }
        }
    }
}


void DirPair::handleFile(DirType dir, fs::path const& srcPath) {
    auto destPath = makePath(OTHER(dir), srcPath);
    if(fs::is_directory(srcPath)) {
        fs::create_directories(destPath);
    }
    else {
        switch (encType) {
            case EncType::z7: {
                std::string cmd;
                if (dir == CLEAR) {
                    cmd = "7za a -y -t7z -ssw -mx9 -mhe=on -m0=lzma2 -mtc=on -stl "
                          + passwd + destPath.string() + " " + srcPath.string();

                } else {
                    cmd = "7za e -y " + passwd + srcPath.string() + " -o"
                          + destPath.parent_path().string();
                }
                std::system(cmd.c_str());
            } break;
            case EncType::None: {
                fs::copy_file(srcPath, destPath, fs::copy_options::overwrite_existing);
                fs::last_write_time(destPath, fs::last_write_time(srcPath));
            } break;
        }
    }
    watcher[OTHER(dir)].ignore(destPath);
}

std::string DirPair::normalize(DirType dir, fs::path const& path) const {
    auto name = path.lexically_proximate(dirs[dir]);
    if(dir == CRYPT) {
        if (name.extension() == getExt(encType)) {
            name.replace_extension();
        }
    }
    return name;
}

fs::path DirPair::makePath(DirType destDir, fs::path const &srcPath, bool* isDir) const {
    auto destPath = dirs[destDir] / normalize(OTHER(destDir), srcPath);
    if(destDir == CRYPT) {
        if(isDir ? (!*isDir) : (!fs::is_directory(srcPath))) {
            destPath += getExt(encType);
        }
    }
    return destPath;
}
