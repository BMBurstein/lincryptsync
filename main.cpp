#include "DirPair.hpp"

int main() {
    DirPair pair{"/home/baruch/test/clrsync", "/home/baruch/test/cryptsync", EncType::z7};
    pair.sync();
}
