#include <3ds.h>
#include <string.h>
#include <stdbool.h>

#define CTGP_COMPAT_ERR             ((Result)0xD8E007ED)
#define CTGP_COMPAT_MAP_ERR         ((Result)0xD86007F3)

#define CTGP_3GX_BASE               0x07000000u
#define CTGP_CODE_BASE              0x07000100u
#define CTGP_HEADER_MAGIC           0x24584733u
#define CTGP_MAX_EXE_SIZE           0x00800000u

#define CTGP_MAP_START              0x01000000u
#define CTGP_MAP_END                0x0F000000u

#define CTGP_STARTUP_PAGE           0x07001000u
#define CTGP_STARTUP_LITERAL_OFF    0x450u
#define CTGP_STARTUP_INSN_OFF       0xD0Cu
#define CTGP_STARTUP_OLD_LITERAL    0xC9A8AD0Fu
#define CTGP_STARTUP_OLD_INSN       0xE021C002u
#define CTGP_STARTUP_GOOD_LITERAL   0x30F72D2Bu
#define CTGP_STARTUP_GOOD_INSN      0xE51FC8C4u
#define CTGP_STARTUP_PAGE_FNV1A     0x5093C1D4u

#define CTGP_LATE_INSTALL_PAGE      0x07000000u
#define CTGP_LATE_INSTALL_SITE      0x07000130u
#define CTGP_LATE_INSTALL_OFFSET    0x130u
#define CTGP_LATE_ORIGINAL0         0xE59F0060u
#define CTGP_LATE_ORIGINAL1         0xE5900000u
#define CTGP_LATE_EPILOGUE          0xE8BDDFFFu
#define CTGP_LATE_MAGIC             0x5052545Au
#define CTGP_LATE_REMOTE_FIRST      0x08F00000u
#define CTGP_LATE_REMOTE_LAST       0x08000000u
#define CTGP_LATE_REMOTE_STEP       0x00100000u

#define CTGP_COMMIT_REMOTE_FIRST    0x08E00000u
#define CTGP_COMMIT_REMOTE_LAST     0x07110000u
#define CTGP_COMMIT_REMOTE_STEP     0x00010000u

#define CTGP_POLL_NS                (250LL * 1000LL)
#define CTGP_WORKER_PRIORITY        0x30
#define CTGP_WORKER_CORE            1
#define CTGP_PAGE_SIZE              0x1000u

typedef struct
{
    u32 magic;
    u32 version;
    u32 heapVA;
    u32 heapSize;
    u32 exeSize;
} CtgpHeaderPrefix;

extern Result svcMapProcessMemoryEx(Handle dstProcessHandle, u32 destAddress,
                                    Handle srcProcessHandle, u32 srcAddress,
                                    u32 size, u32 flags);
extern Result svcUnmapProcessMemoryEx(Handle process, u32 destAddress, u32 size);
extern Result svcFlushEntireDataCache(void);
extern Result svcInvalidateEntireInstructionCache(void);

extern u8 CtgpCompat_LateTrapStubStart[];
extern u8 CtgpCompat_LateTrapStubEnd[];
extern u8 CtgpCompat_LateInstallerStart[];
extern u8 CtgpCompat_LateTrapEntry[];
extern u8 CtgpCompat_LateTrapControl[];
extern const u32 CtgpCompat_CommitBlob[23];

__attribute__((aligned(0x1000), used))
static u8 g_ctgpLatePage[CTGP_PAGE_SIZE];

__attribute__((aligned(0x1000), used))
static u8 g_ctgpCommitPage[CTGP_PAGE_SIZE];

static u8 CTR_ALIGN(8) g_ctgpWorkerStack[0x4000];

static volatile bool g_ctgpActive;
static Handle g_ctgpProcess;
static u32 g_ctgpExeSize;
static u32 g_ctgpLateRemote;
static u32 g_ctgpCommitRemote;
static bool g_ctgpCommitPrepared;

static bool CtgpCompat_IsMk7Title(u64 titleId)
{
    u32 low = (u32)titleId;
    return low == 0x00030600u || low == 0x00030700u || low == 0x00030800u;
}

