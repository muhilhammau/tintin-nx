/* bionic_shims.c -- Tintin-specific bionic shims for MISSING imports.
 *
 * These are symbols that gen_imports.py reports as MISSING because they are
 * not provided by the runtime's bionic shims. We implement them here so the
 * import table can be generated cleanly.
 *
 * MISSING: cacheflush fgetpos fsetpos rewind
 *
 * cacheflush: use the runtime's dcr_code_flush (code_flush.c)
 * fgetpos/fsetpos/rewind: implement on top of the runtime's bionic FILE funcs
 *   (b_fseek, b_ftell, b_clearerr) which handle b___sF stdin/out/err wrappers.
 *
 * The functions are prefixed with b_ so gen_imports.py finds them (it scans
 * for b_<symbol> patterns).
 *
 * MIT.
 */
#include <stdio.h>
#include <stdint.h>

#include "bionic.h"
#include "code_flush.h"
#include "util.h"

/* --------------------------------------------------------------- b_cacheflush
 *
 * Android's cacheflush syscall: flush instruction cache for a range.
 * On the Switch we use the runtime's dcr_code_flush which does:
 *   1. svcFlushProcessDataCache on the range
 *   2. dcr_icache_invalidate (flips a dedicated code page R <-> RX)
 *
 * The bionic cacheflush signature: int cacheflush(char *addr, int nbytes, int cache);
 * cache values: 0 = ICACHE, 1 = DCACHE, 2 = BCACHE (both)
 * We only care about ICACHE (0) and BCACHE (2) for instruction cache.
 */
int b_cacheflush(void *addr, int nbytes, int cache) {
    (void)cache;
    if (!addr || nbytes <= 0)
        return 0;
    dcr_code_flush(addr, (size_t)nbytes);
    return 0;
}

/* --------------------------------------------------------------- b_fgetpos
 * Guest (bionic 32-bit) fpos_t is a 4-byte offset. newlib's fpos_t is NOT
 * 4 bytes here, so the shims use int32_t explicitly. */
_Static_assert(sizeof(int32_t) == 4, "guest fpos_t must be 4 bytes");

/* defined in runtime/source/bionic_stdio.c (no header) */
extern int  b_fseek(void *fp, long off, int whence);
extern long b_ftell(void *fp);
extern void b_clearerr(void *fp);

int b_fgetpos(FILE *stream, int32_t *pos) {
    if (!stream || !pos) {
        b_fix_errno();
        return -1;
    }
    long p = b_ftell(stream);
    if (p < 0) {
        b_fix_errno();
        return -1;
    }
    *pos = (int32_t)p;
    return 0;
}

/* --------------------------------------------------------------- b_fsetpos */
int b_fsetpos(FILE *stream, const int32_t *pos) {
    if (!stream || !pos) {
        b_fix_errno();
        return -1;
    }
    int r = b_fseek(stream, (long)*pos, SEEK_SET);
    if (r)
        b_fix_errno();
    return r;
}

/* --------------------------------------------------------------- b_rewind */
void b_rewind(FILE *stream) {
    if (!stream)
        return;
    b_fseek(stream, 0, SEEK_SET);
    b_clearerr(stream);
}
