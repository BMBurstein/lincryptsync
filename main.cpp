#include "DirPair.hpp"

#include <thread>
#include <chrono>
#include <iostream>
using namespace std::chrono_literals;

int main() {
    DirPair pair{"/home/baruch/test/clrsync", "/home/baruch/test/cryptsync", EncType::z7};
    pair.sync();

    std::cout << "Syncing...\n";
    for(int i=0; i<60; ++i) {
        std::this_thread::sleep_for(2s);
        pair.sync();
    }
}
