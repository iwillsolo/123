#include "stdafx.h"

// ─── Internal helpers ─────────────────────────────────────────────────────────

/*
 * Walk the process module list and return info for the first module whose
 * name contains the sub-string 'name'.
 * Returns true and fills *out_info on success; false otherwise.
 *
 * info.size must be set by the caller.
 */
static bool find_module_info(const char *name, SceKernelModuleInfo *out_info) {
    static const size_t kMaxModules = 256;
    SceKernelModule     handles[kMaxModules];
    size_t              count = 0;

    if (sceKernelGetModuleList(handles, kMaxModules, &count) != SCE_OK) {
        return false;
    }

    for (size_t i = 0; i < count; ++i) {
        SceKernelModuleInfo info;
        info.size = sizeof(SceKernelModuleInfo);

        if (sceKernelGetModuleInfo(handles[i], &info) != SCE_OK) {
            continue;
        }

        if (strstr(info.name, name)) {
            *out_info = info;
            return true;
        }
    }
    return false;
}

// ─── Public API ───────────────────────────────────────────────────────────────

uint64_t resolve_module_base(const char *name) {
    SceKernelModuleInfo info;
    info.size = sizeof(SceKernelModuleInfo);

    if (!find_module_info(name, &info)) {
        return 0;
    }

    // Segment 0 is always the text segment (index 0 = PT_LOAD #0 = .text).
    return reinterpret_cast<uint64_t>(info.segmentInfo[0].address);
}

int32_t get_module_text_size(const char *name) {
    SceKernelModuleInfo info;
    info.size = sizeof(SceKernelModuleInfo);

    if (!find_module_info(name, &info)) {
        return 0;
    }

    int32_t total = 0;
    for (uint32_t i = 0; i < info.segmentCount; ++i) {
        // prot & 4 → execute permission – collect all executable segments.
        // On PS5 executable segments typically have prot = 5 (r-x).
        if (info.segmentInfo[i].prot == 5) {
            total += static_cast<int32_t>(info.segmentInfo[i].size);
        }
    }
    return total;
}

uintptr_t find_pattern(const char *module_name, const char *pattern) {
    const uintptr_t module_start =
        static_cast<uintptr_t>(resolve_module_base(module_name));
    if (module_start == 0) {
        return 0;
    }

    const uintptr_t module_end =
        module_start + static_cast<uintptr_t>(get_module_text_size(module_name));

    // Walk executable pages with sceKernelVirtualQuery (same API as PS4).
    SceKernelVirtualQueryInfo page{};
    const char   *pattern_current = pattern;
    uintptr_t     current_match   = 0;

    auto *cursor = reinterpret_cast<unsigned char *>(module_start);
    auto *limit  = reinterpret_cast<unsigned char *>(module_end);

    while (cursor < limit) {
        int rc = sceKernelVirtualQuery(
            reinterpret_cast<void *>(cursor),
            SCE_KERNEL_VQ_FIND_NEXT,
            &page,
            sizeof(SceKernelVirtualQueryInfo));

        if (rc != SCE_OK || page.protection == 0) {
            // Advance past an unmapped / unreadable page (assume 4 KiB).
            cursor += 0x1000;
            pattern_current = pattern;
            current_match   = 0;
            continue;
        }

        auto *page_start = reinterpret_cast<unsigned char *>(page.start);
        auto *page_end   = reinterpret_cast<unsigned char *>(page.end);
        auto *scan_end   = (page_end < limit) ? page_end - 8 : limit - 8;

        for (auto *addr = page_start; addr < scan_end; ++addr) {
            if (*addr != GET_BYTE(pattern_current) &&
                *pattern_current != '?') {
                // Mismatch – reset.
                current_match   = 0;
                pattern_current = pattern;
                continue;
            }

            if (!current_match) {
                current_match = reinterpret_cast<uintptr_t>(addr);
            }

            pattern_current += 3;               // "XX " or "?? "
            if (pattern_current[-1] == '\0') {  // past the NUL terminator
                return current_match;
            }
        }

        // Advance cursor to the next region.
        cursor = reinterpret_cast<unsigned char *>(
            reinterpret_cast<uintptr_t>(page.start) +
            (reinterpret_cast<uintptr_t>(page.end) -
             reinterpret_cast<uintptr_t>(page.start)));
    }

    return 0;
}
