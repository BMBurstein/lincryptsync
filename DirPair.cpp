#include "DirPair.hpp"

#include <algorithm>
#include <cstdlib>
#include <chrono>

using namespace std::chrono_literals;

DirPair::DirPair(fs::path clrPath, fs::path encPath, EncType encType, SyncType syncType)
  : dirs{std::move(clrPath), std::move(encPath)},
    encType(encType),
    syncType(syncType),
    watcher{ dirs[0].string(), dirs[1].string() }
{
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
                    encryptFile(clr.second);
                }
                else if (clrTime + 1s < encTime) {
                    decryptFile(enc->second);
                }
            }
            files[1].erase(enc);
        }
        else {
            encryptFile(clr.second);
        }
    }
    files[0].clear();
    for (auto const& enc : files[1]) {
        decryptFile(enc.second);
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
        case DirWatcher::DirEventTypes::MODIFY:
            if(ev.isDir) {
                for(auto const& f : fs::recursive_directory_iterator(ev.path)) {
                    encryptFile(f);
                }
            }
            else {
                encryptFile(ev.path);
            }
            break;
        case DirWatcher::DirEventTypes::DELETE:
            auto encPath = makeEncPath(ev.path, &ev.isDir);
            fs::remove_all(encPath);
            watcher[1].ignore(encPath);
            break;
        }
    }
}

void DirPair::encryptFile(fs::path const& path) {
    auto encPath = makeEncPath(path);
    if(fs::is_directory(path)) {
        fs::create_directories(encPath);
    }
    else {
        auto cmd = "7zr a -y -t7z -ssw -mx9 -mhe=on -m0=lzma2 -mtc=on -w -stl "
                   + encPath.string() + " " + path.string();
        std::system(cmd.c_str());
        fs::last_write_time(encPath, fs::last_write_time(path));
    }
    watcher[1].ignore(encPath);
}

void DirPair::decryptFile(fs::path const& path) {
    auto clrPath = makeClrPath(path);
    if(fs::is_directory(path)) {
        fs::create_directories(clrPath);
    }
    else {
        clrPath.remove_filename();
        auto cmd = "7zr e -y " + path.string() + " -o" + clrPath.string();
        std::system(cmd.c_str());
    }
    watcher[0].ignore(clrPath);
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

fs::path DirPair::makeClrPath(fs::path const &encPath) {
    return dirs[0] / normalizeEnc(encPath);
}

fs::path DirPair::makeEncPath(fs::path const &clrPath, bool* isDir) {
    auto encPath = dirs[1] / normalizeClr(clrPath);
    if((isDir && !*isDir) || (!isDir && !fs::is_directory(clrPath))) {
        encPath += ".7z";
    }
    return encPath;
}
