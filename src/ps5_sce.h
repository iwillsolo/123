#pragma once

/*
 * PS5 SCE kernel / dynlib type definitions and function declarations.
 *
 * The ps5-payload-sdk does not ship the full official PlayStation SDK headers,
 * so we define only what rtdump-il2cpp-ps5 needs, based on the PS5 ABI and
 * documented structures used in community payloads (ps5debug-NG, etaHEN, …).
 *
 * Most of these functions are exported by libkernel (libSceLibcInternal/
 * libSceKernel) and are available through the sce_stubs in the sdk.
 */

#include <stdint.h>
#include <stddef.h>

// ─── Types ────────────────────────────────────────────────────────────────────

typedef int32_t SceKernelModule;  // dynlib handle  (-1 = invalid)

/*
 * Per-segment information returned by sceKernelGetModuleInfo.
 * On PS5 the struct has an extra 'flags' dword after 'prot'.
 */
typedef struct {
    void    *address;
    uint32_t size;
    int32_t  prot;
    int32_t  flags;
} SceKernelModuleSegmentInfo;

/*
 * Module information (PS5 compatible).
 * The 'size' field must be initialised to sizeof(SceKernelModuleInfo) before
 * calling sceKernelGetModuleInfo.
 */
typedef struct {
    uint32_t                   size;          // IN: sizeof(SceKernelModuleInfo)
    char                       reserved1[4];
    char                       name[256];
    SceKernelModuleSegmentInfo segmentInfo[4];
    uint32_t                   segmentCount;
    uint8_t                    fingerprint[20];
} SceKernelModuleInfo;

/*
 * Virtual memory page information – returned by sceKernelVirtualQuery.
 */
typedef struct {
    void    *start;
    void    *end;
    uint32_t protection;
    uint32_t memoryType;
    uint32_t isFlexibleMemory;
    uint32_t isDirectMemory;
    uint32_t isStack;
    uint32_t isPooledMemory;
    uint32_t isCommitted;
    char     name[32];
} SceKernelVirtualQueryInfo;

// ─── Constants ────────────────────────────────────────────────────────────────

#define SCE_KERNEL_VQ_FIND_NEXT     0x01
#define SCE_KERNEL_S_IRWU           0000600
#define SCE_OK                      0

// ─── Function declarations ────────────────────────────────────────────────────

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Enumerate loaded modules in the calling process.
 * Returns SCE_OK; actual count is written to *count.
 */
int sceKernelGetModuleList(SceKernelModule *handles, size_t maxCount,
                           size_t *count);

/*
 * Fill *info with details for a loaded module.
 * info->size must be set to sizeof(SceKernelModuleInfo) on entry.
 */
int sceKernelGetModuleInfo(SceKernelModule handle, SceKernelModuleInfo *info);

/*
 * Resolve a symbol in an already-loaded module.
 * Equivalent of dlsym; handle may be 0 for the current process.
 */
int sceKernelDlsym(SceKernelModule handle, const char *symbol, void **address);

/*
 * Load and start a module (PRX/SPRX/ELF) by path.
 * Returns the module handle on success, a negative SCE error code on failure.
 */
SceKernelModule sceKernelLoadStartModule(const char *path,
                                         size_t      argc,
                                         const void *argv,
                                         uint32_t    flags,
                                         void       *pOpt,
                                         int        *pRes);

/*
 * Inspect the virtual memory region that contains 'addr'.
 *  flags = SCE_KERNEL_VQ_FIND_NEXT  → advance to the next region after addr.
 */
int sceKernelVirtualQuery(const void *addr, int flags,
                           SceKernelVirtualQueryInfo *info, size_t infoSize);

/*
 * Send a system notification (toast pop-up).
 */
int sceKernelSendNotificationRequest(int type, const char *msg, size_t msgLen,
                                     int unk);

#ifdef __cplusplus
}
#endif