static Result CtgpCompat_FindFreeRange(u32 size, u32 *out)
{
    if(size == 0 || size > CTGP_MAP_END - CTGP_MAP_START)
        return CTGP_COMPAT_MAP_ERR;

    size = (size + 0xFFFu) & ~0xFFFu;
    u32 address = CTGP_MAP_START;

    while(address < CTGP_MAP_END)
    {
        MemInfo info;
        PageInfo page;
        Result res = svcQueryMemory(&info, &page, address);
        if(R_FAILED(res))
            return res;

        if(info.state == MEMSTATE_FREE)
        {
            u32 lower = info.base_addr < address ? address : info.base_addr;
            u32 start = (lower + 0xFFFu) & ~0xFFFu;
            if(start >= lower && start < CTGP_MAP_END &&
               info.size >= start - info.base_addr)
            {
                u32 available = info.size - (start - info.base_addr);
                if(available >= size && start + size >= start &&
                   start + size <= CTGP_MAP_END)
                {
                    *out = start;
                    return 0;
                }
            }
        }

        u32 next = info.base_addr + info.size;
        if(next <= address)
            break;
        address = next;
    }

    return CTGP_COMPAT_MAP_ERR;
}

static Result CtgpCompat_MapTargetPage(Handle process, u32 remotePage, u32 *localOut)
{
    u32 local = 0;
    Result res = CtgpCompat_FindFreeRange(0x3000u, &local);
    if(R_FAILED(res))
        return res;

    local += 0x1000u;
    res = svcMapProcessMemoryEx(CUR_PROCESS_HANDLE, local, process,
                                remotePage, CTGP_PAGE_SIZE, 0);
    if(R_FAILED(res))
        return res;

    *localOut = local;
    return 0;
}

static void CtgpCompat_UnmapLocalPage(u32 local)
{
    if(local)
        (void)svcUnmapProcessMemoryEx(CUR_PROCESS_HANDLE, local, CTGP_PAGE_SIZE);
}

static bool CtgpCompat_StartupPageMatches(const volatile u32 *page, bool patched)
{
    if(page[CTGP_STARTUP_LITERAL_OFF / 4u] !=
           (patched ? CTGP_STARTUP_GOOD_LITERAL : CTGP_STARTUP_OLD_LITERAL) ||
       page[CTGP_STARTUP_INSN_OFF / 4u] !=
           (patched ? CTGP_STARTUP_GOOD_INSN : CTGP_STARTUP_OLD_INSN))
        return false;

    u32 hash = 0x811C9DC5u;
    for(u32 i = 0; i < CTGP_PAGE_SIZE / 4u; i++)
    {
        u32 value = page[i];
        if(i == CTGP_STARTUP_LITERAL_OFF / 4u) value = CTGP_STARTUP_OLD_LITERAL;
        if(i == CTGP_STARTUP_INSN_OFF / 4u) value = CTGP_STARTUP_OLD_INSN;
        for(u32 byte = 0; byte < 4u; byte++)
        {
            hash ^= (value >> (byte * 8u)) & 0xFFu;
            hash *= 0x01000193u;
        }
    }
    return hash == CTGP_STARTUP_PAGE_FNV1A;
}

static Result CtgpCompat_Detect(Handle process, u32 *exeSize, bool *detected)
{
    *detected = false;
    *exeSize = 0;

    u32 local = 0;
    Result res = CtgpCompat_MapTargetPage(process, CTGP_3GX_BASE, &local);
    if(R_FAILED(res))
        return 0;

    const volatile CtgpHeaderPrefix *header = (const volatile CtgpHeaderPrefix *)local;
    u32 magic = header->magic;
    u32 size = header->exeSize;
    CtgpCompat_UnmapLocalPage(local);

    if(magic != CTGP_HEADER_MAGIC || size < CTGP_PAGE_SIZE ||
       size > CTGP_MAX_EXE_SIZE || (size & 0xFFFu))
        return 0;

    local = 0;
    res = CtgpCompat_MapTargetPage(process, CTGP_STARTUP_PAGE, &local);
    if(R_FAILED(res))
        return 0;

    bool match = CtgpCompat_StartupPageMatches((const volatile u32 *)local, false);
    CtgpCompat_UnmapLocalPage(local);
    if(!match)
        return 0;

    *exeSize = size;
    *detected = true;
    return 0;
}

