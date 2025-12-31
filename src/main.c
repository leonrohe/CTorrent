#include <stdio.h>

#include "bencode.h"

int main(int argc, char** argv) {
    if(argc != 2) {
        printf("Invalid number of arguments!");
        return 1;
    }

    BNode* torrent = bencode_parse_torrent(argv[1]);
    bencode_print_recursive(torrent, 0);

    // DEBUG write to file
    BEncodeBuf* encoded = bencode_encode_node(torrent);
    FILE* f = fopen("out.bin", "wb");
    fwrite(encoded->data, 1, encoded->len, f);
    fclose(f);
    free(encoded->data);
    free(encoded);

    return 0;
}