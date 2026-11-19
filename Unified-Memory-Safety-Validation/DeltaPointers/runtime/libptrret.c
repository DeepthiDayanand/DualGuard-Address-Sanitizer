#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <locale.h>

#include "noinstrument.h"
#include "addrspace.h"

FILE *deltapointers_stderr = NULL;
FILE *deltapointers_stdout = NULL;
FILE *deltapointers_stdin = NULL;

/* Special version of strlen that determines size of buffer, i.e. strlen + 1 or
 * 0 if given buffer is NULL. */
size_t NOINSTRUMENT(strsize_nullsafe)(char *str) {
    return str ? strlen(str) + 1 : 0;
}

char *NOINSTRUMENT(strtok)(char *str, const char *delim) {
    static uintptr_t base_ptr;
    static size_t base_size;
    char *maskedptr = (char*)((uintptr_t)str & PRESERVE_MASK);
    char *maskeddelim = (char*)((uintptr_t)delim & PRESERVE_MASK);

    if (maskedptr) {
        base_ptr = (uintptr_t)maskedptr;
        base_size = (uintptr_t)str >> ADDRSPACE_BITS;
    }

    uintptr_t strtok_ret = (uintptr_t)strtok(maskedptr, maskeddelim);
    if (!strtok_ret)
        return NULL;

    /* If the base pointer had no metadata, don't add it not to break stuff */
    if (!base_size)
        return (char*)strtok_ret;

    uintptr_t newsize = (base_size + (strtok_ret - base_ptr)) << ADDRSPACE_BITS;
    strtok_ret &= ADDRSPACE_MASK;
    return (char*)(strtok_ret | newsize);
}

char *NOINSTRUMENT(strtok_ubound)(char *str, const char *delim) {
    static uintptr_t endptr;
    char *maskedptr = (char*)((uintptr_t)str & ADDRSPACE_MASK);
    char *maskeddelim = (char*)((uintptr_t)delim & ADDRSPACE_MASK);

    if (maskedptr)
        endptr = (uintptr_t)str & BOUND_MASK_HIGH;

    uintptr_t strtok_ret = (uintptr_t)strtok(maskedptr, maskeddelim);
    if (!strtok_ret)
        return endptr; // instrumented NULL pointer

    return (char*)(strtok_ret | endptr);
}

/*
 * Wrappers for libc functions that return pointers into libc's own data
 * segment, which may be mapped above the 4 GB address-space boundary used by
 * DeltaPointers.  Each wrapper strips the DeltaPointers tag from its arguments,
 * calls the real function, and strdup()s the result into the heap (which
 * libshrink guarantees is always below 4 GB).  DeltaTagProp then applies
 * normal strlen-based tagging to the heap pointer.
 *
 * These are marked NOINSTRUMENT so the pass does not instrument them.
 */
static char *NOINSTRUMENT(strdup_nullsafe)(const char *s) {
    return s ? strdup(s) : NULL;
}

char *NOINSTRUMENT(ctime)(const time_t *timep) {
    const time_t *masked = (const time_t *)((uintptr_t)timep & PRESERVE_MASK);
    return NOINSTRUMENT(strdup_nullsafe)(ctime(masked));
}

char *NOINSTRUMENT(asctime)(const struct tm * timeptr) {
    const struct tm *masked = (const struct tm *)((uintptr_t)timeptr & PRESERVE_MASK);
    return NOINSTRUMENT(strdup_nullsafe)(asctime(masked));
}

char *NOINSTRUMENT(getenv)(const char *name) {
    const char *masked = (const char *)((uintptr_t)name & PRESERVE_MASK);
    return NOINSTRUMENT(strdup_nullsafe)(getenv(masked));
}

char *NOINSTRUMENT(strerror)(int errnum) {
    return NOINSTRUMENT(strdup_nullsafe)(strerror(errnum));
}

char *NOINSTRUMENT(strsignal)(int sig) {
    return NOINSTRUMENT(strdup_nullsafe)(strsignal(sig));
}

char *NOINSTRUMENT(ttyname)(int fd) {
    return NOINSTRUMENT(strdup_nullsafe)(ttyname(fd));
}

const unsigned short **NOINSTRUMENT(__ctype_b_loc)(void) {
    /* Get the real ctype table from libc's TLS */
    const unsigned short **real_tls_ptr = __ctype_b_loc();
    if (!real_tls_ptr || !(*real_tls_ptr))
        return real_tls_ptr;
    
    const unsigned short *ctype_table = *real_tls_ptr;
    
    /* Allocate and copy the table into heap (guaranteed lower 32-bit) */
    size_t ctype_size = 384;  /* 256 chars + 128 for negative indices */
    unsigned short *heap_copy = (unsigned short *)malloc(ctype_size * sizeof(unsigned short));
    if (!heap_copy)
        return real_tls_ptr;  /* Fall back to original if malloc fails */
    
    memcpy(heap_copy, ctype_table, ctype_size * sizeof(unsigned short));
    
    
    /* Return pointer to heap copy stored in static location
     * (required to maintain i.e. **ptr signature) */
    static unsigned short *cached_ptr = NULL;
    cached_ptr = heap_copy;
    return &cached_ptr;
}

