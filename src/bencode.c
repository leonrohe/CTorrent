#include "bencode.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>


static void bencode_free(BNode* node) {
    if(!node) return;

    switch (node->type)
    {
        case BDICT:
            for(size_t i = 0; i < node->value.bdict.len; ++i) {
                free(node->value.bdict.keys[i].data);
                bencode_free(node->value.bdict.values[i]);
            }
            free(node->value.bdict.keys);
            free(node->value.bdict.values);
            break;
        case BLIST:
            for(size_t i = 0; i < node->value.blist.len; ++i) {
                bencode_free(node->value.blist.items[i]);
            }
            free(node->value.blist.items);
            break;
        case BSTRING:
            free(node->value.bstring.data);
            break;
        case BINT:
            break;
    }
    free(node);
};

static BNode* bencode_parse_any(FILE* f);

static BNode* bencode_parse_string(FILE* f) {
    BNode* result = NULL;
    char* str_data = NULL;
    
    if(!f) goto cleanup;

    // read in Length of encoded string
    bool found_delim = false;
    char str_len_buf[BENC_MAX_LOOKAHEAD] = {0};    
    for(size_t i = 0; i < BENC_MAX_LOOKAHEAD-1; i++) {
        int c = fgetc(f);
        if(c == EOF) goto cleanup;
        if(c == BENC_DELIMITER) {
            found_delim = true;
            break;
        };
        if(c < '0' || c > '9') goto cleanup;

        str_len_buf[i] = c;
    }
    if(!found_delim) goto cleanup;
    
    // convert str to long
    char* end;
    long len = strtol(str_len_buf, &end, 10);
    if (*end != '\0' || len < 0) goto cleanup;
    if(len > BENC_MAX_STRSIZE) goto cleanup;

    // read in encoded string data
    str_data = malloc(len);
    if(!str_data) goto cleanup;

    size_t read = fread(str_data, sizeof(char), len, f);
    if(read != len) goto cleanup;

    result = malloc(sizeof(*result));
    if(!result) goto cleanup;

    result->type = BSTRING;
    result->value.bstring = (BString){ .len = len, .data = str_data };

    // return the valid BNode
    return result;

cleanup:
    if(str_data) free(str_data);
    if(result) free(result);
    return NULL;
}

static BNode* bencode_parse_dict(FILE* f) {
    BNode* result = NULL;
    BString* keys = NULL;
    BNode** values = NULL;
    size_t capacity = 0;
    size_t len = 0;

    if(!f) goto cleanup;
    if(fgetc(f) != BENC_DICT_START) goto cleanup;

    // parse all key-value pairs
    int c;
    while(1) {
        c = fgetc(f);
        if(c == EOF) goto cleanup;
        if(c == BENC_TERMINATOR) break;
        ungetc(c, f);

        BNode* key_node = bencode_parse_string(f);
        if(!key_node) goto cleanup;

        BNode* value_node = bencode_parse_any(f);
        if(!value_node) {
            bencode_free(key_node);
            goto cleanup;
        }

        if(len >= capacity) {
            size_t new_cap = capacity == 0 ? 8 : capacity * 2;
            
            BString* new_keys = realloc(keys, new_cap*sizeof(BString));
            if(!new_keys) {
                bencode_free(key_node);
                bencode_free(value_node);
                goto cleanup;
            }
            keys = new_keys;
            
            BNode** new_values = realloc(values, new_cap*sizeof(BNode*));
            if(!new_values) {
                bencode_free(key_node);
                bencode_free(value_node);
                goto cleanup;
            }
            values = new_values;

            capacity = new_cap;
        }

        keys[len] = key_node->value.bstring;
        free(key_node);
        values[len] = value_node;
        len++;
    }

    // create BNode
    result = malloc(sizeof(*result));
    if(!result) goto cleanup;

    result->type = BDICT;
    result->value.bdict = (BDict){.len = len, .keys = keys, .values = values};

    // return valid BNode
    return result;
cleanup:
    if(keys) {
        for(size_t i = 0; i < len; i++) {
            free(keys[i].data);  // Free each key's string data
        }
        free(keys);
    }
    if(values) {
        for(size_t i = 0; i < len; i++) {
            bencode_free(values[i]);  // Free each value node
        }
        free(values);
    }
    free(result);
    return NULL;
}

