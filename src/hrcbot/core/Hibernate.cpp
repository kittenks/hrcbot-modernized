// Hibernate.cpp
// Inline detour for the engine's internal CGameServer::SetHibernating(bool).
// See Hibernate.h for background.  All comments are English on purpose.

#include "Hibernate.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>

#include <eiface.h>

#include "HrcEngine.h"
#include "Tools.h"

#ifndef SUBHOOK_STATIC
#define SUBHOOK_STATIC
#endif
#include "../third_party/subhook/subhook.h"

#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#elif defined(__linux__)
#  include <dlfcn.h>
#  include <unistd.h>
#  include <fcntl.h>
#endif

// File-scope state.  These are deliberately outside any namespace so the
// 32-bit MSVC naked thunk can reference them directly from inline assembly.
static volatile int hrc_g_keepAwake = 0;
static subhook_t hrc_g_hook = NULL;
static void *hrc_g_original = NULL;
static bool hrc_g_tried = false;
static bool hrc_g_ok = false;

// Function type of CGameServer::SetHibernating.  On every platform except
// 32-bit Windows the C++ this pointer is passed like a normal first argument
// (stack on Linux x86, rdi on Linux x64, rcx on Windows x64), so an ordinary
// function pointer matches.  32-bit Windows uses the MSVC __thiscall ABI which
// is handled separately with a naked thunk below.  These live at file scope
// so the 32-bit MSVC inline assembler can resolve the global symbols.
typedef void (*HrcSetHibernatingFn)(void *self, bool bHibernating);

#if defined(_WIN32) && !defined(_WIN64)
static __declspec(naked) void HrcDetourWin32()
{
    __asm
    {
        cmp  hrc_g_keepAwake, 0
        je   Lpass
        mov  byte ptr [esp + 4], 0
    Lpass:
        mov  eax, hrc_g_original
        jmp  eax
    }
}
#else
static void HrcDetour(void *self, bool bHibernating)
{
    if (hrc_g_keepAwake && bHibernating)
        bHibernating = false;
    HrcSetHibernatingFn original =
        reinterpret_cast<HrcSetHibernatingFn>(hrc_g_original);
    original(self, bHibernating);
}
#endif

namespace hrc
{

namespace
{

// IDA-style code pattern, '?' bytes are wildcards (encoded as 0x00 here and
// skipped via the mask).
#if defined(_WIN32) && defined(_WIN64)
static const char *kPattern =
    "\x48\x89\x5C\x24\x18\x56\x48\x83\xEC\x40\x8B\x05"
    "\x00\x00\x00\x00"
    "\x0F\xB6\xDA\x48\x8B\xF1\xA8\x01\x75\x17\x83\xC8\x01";
static const char *kMask = "xxxxxxxxxxxx????xxxxxxxxxxxxx";
#elif defined(_WIN32)
static const char *kPattern =
    "\x55\x8B\xEC\xA1\x00\x00\x00\x00\x53\x8B\xD9\xA8\x01\x75\x14\x83\xC8\x01";
static const char *kMask = "xxxx????xxxxxxxxxx";
#elif defined(__linux__) && defined(__x86_64__)
static const char *kPattern =
    "\x55\x48\x89\xE5\x41\x57\x41\x56\x41\x55\x41\x54\x41\x89\xF4\x53"
    "\x48\x89\xFB\x48\x83\xEC\x18"
    "\x0F\xB6\x05\x00\x00\x00\x00"
    "\x84\xC0"
    "\x0F\x84\x00\x00\x00\x00"
    "\x44\x38\xA3\xC8\x42\x05\x00";
static const char *kMask =
    "xxxxxxxxxxxxxxxxxxxxxxx"
    "xxx????"
    "xx"
    "xx????"
    "xxxxxxx";
#elif defined(__linux__)
static const char *kPattern =
    "\x55\x89\xE5\x57\x56\x53\x83\xEC\x2C"
    "\x8B\x7D\x08\x8B\x5D\x0C"
    "\x0F\xB6\x05\x00\x00\x00\x00"
    "\x84\xC0"
    "\x0F\x84\x00\x00\x00\x00"
    "\x38\x9F\x54\xA2\x02\x00";
static const char *kMask =
    "xxxxxxxxxxxxxxx"
    "xxx????"
    "xx"
    "xx????"
    "xxxxxx";
#endif

#if defined(__linux__)
// The shipping Linux libraries keep a full .symtab (not loaded into the
// process, so dlsym usually cannot see it); try it before scanning.
static const char *kSymbolName = "_ZN11CGameServer14SetHibernatingEb";
#endif

struct ModuleRange { unsigned char *base; size_t size; };

#if defined(_WIN32)
static ModuleRange GetEngineModule()
{
    ModuleRange r = { NULL, 0 };
    HMODULE h = GetModuleHandleA("engine.dll");
    if (!h)
        return r;
    unsigned char *base = reinterpret_cast<unsigned char *>(h);
    IMAGE_DOS_HEADER *dos = reinterpret_cast<IMAGE_DOS_HEADER *>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE)
        return r;
    IMAGE_NT_HEADERS *nt =
        reinterpret_cast<IMAGE_NT_HEADERS *>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE)
        return r;
    r.base = base;
    r.size = nt->OptionalHeader.SizeOfImage;
    return r;
}
#else
static ModuleRange GetEngineModule()
{
    ModuleRange r = { NULL, 0 };
    FILE *f = fopen("/proc/self/maps", "r");
    if (!f)
        return r;

    // Use the readable+executable segment of engine_srv.so (modern 64-bit
    // servers sometimes ship an identical engine.so).  Scanning only the
    // executable segment keeps us inside one mapped object.
    unsigned long bestStart = 0;
    unsigned long bestEnd = 0;
    char line[1024];
    while (fgets(line, sizeof(line), f))
    {
        const char *slash = strrchr(line, '/');
        if (!slash)
            continue;
        if (strncmp(slash, "engine_srv.so", 12) != 0 &&
            strncmp(slash, "engine.so", 9) != 0)
            continue;
        unsigned long start = 0, end = 0;
        char perms[8] = {0};
        if (sscanf(line, "%lx-%lx %7s", &start, &end, perms) != 3)
            continue;
        if (perms[0] != 'r' || perms[2] != 'x')
            continue;
        if (bestStart == 0 || start < bestStart)
            bestStart = start;
        if (end > bestEnd)
            bestEnd = end;
    }
    fclose(f);
    if (bestStart && bestEnd > bestStart)
    {
        r.base = reinterpret_cast<unsigned char *>(bestStart);
        r.size = bestEnd - bestStart;
    }
    return r;
}
#endif

