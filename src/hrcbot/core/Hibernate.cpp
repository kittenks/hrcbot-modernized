// Hibernate.cpp
// Runtime resolver for the engine's internal CGameServer::SetHibernating(bool).
// See Hibernate.h for background.  All comments are English on purpose.

#include "Hibernate.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>

#include <eiface.h>

#include "HrcEngine.h"
#include "Tools.h"

#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#elif defined(__linux__)
#  include <dlfcn.h>
#  include <unistd.h>
#  include <fcntl.h>
#endif

namespace hrc
{

namespace
{

typedef void (*HibernateFnSysV)(void *self, bool bHibernating);

#if defined(_WIN32) && !defined(_WIN64)
// MSVC x86 __thiscall: this in ecx, the bool argument is pushed on the stack
// and the callee cleans it (the engine function ends with `ret 4`).
typedef void (__thiscall *HibernateFnWin32)(void *self, bool bHibernating);
#endif

HibernateFnSysV g_setHibernating = NULL;
bool g_resolveTried = false;
bool g_resolveOk = false;

// IDA-style code pattern, "??" is a wildcard byte.
struct Pattern { const char *sig; };

#if defined(_WIN32) && defined(_WIN64)
// x64 Windows HL2DM (rel_multiplayer 64-bit rerelease).
static const char *kPatterns[] = {
    "\x48\x89\x5C\x24\x18\x56\x48\x83\xEC\x40\x8B\x05"
    "\x00\x00\x00\x00"
    "\x0F\xB6\xDA\x48\x8B\xF1\xA8\x01\x75\x17\x83\xC8\x01",
    NULL
};
// Byte mask: 'x' matches, '?' wildcards.
static const char *kMasks[] = {
    "xxxxxxxxxxxx????xxxxxxxxxxxxx",
    NULL
};
#elif defined(_WIN32)
// x86 Windows HL2DM.
static const char *kPatterns[] = {
    "\x55\x8B\xEC\xA1\x00\x00\x00\x00\x53\x8B\xD9\xA8\x01\x75\x14\x83\xC8\x01",
    NULL
};
static const char *kMasks[] = {
    "xxxx????xxxxxxxxxx",
    NULL
};
#elif defined(__linux__)
// Linux signatures are filled in from the shipping engine_srv.so /
// engine(.so).  They are kept empty until resolved; on Linux we additionally
// try to bind the (occasionally exported) symbol by name.
static const char *kPatterns[] = { NULL };
static const char *kMasks[] = { NULL };
static const char *kSymbolCandidates[] = {
    "_ZN11CGameServer16SetHibernatingEb",
    "_ZN11CGameServer16SetHibernatingEh",
    NULL
};
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
    // Read SizeOfImage out of the mapped PE headers.
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
    unsigned long bestStart = 0;
    unsigned long bestEnd = 0;
    char line[1024];
    while (fgets(line, sizeof(line), f))
    {
        // We need the engine server library, e.g. bin/linux64/engine_srv.so
        // or the legacy bin/engine_srv.so / engine.so.
        if (!strstr(line, "engine"))
            continue;
        if (!strstr(line, ".so"))
            continue;
        unsigned long start = 0, end = 0;
        if (sscanf(line, "%lx-%lx", &start, &end) != 2)
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
            if (mask[j] == 'x' && base[i + j] !=
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

} // namespace

bool HibernateResolve()
{
    if (g_resolveTried)
        return g_resolveOk;
    g_resolveTried = true;

    ModuleRange mod = GetEngineModule();
    if (!mod.base || mod.size == 0)
    {
        HRC_WARN("Hibernate: engine module not found, bots may not spawn on an "
                 "empty server.\n");
        return false;
    }

#if defined(__linux__)
    // Prefer an exported symbol when the build keeps one.
    for (int i = 0; kSymbolCandidates[i]; ++i)
    {
        void *p = dlsym(RTLD_DEFAULT, kSymbolCandidates[i]);
        if (p)
        {
            g_setHibernating = reinterpret_cast<HibernateFnSysV>(p);
            g_resolveOk = true;
            break;
        }
    }
#endif

    if (!g_resolveOk)
    {
        for (int i = 0; kPatterns[i]; ++i)
        {
            unsigned char *hit =
                ScanMask(mod.base, mod.size, kPatterns[i], kMasks[i]);
            if (hit)
            {
                g_setHibernating =
                    reinterpret_cast<HibernateFnSysV>(hit);
                g_resolveOk = true;
                break;
            }
        }
    }

    if (g_resolveOk)
    {
        HRC_MSG("Hibernate: server wake support resolved.\n");
    }
    else
    {
        HRC_WARN("Hibernate: could not locate the engine wake routine; bots "
                 "may fail to spawn on an empty server.\n");
    }
    return g_resolveOk;
}

void HibernateWake()
{
    if (!g_resolveTried)
        HibernateResolve();
    if (!g_resolveOk)
        return;
    void *self = static_cast<void *>(g_engine);
    if (!self)
        return;
#if defined(_WIN32) && !defined(_WIN64)
    HibernateFnWin32 fn =
        reinterpret_cast<HibernateFnWin32>(
            reinterpret_cast<void *>(g_setHibernating));
    fn(self, false);
#else
    g_setHibernating(self, false);
#endif
}

bool HibernateAvailable()
{
    return g_resolveOk;
}

} // namespace hrc
