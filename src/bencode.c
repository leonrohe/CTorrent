#include "bencode.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#pragma region Decoding

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

static BNode* bencode_decode_any(FILE* f);

static BNode* bencode_decode_string(FILE* f) {
    BNode* result = NULL;
    char* str_data = NULL;
    
    if(!f) goto cleanup;

    // read in Length of encoded string
    size_t len_buf_size = 0;
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
        len_buf_size++;
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
    result->value.bstring = (BString){ .pre_delim_len = len_buf_size, .post_delim_len = len, .data = str_data };

    // return the valid BNode
    return result;

cleanup:
    if(str_data) free(str_data);
    if(result) free(result);
    return NULL;
}

static BNode* bencode_decode_dict(FILE* f) {
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

        BNode* key_node = bencode_decode_string(f);
        if(!key_node) goto cleanup;

        BNode* value_node = bencode_decode_any(f);
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

static BNode* bencode_decode_list(FILE* f) {
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

        BNode* item = bencode_decode_any(f);
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

static BNode* bencode_decode_int(FILE* f) {
    BNode* result = NULL;
    
    if(!f) goto cleanup;
    if(fgetc(f) != BENC_INT_START) goto cleanup;

    // read in integer string
    size_t buf_size = 0;
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
        buf_size++;
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
    result->value.bint = (BInt){.len = buf_size, .value = value};

    // return valid BNode
    return result;
cleanup:
    if(result) free(result);
    return NULL;
}

static BNode* bencode_decode_any(FILE* f) {
    if(!f) return NULL;
    
    int c = fgetc(f);
    if(c == EOF) return NULL;
    ungetc(c, f);

    switch (c) {
        case BENC_DICT_START:
            return bencode_decode_dict(f);
        
        case BENC_LIST_START:
            return bencode_decode_list(f);

        case BENC_INT_START:
            return bencode_decode_int(f);

        default:
            return bencode_decode_string(f);
    }
}

#pragma endregion Decoding

#pragma region Encoding

static size_t bencode_get_encoded_size(const BNode* node) {
    size_t result = 0;
    
    switch (node->type)
    {
        case BDICT:
            for(size_t i = 0; i < node->value.bdict.len; ++i) {
                const BString key = node->value.bdict.keys[i];
                result += key.pre_delim_len;                        // chars before ':'
                result += 1;                                        // ':'
                result += key.post_delim_len;                       // chars after ':'

                const BNode* value = node->value.bdict.values[i];
                result += bencode_get_encoded_size(value);
            }
            result += 2;                                            // 'd' and 'e'
            break;
        case BLIST:
            for(size_t i = 0; i < node->value.blist.len; ++i) {
                const BNode* item = node->value.blist.items[i];
                result += bencode_get_encoded_size(item);           // encoded size of child
            }
            result += 2;                                            // 'l' and 'e'
            break;
        case BSTRING:
            result += node->value.bstring.pre_delim_len;            // chars before ':'
            result += 1;                                            // ':'
            result += node->value.bstring.post_delim_len;           // chars after ':'
            break;
        case BINT:
            result += node->value.bint.len;                         // digits of bint + possible '-'
            result += 2;                                            // 'i' and 'e'
            break;
    }
    
    return result;
}

/**
 * Write the BEncoded BNode to the given buffer. This function assumes the buffer is already allocated to the right size.
 * @param node The BNode to BEncode.
 * @param buffer The buffer the BEncoded BNode is written to.
 * @param buffer_size The pre allocated size of the buffer. Used for bounds checks.
 * @param offset The current offset into the buffer.
 * @return Returns the offset into the buffer after finishing the write of the current Node
 */
static size_t bencode_write_node_to_buffer(const BNode* node, char* buffer, size_t buffer_size, size_t offset) { // (TODO) Error handling
    if(offset > buffer_size) return 0;
    if(!node) return offset;

    size_t current_offset = offset;
    
    int written;
    switch (node->type) {
        case BDICT:
            buffer[current_offset++] = 'd';
            for(size_t i = 0; i < node->value.bdict.len; ++i) {
                // encode key
                BString key = node->value.bdict.keys[i];
                written = snprintf(buffer + current_offset,
                                    buffer_size - current_offset,
                                    "%zu",
                                    key.post_delim_len);
                current_offset += written;
                buffer[current_offset++] = ':';
                memcpy(buffer + current_offset,
                        key.data,
                        key.post_delim_len);
                current_offset += key.post_delim_len;
                
                // encode value
                current_offset = bencode_write_node_to_buffer(node->value.bdict.values[i], buffer, buffer_size, current_offset);
            }
            buffer[current_offset++] = 'e';
            break;
        case BLIST:
            buffer[current_offset++] = 'l';
            for(size_t i = 0; i < node->value.blist.len; ++i) {
                current_offset = bencode_write_node_to_buffer(node->value.blist.items[i], buffer, buffer_size, current_offset);
            }
            buffer[current_offset++] = 'e';
            break;
        case BSTRING:
            written = snprintf(buffer + current_offset,
                                buffer_size - current_offset,
                                "%zu",
                                node->value.bstring.post_delim_len);
            current_offset += written;
            buffer[current_offset++] = ':';
            memcpy(buffer + current_offset,
                    node->value.bstring.data,
                    node->value.bstring.post_delim_len);
            current_offset += node->value.bstring.post_delim_len;
            break;
        case BINT:
            buffer[current_offset++] = 'i';
            written = snprintf(buffer + current_offset, 
                                buffer_size - current_offset,
                                "%lld", 
                                node->value.bint.value);
            current_offset += written;
            buffer[current_offset++] = 'e';
            break;
    }
    return current_offset;
}

#pragma endregion Encoding

#pragma region Public

BNode* bencode_parse_torrent(const char* fpath) {
    FILE* f = fopen(fpath, "rb");
    if(!f) return NULL;

    BNode* root = bencode_decode_dict(f);

    fclose(f);
    return root;
}

BEncodeBuf* bencode_encode_node(const BNode* node) {
    BEncodeBuf* result = NULL;
    
    size_t encoded_size = bencode_get_encoded_size(node);
    
    char* encoded_data = malloc(encoded_size);
    if(!encoded_data) goto cleanup;
    bencode_write_node_to_buffer(node, encoded_data, encoded_size, 0);

    result = malloc(sizeof(*result));
    if(!result) goto cleanup;

    result->len = encoded_size;
    result->data = encoded_data;

    return result;

cleanup:
    free(encoded_data);
    free(result);
    return NULL;
}

void bencode_print_recursive(const BNode* node, size_t indent) {
    if (!node) return;

    printf("%*s", (int)indent, "");

    switch (node->type) {
    case BSTRING:
        if(node->value.bstring.post_delim_len >= 100) { //assume binary blob
            printf("<blob>...</blob>\n");
        } else {
            printf("String: %zu, %zu, %.*s\n",
                node->value.bstring.pre_delim_len,
                node->value.bstring.post_delim_len,
                (int)node->value.bstring.post_delim_len,
                node->value.bstring.data);
        }
        break;

    case BINT:
        printf("Integer: %zu, %ld\n", 
            node->value.bint.len,
            node->value.bint.value);
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
            printf("%.*s:\n", (int)key.post_delim_len, key.data);
            bencode_print_recursive(value, indent+2*BENC_PRINT_INDENT);
        }
        break;
    }
}

#pragma endregion Public