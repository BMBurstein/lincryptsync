#include "DirPair.hpp"

#include <algorithm>
#include <cstdlib>
#include <chrono>

using namespace std::chrono_literals;

DirPair::DirPair(fs::path clrPath, fs::path encPath, EncType encType, SyncType syncType)
  : dirs{std::move(clrPath), std::move(encPath)},
    encType(encType),
    syncType(syncType),
    watcher{ DirWatcher(dirs[0].string()), DirWatcher(dirs[1].string()) }
{
    for(auto const& d: dirs) {
        if (!fs::is_directory(d)) {
            fs::create_directories(d);
        }
    }

    switch(encType) {
    case EncType::z7:
        encExt = ".7z"; break;
    }

    fullSync();
}

void DirPair::sync() {
    handleEvents();

    for(auto const& clr : files[0]) {
        auto clrTime = fs::last_write_time(clr.second);
        auto enc = files[1].find(clr.first);
        if(enc != files[1].end()) {
            if(!fs::is_directory(clr.second)) {
                auto encTime = fs::last_write_time(enc->second);
                if(encTime + 1s < clrTime) {
                    encryptFile(clr.first);
                }
                else if (clrTime + 1s < encTime) {
                    decryptFile(clr.first);
                }
            }
            files[1].erase(enc);
        }
        else {
            if(fs::is_directory(clr.second)) {
                fs::create_directories(dirs[1] / clr.first);
            }
            else {
                encryptFile(clr.first);
            }
        }
    }
    files[0].clear();
    for (auto const& enc : files[1]) {
        if(fs::is_directory(enc.second)) {
            fs::create_directories(dirs[0] / enc.first);
        }
        else {
            decryptFile(enc.first);
        }
    }
    files[1].clear();
}

void DirPair::fullSync() {
    files[0].clear();
    files[1].clear();
    for(auto const& f : fs::recursive_directory_iterator(dirs[0])) {
        files[0][normalizeClr(f)] = f;
    }
    for(auto const& f : fs::recursive_directory_iterator(dirs[1])) {
        files[1][normalizeEnc(f)] = f;
    }
    sync();
}

void DirPair::handleEvents() {
    DirWatcher::DirEvent ev;
    while(watcher[0].getEvent(ev)) {
        switch(ev.evType) {
        case DirWatcher::DirEventTypes::CREATE:
            if(ev.isDir) {
                for(auto const& f : fs::recursive_directory_iterator(ev.path)) {
                    files[0][normalizeClr(f)] = f;
                }
            }
            else {
                files[0][normalizeClr(ev.path)] = ev.path;
            }
            break;
        case DirWatcher::DirEventTypes::DELETE:
            fs::remove(ev.path); //TODO
            break;
        case DirWatcher::DirEventTypes ::MODIFY:
            if(ev.isDir) {
                for(auto const& f : fs::recursive_directory_iterator(ev.path)) {
                    auto name = normalizeClr(f);
                    files[0][name] = f;
                    files[1][name] = f;
                }
            }
            else {
                auto name = normalizeClr(ev.path);
                files[0][name] = ev.path;
                files[1][name] = ev.path;
            }
            break;
        }
    }
}

void DirPair::encryptFile(std::string const& name) {
    auto const& path = files[0][name];
    auto encPath = dirs[1] / (name + ".7z");
    auto cmd = "7zr a -y -t7z -ssw -mx9 -mhe=on -m0=lzma2 -mtc=on -w -stl "
               + encPath.string() + " " + path.string();
    std::system(cmd.c_str());
    fs::last_write_time(encPath, fs::last_write_time(path));
    watcher[1].ignore(encPath);
}

void DirPair::decryptFile(std::string const& name) {
    auto const& path = files[1][name];
    auto cmd = "7zr e -y " + path.string() + " -o" + dirs[0].string();
    std::system(cmd.c_str());
    watcher[0].ignore(dirs[0] / normalizeEnc(path));
}

std::string DirPair::normalizeClr(fs::path const& path) {
    return path.lexically_proximate(dirs[0]);
}

std::string DirPair::normalizeEnc(fs::path const& path) {
    auto name = path.lexically_proximate(dirs[1]);
    if(name.extension() == encExt) {
        name.replace_extension();
    }
    return name;
}
