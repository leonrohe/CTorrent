#include "cryptography.h"

#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>


static uint32_t rotl32 (uint32_t value, unsigned int count) {
    const unsigned int mask = CHAR_BIT * sizeof(value) - 1;
    count &= mask;
    return (value << count) | (value >> (-count & mask));
}

sha1hash sha1(const uint8_t* message, size_t message_len) {
    uint32_t h0 = 0x67452301;
    uint32_t h1 = 0xEFCDAB89;
    uint32_t h2 = 0x98BADCFE;
    uint32_t h3 = 0x10325476;
    uint32_t h4 = 0xC3D2E1F0;
    
    uint64_t bitlen = message_len << 3;

    /* Pre Processing */
    size_t new_message_len = message_len;
    
    new_message_len +=1;                // account for the appended 0x80 byte
    while(new_message_len % 64 != 56) { // leave room for the 64-bit message len that needs to be appended at the end
        new_message_len++;
    }
    new_message_len += 8;               // account for the added 64-bit message len
    
    assert(new_message_len % 64 == 0);

    uint8_t* new_message = calloc(new_message_len, sizeof(uint8_t));
    if(!new_message) goto fail;

    memcpy(new_message, message, message_len);                                  // copy the old message into the new buffer
    new_message[message_len] = 0x80;                                            // add the 0x80 byte
    for (int i = 0; i < 8; i++) {                                               // add the message len in bits to the end of the new buffer
        new_message[new_message_len - 8 + i] = (bitlen >> (56 - i * 8)) & 0xFF;
    }

    /* Process the message in 512-bit chunks */
    for(size_t i = 0; i < new_message_len; i += 64) {
        
        /* Break chunk into 16 32-bit big endian words */
        uint32_t words[80] = {0};
        for(size_t j = 0; j < 16; ++j) {
            words[j] = (new_message[i + j*4 + 0] << 24) |
                       (new_message[i + j*4 + 1] << 16) |
                       (new_message[i + j*4 + 2] << 8)  |
                       (new_message[i + j*4 + 3] << 0);
        }

        /* Extend the sixteen 32-bit words into eighty 32-bit words */
        for(size_t j = 16; j < 80; ++j) {
            words[j] = rotl32((words[j-3] ^ words[j-8] ^ words[j-14] ^ words[j-16]), 1);
        }

        /* Initialize hash values for this chunk */
        uint32_t a = h0;
        uint32_t b = h1;
        uint32_t c = h2;
        uint32_t d = h3;
        uint32_t e = h4;

        /* Main Loop */
        for(size_t j = 0; j < 80; ++j) {
            uint32_t f, k = 0;
            
            if(j >= 0 && j <= 19) {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999;
            } 
            else if(j >= 20 && j <= 39) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            }
            else if(j >= 40 && j <= 59) {
                f = (b & c) | (b & d) | (c & d); 
                k = 0x8F1BBCDC;
            }
            else if(j >= 60 && j <= 79) {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }

            uint32_t temp = rotl32(a, 5) + f + e + k + words[j];
            e = d;
            d = c;
            c = rotl32(b, 30);
            b = a;
            a = temp;
        }

        h0 = h0 + a;
        h1 = h1 + b; 
        h2 = h2 + c;
        h3 = h3 + d;
        h4 = h4 + e;
    }

    /* Produce the final hash value */
    sha1hash result;
    for(int i = 0; i < 4; i++) {
        result.bytes[0 + i]  = (h0 >> (24 - i * 8)) & 0xFF;
        result.bytes[4 + i]  = (h1 >> (24 - i * 8)) & 0xFF;
        result.bytes[8 + i]  = (h2 >> (24 - i * 8)) & 0xFF;
        result.bytes[12 + i] = (h3 >> (24 - i * 8)) & 0xFF;
        result.bytes[16 + i] = (h4 >> (24 - i * 8)) & 0xFF;
    }
    free(new_message);
    return result;
fail:
    free(new_message);
    return (sha1hash){.bytes = {0}};
}

void print_sha1(sha1hash hash) {
    for(int i = 0; i < 20; i++) {
        printf("%02x", (unsigned int)hash.bytes[i]);
    }
    printf("\n");
}