struct tm *NOINSTRUMENT(localtime)(const time_t *timep) {
    struct tm *real_result = localtime(timep);
    if (!real_result)
        return NULL;

    struct tm *heap_copy = (struct tm *)malloc(sizeof(struct tm));

    memcpy(heap_copy, real_result, sizeof(struct tm));
    return heap_copy;
}

struct lconv *NOINSTRUMENT(localeconv)() {
    struct lconv *real_result = localeconv();
    if (!real_result)
        return NULL;

    struct lconv *heap_copy = (struct lconv *)malloc(sizeof(struct lconv));
    memcpy(heap_copy, real_result, sizeof(struct lconv));

    heap_copy->decimal_point = real_result->decimal_point ? NOINSTRUMENT(strdup_nullsafe)(real_result->decimal_point) : NULL;
    heap_copy->thousands_sep = real_result->thousands_sep ? NOINSTRUMENT(strdup_nullsafe)(real_result->thousands_sep) : NULL;
    heap_copy->grouping = real_result->grouping ? NOINSTRUMENT(strdup_nullsafe)(real_result->grouping) : NULL;
    heap_copy->int_curr_symbol = real_result->int_curr_symbol ? NOINSTRUMENT(strdup_nullsafe)(real_result->int_curr_symbol) : NULL;
    heap_copy->currency_symbol = real_result->currency_symbol ? NOINSTRUMENT(strdup_nullsafe)(real_result->currency_symbol) : NULL;
    heap_copy->mon_decimal_point = real_result->mon_decimal_point ? NOINSTRUMENT(strdup_nullsafe)(real_result->mon_decimal_point) : NULL;
    heap_copy->mon_thousands_sep = real_result->mon_thousands_sep ? NOINSTRUMENT(strdup_nullsafe)(real_result->mon_thousands_sep) : NULL;
    heap_copy->mon_grouping = real_result->mon_grouping ? NOINSTRUMENT(strdup_nullsafe)(real_result->mon_grouping) : NULL;
    heap_copy->positive_sign = real_result->positive_sign ? NOINSTRUMENT(strdup_nullsafe)(real_result->positive_sign) : NULL;
    heap_copy->negative_sign = real_result->negative_sign ? NOINSTRUMENT(strdup_nullsafe)(real_result->negative_sign) : NULL;

    return heap_copy;
}

void NOINSTRUMENT(replace_globals)(void) {
    /* Initialize deltapointers_* from standard file descriptors.
     * These FDs (0, 1, 2) are always available, regardless of whether
     * the FILE* globals are initialized. */

    /* stdout -> FD 1 */
    int stdout_fd = dup(STDOUT_FILENO);
    if (stdout_fd >= 0) {
        deltapointers_stdout = fdopen(stdout_fd, "w");
        if (deltapointers_stdout) {
            setvbuf(deltapointers_stdout, NULL, _IONBF, 0);  /* Line buffered */
        } else {
            close(stdout_fd);
        }
    }

    /* stderr -> FD 2 */
    int stderr_fd = dup(STDERR_FILENO);
    if (stderr_fd >= 0) {
        deltapointers_stderr = fdopen(stderr_fd, "w");
        if (deltapointers_stderr) {
            setvbuf(deltapointers_stderr, NULL, _IONBF, 0);  /* Line buffered */
        } else {
            close(stderr_fd);
        }
    }

    /* stdin -> FD 0 */
    int stdin_fd = dup(STDIN_FILENO);
    if (stdin_fd >= 0) {
        deltapointers_stdin = fdopen(stdin_fd, "r");
        if (!deltapointers_stdin) {
            close(stdin_fd);
        }
    }

    setvbuf(stdout, NULL, _IONBF, 0);  /* Line buffered */
    setvbuf(stderr, NULL, _IONBF, 0);  /* Line buffered */
}


extern char **environ;
// Masking global environ that is tagged in perlbench
void NOINSTRUMENT(sanitize_environment)() {
    if (environ == NULL) return;

    for (char **env = environ; *env != NULL; env++) {
        uintptr_t tagged_ptr = (uintptr_t)(*env);
        uintptr_t clean_ptr = tagged_ptr & 0x00000000FFFFFFFF;
        *env = (char *)clean_ptr;
    }
}