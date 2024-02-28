#include <jbinit.h>
#include <stdint.h>
#include "patch_dyld/plooshfinder.h"
#include "patch_dyld/plooshfinder32.h"
#include "patch_dyld/macho_defs.h"
#include "patch_dyld/macho.h"
#include "patch_dyld/patches/platform/patch.h"

#define platform_check_symbol "____ZNK5dyld39MachOFile24forEachSupportedPlatformEU13block_pointerFvNS_8PlatformEjjE_block_invoke"
#define start_symbol "start"

#define LC_UUID         0x1b
struct uuid_command {
    uint32_t    cmd;            /* LC_UUID */
    uint32_t    cmdsize;        /* sizeof(struct uuid_command) */
    uint8_t     uuid[16];       /* the 128-bit uuid */
};

void *dyld_buf;
size_t dyld_len;
int platform = 0;

void platform_check_patch() {
    // this patch tricks dyld into thinking everything is for the current platform
    struct nlist_64 *forEachSupportedPlatform = macho_find_symbol(dyld_buf, platform_check_symbol);

    void *func_addr = dyld_buf + forEachSupportedPlatform->offset;
    uint64_t func_len = macho_get_symbol_size(forEachSupportedPlatform);

    patch_platform_check(dyld_buf, func_addr, func_len, platform);
}

bool has_found_dyld_in_cache = false;
bool patch_dyld_in_cache(struct pf_patch32_t *patch, uint32_t *stream) {
    char* env_name = pf_follow_xref(dyld_buf, &stream[2]);
    char* env_value = pf_follow_xref(dyld_buf, &stream[6]);

    if (!env_name || !env_value) return false;
    if (strcmp(env_name, "DYLD_IN_CACHE") != 0 || strcmp(env_value, "0") != 0)
        return false;

    stream[5] = 0xd503201f; /* nop */
    stream[8] = 0x52800000; /* mov w0, #0 */
    has_found_dyld_in_cache = true;
    return true;
}

void dyld_in_cache_patch(void* buf) {
    struct mach_header_64 *header = buf;

    uint32_t matches[] = {
        0xaa1303e0, // mov x0, x19
        0x94000000, // bl dyld4::KernelArgs::findEnvp
        0x90000001, // adrp x1, "DYLD_IN_CACHE"@PAGE
        0x91000021, // add x1, "DYLD_IN_CACHE"@PAGEOFF
        0x94000000, // bl __simple_getenv
        0xb4000000, // cbz x0, ...
        0x90000001, // adrp x1, "0"@PAGE
        0x91000021, // add x1, "0"@PAGEOFF
        0x94000000, // bl strcmp
        0x34000000  // cbz w0, ...
    };
    uint32_t masks[] = {
        0xffffffff,
        0xfc000000,
        0x9f00001f,
        0xffc003ff,
        0xfc000000,
        0xff00001f,
        0x9f00001f,
        0xffc003ff,
        0xfc000000,
        0xff00001f
    };
    struct pf_patch32_t dyld_in_cache = pf_construct_patch32(matches, masks, sizeof(matches) / sizeof(uint32_t), (void *) patch_dyld_in_cache);
    struct pf_patch32_t patches[] = {
        dyld_in_cache
    };
    struct pf_patchset32_t patchset = pf_construct_patchset32(patches, sizeof(patches) / sizeof(struct pf_patch32_t), (void *) pf_find_maskmatch32);
    struct nlist_64 *start = macho_find_symbol(dyld_buf, start_symbol);
    void *func_addr = dyld_buf + start->offset;
    uint64_t func_len = macho_get_symbol_size(start);

    pf_patchset_emit32(func_addr, func_len, patchset);

    if (!has_found_dyld_in_cache) {
        LOG("failed to find dyld-in-cache");
        spin();
    }
    LOG("Patched dyld-in-cache\n");
}

void patch_dyld() {
    LOG("Plooshi(TM) libDyld64Patcher starting up...\n");
    LOG("patching dyld...\n");
    
    if (!dyld_buf) {
        LOG("refusing to patch dyld buf at NULL\n");
        spin();   
    }
    uint32_t magic = macho_get_magic(dyld_buf);
    if (!magic) {
        LOG("detected corrupted dyld\n");
        spin();
    }
    void *orig_dyld_buf = dyld_buf;
    if (magic == 0xbebafeca) {
        dyld_buf = macho_find_arch(dyld_buf, CPU_TYPE_ARM64);
        if (!dyld_buf) {
            LOG("detected unsupported or invalid dyld architecture\n");
            spin();
        }
    }
    platform = macho_get_platform(dyld_buf);
    if (platform == 0) {
        LOG("detected unsupported or invalid platform\n");
        spin();
    }
    platform_check_patch();
    struct section_64* cstring = macho_find_section(dyld_buf, "__TEXT", "__cstring");
    if (!cstring) {
        LOG("failed to find dyld cstring\n");
        spin();
    }
    if (memmem(macho_va_to_ptr(dyld_buf, cstring->addr), cstring->size, "DYLD_IN_CACHE", sizeof("DYLD_IN_CACHE"))) {
        dyld_in_cache_patch(dyld_buf);
    }
    LOG("done patching dyld\n");
}

void get_and_patch_dyld(void) {
    dyld_buf = read_file("/usr/lib/dyld", &dyld_len);
    patch_dyld();
    write_file("/cores/dyld", dyld_buf, dyld_len);
}
