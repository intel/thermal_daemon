#include <errno.h>
#include <inttypes.h>
#include <linux/input.h>
#include <sys/types.h>

int  lzma_decompress(
        unsigned char *dest,
        size_t *destLen,
        const unsigned char *src,
        size_t srcLen
);
