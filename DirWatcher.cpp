#include "DirWatcher.hpp"

#include <filesystem>
#include <stdexcept>
#include <unistd.h>

#include <sys/inotify.h>

static const int mask = IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVE |
                              IN_ONLYDIR | IN_DONT_FOLLOW | IN_EXCL_UNLINK;

DirWatcher::DirWatcher(fs::path directory)
  : fd(inotify_init1(IN_NONBLOCK))
{
    if(fd == -1) throw std::runtime_error("init");

    if (!fs::is_directory(directory)) {
        fs::create_directories(directory);
    }

    addNotify(directory);
}

DirWatcher::~DirWatcher() {
    close(fd);
}

bool DirWatcher::getEvent(DirEvent& ev) {
    if(events.empty()) {
        check();
    }
    if(events.empty()) {
        ignoreList.clear();
        return false;
    }

    ev = events.front();
    events.pop();

    return true;
}

void DirWatcher::check() {
    while(true) {
        char buf[4096];
        auto len = read(fd, buf, sizeof buf);
        
        if(len < 0) {
            if(errno == EAGAIN)
                break;
            throw std::runtime_error("read");
        }

        const inotify_event* event;
        for (char* ptr = buf; ptr < buf + len; ptr += sizeof(inotify_event) + event->len) {
            event = (const inotify_event*) ptr;

            DirEvent e;
            
            if (event->mask & IN_CREATE || event->mask & IN_MOVED_TO) {
                e.evType = DirEventTypes::CREATE;
            } else if (event->mask & IN_DELETE || event->mask & IN_MOVED_FROM) {
                e.evType = DirEventTypes::DELETE;
            } else if (event->mask & IN_MODIFY) {
                e.evType = DirEventTypes::MODIFY;
            } else if (event->mask & IN_IGNORED) {
                wd.erase(event->wd);
                continue;
            } else {
                continue;
            }

            e.path = wd[event->wd];
            if (event->len)
                e.path /= event->name;

            e.isDir = event->mask & IN_ISDIR;

            if(e.isDir) {
                switch(e.evType) {
                    case DirEventTypes::CREATE:
                        addNotify(e.path);
                        break;
                }
            }

            if(ignoreList.find(e.path) != ignoreList.end()) {
                continue;
            }

            events.push(std::move(e));
        }
    }
}

void DirWatcher::ignore(std::string path) {
    ignoreList.emplace(std::move(path));
}

void DirWatcher::addNotify(fs::path const& directory) {
    int i = inotify_add_watch(fd, directory.c_str(), mask);
    wd[i] = directory;

    for(auto const& f : fs::recursive_directory_iterator(directory)) {
        if(fs::is_directory(f)) {
            i = inotify_add_watch(fd, f.path().c_str(), mask);
            wd[i] = f;
        }
    }
}
