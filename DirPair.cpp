#include "DirPair.hpp"

#include <algorithm>
#include <cstdlib>
#include <chrono>
#include <iostream>

const unsigned int CLEAR = 0;
const unsigned int CRYPT = 1;
auto const OTHER  = [](unsigned int dir){ return 1 - dir; };

using namespace std::chrono_literals;

DirPair::DirPair(fs::path clrPath, fs::path encPath, EncType encType, SyncType syncType)
  : dirs{std::move(clrPath), std::move(encPath)},
    encType(encType),
    syncType(syncType),
    watcher{ dirs[CLEAR].string(), dirs[CRYPT].string() }
{
    switch(encType) {
    case EncType::z7:
        encExt = ".7z"; break;
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
            std::cerr << int(ev.evType) << ' ' << ev.path << '\n';
        }
    }
}


void DirPair::handleFile(DirType dir, fs::path const& srcPath) {
    auto destPath = makePath(OTHER(dir), srcPath);
    if(fs::is_directory(srcPath)) {
        fs::create_directories(destPath);
    }
    else {
        if (dir == CLEAR) {
            auto cmd = "7zr a -y -t7z -ssw -mx9 -mhe=on -m0=lzma2 -mtc=on -w -stl "
                       + destPath.string() + " " + srcPath.string();
            std::system(cmd.c_str());
            fs::last_write_time(destPath, fs::last_write_time(srcPath));
        } else {
            auto destDir = destPath;
            destDir.remove_filename();
            auto cmd = "7zr e -y " + srcPath.string() + " -o" + destDir.string();
            std::system(cmd.c_str());
        }
    }
    watcher[OTHER(dir)].ignore(destPath);
}

std::string DirPair::normalize(DirType dir, fs::path const& path) {
    auto name = path.lexically_proximate(dirs[dir]);
    if(dir == CRYPT) {
        if (name.extension() == encExt) {
            name.replace_extension();
        }
    }
    return name;
}

fs::path DirPair::makePath(DirType destDir, fs::path const &srcPath, bool* isDir) {
    auto destPath = dirs[destDir] / normalize(OTHER(destDir), srcPath);
    if(destDir == CRYPT) {
        if(isDir ? (!*isDir) : (!fs::is_directory(srcPath))) {
            destPath += ".7z";
        }
    }
    return destPath;
}