static unsigned char *ScanMask(const unsigned char *base, size_t size,
                               const char *pattern, const char *mask)
{
    size_t plen = strlen(mask);
    if (size < plen)
        return NULL;
    for (size_t i = 0; i + plen <= size; ++i)
    {
        bool match = true;
        for (size_t j = 0; j < plen; ++j)
        {
            if (mask[j] == 'x' &&
                base[i + j] !=
                    static_cast<unsigned char>(pattern[j]))
            {
                match = false;
                break;
            }
        }
        if (match)
            return const_cast<unsigned char *>(base + i);
    }
    return NULL;
}

static void *ResolveSetter()
{
#if defined(__linux__)
    void *sym = dlsym(RTLD_DEFAULT, kSymbolName);
    if (sym)
        return sym;
#endif
    ModuleRange mod = GetEngineModule();
    if (!mod.base || mod.size == 0)
        return NULL;
    return ScanMask(mod.base, mod.size, kPattern, kMask);
}

} // namespace

bool HibernateReady()
{
    if (hrc_g_tried)
        return hrc_g_ok;
    hrc_g_tried = true;

    void *setter = ResolveSetter();
    if (!setter)
    {
        HRC_WARN("Hibernate: could not locate the engine wake routine; bots "
                 "may fail to spawn on an empty server.\n");
        return false;
    }

    unsigned flags = SUBHOOK_TRAMPOLINE;
#if defined(_M_X64) || defined(__x86_64__) || defined(__amd64__)
    flags |= SUBHOOK_64BIT_OFFSET;
#endif
#if defined(_WIN32) && (defined(_M_X64) || defined(_M_AMD64))
    flags |= SUBHOOK_TRAMPOLINE_ALLOC_NEARBY;
#endif

#if defined(_WIN32) && !defined(_WIN64)
    void *detour = reinterpret_cast<void *>(&HrcDetourWin32);
#else
    void *detour = reinterpret_cast<void *>(&HrcDetour);
#endif

    hrc_g_hook = subhook_new(setter, detour,
                             static_cast<subhook_flags_t>(flags));
    if (!hrc_g_hook || subhook_install(hrc_g_hook) != 0)
    {
        HRC_WARN("Hibernate: failed to install the wake detour; bots may fail "
                 "to spawn on an empty server.\n");
        return false;
    }

    hrc_g_original = subhook_get_trampoline(hrc_g_hook);
    if (!hrc_g_original)
    {
        HRC_WARN("Hibernate: trampoline unavailable; bots may fail to spawn "
                 "on an empty server.\n");
        return false;
    }

    hrc_g_ok = true;
    HRC_MSG("Hibernate: server wake detour installed.\n");
    return true;
}

void HibernateSetKeepAwake(bool keep)
{
    hrc_g_keepAwake = keep ? 1 : 0;
}

bool HibernateAvailable()
{
    return hrc_g_ok;
}

} // namespace hrc
