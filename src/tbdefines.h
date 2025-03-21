#ifndef _TBDEFINES_H_
#define _TBDEFINES_H_

#include <assert.h>
#ifdef __cplusplus
#include <atomic>
#else
#include <stdatomic.h>
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef TB_NO_STDBOOL
#typedef uint8 bool
#else
#include <stdbool.h>
#endif
#include "tbprobe.h"

#define TB_PIECES 7
#define TB_HASHBITS (TB_PIECES < 7 ? 11 : 12)
#define TB_MAX_PIECE (TB_PIECES < 7 ? 254 : 650)
#define TB_MAX_PAWN (TB_PIECES < 7 ? 256 : 861)
#define TB_MAX_SYMS 4096

#ifndef _WIN32
#include <fcntl.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#define SEP_CHAR ':'
#define FD int
#define FD_ERR -1
typedef size_t map_t;
#else
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#define SEP_CHAR ';'
#define FD HANDLE
#define FD_ERR INVALID_HANDLE_VALUE
typedef HANDLE map_t;
#endif

// This must be after the inclusion of Windows headers, because otherwise
// std::byte conflicts with "byte" in rpcndr.h . The error occurs if C++
// standard is at lest 17, as std::byte was introduced in C++17.
#ifdef __cplusplus
using namespace std;
#endif

#define DECOMP64

// Threading support
#ifndef TB_NO_THREADS
#if defined(__cplusplus) && (__cplusplus >= 201103L)

#include <mutex>
#define LOCK_T std::mutex
#define LOCK_INIT(x)
#define LOCK_DESTROY(x)
#define LOCK(x) x.lock()
#define UNLOCK(x) x.unlock()

#else
#ifndef _WIN32
#define LOCK_T pthread_mutex_t
#define LOCK_INIT(x) pthread_mutex_init(&(x), NULL)
#define LOCK_DESTROY(x) pthread_mutex_destroy(&(x))
#define LOCK(x) pthread_mutex_lock(&(x))
#define UNLOCK(x) pthread_mutex_unlock(&(x))
#else
#define LOCK_T HANDLE
#define LOCK_INIT(x)                        \
    do {                                    \
        x = CreateMutex(NULL, FALSE, NULL); \
    } while (0)
#define LOCK_DESTROY(x) CloseHandle(x)
#define LOCK(x) WaitForSingleObject(x, INFINITE)
#define UNLOCK(x) ReleaseMutex(x)
#endif

#endif
#else /* TB_NO_THREADS */
#define LOCK_T int
#define LOCK_INIT(x)    /* NOP */
#define LOCK_DESTROY(x) /* NOP */
#define LOCK(x)         /* NOP */
#define UNLOCK(x)       /* NOP */
#endif

// population count implementation
#undef TB_SOFTWARE_POP_COUNT

#if defined(TB_CUSTOM_POP_COUNT)
#define popcount(x) TB_CUSTOM_POP_COUNT(x)
#elif defined(TB_NO_HW_POP_COUNT)
#define TB_SOFTWARE_POP_COUNT
#elif defined(__GNUC__) && defined(__x86_64__) && defined(__SSE4_2__)
#include <popcntintrin.h>
#define popcount(x) (int)_mm_popcnt_u64((x))
#elif defined(_MSC_VER) && (_MSC_VER >= 1500) && defined(_M_AMD64)
#include <nmmintrin.h>
#define popcount(x) (int)_mm_popcnt_u64((x))
#else
// try to use a builtin
#if defined(__has_builtin)
#if __has_builtin(__builtin_popcountll)
#define popcount(x) __builtin_popcountll((x))
#else
#define TB_SOFTWARE_POP_COUNT
#endif
#else
#define TB_SOFTWARE_POP_COUNT
#endif
#endif

