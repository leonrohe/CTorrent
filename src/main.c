#include <stdio.h>

#include "bencode.h"
#include "cryptography.h"

int main(int argc, char** argv) {
    if(argc != 2) {
        printf("Invalid number of arguments!");
        return 1;
    }

    BNode* root = bencode_parse_torrent(argv[1]);
    BEncodeBuf* info_buf = bencode_encode_node(
        bencode_find_node_by_key(root, "info")
    );
    sha1hash info_hash = sha1(
        (const uint8_t*)info_buf->data,
        info_buf->len
    );
    bencode_free_node(root);
    bencode_free_buf(info_buf);
    
    print_sha1(info_hash);

    return 0;
}