static Result CtgpCompat_SetStartupPatch(Handle process, bool install)
{
    u32 local = 0;
    Result res = CtgpCompat_MapTargetPage(process, CTGP_STARTUP_PAGE, &local);
    if(R_FAILED(res))
        return res;

    volatile u32 *page = (volatile u32 *)local;
    if(!CtgpCompat_StartupPageMatches(page, !install))
    {
        CtgpCompat_UnmapLocalPage(local);
        return CTGP_COMPAT_ERR;
    }

    if(install)
    {
        page[CTGP_STARTUP_LITERAL_OFF / 4u] = CTGP_STARTUP_GOOD_LITERAL;
        page[CTGP_STARTUP_INSN_OFF / 4u] = CTGP_STARTUP_GOOD_INSN;
    }
    else
    {
        page[CTGP_STARTUP_INSN_OFF / 4u] = CTGP_STARTUP_OLD_INSN;
        page[CTGP_STARTUP_LITERAL_OFF / 4u] = CTGP_STARTUP_OLD_LITERAL;
    }

    (void)svcFlushEntireDataCache();
    (void)svcInvalidateEntireInstructionCache();

    bool ok = CtgpCompat_StartupPageMatches(page, install);
    if(!ok)
    {
        page[CTGP_STARTUP_INSN_OFF / 4u] =
            install ? CTGP_STARTUP_OLD_INSN : CTGP_STARTUP_GOOD_INSN;
        page[CTGP_STARTUP_LITERAL_OFF / 4u] =
            install ? CTGP_STARTUP_OLD_LITERAL : CTGP_STARTUP_GOOD_LITERAL;
        (void)svcFlushEntireDataCache();
        (void)svcInvalidateEntireInstructionCache();
    }

    CtgpCompat_UnmapLocalPage(local);
    return ok ? 0 : CTGP_COMPAT_ERR;
}

static u32 CtgpCompat_LateStubSize(void)
{
    return (u32)(CtgpCompat_LateTrapStubEnd - CtgpCompat_LateTrapStubStart);
}

static u32 CtgpCompat_LateControlOffset(void)
{
    return (u32)(CtgpCompat_LateTrapControl - CtgpCompat_LateTrapStubStart);
}

static u32 CtgpCompat_LateInstallerOffset(void)
{
    return (u32)(CtgpCompat_LateInstallerStart - CtgpCompat_LateTrapStubStart);
}

static u32 CtgpCompat_LateEntryOffset(void)
{
    return (u32)(CtgpCompat_LateTrapEntry - CtgpCompat_LateTrapStubStart);
}

static volatile u32 *CtgpCompat_LateControl(void)
{
    u32 off = CtgpCompat_LateControlOffset();
    if(off + 48u > CTGP_PAGE_SIZE)
        return NULL;
    return (volatile u32 *)(g_ctgpLatePage + off);
}

static bool CtgpCompat_EncodeArmBranch(u32 instructionAddress, u32 targetAddress,
                                       bool link, u32 *out)
{
    s64 delta = (s64)(u64)targetAddress -
                (s64)(u64)(instructionAddress + 8u);
    if((delta & 3) != 0 || delta < -0x02000000LL ||
       delta > 0x01FFFFFCLL)
        return false;

    *out = (link ? 0xEB000000u : 0xEA000000u) |
           ((u32)(delta >> 2) & 0x00FFFFFFu);
    return true;
}