static BNode* bencode_parse_list(FILE* f) {
    BNode* result = NULL;
    BNode** items = NULL;
    size_t capacity = 0;
    size_t len = 0;

    if(!f) goto cleanup;
    if(fgetc(f) != BENC_LIST_START) goto cleanup;

    // parse all list children
    int c;
    while(1) {
        c = fgetc(f);
        if(c == EOF) goto cleanup;
        if(c == BENC_TERMINATOR) break;
        ungetc(c, f);

        BNode* item = bencode_parse_any(f);
        if(!item) goto cleanup;

        // grow array if needed
        if(len >= capacity) {
            size_t new_cap = capacity == 0 ? 8 : capacity * 2;
            BNode** new_items = realloc(items, new_cap*sizeof(BNode*));
            if(!new_items) {
                bencode_free(item);
                goto cleanup;
            }
            items = new_items;
            capacity = new_cap;
        }

        items[len++] = item;
    }

    // create BNode
    result = malloc(sizeof(*result));
    if(!result) goto cleanup;

    result->type = BLIST;
    result->value.blist = (BList){.len = len, .items = items};

    // return valid BNode
    return result;

cleanup:
    if(items) {
        for(size_t i = 0; i < len; i++) {
            bencode_free(items[i]);
        }
        free(items);
    }
    if(result) free(result);
    return NULL;
}

static BNode* bencode_parse_int(FILE* f) {
    BNode* result = NULL;
    
    if(!f) goto cleanup;
    if(fgetc(f) != BENC_INT_START) goto cleanup;

    // read in integer string
    bool found_terminator = false;
    char integer_buf[BENC_MAX_LOOKAHEAD] = {0};
    for(size_t i = 0; i < BENC_MAX_LOOKAHEAD-1; i++) {
        int c = fgetc(f);
        if(c == EOF) goto cleanup;
        if(c == BENC_TERMINATOR) {
            found_terminator = true;
            break;
        }

        integer_buf[i] = c;
    }
    if(!found_terminator) goto cleanup;
    if(integer_buf[0] == '0' && integer_buf[1] != '\0') goto cleanup;
    if(integer_buf[0] == '-' && integer_buf[1] == '0') goto cleanup;

    // convert integer string to long
    char* end;
    long value = strtol(integer_buf, &end, 10);
    if (*end != '\0') goto cleanup;

    // create BNode
    result = malloc(sizeof(*result));
    if(!result) goto cleanup;

    result->type = BINT;
    result->value.bint = (BInt){.value = value};

    // return valid BNode
    return result;
cleanup:
    if(result) free(result);
    return NULL;
}

static BNode* bencode_parse_any(FILE* f) {
    if(!f) return NULL;
    
    int c = fgetc(f);
    if(c == EOF) return NULL;
    ungetc(c, f);

    switch (c) {
        case BENC_DICT_START:
            return bencode_parse_dict(f);
        
        case BENC_LIST_START:
            return bencode_parse_list(f);

        case BENC_INT_START:
            return bencode_parse_int(f);

        default:
            return bencode_parse_string(f);
    }
}


BNode* bencode_parse_torrent(const char* fpath) {
    FILE* f = fopen(fpath, "rb");
    if(!f) return NULL;

    BNode* root = bencode_parse_dict(f);

    fclose(f);
    return root;
}

void bencode_print_recursive(const BNode* node, size_t indent) {
    if (!node) return;

    printf("%*s", (int)indent, "");

    switch (node->type) {
    case BSTRING:
        if(node->value.bstring.len >= 100) { //assume binary blob
            printf("<blob>...</blob>\n");
        } else {
            printf("String: %zu, %.*s\n",
                node->value.bstring.len,
                (int)node->value.bstring.len,
                node->value.bstring.data);
        }
        break;

    case BINT:
        printf("Integer: %ld\n", node->value.bint.value);
        break;

    case BLIST:
        printf("List:\n");
        for (size_t i = 0; i < node->value.blist.len; ++i) {
            bencode_print_recursive(node->value.blist.items[i], indent + BENC_PRINT_INDENT);
        }
        break;

    case BDICT:
        printf("Dict:\n");
        const size_t len = node->value.bdict.len;
        for(size_t i = 0; i < len; ++i) {
            const BString key = node->value.bdict.keys[i];
            const BNode* value = node->value.bdict.values[i];
            
            printf("%*s", (int)indent+BENC_PRINT_INDENT, "");
            printf("%.*s:\n", (int)key.len, key.data);
            bencode_print_recursive(value, indent+2*BENC_PRINT_INDENT);
        }
        break;
    }
}