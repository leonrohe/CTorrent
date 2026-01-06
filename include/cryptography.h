#include <stdint.h>

typedef struct sha1hash {
    uint8_t bytes[20];
} sha1hash;

sha1hash sha1(const uint8_t* message, size_t message_len);

void print_sha1(sha1hash hash);