#ifdef TB_SOFTWARE_POP_COUNT
// Not a recognized compiler/architecture that has popcount, and
// no builtin available: fall back to a software popcount. This one
// is still reasonably fast.
static inline unsigned tb_software_popcount(uint64_t x) {
    x = x - ((x >> 1) & 0x5555555555555555ull);
    x = (x & 0x3333333333333333ull) + ((x >> 2) & 0x3333333333333333ull);
    x = (x + (x >> 4)) & 0x0f0f0f0f0f0f0f0full;
    return (x * 0x0101010101010101ull) >> 56;
}
#define popcount(x) tb_software_popcount(x)
#endif

// LSB (least-significant bit) implementation
#ifdef TB_CUSTOM_LSB
#define lsb(b) TB_CUSTOM_LSB(b)
#else
#if defined(__GNUC__)
static inline unsigned lsb(uint64_t b) {
    assert(b != 0);
    return __builtin_ffsll(b) - 1;
}
#elif defined(_MSC_VER)
static inline unsigned lsb(uint64_t b) {
    assert(b != 0);
    DWORD index;
#ifdef _WIN64
    _BitScanForward64(&index, b);
    return (unsigned)index;
#else
    if (b & 0xffffffffULL) {
        _BitScanForward(&index, (unsigned long)(b & 0xffffffffULL));
        return (unsigned)index;
    } else {
        _BitScanForward(&index, (unsigned long)(b >> 32));
        return 32 + (unsigned)index;
    }
#endif
}
#else
/* not a compiler/architecture with recognized builtins */
static uint32_t get_bit32(uint64_t x) {
    return (uint32_t)(((int32_t)(x)) & -((int32_t)(x)));
}
static const unsigned MAGIC32 = 0xe89b2be;
static const uint32_t MagicTable32[32] = {
    31, 0, 9,  1,  10, 20, 13, 2,  7,  11, 21, 23, 17, 14, 3,  25,
    30, 8, 19, 12, 6,  22, 16, 24, 29, 18, 5,  15, 28, 4,  27, 26};
static unsigned lsb(uint64_t b) {
    if (b & 0xffffffffULL)
        return MagicTable32[(get_bit32(b & 0xffffffffULL) * MAGIC32) >> 27];
    else
        return MagicTable32[(get_bit32(b >> 32) * MAGIC32) >> 27] + 32;
}
#endif
#endif

#define max(a, b) a > b ? a : b
#define min(a, b) a < b ? a : b

#include "stdendian.h"

#if _BYTE_ORDER == _BIG_ENDIAN

/* (unused)
static uint64_t from_le_u64(uint64_t input) {
  return bswap64(input);
}
*/

static uint32_t from_le_u32(uint32_t input) { return bswap32(input); }

static uint16_t from_le_u16(uint16_t input) { return bswap16(input); }

static uint64_t from_be_u64(uint64_t x) { return x; }

static uint32_t from_be_u32(uint32_t x) { return x; }

/* (unused)
  static uint16_t from_be_u16(uint16_t x) {
  return x;
 }*/

#else

/* (unused)
static uint64_t from_le_u64(uint64_t x) {
  return x;
}
*/

static uint32_t from_le_u32(uint32_t x) { return x; }

static uint16_t from_le_u16(uint16_t x) { return x; }

static uint64_t from_be_u64(uint64_t input) { return bswap64(input); }

static uint32_t from_be_u32(uint32_t input) { return bswap32(input); }

/* (unused)
static uint16_t from_be_u16(const uint16_t input) {
  return bswap16(input);
}
*/

#endif

inline static uint32_t read_le_u32(void* p) {
    return from_le_u32(*(uint32_t*)p);
}

inline static uint16_t read_le_u16(void* p) {
    return from_le_u16(*(uint16_t*)p);
}

static size_t file_size(FD fd) {
#ifdef _WIN32
    LARGE_INTEGER fileSize;
    if (GetFileSizeEx(fd, &fileSize) == 0) {
        return 0;
    }
    return (size_t)fileSize.QuadPart;
#else
    struct stat buf;
    if (fstat(fd, &buf)) {
        return 0;
    } else {
        return buf.st_size;
    }
#endif
}

#ifndef TB_NO_THREADS
static LOCK_T tbMutex;
#endif
static int initialized = 0;
static int numPaths = 0;
static char* pathString = NULL;
static char** paths = NULL;

