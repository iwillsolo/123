#pragma once

#include <stdint.h>

/*
 * Nibble-level hex helpers used by the byte-pattern scanner.
 *
 * Pattern syntax (same as the PS4 original):
 *   "48 8B 05 ?? ?? ?? ??"
 * Each token is either two hex digits or "??" (wildcard).
 */
#define IN_RANGE(x, a, b)   ((x) >= (a) && (x) <= (b))
#define GET_BITS(x)         (IN_RANGE(((x) & ~0x20), 'A', 'F') \
                              ? (((x) & ~0x20) - 'A' + 0x0A)    \
                              : (IN_RANGE((x), '0', '9') ? (x) - '0' : 0))
#define GET_BYTE(x)         ((GET_BITS((x)[0]) << 4) | GET_BITS((x)[1]))

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Return the base address of the first loaded module whose name contains
 * the sub-string 'name'.  Returns 0 when not found.
 */
uint64_t resolve_module_base(const char *name);

/*
 * Return the total mapped size of the text (executable) segment(s) belonging
 * to the module whose name contains 'name'.  Returns 0 when not found.
 */
int32_t get_module_text_size(const char *name);

/*
 * Scan the text segment of the module named 'module_name' for the byte
 * pattern 'pattern'.  Returns the address of the first match, or 0.
 */
uintptr_t find_pattern(const char *module_name, const char *pattern);

#ifdef __cplusplus
}
#endif
