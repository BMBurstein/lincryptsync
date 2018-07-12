#include <utility>
#include <filesystem>
#include <string>
#include <map>
#include <queue>
#include <set>

namespace fs = std::filesystem;

class DirWatcher {
public:
    DirWatcher(fs::path directory);
    ~DirWatcher();

    enum class DirEventTypes {
        CREATE,
        DELETE,
        MODIFY,
    };

    struct DirEvent {
        fs::path path;
        DirEventTypes evType;
        bool isDir;
    };

    bool getEvent(DirEvent& ev);

    void ignore(std::string path);

private:
    int fd;
    std::map<int, fs::path> wd;
    std::queue<DirEvent> events;
    std::set<std::string> ignoreList;

    void check();
    void addNotify(fs::path const& directory);
};