static FD open_tb(const char* str, const char* suffix) {
    int i;
    FD fd;
    char* file;

    for (i = 0; i < numPaths; i++) {
        file =
            (char*)malloc(strlen(paths[i]) + strlen(str) + strlen(suffix) + 2);
        strcpy(file, paths[i]);
#ifdef _WIN32
        strcat(file, "\\");
#else
        strcat(file, "/");
#endif
        strcat(file, str);
        strcat(file, suffix);
#ifndef _WIN32
        fd = open(file, O_RDONLY);
#else
#ifdef _UNICODE
        wchar_t ucode_name[4096];
        size_t len;
        mbstowcs_s(&len, ucode_name, 4096, file, _TRUNCATE);
        /* use FILE_FLAG_RANDOM_ACCESS because we are likely to access this file
           randomly, so prefetch is not helpful. See
           https://github.com/official-stockfish/Stockfish/pull/1829 */
        fd = CreateFile(ucode_name, GENERIC_READ, FILE_SHARE_READ, NULL,
                        OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS, NULL);
#else
        fd = CreateFile(file, GENERIC_READ, FILE_SHARE_READ, NULL,
                        OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS, NULL);
#endif
#endif
        free(file);
        if (fd != FD_ERR) {
            return fd;
        }
    }
    return FD_ERR;
}

static void close_tb(FD fd) {
#ifndef _WIN32
    close(fd);
#else
    CloseHandle(fd);
#endif
}

static void* map_file(FD fd, map_t* mapping) {
#ifndef _WIN32
    struct stat statbuf;
    if (fstat(fd, &statbuf)) {
        perror("fstat");
        close_tb(fd);
        return NULL;
    }
    *mapping = statbuf.st_size;
    void* data = mmap(NULL, statbuf.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        perror("mmap");
        return NULL;
    }
#ifdef POSIX_MADV_RANDOM
    /* Advise the kernel that we are likely to access this data
       region randomly, so prefetch is not helpful. See
       https://github.com/official-stockfish/Stockfish/pull/1829 */
    posix_madvise(data, statbuf.st_size, POSIX_MADV_RANDOM);
#endif
#else
    DWORD size_low, size_high;
    size_low = GetFileSize(fd, &size_high);
    HANDLE map =
        CreateFileMapping(fd, NULL, PAGE_READONLY, size_high, size_low, NULL);
    if (map == NULL) {
        fprintf(stderr, "CreateFileMapping() failed, error = %lu.\n",
                GetLastError());
        return NULL;
    }
    *mapping = (map_t)map;
    void* data = (void*)MapViewOfFile(map, FILE_MAP_READ, 0, 0, 0);
    if (data == NULL) {
        fprintf(stderr, "MapViewOfFile() failed, error = %lu.\n",
                GetLastError());
    }
#endif
    return data;
}

#ifndef _WIN32
static void unmap_file(void* data, map_t size) {
    if (!data) return;
    if (munmap(data, size) != 0) {
        perror("munmap");
    }
}
#else
static void unmap_file(void* data, map_t mapping) {
    if (!data) return;
    if (!UnmapViewOfFile(data)) {
        fprintf(stderr, "unmap failed, error code %lu\n", GetLastError());
    }
    if (!CloseHandle((HANDLE)mapping)) {
        fprintf(stderr, "CloseHandle failed, error code %lu\n", GetLastError());
    }
}
#endif

#define poplsb(x) ((x) & ((x) - 1))

int TB_MaxCardinality = 0, TB_MaxCardinalityDTM = 0;
unsigned TB_LARGEST = 0;
// extern int TB_CardinalityDTM;

static const char* tbSuffix[] = {".rtbw", ".rtbm", ".rtbz"};
static uint32_t tbMagic[] = {0x5d23e871, 0x88ac504b, 0xa50c66d7};

enum { WDL, DTM, DTZ };
enum { PIECE_ENC, FILE_ENC, RANK_ENC };

#endif