static Result CtgpCompat_PrepareLateTrap(Handle process)
{
    u32 stubSize = CtgpCompat_LateStubSize();
    u32 ctrlOff = CtgpCompat_LateControlOffset();
    u32 installerOff = CtgpCompat_LateInstallerOffset();
    u32 trapOff = CtgpCompat_LateEntryOffset();

    if(stubSize == 0 || stubSize > CTGP_PAGE_SIZE ||
       ctrlOff + 48u > stubSize || installerOff >= stubSize ||
       trapOff >= stubSize)
        return CTGP_COMPAT_ERR;

    memset(g_ctgpLatePage, 0, sizeof(g_ctgpLatePage));
    memcpy(g_ctgpLatePage, CtgpCompat_LateTrapStubStart, stubSize);

    volatile u32 *ctrl = CtgpCompat_LateControl();
    if(ctrl == NULL)
        return CTGP_COMPAT_ERR;

    ctrl[0] = 0;                 /* late trap state */
    ctrl[1] = 0;                 /* release */
    ctrl[2] = CTGP_LATE_MAGIC;
    ctrl[3] = 0;                 /* installer state */
    ctrl[4] = 0;                 /* saved 0x130 */
    ctrl[5] = 0;                 /* saved 0x134 */
    ctrl[6] = 0;                 /* discovered late epilogue */
    ctrl[7] = 0;                 /* installed late branch */
    ctrl[8] = 0;                 /* Loader compatibility state */
    ctrl[9] = 0;                 /* Loader Result */
    ctrl[10] = 0;
    ctrl[11] = 0;
    (void)svcFlushEntireDataCache();

    u32 trapVA = 0;
    for(u32 va = CTGP_LATE_REMOTE_FIRST;; va -= CTGP_LATE_REMOTE_STEP)
    {
        u32 dummy = 0;
        if(CtgpCompat_EncodeArmBranch(0x070F2520u, va + trapOff, false, &dummy))
        {
            Result mapRes = svcMapProcessMemoryEx(process, va, CUR_PROCESS_HANDLE,
                                                  (u32)g_ctgpLatePage,
                                                  CTGP_PAGE_SIZE, 0);
            if(R_SUCCEEDED(mapRes))
            {
                trapVA = va;
                break;
            }
        }

        if(va == CTGP_LATE_REMOTE_LAST)
            break;
    }

    if(!trapVA)
        return CTGP_COMPAT_MAP_ERR;

    g_ctgpLateRemote = trapVA;
    return 0;
}

static Result CtgpCompat_InstallLateInstaller(Handle process)
{
    volatile u32 *ctrl = CtgpCompat_LateControl();
    if(ctrl == NULL || ctrl[2] != CTGP_LATE_MAGIC || !g_ctgpLateRemote)
        return CTGP_COMPAT_ERR;

    u32 local = 0;
    Result res = CtgpCompat_MapTargetPage(process, CTGP_LATE_INSTALL_PAGE, &local);
    if(R_FAILED(res))
        return res;

    volatile u32 *site = (volatile u32 *)(local + CTGP_LATE_INSTALL_OFFSET);
    u32 old0 = site[0];
    u32 old1 = site[1];

    if(old0 != CTGP_LATE_ORIGINAL0 || old1 != CTGP_LATE_ORIGINAL1)
    {
        CtgpCompat_UnmapLocalPage(local);
        return CTGP_COMPAT_ERR;
    }

    ctrl[4] = old0;
    ctrl[5] = old1;
    ctrl[0] = 0;
    ctrl[1] = 0;
    ctrl[3] = 0;
    ctrl[6] = 0;
    ctrl[7] = 0;
    ctrl[8] = 0;
    ctrl[9] = 0;
    (void)svcFlushEntireDataCache();

    u32 installerRemote = g_ctgpLateRemote + CtgpCompat_LateInstallerOffset();
    site[1] = installerRemote;
    site[0] = 0xE51FF004u;
    (void)svcFlushEntireDataCache();
    (void)svcInvalidateEntireInstructionCache();

    bool ok = site[0] == 0xE51FF004u && site[1] == installerRemote;
    if(!ok)
    {
        site[0] = old0;
        site[1] = old1;
        (void)svcFlushEntireDataCache();
        (void)svcInvalidateEntireInstructionCache();
    }

    CtgpCompat_UnmapLocalPage(local);
    return ok ? 0 : CTGP_COMPAT_ERR;
}

static Result CtgpCompat_RestoreSetupInstaller(Handle process)
{
    if(!g_ctgpLateRemote)
        return 0;

    volatile u32 *ctrl = CtgpCompat_LateControl();
    if(ctrl == NULL || ctrl[2] != CTGP_LATE_MAGIC)
        return CTGP_COMPAT_ERR;

    u32 local = 0;
    Result res = CtgpCompat_MapTargetPage(process, CTGP_LATE_INSTALL_PAGE, &local);
    if(R_FAILED(res))
        return res;

    volatile u32 *site = (volatile u32 *)(local + CTGP_LATE_INSTALL_OFFSET);
    u32 installerRemote = g_ctgpLateRemote + CtgpCompat_LateInstallerOffset();

    if(site[0] == 0xE51FF004u && site[1] == installerRemote)
    {
        site[0] = ctrl[4];
        site[1] = ctrl[5];
        (void)svcFlushEntireDataCache();
        (void)svcInvalidateEntireInstructionCache();
    }
    else if(site[0] != ctrl[4] || site[1] != ctrl[5])
    {
        CtgpCompat_UnmapLocalPage(local);
        return CTGP_COMPAT_ERR;
    }

    CtgpCompat_UnmapLocalPage(local);
    return 0;
}

