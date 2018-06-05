#include "DirPair.hpp"

#include <thread>
#include <chrono>
using namespace std::chrono_literals;

int main() {
    DirPair pair{"/home/baruch/test/clrsync", "/home/baruch/test/cryptsync", EncType::z7};
    pair.sync();

    for(int i=0; i<30; ++i) {
        std::this_thread::sleep_for(2s);
        pair.sync();
    }
}
