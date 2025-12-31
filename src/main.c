#include <stdio.h>

#include "bencode.h"

int main(int argc, char** argv) {
    if(argc != 2) {
        printf("Invalid number of arguments!");
        return 1;
    }

    BNode* torrent = bencode_parse_torrent(argv[1]);
    bencode_print_recursive(torrent, 0);

    return 0;
}