static bool CtgpCompat_CommitTail(const volatile u32 *p)
{
    return p[1] == 0xEF00002Au &&
           (p[2] == 0xE8BD0008u || p[2] == 0xE49D3004u) &&
           p[3] == 0xE5831000u && p[4] == 0xE5832004u &&
           p[5] == 0xE12FFF1Eu;
}

static bool CtgpCompat_CommitStart(u32 x)
{
    return x == 0xE52D0004u || x == 0xE92D0001u;
}

static u32 CtgpCompat_FindCommitWrapper(const volatile u32 *image,
                                         u32 base, u32 size, u32 *matches)
{
    *matches = 0;
    if(!image || (base & 3u) || (size & 3u) || size < 24u ||
       size > 0x02000000u || base + size < base)
        return 0;

    u32 found = 0;
    const u32 known = 0x070A8288u;

    if(known >= base && known - base <= size - 24u)
    {
        const volatile u32 *p = image + (known - base) / 4u;
        if(CtgpCompat_CommitStart(p[0]) && CtgpCompat_CommitTail(p))
        {
            found = known;
            *matches = 1;
        }
    }

    for(u32 off = 0; off <= size - 24u; off += 4u)
    {
        u32 address = base + off;
        if(address == known)
            continue;

        const volatile u32 *p = image + off / 4u;
        if(CtgpCompat_CommitStart(p[0]) && CtgpCompat_CommitTail(p))
        {
            found = address;
            if(++*matches > 1u)
                return 0;
        }
    }

    return *matches == 1u ? found : 0;
}

static u32 CtgpCompat_ArmImmediate(u32 insn)
{
    u32 x = insn & 0xFFu;
    u32 shift = ((insn >> 8) & 15u) * 2u;
    return shift ? (x >> shift) | (x << (32u - shift)) : x;
}

static u32 CtgpCompat_FindStackCommitQuery(const volatile u32 *image,
                                            u32 base, u32 size, u32 *matches)
{
    *matches = 0;
    if(!image || (base & 3u) || (size & 3u) || size < 36u ||
       size > 0x02000000u || base + size < base)
        return 0;

    u32 found = 0;
    for(u32 off = 0; off <= size - 36u; off += 4u)
    {
        const volatile u32 *p = image + off / 4u;
        u32 site = base + off + 16u;

        if((p[0] & 0xFFFFF000u) != 0xE3A02000u ||
           CtgpCompat_ArmImmediate(p[0]) != 1u ||
           (p[1] & 0xFFFFF000u) != 0xE3A01000u ||
           CtgpCompat_ArmImmediate(p[1]) != 0x10000u ||
           (p[2] & 0xFFFFF000u) != 0xE28D0000u ||
           (p[3] & 0xFFFFF000u) != 0xE28D3000u ||
           p[4] != 0xE12FFF33u ||
           (p[5] & 0xFFFFF000u) != 0xE59D1000u ||
           (p[6] & 0xFFFFF000u) != 0xE28D3000u ||
           p[7] != 0xE1A00001u ||
           p[8] != 0xE12FFF33u)
            continue;

        u32 output = CtgpCompat_ArmImmediate(p[2]);
        u32 stub = CtgpCompat_ArmImmediate(p[3]);
        u32 compare = CtgpCompat_ArmImmediate(p[6]);

        if((output | stub | compare) & 3u ||
           output != (p[5] & 0xFFFu) ||
           compare != stub + 24u ||
           output < compare + 24u)
            continue;

        found = site;
        if(++*matches > 1u)
            return 0;
    }

    return *matches == 1u ? found : 0;
}

static Result CtgpCompat_MapExecutable(Handle process, u32 exeSize, u32 *localOut)
{
    u32 local = 0;
    Result res = CtgpCompat_FindFreeRange(exeSize, &local);
    if(R_FAILED(res))
        return res;

    res = svcMapProcessMemoryEx(CUR_PROCESS_HANDLE, local, process,
                                CTGP_3GX_BASE, exeSize, 0);
    if(R_FAILED(res))
        return res;

    (void)svcInvalidateProcessDataCache(CUR_PROCESS_HANDLE,
                                        (const void *)local, exeSize);
    *localOut = local;
    return 0;
}

