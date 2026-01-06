#include <stdio.h>

#include "bencode.h"
#include "cryptography.h"

int main(int argc, char** argv) {
    if(argc != 2) {
        printf("Invalid number of arguments!");
        return 1;
    }

    // BNode* torrent = bencode_parse_torrent(argv[1]);
    // bencode_print_recursive(torrent, 0);

    // // DEBUG write to file
    // BEncodeBuf* encoded = bencode_encode_node(torrent);
    // FILE* f = fopen("out.bin", "wb");
    // fwrite(encoded->data, 1, encoded->len, f);
    // fclose(f);
    // free(encoded->data);
    // free(encoded);

    // sha1hash empty = sha1((const uint8_t*)"", 0);
    // print_sha1(empty);
    // sha1hash abc = sha1((const uint8_t*)"abc", 3);
    // print_sha1(abc);
    // sha1hash fox = sha1((const uint8_t*)"The quick brown fox jumps over the lazy dog", 43);
    // print_sha1(fox);

    return 0;
}