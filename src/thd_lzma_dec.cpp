// From ESIF/Products/ESIF_CMP/Sources esif_cmp.c
// Remove white spaces

#include <errno.h>
#include <inttypes.h>
//#include <lzma.h>
#include <linux/input.h>
#include <sys/types.h>
#include "LzmaDec.h"
#include "thd_common.h"

// Duplicate ESIF_CMP/Sources/Alloc.c MyAlloc
void *MyAlloc(size_t size)
{
  if (size == 0)
    return NULL;
  #ifdef _SZ_ALLOC_DEBUG
  {
    void *p = malloc(size);
 //   PRINT_ALLOC("Alloc    ", g_allocCount, size, p);
    return p;
  }
  #else
  return malloc(size);
  #endif
}

void MyFree(void *address)
{
  //PRINT_FREE("Free    ", g_allocCount, address);

  free(address);
}
#define UNUSED_VAR(x) (void)x;

static void *SzAlloc(ISzAllocPtr p, size_t size) { UNUSED_VAR(p); return MyAlloc(size); }
static void SzFree(ISzAllocPtr p, void *address) { UNUSED_VAR(p); MyFree(address); }
const ISzAlloc g_Alloc = { SzAlloc, SzFree };

// Standard LZMA File Header
#define LZMA_PROPS_SIZE 5       // [XX YY YY YY YY] Where XX = Encoded -lc -lp -pb options, YY = Encoded -d option
#pragma pack(push, 1)
struct LzmaHeader {
        unsigned char           properties[LZMA_PROPS_SIZE];// encoded LZMA_PROPS options
        unsigned long long      original_size;                          // original uncompressed data size
};
#pragma pack(pop)

 #define ENCODED_SIGNATURE              {'\x5D','\x00'}

 /* Hardcoded LZMA Compression Property Values (and their LZMA_SDK v18.01 lzma.exe command line equivalents)
 * Items marked with "##" should never be changed since they affect the 5-byte LZMA Properties Header Signature
 * The following parameters correspond to to the ESIF_COMPRESS_SIGNATURE defined in esif_sdk_iface_compress.h,
 * which always maps to [5D 00 XX XX XX] for -lc3 -lp0 -pb2 and -d12 to -d27 lzma.exe options.
 */
#define LZMA_PROPS_LEVEL                        9                       // Compression Level [-a1 = 9]
#define LZMA_PROPS_DICTSIZE                     (1 << 24)       // Dictionary Size [-d24] ##
#define LZMA_PROPS_LITCTXBITS           3                       // Literal Context Bits [-lc3] ##
#define LZMA_PROPS_LITPOSBITS           0                       // Literal Pos Bits [-lp0] ##
#define LZMA_PROPS_NUMPOSBITS           2                       // Number of Pos Bits [-pb2] ##
#define LZMA_PROPS_FASTBYTES            128                     // Number of Fast Bytes [-fb128]
#define LZMA_PROPS_THREADS                      1                       // Number of Threads [-mt1]

#define LZMA_PADDING_MINSIZE            256                     // Minimum Padding Bytes for Compression Buffer
#define LZMA_PADDING_PERCENT            0.05            // Percent Padding Bytes for Compression Buffer (0.0-1.0)
#define LZMA_MAX_COMPRESSED_SIZE        (((size_t)(-1) >> 1) - 1)


int  lzma_decompress(
        unsigned char *dest,
        size_t *destLen,
        const unsigned char *src,
        size_t srcLen
)
{
        int rc = THD_ERROR;
        struct LzmaHeader *header = NULL;

        // NULL dest = Return Required Buffer Size
        if (dest == NULL && destLen && src && srcLen > sizeof(*header)) {
                header = (struct LzmaHeader *)src;
                unsigned char encoded_signature[] = ENCODED_SIGNATURE;

                // Compute Original Decompressed Size if valid Header Properties
                if ((memcmp(header->properties, encoded_signature, sizeof(encoded_signature)) == 0) &&
                        (header->original_size > 0 && header->original_size != (unsigned long long)(-1))) {
                        *destLen = (size_t)header->original_size;
                        rc = 0;
                }
                else {
                        rc = THD_ERROR;
                }
        }
        else if (dest && destLen && src && srcLen > sizeof(*header)) {
                header = (struct LzmaHeader *)src;
                size_t lzmaSrcLen = srcLen - sizeof(*header);

                ELzmaStatus status;
                rc = LzmaDecode(
                        dest,
                        destLen,
                        src + sizeof(*header),
                        &lzmaSrcLen,
                        src,
                        sizeof(*header),
                        LZMA_FINISH_ANY,
                        &status,
                        &g_Alloc);

                // Validate Data not Truncated since LzmaDecode returns OK if destLen too small
                if (*destLen < header->original_size) {
                        rc = THD_ERROR;
                }
                // Bounds Check
                if (*destLen > LZMA_MAX_COMPRESSED_SIZE) {
                        *destLen = LZMA_MAX_COMPRESSED_SIZE;
                        rc = THD_ERROR;
                }
        }
        return rc;
}
