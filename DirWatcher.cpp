#include "DirWatcher.hpp"

#include <filesystem>
#include <stdexcept>
#include <unistd.h>

#include <sys/inotify.h>

DirWatcher::DirWatcher(std::string directory)
  : fd(inotify_init1(IN_NONBLOCK))
{
    if(fd == -1) throw std::runtime_error("init");

    int i = inotify_add_watch(fd, directory.c_str(),
                              IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVE |
                              IN_ONLYDIR | IN_DONT_FOLLOW | IN_EXCL_UNLINK );
    wd[i] = directory;

    for(auto const& f : fs::recursive_directory_iterator(std::move(directory))) {
        if(fs::is_directory(f)) {
            i = inotify_add_watch(fd, f.path().c_str(),
                                  IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVE |
                                  IN_ONLYDIR | IN_DONT_FOLLOW | IN_EXCL_UNLINK );
            wd[i] = f.path();
        }
    }
}

DirWatcher::~DirWatcher() {
    close(fd);
}

bool DirWatcher::getEvent(DirEvent& ev) {
    if(events.empty()) {
        check();
    }
    if(events.empty()) {
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

            auto it = ignoreList.find(e.path);
            if(it != ignoreList.end()) {
                ignoreList.erase(it);
                continue;
            }

            e.isDir = event->mask & IN_ISDIR;

            if(e.isDir) {
                switch(e.evType) {
                    case DirEventTypes::CREATE:
                        int i = inotify_add_watch(fd, e.path.c_str(),
                                                  IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVE |
                                                  IN_ONLYDIR | IN_DONT_FOLLOW | IN_EXCL_UNLINK );
                        wd[i] = e.path;
                        break;
                }
            }

            events.push(std::move(e));
        }
    }
}

void DirWatcher::ignore(std::string path) {
    ignoreList.emplace(std::move(path));
}