static Result CtgpCompat_PrepareCommitBody(Handle process, u32 wrapperSite)
{
    if(!g_ctgpCommitPrepared)
    {
        memset(g_ctgpCommitPage, 0, sizeof(g_ctgpCommitPage));
        memcpy(g_ctgpCommitPage, CtgpCompat_CommitBlob, 23u * sizeof(u32));

        const u32 *copy = (const u32 *)g_ctgpCommitPage;
        for(u32 i = 0; i < 23u; i++)
            if(copy[i] != CtgpCompat_CommitBlob[i])
                return CTGP_COMPAT_ERR;

        (void)svcFlushEntireDataCache();
        g_ctgpCommitPrepared = true;
    }

    if(g_ctgpCommitRemote)
        return 0;

    for(u32 address = CTGP_COMMIT_REMOTE_FIRST;; address -= CTGP_COMMIT_REMOTE_STEP)
    {
        u32 branch = 0;
        if(CtgpCompat_EncodeArmBranch(wrapperSite, address, false, &branch))
        {
            Result res = svcMapProcessMemoryEx(process, address,
                                               CUR_PROCESS_HANDLE,
                                               (u32)g_ctgpCommitPage,
                                               CTGP_PAGE_SIZE, 0);
            if(R_SUCCEEDED(res))
            {
                g_ctgpCommitRemote = address;
                break;
            }
        }

        if(address == CTGP_COMMIT_REMOTE_LAST)
            break;
    }

    if(!g_ctgpCommitRemote)
        return CTGP_COMPAT_MAP_ERR;

    u32 local = 0;
    Result res = CtgpCompat_MapTargetPage(process, g_ctgpCommitRemote, &local);
    if(R_FAILED(res))
        return res;

    const volatile u32 *body = (const volatile u32 *)local;
    bool ok = true;
    for(u32 i = 0; i < 23u; i++)
        if(body[i] != CtgpCompat_CommitBlob[i])
            ok = false;

    CtgpCompat_UnmapLocalPage(local);
    return ok ? 0 : CTGP_COMPAT_ERR;
}

static Result CtgpCompat_InstallCommitCompat(Handle process, u32 exeSize)
{
    if(exeSize < CTGP_PAGE_SIZE || exeSize > CTGP_MAX_EXE_SIZE ||
       (exeSize & 0xFFFu))
        return CTGP_COMPAT_ERR;

    u32 localExe = 0;
    Result res = CtgpCompat_MapExecutable(process, exeSize, &localExe);
    if(R_FAILED(res))
        return res;

    const u32 scanSize = exeSize - 0x100u;
    const volatile u32 *image =
        (const volatile u32 *)(localExe + 0x100u);

    u32 wrapperMatches = 0;
    u32 queryMatches = 0;
    u32 wrapperSite = CtgpCompat_FindCommitWrapper(
        image, CTGP_CODE_BASE, scanSize, &wrapperMatches);
    u32 querySite = CtgpCompat_FindStackCommitQuery(
        image, CTGP_CODE_BASE, scanSize, &queryMatches);

    (void)svcUnmapProcessMemoryEx(CUR_PROCESS_HANDLE, localExe, exeSize);

    if(!wrapperSite || wrapperMatches != 1u ||
       !querySite || queryMatches != 1u)
        return CTGP_COMPAT_ERR;

    if((wrapperSite & 0xFFFu) > 0xFE8u)
        return CTGP_COMPAT_ERR;

    u32 queryBranch = 0;
    if(!CtgpCompat_EncodeArmBranch(querySite, wrapperSite, true, &queryBranch))
        return CTGP_COMPAT_ERR;

    res = CtgpCompat_PrepareCommitBody(process, wrapperSite);
    if(R_FAILED(res))
        return res;

    u32 wrapperBranch = 0;
    if(!CtgpCompat_EncodeArmBranch(wrapperSite, g_ctgpCommitRemote,
                                   false, &wrapperBranch))
        return CTGP_COMPAT_ERR;

    u32 wrapperLocal = 0;
    u32 queryLocal = 0;

    res = CtgpCompat_MapTargetPage(process, wrapperSite & ~0xFFFu,
                                   &wrapperLocal);
    if(R_FAILED(res))
        return res;

    res = CtgpCompat_MapTargetPage(process, querySite & ~0xFFFu,
                                   &queryLocal);
    if(R_FAILED(res))
    {
        CtgpCompat_UnmapLocalPage(wrapperLocal);
        return res;
    }

    volatile u32 *p =
        (volatile u32 *)(wrapperLocal + (wrapperSite & 0xFFFu));
    volatile u32 *q =
        (volatile u32 *)(queryLocal + (querySite & 0xFFFu));

    u32 wrapperOriginal = p[0];
    u32 queryOriginal = q[0];

    bool ok = CtgpCompat_CommitStart(wrapperOriginal) &&
              CtgpCompat_CommitTail(p) &&
              queryOriginal == 0xE12FFF33u;

    if(ok)
    {
        p[0] = wrapperBranch;
        q[0] = queryBranch;
        (void)svcFlushEntireDataCache();
        (void)svcInvalidateEntireInstructionCache();

        ok = p[0] == wrapperBranch &&
             q[0] == queryBranch &&
             CtgpCompat_CommitTail(p);

        if(!ok)
        {
            q[0] = queryOriginal;
            p[0] = wrapperOriginal;
            (void)svcFlushEntireDataCache();
            (void)svcInvalidateEntireInstructionCache();
        }
    }

    CtgpCompat_UnmapLocalPage(queryLocal);
    CtgpCompat_UnmapLocalPage(wrapperLocal);
    return ok ? 0 : CTGP_COMPAT_ERR;
}

