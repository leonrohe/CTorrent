#ifndef BENCODE_H
#define BENCODE_H

#include <stdio.h>

#define BENC_MAX_LOOKAHEAD  32
#define BENC_MAX_STRSIZE    100000
#define BENC_PRINT_INDENT   4

#define BENC_DICT_START     'd'
#define BENC_LIST_START     'l'
#define BENC_INT_START      'i'
    
#define BENC_DELIMITER      ':'
#define BENC_TERMINATOR     'e'

typedef enum BTYPE {
    BSTRING,
    BDICT,
    BLIST,
    BINT,
} BTYPE;

typedef struct BNode BNode;

typedef struct BString {
    size_t  pre_delim_len;
    size_t  post_delim_len;
    char*   data;
} BString;

typedef struct BDict {
    size_t      len;
    BString*    keys;
    BNode**     values;
} BDict;

typedef struct BList {
    size_t  len;
    BNode** items;
} BList;

typedef struct BInt {
    size_t len;
    long long value;
} BInt;

struct BNode {
    BTYPE type;
    union {
        BString bstring;
        BDict   bdict;
        BList   blist;
        BInt    bint;
    } value;
};

typedef struct BEncodeBuf {
    size_t len;
    char* data;
} BEncodeBuf;

void bencode_free_node(BNode* node);

/**
 * Parse a torrent file and return the root BNode.
 * @param fpath Path to the torrent file
 * @return Pointer to the root BNode, or NULL on error
 */
BNode* bencode_parse_torrent(const char* fpath);

BNode* bencode_find_node_by_key(const BNode* dict, const char* key);

void bencode_free_buf(BEncodeBuf* buffer);

BEncodeBuf* bencode_encode_node(const BNode* node);

/**
 * Print the Contents of the BNode recursively.
 * @param node The BNode to print recursively
 * @param indent The indentation for the elements printed during this call 
 */
void bencode_print_recursive(const BNode* node, size_t indent);

#endif