static Result CtgpCompat_RestoreLateEpilogue(Handle process)
{
    volatile u32 *ctrl = CtgpCompat_LateControl();
    if(ctrl == NULL || ctrl[2] != CTGP_LATE_MAGIC ||
       ctrl[3] != 1u || ctrl[6] == 0 || ctrl[7] == 0)
        return CTGP_COMPAT_ERR;

    u32 lateSite = ctrl[6];
    u32 local = 0;
    Result res = CtgpCompat_MapTargetPage(process, lateSite & ~0xFFFu, &local);
    if(R_FAILED(res))
        return res;

    volatile u32 *site =
        (volatile u32 *)(local + (lateSite & 0xFFFu));

    if(*site == ctrl[7])
    {
        *site = CTGP_LATE_EPILOGUE;
        (void)svcFlushEntireDataCache();
        (void)svcInvalidateEntireInstructionCache();
    }
    else if(*site != CTGP_LATE_EPILOGUE)
    {
        CtgpCompat_UnmapLocalPage(local);
        return CTGP_COMPAT_ERR;
    }

    bool ok = *site == CTGP_LATE_EPILOGUE;
    CtgpCompat_UnmapLocalPage(local);
    return ok ? 0 : CTGP_COMPAT_ERR;
}

static void CtgpCompat_SetWorkerStatus(u32 state, Result result)
{
    volatile u32 *ctrl = CtgpCompat_LateControl();
    if(ctrl == NULL || ctrl[2] != CTGP_LATE_MAGIC)
        return;

    ctrl[8] = state;
    ctrl[9] = (u32)result;
    (void)svcFlushEntireDataCache();
    if(g_ctgpProcess && g_ctgpLateRemote)
        (void)svcInvalidateProcessDataCache(g_ctgpProcess,
                                            (const void *)g_ctgpLateRemote,
                                            CTGP_PAGE_SIZE);
}

static void CtgpCompat_ResetRuntimeState(void)
{
    if(g_ctgpProcess)
        svcCloseHandle(g_ctgpProcess);

    g_ctgpProcess = 0;
    g_ctgpExeSize = 0;
    g_ctgpLateRemote = 0;
    g_ctgpCommitRemote = 0;
    g_ctgpActive = false;
    memset(g_ctgpLatePage, 0, sizeof(g_ctgpLatePage));
}

static void CtgpCompat_Worker(void *arg)
{
    (void)arg;

    Handle process = g_ctgpProcess;
    bool released = false;

    while(process && svcWaitSynchronization(process, 0) != 0)
    {
        (void)svcInvalidateProcessDataCache(CUR_PROCESS_HANDLE,
                                            g_ctgpLatePage,
                                            CTGP_PAGE_SIZE);

        volatile u32 *ctrl = CtgpCompat_LateControl();
        if(ctrl == NULL || ctrl[2] != CTGP_LATE_MAGIC)
        {
            CtgpCompat_SetWorkerStatus(0xE0u, CTGP_COMPAT_ERR);
            break;
        }

        if(ctrl[3] == 0xE1u)
        {
            CtgpCompat_SetWorkerStatus(0xE1u, CTGP_COMPAT_ERR);
            break;
        }

        if(ctrl[3] == 1u && ctrl[0] == 1u)
        {
            Result res = CtgpCompat_InstallCommitCompat(process, g_ctgpExeSize);
            if(R_FAILED(res))
            {
                CtgpCompat_SetWorkerStatus(0xE2u, res);
                break;
            }

            res = CtgpCompat_RestoreLateEpilogue(process);
            if(R_FAILED(res))
            {
                CtgpCompat_SetWorkerStatus(0xE3u, res);
                break;
            }

            CtgpCompat_SetWorkerStatus(2u, 0);
            ctrl[1] = 1u;
            (void)svcFlushEntireDataCache();
            (void)svcInvalidateProcessDataCache(process,
                                                (const void *)g_ctgpLateRemote,
                                                CTGP_PAGE_SIZE);
            released = true;
            break;
        }

        svcSleepThread(CTGP_POLL_NS);
    }

    if(released && process)
        (void)svcWaitSynchronization(process, -1LL);
    else if(process)
        (void)svcWaitSynchronization(process, -1LL);

    CtgpCompat_ResetRuntimeState();
    svcExitThread();
}

static void CtgpCompat_RollbackSetup(Handle process, bool startupPatched,
                                     bool installerPatched)
{
    if(installerPatched)
        (void)CtgpCompat_RestoreSetupInstaller(process);
    if(startupPatched)
        (void)CtgpCompat_SetStartupPatch(process, false);

    if(g_ctgpLateRemote)
        (void)svcUnmapProcessMemoryEx(process, g_ctgpLateRemote,
                                      CTGP_PAGE_SIZE);

    CtgpCompat_ResetRuntimeState();
}

Result CtgpCompat_OnPluginLoaded(Handle process, u64 titleId)
{
    if(!CtgpCompat_IsMk7Title(titleId))
        return 0;

    u32 exeSize = 0;
    bool detected = false;
    Result res = CtgpCompat_Detect(process, &exeSize, &detected);
    if(R_FAILED(res) || !detected)
        return res;

    if(g_ctgpActive)
        return CTGP_COMPAT_ERR;

    g_ctgpActive = true;
    g_ctgpExeSize = exeSize;
    g_ctgpCommitRemote = 0;

    res = CtgpCompat_PrepareLateTrap(process);
    if(R_FAILED(res))
    {
        g_ctgpActive = false;
        g_ctgpExeSize = 0;
        return res;
    }

    Handle duplicate = 0;
    res = svcDuplicateHandle(&duplicate, process);
    if(R_FAILED(res))
    {
        (void)svcUnmapProcessMemoryEx(process, g_ctgpLateRemote,
                                      CTGP_PAGE_SIZE);
        g_ctgpLateRemote = 0;
        g_ctgpActive = false;
        g_ctgpExeSize = 0;
        return res;
    }
    g_ctgpProcess = duplicate;

    bool startupPatched = false;
    bool installerPatched = false;

    res = CtgpCompat_SetStartupPatch(process, true);
    if(R_FAILED(res))
    {
        CtgpCompat_RollbackSetup(process, startupPatched, installerPatched);
        return res;
    }
    startupPatched = true;

    res = CtgpCompat_InstallLateInstaller(process);
    if(R_FAILED(res))
    {
        CtgpCompat_RollbackSetup(process, startupPatched, installerPatched);
        return res;
    }
    installerPatched = true;

    Handle thread = 0;
    res = svcCreateThread(&thread, CtgpCompat_Worker, 0,
                          (u32 *)(g_ctgpWorkerStack +
                                  sizeof(g_ctgpWorkerStack)),
                          CTGP_WORKER_PRIORITY, CTGP_WORKER_CORE);
    if(R_FAILED(res))
    {
        CtgpCompat_RollbackSetup(process, startupPatched, installerPatched);
        return res;
    }

    svcCloseHandle(thread);
    return 0;
}
