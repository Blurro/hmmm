#include <3ds.h>
#include <string.h>
#include "ifile.h"

extern bool isN3DS;

#define DIAG_MENU_END 0
#define DIAG_METHOD 1
#define DIAG_MENU 2
#define DIAG_MENU_CAPACITY 25
#define DIAG_MAP_START 0x01000000u
#define DIAG_MAP_END 0x0F000000u
#define DIAG_MAPPABLE_START 0x10000000u
#define DIAG_MAPPABLE_END 0x14000000u
#define PROCESSOP_SET_MMU_TO_RWX 1u
#define DIAG_3GX_BASE 0x07000000u
#define DIAG_3GX_DUMP_SKIP 0x100u
#define DIAG_3GX_DUMP_TAIL 0x1000u
#define DIAG_3GX_MAX_SIZE 0x00800000u
#define DIAG_POLL_NS (20LL * 1000LL * 1000LL)
#define DIAG_WORKER_PRIORITY 0x30
#define DIAG_CORE_SYSTEM 1
#define DIAG_OPEN_HOOK_OFFSET 0x100u
#define DIAG_OPEN_LITERALS_OFFSET 0x1A0u
#define DIAG_RESTORE_FLAG_OFFSET 0x1F0u
#define DIAG_DUMP_FLAG_OFFSET 0x1F4u
#define DIAG_ACTIVE_FLAG_OFFSET 0x1F8u
#define DIAG_OPEN_FLAG_OFFSET 0x1FCu

#define DIAG_TRAP_PAGE_SIZE 0x1000u
#define DIAG_TRAP_MAGIC     0x50525459u /* YTRP */
#define DIAG_TRAP_VA_FIRST  0x0E000000u
#define DIAG_TRAP_VA_LAST   0x08000000u
#define DIAG_TRAP_VA_STEP   0x00100000u

#define DIAG_LATE_INSTALL_SITE      0x07000130u
#define DIAG_LATE_INSTALL_PAGE      0x07000000u
#define DIAG_LATE_INSTALL_OFFSET    (DIAG_LATE_INSTALL_SITE - DIAG_LATE_INSTALL_PAGE)
#define DIAG_LATE_INSTALL_ORIGINAL0 0xE59F0060u
#define DIAG_LATE_INSTALL_ORIGINAL1 0xE5900000u
#define DIAG_LATE_ORIGINAL          0xE8BDDFFFu
#define DIAG_LATE_TRAP_MAGIC    0x5052545Au /* ZTRP */
#define DIAG_LATE_TRAP_VA_FIRST 0x08F00000u
#define DIAG_LATE_TRAP_VA_LAST  0x08000000u
#define DIAG_LATE_TRAP_VA_STEP  0x00100000u
#define DIAG_LATE_POLL_NS       (250LL * 1000LL)

#define DIAG_STARTUP_IV_PAGE       0x07001000u
#define DIAG_STARTUP_IV_LITERAL    0x450u
#define DIAG_STARTUP_IV_INSN       0xD0Cu
#define DIAG_STARTUP_IV_OLD_LITERAL 0xC9A8AD0Fu
#define DIAG_STARTUP_IV_OLD_INSN    0xE021C002u
#define DIAG_STARTUP_IV_GOOD        0x30F72D2Bu
#define DIAG_STARTUP_IV_LOAD        0xE51FC8C4u /* ldr ip,[pc,#-0x8C4] */
#define DIAG_STARTUP_IV_PAGE_FNV1A  0x5093C1D4u

#define DIAG_ROS_LAUNCH_WORD_ORIGINAL 0xEBFFD05Eu
#define DIAG_ROS_LAUNCH_WORD_CLEAN    0xE3E00000u

typedef struct
{
    u32 title;
    u32 actionType;
    u32 target;
    u32 visibility;
} LoaderDiagMenuItem;

typedef struct
{
    u32 title;
    LoaderDiagMenuItem items[DIAG_MENU_CAPACITY];
} LoaderDiagMenu;

__attribute__((aligned(0x1000), used)) static u8 g_loaderDiagCallbackPage[0x1000];
__attribute__((aligned(0x1000), used)) static u8 g_loaderDiagTrapPage[DIAG_TRAP_PAGE_SIZE];
__attribute__((aligned(0x1000), used)) static u8 g_loaderDiagLateTrapPage[DIAG_TRAP_PAGE_SIZE];
extern u8 LoaderDiag_TrapStubStart[];
extern u8 LoaderDiag_TrapStubEnd[];
extern u8 LoaderDiag_TrapControl[];
extern u8 LoaderDiag_LateTrapStubStart[];
extern u8 LoaderDiag_LateTrapStubEnd[];
extern u8 LoaderDiag_LateInstallerStart[];
extern u8 LoaderDiag_LateTrapEntry[];
extern u8 LoaderDiag_LateTrapControl[];

extern Result svcMapProcessMemoryEx(Handle dstProcessHandle, u32 destAddress, Handle srcProcessHandle, u32 srcAddress, u32 size, u32 flags);
extern Result svcUnmapProcessMemoryEx(Handle process, u32 destAddress, u32 size);
extern Result svcFlushEntireDataCache(void);
extern Result svcInvalidateEntireInstructionCache(void);
extern Result svcControlProcess(Handle process, u32 op, u32 varg2, u32 varg3);
extern void LoaderDiag_Fatal(u32 stage, Result result);

static bool g_loaderDiagInjected;
static bool g_loaderDiagThreadStarted;
static u8 CTR_ALIGN(8) g_loaderDiagThreadStack[0x4000];
static Handle g_loaderDiagHidMemHandle;
static volatile u32 *g_loaderDiagHidSharedMem;
static u32 g_loaderDiagHidMapBase;
static Handle g_loaderDiagIrrstHandle;
static Handle g_loaderDiagIrrstMemHandle;
static volatile u32 *g_loaderDiagIrrstSharedMem;
static u32 g_loaderDiagIrrstMapBase;
static volatile bool g_loaderDiagShutdownRequested;
static volatile bool g_loaderDiagThreadExited;

/* Exact Rosalina bytes/data changed by the X dump injection. */
static bool g_loaderDiagSavedXStateValid;
static LoaderDiagMenu g_loaderDiagSavedMenu;
static u32 g_loaderDiagSavedMenuRemote;
static u32 g_loaderDiagSavedHookSite;
static u32 g_loaderDiagSavedHookInsn;
static u32 g_loaderDiagCallbackRemotePage;
static bool g_loaderDiagXHookRestored;
static bool g_loaderDiagReleasePending;

/* Y-controlled pre-3GX gate state. */
static bool g_loaderDiagYArmed;
static bool g_loaderDiagYHookPatched;
static bool g_loaderDiagTrapMapped;
static Handle g_loaderDiagTrapProcess;
static u32 g_loaderDiagTrapRemote;
static u32 g_loaderDiagTrapLocal;
static u32 g_loaderDiagGamePatchLiteral;
static u32 g_loaderDiagGamePatchOriginal;

/* ZL-controlled post-unpack gate at the established DUMP return epilogue. */
static bool g_loaderDiagZlArmed;
static bool g_loaderDiagLateHookPatched;
static bool g_loaderDiagLateTrapMapped;
static Handle g_loaderDiagLateProcess;
static u32 g_loaderDiagLateTrapRemote;
static u32 g_loaderDiagLateTrapLocal;
static bool g_loaderDiagLateInstallerPatched;
static bool g_loaderDiagStartupIvPatched;

typedef struct
{
    u32 hookSite;
    u32 originalScanHeldKeys;
    u32 menuEnter;
    u32 n3dsUpdate;
    u32 pluginLoaderUpdate;
    u32 rosalinaMenu;
    u32 menuShow;
    u32 menuLeave;
} LoaderDiagRosalinaOpenFlow;

static Result LoaderDiag_FindFreeRange(u32 size, u32 *out);
static Result LoaderDiag_MapProcessImage(Handle process, u32 *localBase, u32 *remoteStart, u32 *textSize, u32 *totalSize);
static Result LoaderDiag_PrepareLateTrapForProcess(Handle process);
static Result LoaderDiag_InstallLateMidHook(void);
static bool LoaderDiag_TrapIsActive(void);

static bool LoaderDiag_BytesEqual(const void *a, const void *b, u32 size)
{
    const u8 *aa = (const u8 *)a;
    const u8 *bb = (const u8 *)b;
    for(u32 i = 0; i < size; i++)
        if(aa[i] != bb[i])
            return false;
    return true;
}

static bool LoaderDiag_StringEqualAt(const u8 *image, u32 imageSize, u32 offset, const char *wanted)
{
    u32 i = 0;
    while(wanted[i] != 0)
    {
        if(offset + i >= imageSize || image[offset + i] != (u8)wanted[i])
            return false;
        i++;
    }
    return offset + i < imageSize && image[offset + i] == 0;
}

static u32 LoaderDiag_FindString(const u8 *image, u32 imageSize, const char *wanted)
{
    if(imageSize == 0)
        return 0xFFFFFFFFu;
    for(u32 i = 0; i < imageSize; i++)
        if(LoaderDiag_StringEqualAt(image, imageSize, i, wanted))
            return i;
    return 0xFFFFFFFFu;
}

static const u8 *LoaderDiag_RemoteToLocal(const u8 *mapped, u32 remoteStart, u32 imageSize, u32 remote)
{
    if(remote < remoteStart || remote - remoteStart >= imageSize)
        return NULL;
    return mapped + (remote - remoteStart);
}

static bool LoaderDiag_RemoteStringEquals(const u8 *mapped, u32 remoteStart, u32 imageSize, u32 remote, const char *wanted)
{
    const u8 *p = LoaderDiag_RemoteToLocal(mapped, remoteStart, imageSize, remote);
    if(p == NULL)
        return false;
    return LoaderDiag_StringEqualAt(mapped, imageSize, (u32)(p - mapped), wanted);
}

static bool LoaderDiag_HasArmLiteralReference(const u8 *mapped, u32 remoteStart, u32 imageSize, u32 textSize, u32 wantedValue)
{
    if(textSize > imageSize)
        textSize = imageSize;

    for(u32 off = 0; off + 4 <= textSize; off += 4)
    {
        u32 insn = *(const u32 *)(mapped + off);
        if((insn & 0x0F7F0000u) != 0x051F0000u)
            continue;

        u32 pc = remoteStart + off + 8;
        u32 imm = insn & 0xFFFu;
        u32 literal = (insn & (1u << 23)) != 0 ? pc + imm : pc - imm;
        const u8 *lit = LoaderDiag_RemoteToLocal(mapped, remoteStart, imageSize, literal);
        if(lit != NULL && literal + 4 >= literal && literal + 4 <= remoteStart + imageSize && *(const u32 *)lit == wantedValue)
            return true;
    }
    return false;
}

static u32 LoaderDiag_DecodeArmBranch(u32 instructionAddress, u32 insn)
{
    s32 imm24 = (s32)(insn & 0x00FFFFFFu);
    if((imm24 & 0x00800000) != 0)
        imm24 |= (s32)0xFF000000u;
    return instructionAddress + 8u + (u32)(imm24 << 2);
}

static bool LoaderDiag_DecodeArmLiteral(u32 instructionAddress, u32 insn, u32 *literalAddress)
{
    if((insn & 0x0F7F0000u) != 0x051F0000u)
        return false;
    u32 pc = instructionAddress + 8u;
    u32 imm = insn & 0xFFFu;
    *literalAddress = (insn & (1u << 23)) != 0 ? pc + imm : pc - imm;
    return true;
}

static bool LoaderDiag_EncodeArmBL(u32 instructionAddress, u32 targetAddress, u32 *out)
{
    s64 delta = (s64)(u64)targetAddress - (s64)(u64)(instructionAddress + 8u);
    if((delta & 3) != 0 || delta < -0x02000000LL || delta > 0x01FFFFFCLL)
        return false;
    *out = 0xEB000000u | ((u32)(delta >> 2) & 0x00FFFFFFu);
    return true;
}

static bool LoaderDiag_EncodeArmB(u32 instructionAddress, u32 targetAddress, u32 *out)
{
    s64 delta = (s64)(u64)targetAddress - (s64)(u64)(instructionAddress + 8u);
    if((delta & 3) != 0 || delta < -0x02000000LL || delta > 0x01FFFFFCLL)
        return false;
    *out = 0xEA000000u | ((u32)(delta >> 2) & 0x00FFFFFFu);
    return true;
}

static bool LoaderDiag_IsArmBL(u32 insn)
{
    return (insn & 0xFF000000u) == 0xEB000000u;
}

static bool LoaderDiag_NameMatches(u64 rawName, const char wanted[8])
{
    return LoaderDiag_BytesEqual(&rawName, wanted, 8);
}

static Result LoaderDiag_OpenProcessByName(const char wanted[8], Handle *out)
{
    u32 pids[0x40];
    s32 count = 0;
    Result res = svcGetProcessList(&count, pids, 0x40);
    if(R_FAILED(res))
        return res;

    for(s32 i = 0; i < count; i++)
    {
        Handle process = 0;
        if(R_FAILED(svcOpenProcess(&process, pids[i])))
            continue;

        u64 rawName = 0;
        res = svcGetProcessInfo((s64 *)&rawName, process, 0x10000);
        if(R_SUCCEEDED(res) && LoaderDiag_NameMatches(rawName, wanted))
        {
            *out = process;
            return 0;
        }
        svcCloseHandle(process);
    }
    return (Result)0xD8E007F7;
}


static bool LoaderDiag_IsMk7Title(u64 titleId)
{
    u32 low = (u32)titleId;
    return low == 0x00030600u || low == 0x00030700u || low == 0x00030800u;
}

static u32 LoaderDiag_TrapStubSize(void)
{
    return (u32)(LoaderDiag_TrapStubEnd - LoaderDiag_TrapStubStart);
}

static u32 LoaderDiag_TrapControlOffset(void)
{
    return (u32)(LoaderDiag_TrapControl - LoaderDiag_TrapStubStart);
}

static volatile u32 *LoaderDiag_TrapControlLocal(void)
{
    u32 off = LoaderDiag_TrapControlOffset();
    if(!g_loaderDiagTrapLocal || off + 16u > DIAG_TRAP_PAGE_SIZE)
        return NULL;
    return (volatile u32 *)(g_loaderDiagTrapLocal + off);
}

static bool LoaderDiag_FindGamePatchLiteral(const u8 *mapped, u32 remoteStart, u32 imageSize, u32 textSize, u32 *literalAddress)
{
    (void)textSize;
    /* gamePatchFunc is deliberately assembled into Rosalina's .data section,
       so search the complete mapped image rather than only executable text. */
    u32 found = 0, matches = 0;
    for(u32 off = 4; off + 8 <= imageSize; off += 4)
    {
        const u32 *w = (const u32 *)(mapped + off);
        /* stock pluginLoader.s: push {r0}; ldr r5,=0x07000100; blx r5; add sp,#4 */
        if(w[-1] != 0xE52D0004u ||
           (w[0] & 0xFFFFF000u) != 0xE59F5000u ||
           w[1] != 0xE12FFF35u ||
           w[2] != 0xE28DD004u)
            continue;

        u32 insnAddr = remoteStart + off;
        u32 litAddr = 0;
        if(!LoaderDiag_DecodeArmLiteral(insnAddr, w[0], &litAddr))
            continue;
        const u8 *lit = LoaderDiag_RemoteToLocal(mapped, remoteStart, imageSize, litAddr);
        if(lit == NULL || *(const u32 *)lit != DIAG_3GX_BASE + 0x100u)
            continue;
        found = litAddr;
        matches++;
    }

    if(matches != 1)
        return false;
    *literalAddress = found;
    return true;
}

static Result LoaderDiag_DiscoverGamePatchLiteral(void)
{
    if(g_loaderDiagGamePatchLiteral != 0)
        return 0;

    static const char rosalinaName[8] = {'r','o','s','a','l','i','n','a'};
    Handle rosalina = 0;
    Result res = LoaderDiag_OpenProcessByName(rosalinaName, &rosalina);
    if(R_FAILED(res)) return res;

    u32 localBase = 0, remoteStart = 0, textSize = 0, totalSize = 0;
    res = LoaderDiag_MapProcessImage(rosalina, &localBase, &remoteStart, &textSize, &totalSize);
    if(R_FAILED(res)) { svcCloseHandle(rosalina); return res; }

    u32 literal = 0;
    if(!LoaderDiag_FindGamePatchLiteral((const u8 *)localBase, remoteStart, totalSize, textSize, &literal))
        res = (Result)0xD8E007ED;
    else
    {
        u32 *word = (u32 *)(localBase + literal - remoteStart);
        if(*word != DIAG_3GX_BASE + 0x100u)
            res = (Result)0xD8E007ED;
        else
        {
            g_loaderDiagGamePatchLiteral = literal;
            g_loaderDiagGamePatchOriginal = *word;
        }
    }

    svcUnmapProcessMemoryEx(CUR_PROCESS_HANDLE, localBase, totalSize);
    svcCloseHandle(rosalina);
    return res;
}

static Result LoaderDiag_SetGamePatchLiteral(u32 value)
{
    Result res = LoaderDiag_DiscoverGamePatchLiteral();
    if(R_FAILED(res)) return res;

    static const char rosalinaName[8] = {'r','o','s','a','l','i','n','a'};
    Handle rosalina = 0;
    res = LoaderDiag_OpenProcessByName(rosalinaName, &rosalina);
    if(R_FAILED(res)) return res;

    u32 localBase = 0, remoteStart = 0, textSize = 0, totalSize = 0;
    res = LoaderDiag_MapProcessImage(rosalina, &localBase, &remoteStart, &textSize, &totalSize);
    if(R_FAILED(res)) { svcCloseHandle(rosalina); return res; }

    if(g_loaderDiagGamePatchLiteral < remoteStart ||
       g_loaderDiagGamePatchLiteral - remoteStart > totalSize - 4u)
        res = (Result)0xD8E007ED;
    else
    {
        u32 *word = (u32 *)(localBase + g_loaderDiagGamePatchLiteral - remoteStart);
        if(!g_loaderDiagYHookPatched && *word != g_loaderDiagGamePatchOriginal)
            res = (Result)0xD8E007ED;
        else
        {
            *word = value;
            svcFlushEntireDataCache();
            svcInvalidateEntireInstructionCache();
            if(*word != value)
                res = (Result)0xD8E007ED;
        }
    }

    svcUnmapProcessMemoryEx(CUR_PROCESS_HANDLE, localBase, totalSize);
    svcCloseHandle(rosalina);
    return res;
}

/* CTGP launcher patches this stock-Rosalina site by locating the three-word
   signature below and replacing the following instruction. Keep this scan
   independent of the X/Y hooks so ZR/R can toggle it while CTGP is held. */
static Result LoaderDiag_SetRosalinaLauncherWord(u32 value)
{
    static const char rosalinaName[8] = {'r','o','s','a','l','i','n','a'};
    static const u8 signature[12] = {
        0x40,0x10,0x8D,0xE2,
        0xBC,0x22,0x8D,0xE5,
        0x24,0x30,0x8D,0xE5
    };

    Handle rosalina = 0;
    Result res = LoaderDiag_OpenProcessByName(rosalinaName, &rosalina);
    if(R_FAILED(res)) return res;

    u32 localBase = 0, remoteStart = 0, textSize = 0, totalSize = 0;
    res = LoaderDiag_MapProcessImage(rosalina, &localBase, &remoteStart, &textSize, &totalSize);
    if(R_FAILED(res)) { svcCloseHandle(rosalina); return res; }

    u8 *match = NULL;
    u32 matches = 0;
    u8 *image = (u8 *)localBase;
    for(u32 off = 0; off + sizeof(signature) + 4u <= totalSize; off++)
    {
        if(memcmp(image + off, signature, sizeof(signature)) != 0)
            continue;
        match = image + off + sizeof(signature);
        matches++;
    }

    if(matches != 1 || match == NULL)
        res = (Result)0xD8E007ED;
    else
    {
        u32 current = 0;
        memcpy(&current, match, sizeof(current));
        if(current != DIAG_ROS_LAUNCH_WORD_ORIGINAL &&
           current != DIAG_ROS_LAUNCH_WORD_CLEAN)
            res = (Result)0xD8E007ED;
        else
        {
            memcpy(match, &value, sizeof(value));
            svcFlushEntireDataCache();
            svcInvalidateEntireInstructionCache();
            u32 verify = 0;
            memcpy(&verify, match, sizeof(verify));
            if(verify != value)
                res = (Result)0xD8E007ED;
        }
    }

    svcUnmapProcessMemoryEx(CUR_PROCESS_HANDLE, localBase, totalSize);
    svcCloseHandle(rosalina);
    return res;
}

static Result LoaderDiag_RestoreYHook(void)
{
    if(!g_loaderDiagYHookPatched)
        return 0;
    Result res = LoaderDiag_SetGamePatchLiteral(g_loaderDiagGamePatchOriginal);
    if(R_SUCCEEDED(res))
        g_loaderDiagYHookPatched = false;
    return res;
}

static Result LoaderDiag_PrepareTrapInProcess(Handle process)
{
    if(g_loaderDiagTrapMapped)
        return (Result)0xD8E007ED;

    u32 stubSize = LoaderDiag_TrapStubSize();
    u32 ctrlOff = LoaderDiag_TrapControlOffset();
    if(stubSize == 0 || stubSize > DIAG_TRAP_PAGE_SIZE || ctrlOff + 16u > stubSize)
        return (Result)0xD8E007ED;

    memset(g_loaderDiagTrapPage, 0, sizeof(g_loaderDiagTrapPage));
    memcpy(g_loaderDiagTrapPage, LoaderDiag_TrapStubStart, stubSize);
    volatile u32 *ctrl = (volatile u32 *)(g_loaderDiagTrapPage + ctrlOff);
    ctrl[0] = 0;
    ctrl[1] = 0;
    ctrl[2] = DIAG_TRAP_MAGIC;
    svcFlushEntireDataCache();

    u32 trapVA = 0;
    for(u32 va = DIAG_TRAP_VA_FIRST; va >= DIAG_TRAP_VA_LAST; va -= DIAG_TRAP_VA_STEP)
    {
        Result mapRes = svcMapProcessMemoryEx(process, va, CUR_PROCESS_HANDLE,
                                              (u32)g_loaderDiagTrapPage,
                                              DIAG_TRAP_PAGE_SIZE, 0);
        if(R_SUCCEEDED(mapRes))
        {
            trapVA = va;
            break;
        }
        if(va == DIAG_TRAP_VA_LAST)
            break;
    }
    if(!trapVA)
        return (Result)0xD86007F3;

    Handle dup = 0;
    Result res = svcDuplicateHandle(&dup, process);
    if(R_FAILED(res))
    {
        svcUnmapProcessMemoryEx(process, trapVA, DIAG_TRAP_PAGE_SIZE);
        return res;
    }

    g_loaderDiagTrapProcess = dup;
    g_loaderDiagTrapRemote = trapVA;
    g_loaderDiagTrapLocal = (u32)g_loaderDiagTrapPage;
    g_loaderDiagTrapMapped = true;
    return 0;
}

static void LoaderDiag_ResetTrapMapping(bool unmapRemote)
{
    if(unmapRemote && g_loaderDiagTrapMapped && g_loaderDiagTrapProcess && g_loaderDiagTrapRemote)
        (void)svcUnmapProcessMemoryEx(g_loaderDiagTrapProcess, g_loaderDiagTrapRemote, DIAG_TRAP_PAGE_SIZE);
    if(g_loaderDiagTrapProcess)
        svcCloseHandle(g_loaderDiagTrapProcess);
    g_loaderDiagTrapProcess = 0;
    g_loaderDiagTrapRemote = 0;
    g_loaderDiagTrapLocal = 0;
    g_loaderDiagTrapMapped = false;
    memset(g_loaderDiagTrapPage, 0, sizeof(g_loaderDiagTrapPage));
}

/* Called synchronously from Loader after stock Rosalina has loaded the 3GX,
   but before the newly created application process is allowed to start. */
Result LoaderDiag_OnPluginLoaded(Handle process, u64 titleId)
{
    /* Y alone owns the pre-first-instruction Rosalina handoff hook. ZL is
       deliberately manual and entirely CTGP-side: the user presses it while
       Mario Kart is already alive (normally while held by Y). */
    if(!LoaderDiag_IsMk7Title(titleId) || !g_loaderDiagYArmed)
        return 0;

    Result res = LoaderDiag_PrepareTrapInProcess(process);
    if(R_FAILED(res)) return res;
    res = LoaderDiag_SetGamePatchLiteral(g_loaderDiagTrapRemote);
    if(R_FAILED(res))
    {
        LoaderDiag_ResetTrapMapping(true);
        return res;
    }
    g_loaderDiagYHookPatched = true;
    return 0;
}

static u32 LoaderDiag_LateTrapStubSize(void)
{
    return (u32)(LoaderDiag_LateTrapStubEnd - LoaderDiag_LateTrapStubStart);
}

static u32 LoaderDiag_LateTrapControlOffset(void)
{
    return (u32)(LoaderDiag_LateTrapControl - LoaderDiag_LateTrapStubStart);
}

static u32 LoaderDiag_LateInstallerOffset(void)
{
    return (u32)(LoaderDiag_LateInstallerStart - LoaderDiag_LateTrapStubStart);
}

static u32 LoaderDiag_LateTrapEntryOffset(void)
{
    return (u32)(LoaderDiag_LateTrapEntry - LoaderDiag_LateTrapStubStart);
}

static volatile u32 *LoaderDiag_LateTrapControlLocal(void)
{
    u32 off = LoaderDiag_LateTrapControlOffset();
    if(!g_loaderDiagLateTrapLocal || off + 32u > DIAG_TRAP_PAGE_SIZE)
        return NULL;
    return (volatile u32 *)(g_loaderDiagLateTrapLocal + off);
}

static u32 LoaderDiag_LateInstallerState(void)
{
    if(!g_loaderDiagLateTrapMapped || !g_loaderDiagLateTrapLocal)
        return 0;
    (void)svcInvalidateProcessDataCache(CUR_PROCESS_HANDLE, g_loaderDiagLateTrapLocal, DIAG_TRAP_PAGE_SIZE);
    volatile u32 *ctrl = LoaderDiag_LateTrapControlLocal();
    if(ctrl == NULL || ctrl[2] != DIAG_LATE_TRAP_MAGIC)
        return 0;
    return ctrl[3];
}

static bool LoaderDiag_LateTrapIsActive(void)
{
    if(!g_loaderDiagLateTrapMapped || !g_loaderDiagLateTrapLocal)
        return false;
    (void)svcInvalidateProcessDataCache(CUR_PROCESS_HANDLE, g_loaderDiagLateTrapLocal, DIAG_TRAP_PAGE_SIZE);
    volatile u32 *ctrl = LoaderDiag_LateTrapControlLocal();
    return ctrl != NULL && ctrl[2] == DIAG_LATE_TRAP_MAGIC && ctrl[0] == 1u;
}

static bool LoaderDiag_LateTrapHasDeparted(void)
{
    if(!g_loaderDiagLateTrapMapped || !g_loaderDiagLateTrapLocal)
        return false;
    (void)svcInvalidateProcessDataCache(CUR_PROCESS_HANDLE, g_loaderDiagLateTrapLocal, DIAG_TRAP_PAGE_SIZE);
    volatile u32 *ctrl = LoaderDiag_LateTrapControlLocal();
    return ctrl != NULL && ctrl[2] == DIAG_LATE_TRAP_MAGIC && ctrl[0] == 2u;
}

static void LoaderDiag_ResetLateTrapMapping(bool unmapRemote)
{
    if(unmapRemote && g_loaderDiagLateTrapMapped && g_loaderDiagLateProcess && g_loaderDiagLateTrapRemote)
        (void)svcUnmapProcessMemoryEx(g_loaderDiagLateProcess, g_loaderDiagLateTrapRemote, DIAG_TRAP_PAGE_SIZE);
    if(g_loaderDiagLateProcess)
        svcCloseHandle(g_loaderDiagLateProcess);

    g_loaderDiagLateProcess = 0;
    g_loaderDiagLateTrapRemote = 0;
    g_loaderDiagLateTrapLocal = 0;
    g_loaderDiagLateTrapMapped = false;
    g_loaderDiagLateHookPatched = false;
    g_loaderDiagLateInstallerPatched = false;
    g_loaderDiagStartupIvPatched = false;
    memset(g_loaderDiagLateTrapPage, 0, sizeof(g_loaderDiagLateTrapPage));
}

static Result LoaderDiag_PrepareLateTrapForProcess(Handle process)
{
    if(g_loaderDiagLateTrapMapped)
        return 0;

    u32 stubSize = LoaderDiag_LateTrapStubSize();
    u32 ctrlOff = LoaderDiag_LateTrapControlOffset();
    u32 installerOff = LoaderDiag_LateInstallerOffset();
    u32 trapOff = LoaderDiag_LateTrapEntryOffset();
    if(stubSize == 0 || stubSize > DIAG_TRAP_PAGE_SIZE || ctrlOff + 32u > stubSize ||
       installerOff >= stubSize || trapOff >= stubSize)
        return (Result)0xD8E007ED;

    memset(g_loaderDiagLateTrapPage, 0, sizeof(g_loaderDiagLateTrapPage));
    memcpy(g_loaderDiagLateTrapPage, LoaderDiag_LateTrapStubStart, stubSize);
    volatile u32 *ctrl = (volatile u32 *)(g_loaderDiagLateTrapPage + ctrlOff);
    ctrl[0] = 0; /* late trap state */
    ctrl[1] = 0; /* release */
    ctrl[2] = DIAG_LATE_TRAP_MAGIC;
    ctrl[3] = 0; /* installer pending */
    ctrl[4] = 0; /* saved 0x130 */
    ctrl[5] = 0; /* saved 0x134 */
    ctrl[6] = 0; /* discovered late site */
    ctrl[7] = 0; /* installed late branch */
    svcFlushEntireDataCache();

    u32 trapVA = 0;
    for(u32 va = DIAG_LATE_TRAP_VA_FIRST; va >= DIAG_LATE_TRAP_VA_LAST; va -= DIAG_LATE_TRAP_VA_STEP)
    {
        /* The eventual late epilogue is around 0x070F25xx. Keep the mapped
           page within ARM B range of that region. */
        u32 dummy = 0;
        if(!LoaderDiag_EncodeArmB(0x070F2520u, va + trapOff, &dummy))
        {
            if(va == DIAG_LATE_TRAP_VA_LAST) break;
            continue;
        }
        Result res = svcMapProcessMemoryEx(process, va, CUR_PROCESS_HANDLE,
                                            (u32)g_loaderDiagLateTrapPage, DIAG_TRAP_PAGE_SIZE, 0);
        if(R_SUCCEEDED(res))
        {
            trapVA = va;
            break;
        }
        if(va == DIAG_LATE_TRAP_VA_LAST)
            break;
    }
    if(!trapVA)
        return (Result)0xD86007F3;

    Handle dup = 0;
    Result res = svcDuplicateHandle(&dup, process);
    if(R_FAILED(res))
    {
        (void)svcUnmapProcessMemoryEx(process, trapVA, DIAG_TRAP_PAGE_SIZE);
        return res;
    }

    g_loaderDiagLateProcess = dup;
    g_loaderDiagLateTrapRemote = trapVA;
    g_loaderDiagLateTrapLocal = (u32)g_loaderDiagLateTrapPage;
    g_loaderDiagLateTrapMapped = true;
    return 0;
}

static Result LoaderDiag_MapTargetPage(Handle process, u32 remotePage, u32 *localOut)
{
    u32 local = 0;
    /* Leave a free page on each side. Nested aliases must not coalesce into
       one mapped region whose unmap can also invalidate the earlier alias. */
    Result res = LoaderDiag_FindFreeRange(0x3000u, &local);
    if(R_FAILED(res))
        return res;
    local += 0x1000u;
    res = svcMapProcessMemoryEx(CUR_PROCESS_HANDLE, local, process, remotePage, 0x1000u, 0);
    if(R_FAILED(res))
        return res;
    *localOut = local;
    return 0;
}

static bool LoaderDiag_StartupIvPageMatches(const volatile u32 *page, bool patched)
{
    if(page[DIAG_STARTUP_IV_LITERAL / 4u] !=
           (patched ? DIAG_STARTUP_IV_GOOD : DIAG_STARTUP_IV_OLD_LITERAL) ||
       page[DIAG_STARTUP_IV_INSN / 4u] !=
           (patched ? DIAG_STARTUP_IV_LOAD : DIAG_STARTUP_IV_OLD_INSN))
        return false;

    /* Identify the complete captured startup page, normalizing our two writes
       for cancellation. A decrypted runtime page must never pass this guard. */
    u32 hash = 0x811C9DC5u;
    for(u32 i = 0; i < 0x1000u / 4u; i++)
    {
        u32 value = page[i];
        if(i == DIAG_STARTUP_IV_LITERAL / 4u) value = DIAG_STARTUP_IV_OLD_LITERAL;
        if(i == DIAG_STARTUP_IV_INSN / 4u) value = DIAG_STARTUP_IV_OLD_INSN;
        for(u32 byte = 0; byte < 4u; byte++)
        {
            hash ^= (value >> (byte * 8u)) & 0xFFu;
            hash *= 0x01000193u;
        }
    }
    return hash == DIAG_STARTUP_IV_PAGE_FNV1A;
}

static Result LoaderDiag_SetStartupIvPatch(bool install)
{
    if(!install && !g_loaderDiagStartupIvPatched)
        return 0;

    /* The 0x130 target installer runs after decryption. These writes instead
       happen synchronously on ZL, while Y holds the first CTGP instruction. */
    if(!LoaderDiag_TrapIsActive() || !g_loaderDiagTrapProcess || !g_loaderDiagLateProcess)
        return (Result)0xD8E007ED;

    u32 earlyPid = 0, latePid = 0;
    Result res = svcGetProcessId(&earlyPid, g_loaderDiagTrapProcess);
    if(R_FAILED(res)) return res;
    res = svcGetProcessId(&latePid, g_loaderDiagLateProcess);
    if(R_FAILED(res)) return res;
    if(earlyPid != latePid)
        return (Result)0xD8E007ED;

    u32 local = 0;
    res = LoaderDiag_MapTargetPage(g_loaderDiagLateProcess, DIAG_STARTUP_IV_PAGE, &local);
    if(R_FAILED(res)) return res;

    volatile u32 *page = (volatile u32 *)local;
    if(!LoaderDiag_StartupIvPageMatches(page, !install))
    {
        (void)svcUnmapProcessMemoryEx(CUR_PROCESS_HANDLE, local, 0x1000u);
        return (Result)0xD8E007ED;
    }

    if(install)
    {
        page[DIAG_STARTUP_IV_LITERAL / 4u] = DIAG_STARTUP_IV_GOOD;
        page[DIAG_STARTUP_IV_INSN / 4u] = DIAG_STARTUP_IV_LOAD;
    }
    else
    {
        page[DIAG_STARTUP_IV_INSN / 4u] = DIAG_STARTUP_IV_OLD_INSN;
        page[DIAG_STARTUP_IV_LITERAL / 4u] = DIAG_STARTUP_IV_OLD_LITERAL;
    }
    svcFlushEntireDataCache();
    svcInvalidateEntireInstructionCache();

    bool ok = LoaderDiag_StartupIvPageMatches(page, install);
    if(!ok)
    {
        page[DIAG_STARTUP_IV_INSN / 4u] = install ? DIAG_STARTUP_IV_OLD_INSN : DIAG_STARTUP_IV_LOAD;
        page[DIAG_STARTUP_IV_LITERAL / 4u] = install ? DIAG_STARTUP_IV_OLD_LITERAL : DIAG_STARTUP_IV_GOOD;
        svcFlushEntireDataCache();
        svcInvalidateEntireInstructionCache();
    }
    (void)svcUnmapProcessMemoryEx(CUR_PROCESS_HANDLE, local, 0x1000u);
    if(!ok) return (Result)0xD8E007ED;

    g_loaderDiagStartupIvPatched = install;
    return 0;
}

static Result LoaderDiag_InstallLateMidHook(void)
{
    if(g_loaderDiagLateInstallerPatched || LoaderDiag_LateInstallerState() == 1u || LoaderDiag_LateTrapIsActive())
        return 0;

    if(!LoaderDiag_TrapIsActive())
        return (Result)0xD8E007ED;

    static const char marioKartName[8] = {'M','a','r','i','o','K','a','r'};
    Handle process = 0;
    Result res = LoaderDiag_OpenProcessByName(marioKartName, &process);
    if(R_FAILED(res))
        return res;

    res = LoaderDiag_PrepareLateTrapForProcess(process);
    if(R_FAILED(res))
    {
        svcCloseHandle(process);
        return res;
    }

    u32 local = 0;
    res = LoaderDiag_MapTargetPage(process, DIAG_LATE_INSTALL_PAGE, &local);
    svcCloseHandle(process);
    if(R_FAILED(res))
        return res;

    volatile u32 *site = (volatile u32 *)(local + DIAG_LATE_INSTALL_OFFSET);
    u32 old0 = site[0];
    u32 old1 = site[1];
    if(old0 != DIAG_LATE_INSTALL_ORIGINAL0 || old1 != DIAG_LATE_INSTALL_ORIGINAL1)
    {
        (void)svcUnmapProcessMemoryEx(CUR_PROCESS_HANDLE, local, 0x1000u);
        return (Result)0xD8E007ED;
    }

    volatile u32 *ctrl = LoaderDiag_LateTrapControlLocal();
    if(ctrl == NULL || ctrl[2] != DIAG_LATE_TRAP_MAGIC)
    {
        (void)svcUnmapProcessMemoryEx(CUR_PROCESS_HANDLE, local, 0x1000u);
        return (Result)0xD8E007ED;
    }

    res = LoaderDiag_SetStartupIvPatch(true);
    if(R_FAILED(res))
    {
        (void)svcUnmapProcessMemoryEx(CUR_PROCESS_HANDLE, local, 0x1000u);
        return res;
    }
    ctrl[4] = old0;
    ctrl[5] = old1;
    ctrl[3] = 0;
    ctrl[0] = 0;
    ctrl[1] = 0;
    ctrl[6] = 0;
    ctrl[7] = 0;
    svcFlushEntireDataCache();

    u32 installerRemote = g_loaderDiagLateTrapRemote + LoaderDiag_LateInstallerOffset();
    site[1] = installerRemote;
    site[0] = 0xE51FF004u; /* ldr pc,[pc,#-4] */
    svcFlushEntireDataCache();
    svcInvalidateEntireInstructionCache();

    bool ok = site[0] == 0xE51FF004u && site[1] == installerRemote;
    if(!ok)
    {
        site[0] = old0;
        site[1] = old1;
        svcFlushEntireDataCache();
        svcInvalidateEntireInstructionCache();
    }
    (void)svcUnmapProcessMemoryEx(CUR_PROCESS_HANDLE, local, 0x1000u);
    if(!ok)
    {
        (void)LoaderDiag_SetStartupIvPatch(false);
        return (Result)0xD8E007ED;
    }

    g_loaderDiagLateInstallerPatched = true;
    g_loaderDiagZlArmed = true;
    return 0;
}

static Result LoaderDiag_RestoreLateInstaller(void)
{
    if(!g_loaderDiagLateInstallerPatched)
        return LoaderDiag_SetStartupIvPatch(false);

    volatile u32 *ctrl = LoaderDiag_LateTrapControlLocal();
    if(ctrl == NULL || ctrl[2] != DIAG_LATE_TRAP_MAGIC)
        return (Result)0xD8E007ED;
    (void)svcInvalidateProcessDataCache(CUR_PROCESS_HANDLE, g_loaderDiagLateTrapLocal, DIAG_TRAP_PAGE_SIZE);

    /* Once the target-side installer has run it already restored 0x130/0x134. */
    if(ctrl[3] != 0)
    {
        g_loaderDiagLateInstallerPatched = false;
        g_loaderDiagStartupIvPatched = false;
        return 0;
    }

    u32 local = 0;
    Result res = LoaderDiag_MapTargetPage(g_loaderDiagLateProcess, DIAG_LATE_INSTALL_PAGE, &local);
    if(R_FAILED(res))
        return res;

    volatile u32 *site = (volatile u32 *)(local + DIAG_LATE_INSTALL_OFFSET);
    u32 installerRemote = g_loaderDiagLateTrapRemote + LoaderDiag_LateInstallerOffset();
    bool installed = site[0] == 0xE51FF004u && site[1] == installerRemote;
    if(!installed && (site[0] != ctrl[4] || site[1] != ctrl[5]))
    {
        (void)svcUnmapProcessMemoryEx(CUR_PROCESS_HANDLE, local, 0x1000u);
        return (Result)0xD8E007ED;
    }

    Result restoreIv = LoaderDiag_SetStartupIvPatch(false);
    if(R_FAILED(restoreIv))
    {
        (void)svcUnmapProcessMemoryEx(CUR_PROCESS_HANDLE, local, 0x1000u);
        return restoreIv;
    }
    if(installed)
    {
        site[0] = ctrl[4];
        site[1] = ctrl[5];
        svcFlushEntireDataCache();
        svcInvalidateEntireInstructionCache();
    }

    (void)svcUnmapProcessMemoryEx(CUR_PROCESS_HANDLE, local, 0x1000u);
    g_loaderDiagLateInstallerPatched = false;
    return 0;
}

static Result LoaderDiag_RestoreLateHook(void)
{
    if(!g_loaderDiagLateTrapMapped)
        return 0;

    volatile u32 *ctrl = LoaderDiag_LateTrapControlLocal();
    if(ctrl == NULL || ctrl[2] != DIAG_LATE_TRAP_MAGIC)
        return (Result)0xD8E007ED;
    (void)svcInvalidateProcessDataCache(CUR_PROCESS_HANDLE, g_loaderDiagLateTrapLocal, DIAG_TRAP_PAGE_SIZE);

    u32 installerState = ctrl[3];
    if(installerState == 0)
        return LoaderDiag_RestoreLateInstaller();

    /* Decryption consumed the startup repair. Its old addresses now contain
       runtime code, so cleanup must not write the pre-unpack words there. */
    g_loaderDiagStartupIvPatched = false;

    /* E1 means the mid-load installer ran and self-restored but couldn't find
       the late epilogue. There is no late hook to remove. */
    if(installerState == 0xE1u)
    {
        g_loaderDiagLateInstallerPatched = false;
        g_loaderDiagLateHookPatched = false;
        return 0;
    }

    if(installerState != 1u || ctrl[6] == 0 || ctrl[7] == 0)
        return (Result)0xD8E007ED;

    u32 lateSite = ctrl[6];
    u32 latePage = lateSite & ~0xFFFu;
    u32 lateOff = lateSite & 0xFFFu;
    u32 local = 0;
    Result res = LoaderDiag_MapTargetPage(g_loaderDiagLateProcess, latePage, &local);
    if(R_FAILED(res))
        return res;

    volatile u32 *site = (volatile u32 *)(local + lateOff);
    if(*site == ctrl[7])
    {
        *site = DIAG_LATE_ORIGINAL;
        svcFlushEntireDataCache();
        svcInvalidateEntireInstructionCache();
    }
    else if(*site != DIAG_LATE_ORIGINAL)
    {
        (void)svcUnmapProcessMemoryEx(CUR_PROCESS_HANDLE, local, 0x1000u);
        return (Result)0xD8E007ED;
    }

    (void)svcUnmapProcessMemoryEx(CUR_PROCESS_HANDLE, local, 0x1000u);
    g_loaderDiagLateInstallerPatched = false;
    g_loaderDiagLateHookPatched = false;
    return 0;
}

static Result LoaderDiag_FindFreeRange(u32 size, u32 *out)
{
    if(size == 0 || size > DIAG_MAP_END - DIAG_MAP_START)
        return (Result)0xD86007F3;
    size = (size + 0xFFFu) & ~0xFFFu;
    u32 address = DIAG_MAP_START;

    while(address < DIAG_MAP_END)
    {
        MemInfo info;
        PageInfo page;
        Result res = svcQueryMemory(&info, &page, address);
        if(R_FAILED(res))
            return res;

        if(info.state == MEMSTATE_FREE)
        {
            /* QueryMemory returns the whole containing region. Its base may
               be zero or lie below our search window, so clamp before align. */
            u32 lower = info.base_addr < address ? address : info.base_addr;
            u32 start = (lower + 0xFFFu) & ~0xFFFu;
            if(start >= lower && start < DIAG_MAP_END && info.size >= start - info.base_addr)
            {
                u32 available = info.size - (start - info.base_addr);
                if(available >= size && start + size >= start && start + size <= DIAG_MAP_END)
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
    return (Result)0xD86007F3;
}

static Result LoaderDiag_FindMappableRange(u32 size, u32 *out)
{
    // Stock Loader itself temporarily allocates title code beginning at
    // 0x10000000. Do not occupy the bottom of the mappable window with HID,
    // or the next RegisterProgram/LoadProcess can fail with 0xE0E01BF5.
    // Search downward from the top instead, keeping the listener away from
    // Loader's normal low-to-high scratch allocation.
    size = (size + 0xFFFu) & ~0xFFFu;
    if(size == 0 || size > DIAG_MAPPABLE_END - DIAG_MAPPABLE_START)
        return (Result)0xD86007F3;

    u32 address = DIAG_MAPPABLE_END - size;
    while(address >= DIAG_MAPPABLE_START)
    {
        MemInfo info;
        PageInfo page;
        Result res = svcQueryMemory(&info, &page, address);
        if(R_FAILED(res))
            return res;

        if(info.state == MEMSTATE_FREE &&
           address >= info.base_addr &&
           address - info.base_addr <= info.size &&
           info.size - (address - info.base_addr) >= size)
        {
            *out = address;
            return 0;
        }

        if(info.base_addr <= DIAG_MAPPABLE_START)
            break;
        u32 next = (info.base_addr - 1u) & ~0xFFFu;
        if(next >= address)
            break;
        address = next;
    }
    return (Result)0xD86007F3;
}

static Result LoaderDiag_GetProcessImageInfo(Handle process, u32 *start, u32 *textSize, u32 *totalSize)
{
    s64 text = 0, ro = 0, rw = 0, base = 0;
    Result res;
    if(R_FAILED(res = svcGetProcessInfo(&text, process, 0x10002))) return res;
    if(R_FAILED(res = svcGetProcessInfo(&ro, process, 0x10003))) return res;
    if(R_FAILED(res = svcGetProcessInfo(&rw, process, 0x10004))) return res;
    if(R_FAILED(res = svcGetProcessInfo(&base, process, 0x10005))) return res;
    if(text <= 0 || ro < 0 || rw < 0 || base <= 0) return (Result)0xD8E007ED;

    u64 total = (u64)text + (u64)ro + (u64)rw;
    if(total == 0 || total > 0x02000000ULL) return (Result)0xD8E007ED;
    *start = (u32)base;
    *textSize = (u32)text;
    *totalSize = (u32)total;
    return 0;
}

static Result LoaderDiag_MapProcessImage(Handle process, u32 *localBase, u32 *remoteStart, u32 *textSize, u32 *totalSize)
{
    Result res = LoaderDiag_GetProcessImageInfo(process, remoteStart, textSize, totalSize);
    if(R_FAILED(res))
        return res;

    if(R_FAILED(res = LoaderDiag_FindFreeRange(*totalSize, localBase)))
        return res;

    return svcMapProcessMemoryEx(CUR_PROCESS_HANDLE, *localBase, process, *remoteStart, *totalSize, 0);
}

static bool LoaderDiag_ValidateMenu(const u8 *mapped, u32 remoteStart, u32 imageSize, const LoaderDiagMenu *menu, u32 screenshotTitle, u32 miscTitle, u32 saveTitle, u32 *miscIndex, u32 *endIndex)
{
    if(menu->title < remoteStart || menu->title - remoteStart >= imageSize)
        return false;
    if(menu->items[0].title != screenshotTitle || menu->items[0].actionType != DIAG_METHOD || menu->items[0].target == 0)
        return false;
    if(!LoaderDiag_RemoteStringEquals(mapped, remoteStart, imageSize, menu->items[0].title, "Take screenshot"))
        return false;

    bool foundMisc = false;
    bool foundSave = false;
    u32 end = DIAG_MENU_CAPACITY;
    for(u32 i = 0; i < DIAG_MENU_CAPACITY; i++)
    {
        const LoaderDiagMenuItem *item = &menu->items[i];
        if(item->actionType == DIAG_MENU_END)
        {
            end = i;
            break;
        }
        if(item->actionType != DIAG_METHOD && item->actionType != DIAG_MENU)
            return false;
        if(item->title == miscTitle && item->actionType == DIAG_MENU)
        {
            *miscIndex = i;
            foundMisc = true;
        }
        if(item->title == saveTitle && item->actionType == DIAG_METHOD)
            foundSave = true;
    }

    if(!foundMisc || !foundSave || end >= DIAG_MENU_CAPACITY - 2 || *miscIndex >= end)
        return false;
    *endIndex = end;
    return true;
}

static Result LoaderDiag_FindRosalinaMenu(u8 *mapped, u32 remoteStart, u32 imageSize, u32 textSize, LoaderDiagMenu **outMenu, u32 *outMiscIndex, u32 *outEndIndex)
{
    u32 rootOff = LoaderDiag_FindString(mapped, imageSize, "Rosalina menu");
    u32 screenshotOff = LoaderDiag_FindString(mapped, imageSize, "Take screenshot");
    u32 miscOff = LoaderDiag_FindString(mapped, imageSize, "Miscellaneous options...");
    u32 saveOff = LoaderDiag_FindString(mapped, imageSize, "Save settings");
    if(rootOff == 0xFFFFFFFFu || screenshotOff == 0xFFFFFFFFu || miscOff == 0xFFFFFFFFu || saveOff == 0xFFFFFFFFu)
        return (Result)0xD8E007ED;

    u32 rootTitle = remoteStart + rootOff;
    u32 screenshotTitle = remoteStart + screenshotOff;
    u32 miscTitle = remoteStart + miscOff;
    u32 saveTitle = remoteStart + saveOff;
    LoaderDiagMenu *found = NULL;
    u32 foundMisc = 0, foundEnd = 0, matches = 0;

    for(u32 off = 0; off + sizeof(LoaderDiagMenu) <= imageSize; off += 4)
    {
        if(*(u32 *)(mapped + off) != rootTitle)
            continue;
        LoaderDiagMenu *candidate = (LoaderDiagMenu *)(mapped + off);
        u32 miscIndex = 0, endIndex = 0;
        if(!LoaderDiag_ValidateMenu(mapped, remoteStart, imageSize, candidate, screenshotTitle, miscTitle, saveTitle, &miscIndex, &endIndex))
            continue;

        u32 remoteMenu = remoteStart + off;
        if(!LoaderDiag_HasArmLiteralReference(mapped, remoteStart, imageSize, textSize, remoteMenu))
            continue;

        found = candidate;
        foundMisc = miscIndex;
        foundEnd = endIndex;
        matches++;
    }

    if(matches != 1)
        return (Result)0xD8E007ED;
    *outMenu = found;
    *outMiscIndex = foundMisc;
    *outEndIndex = foundEnd;
    return 0;
}

static Result LoaderDiag_FindRosalinaOpenFlow(const u8 *mapped, u32 remoteStart, u32 imageSize, u32 textSize, u32 expectedMenu, LoaderDiagRosalinaOpenFlow *out)
{
    if(textSize > imageSize)
        textSize = imageSize;

    LoaderDiagRosalinaOpenFlow found = {0};
    u32 matches = 0;
    for(u32 off = 0; off + 18u * 4u <= textSize; off += 4)
    {
        const u32 *w = (const u32 *)(mapped + off);
        if(!LoaderDiag_IsArmBL(w[0]) ||
           (w[1] & 0xFFFFF000u) != 0xE59F3000u || w[2] != 0xE5933000u ||
           w[3] != 0xE1D33000u || (w[4] & 0xFF000000u) != 0x1A000000u ||
           (w[5] & 0xFFFFF000u) != 0xE59F3000u || w[6] != 0xE5933000u ||
           w[7] != 0xE3530000u || (w[8] & 0xFF000000u) != 0x1A000000u ||
           !LoaderDiag_IsArmBL(w[9]) || w[10] != 0xE5D43000u || w[11] != 0xE3530000u ||
           (w[12] & 0xFF000000u) != 0x0A000000u || !LoaderDiag_IsArmBL(w[13]) ||
           !LoaderDiag_IsArmBL(w[14]) || (w[15] & 0xFFFFF000u) != 0xE59F0000u ||
           !LoaderDiag_IsArmBL(w[16]) || !LoaderDiag_IsArmBL(w[17]))
            continue;

        u32 insnAddr = remoteStart + off;
        u32 menuLiteral = 0;
        if(!LoaderDiag_DecodeArmLiteral(insnAddr + 15u * 4u, w[15], &menuLiteral))
            continue;
        const u8 *menuLit = LoaderDiag_RemoteToLocal(mapped, remoteStart, imageSize, menuLiteral);
        if(menuLit == NULL || *(const u32 *)menuLit != expectedMenu)
            continue;

        found.hookSite = insnAddr;
        found.originalScanHeldKeys = LoaderDiag_DecodeArmBranch(insnAddr, w[0]);
        found.menuEnter = LoaderDiag_DecodeArmBranch(insnAddr + 9u * 4u, w[9]);
        found.n3dsUpdate = LoaderDiag_DecodeArmBranch(insnAddr + 13u * 4u, w[13]);
        found.pluginLoaderUpdate = LoaderDiag_DecodeArmBranch(insnAddr + 14u * 4u, w[14]);
        found.rosalinaMenu = expectedMenu;
        found.menuShow = LoaderDiag_DecodeArmBranch(insnAddr + 16u * 4u, w[16]);
        found.menuLeave = LoaderDiag_DecodeArmBranch(insnAddr + 17u * 4u, w[17]);
        matches++;
    }

    if(matches != 1)
        return (Result)0xD8E007ED;
    *out = found;
    return 0;
}

static u32 LoaderDiag_ArmLdrLiteral(u32 instructionWordIndex, u32 literalWordIndex, u32 rd)
{
    u32 pcByteOffset = instructionWordIndex * 4u + 8u;
    u32 literalByteOffset = literalWordIndex * 4u;
    if(literalByteOffset < pcByteOffset || literalByteOffset - pcByteOffset > 0xFFFu)
        return 0;
    return 0xE59F0000u | (rd << 12) | (literalByteOffset - pcByteOffset);
}

static void LoaderDiag_PrepareCallbackPage(u32 callbackPage, const LoaderDiagRosalinaOpenFlow *flow)
{
    memset(g_loaderDiagCallbackPage, 0, sizeof(g_loaderDiagCallbackPage));

    /* Menu item callback: set a shared Loader-visible dump flag and return.
       This avoids copying an event handle into Rosalina, so X cleanup can
       return Rosalina's handle table to its pre-X state. */
    u32 *dumpCode = (u32 *)g_loaderDiagCallbackPage;
    dumpCode[0] = 0xE59F0010u; // ldr r0,[pc,#0x10] -> word 6
    dumpCode[1] = 0xE3A01001u; // mov r1,#1
    dumpCode[2] = 0xE5801000u; // str r1,[r0]
    dumpCode[3] = 0xEF000092u; // svcFlushEntireDataCache
    dumpCode[4] = 0xE12FFF1Eu; // bx lr
    dumpCode[6] = callbackPage + DIAG_DUMP_FLAG_OFFSET;

    static const char title[] = "Dump CTGP-7 + MK7 memory";
    memcpy(g_loaderDiagCallbackPage + 0x20, title, sizeof(title));

    u32 *restoreCode = (u32 *)(g_loaderDiagCallbackPage + 0x60);
    restoreCode[0] = 0xE59F0010u; // ldr r0,[pc,#0x10] -> word 6
    restoreCode[1] = 0xE3A01001u; // mov r1,#1
    restoreCode[2] = 0xE5801000u; // str r1,[r0]
    restoreCode[3] = 0xEF000092u; // svcFlushEntireDataCache
    restoreCode[4] = 0xE12FFF1Eu; // bx lr
    restoreCode[6] = callbackPage + DIAG_RESTORE_FLAG_OFFSET;

    static const char restoreTitle[] = "Restore CTGP-7 + MK7 memory";
    memcpy(g_loaderDiagCallbackPage + 0x80, restoreTitle, sizeof(restoreTitle));

    /* menuThreadMain replaces its BL scanHeldKeys with a BL to this hook.
       active=1 covers the entire callback, including the synthetic menuShow
       session. R restores the BL immediately, then waits for active=0
       before restoring the menu/unmapping this page. */
    u32 *code = (u32 *)(g_loaderDiagCallbackPage + DIAG_OPEN_HOOK_OFFSET);
    const u32 litBase = (DIAG_OPEN_LITERALS_OFFSET - DIAG_OPEN_HOOK_OFFSET) / 4u;
    code[0]  = 0xE92D51FEu; // push {r1-r8,r12,lr}
    code[1]  = LoaderDiag_ArmLdrLiteral(1,  litBase + 1, 1); // active flag
    code[2]  = 0xE3A02001u; // mov r2,#1
    code[3]  = 0xE5812000u; // str r2,[r1]
    code[4]  = 0xEF000092u; // flush shared flag
    code[5]  = LoaderDiag_ArmLdrLiteral(5,  litBase + 0, 12);
    code[6]  = 0xE12FFF3Cu; // blx original scanHeldKeys
    code[7]  = 0xE1A08000u; // mov r8,r0
    code[8]  = LoaderDiag_ArmLdrLiteral(8,  litBase + 2, 1); // open flag
    code[9]  = 0xE5912000u; // ldr r2,[r1]
    code[10] = 0xE3520000u; // cmp r2,#0
    code[11] = 0x0A000010u; // beq clear-active
    code[12] = 0xE3A02000u; // mov r2,#0
    code[13] = 0xE5812000u; // str r2,[r1]
    code[14] = LoaderDiag_ArmLdrLiteral(14, litBase + 3, 12);
    code[15] = 0xE12FFF3Cu; // blx menuEnter
    code[16] = LoaderDiag_ArmLdrLiteral(16, litBase + 4, 2);
    code[17] = 0xE3520000u; // cmp isN3DS,#0
    code[18] = 0x0A000001u; // beq skip N3DS update
    code[19] = LoaderDiag_ArmLdrLiteral(19, litBase + 5, 12);
    code[20] = 0xE12FFF3Cu;
    code[21] = LoaderDiag_ArmLdrLiteral(21, litBase + 6, 12);
    code[22] = 0xE12FFF3Cu;
    code[23] = LoaderDiag_ArmLdrLiteral(23, litBase + 7, 0);
    code[24] = LoaderDiag_ArmLdrLiteral(24, litBase + 8, 12);
    code[25] = 0xE12FFF3Cu;
    code[26] = LoaderDiag_ArmLdrLiteral(26, litBase + 9, 12);
    code[27] = 0xE12FFF3Cu;
    code[28] = 0xE3A08000u; // synthetic menu path returns no normal combo keys
    code[29] = LoaderDiag_ArmLdrLiteral(29, litBase + 1, 1); // clear-active
    code[30] = 0xE3A02000u;
    code[31] = 0xE5812000u;
    code[32] = 0xEF000092u;
    code[33] = 0xE1A00008u; // mov r0,r8
    code[34] = 0xE8BD51FEu; // pop {r1-r8,r12,lr}
    code[35] = 0xE12FFF1Eu; // bx lr

    u32 *lit = (u32 *)(g_loaderDiagCallbackPage + DIAG_OPEN_LITERALS_OFFSET);
    lit[0] = flow->originalScanHeldKeys;
    lit[1] = callbackPage + DIAG_ACTIVE_FLAG_OFFSET;
    lit[2] = callbackPage + DIAG_OPEN_FLAG_OFFSET;
    lit[3] = flow->menuEnter;
    lit[4] = isN3DS ? 1u : 0u;
    lit[5] = flow->n3dsUpdate;
    lit[6] = flow->pluginLoaderUpdate;
    lit[7] = flow->rosalinaMenu;
    lit[8] = flow->menuShow;
    lit[9] = flow->menuLeave;

    *(volatile u32 *)(g_loaderDiagCallbackPage + DIAG_RESTORE_FLAG_OFFSET) = 0;
    *(volatile u32 *)(g_loaderDiagCallbackPage + DIAG_DUMP_FLAG_OFFSET) = 0;
    *(volatile u32 *)(g_loaderDiagCallbackPage + DIAG_ACTIVE_FLAG_OFFSET) = 0;
    *(volatile u32 *)(g_loaderDiagCallbackPage + DIAG_OPEN_FLAG_OFFSET) = 0;
    svcFlushEntireDataCache();
}

static Result LoaderDiag_MapCallbackPage(Handle rosalina, u32 *remotePage)
{
    // Keep the callback close to stock Rosalina's 0x14000000 text and search
    // downward, mirroring the Loader-side high-to-low mapping rule.
    u32 source = (u32)g_loaderDiagCallbackPage;
    for(u32 address = 0x13FFF000u; address >= 0x13000000u; address -= 0x1000u)
    {
        Result res = svcMapProcessMemoryEx(rosalina, address, CUR_PROCESS_HANDLE, source, 0x1000, 0);
        if(R_SUCCEEDED(res))
        {
            *remotePage = address;
            return 0;
        }
        if(address == 0x13000000u)
            break;
    }
    return (Result)0xD86007F3;
}

static Result LoaderDiag_InjectRosalinaMenu(void)
{
    static const char rosalinaName[8] = {'r','o','s','a','l','i','n','a'};
    Handle rosalina = 0;
    Result res = LoaderDiag_OpenProcessByName(rosalinaName, &rosalina);
    if(R_FAILED(res))
        LoaderDiag_Fatal(0x1001u, res);

    u32 localBase = 0, remoteStart = 0, textSize = 0, totalSize = 0;
    res = LoaderDiag_MapProcessImage(rosalina, &localBase, &remoteStart, &textSize, &totalSize);
    if(R_FAILED(res))
        LoaderDiag_Fatal(0x1002u, res);

    LoaderDiagMenu *menu = NULL;
    u32 miscIndex = 0, endIndex = 0;
    res = LoaderDiag_FindRosalinaMenu((u8 *)localBase, remoteStart, totalSize, textSize, &menu, &miscIndex, &endIndex);
    if(R_FAILED(res))
        LoaderDiag_Fatal(0x1003u, res);

    u32 remoteMenu = remoteStart + (u32)((u8 *)menu - (u8 *)localBase);
    LoaderDiagRosalinaOpenFlow openFlow = {0};
    res = LoaderDiag_FindRosalinaOpenFlow((const u8 *)localBase, remoteStart, totalSize, textSize, remoteMenu, &openFlow);
    if(R_FAILED(res))
        LoaderDiag_Fatal(0x1009u, res);

    u32 callbackPage = 0;
    res = LoaderDiag_MapCallbackPage(rosalina, &callbackPage);
    if(R_FAILED(res))
        LoaderDiag_Fatal(0x1004u, res);

    /* The original working X path explicitly enabled RWX in Rosalina before
       branching into the mapped callback page. Keep that proven behavior:
       without it the callback page faults on execute on stock Luma. */
    res = svcControlProcess(rosalina, PROCESSOP_SET_MMU_TO_RWX, 0, 0);
    if(R_FAILED(res))
        LoaderDiag_Fatal(0x1007u, res);

    LoaderDiag_PrepareCallbackPage(callbackPage, &openFlow);

    u32 *hookLocal = (u32 *)((u8 *)localBase + (openFlow.hookSite - remoteStart));
    g_loaderDiagSavedMenu = *menu;
    g_loaderDiagSavedMenuRemote = remoteMenu;
    g_loaderDiagSavedHookSite = openFlow.hookSite;
    g_loaderDiagSavedHookInsn = *hookLocal;
    g_loaderDiagCallbackRemotePage = callbackPage;
    g_loaderDiagSavedXStateValid = true;

    for(u32 i = endIndex + 2; i > miscIndex + 1; i--)
        menu->items[i] = menu->items[i - 2];

    menu->items[miscIndex].title = callbackPage + 0x20;
    menu->items[miscIndex].actionType = DIAG_METHOD;
    menu->items[miscIndex].target = callbackPage;
    menu->items[miscIndex].visibility = 0;

    menu->items[miscIndex + 1].title = callbackPage + 0x80;
    menu->items[miscIndex + 1].actionType = DIAG_METHOD;
    menu->items[miscIndex + 1].target = callbackPage + 0x60;
    menu->items[miscIndex + 1].visibility = 0;

    u32 hookInsn = 0;
    if(!LoaderDiag_EncodeArmBL(openFlow.hookSite, callbackPage + DIAG_OPEN_HOOK_OFFSET, &hookInsn))
        LoaderDiag_Fatal(0x100Au, (Result)0xD8E007ED);
    *hookLocal = hookInsn;

    svcFlushEntireDataCache();
    svcInvalidateEntireInstructionCache();

    if(menu->items[miscIndex].title != callbackPage + 0x20 ||
       menu->items[miscIndex].actionType != DIAG_METHOD ||
       menu->items[miscIndex].target != callbackPage ||
       menu->items[miscIndex].visibility != 0 ||
       menu->items[miscIndex + 1].title != callbackPage + 0x80 ||
       menu->items[miscIndex + 1].actionType != DIAG_METHOD ||
       menu->items[miscIndex + 1].target != callbackPage + 0x60 ||
       menu->items[miscIndex + 1].visibility != 0)
        LoaderDiag_Fatal(0x1008u, (Result)0xD8E007ED);
    if(*hookLocal != hookInsn)
        LoaderDiag_Fatal(0x100Au, (Result)0xD8E007ED);

    g_loaderDiagXHookRestored = false;
    g_loaderDiagInjected = true;
    svcUnmapProcessMemoryEx(CUR_PROCESS_HANDLE, localBase, totalSize);
    svcCloseHandle(rosalina);
    return 0;
}


static bool LoaderDiag_XCallbackActive(void)
{
    if(!g_loaderDiagInjected)
        return false;
    (void)svcInvalidateProcessDataCache(CUR_PROCESS_HANDLE,
                                        (u32)g_loaderDiagCallbackPage,
                                        sizeof(g_loaderDiagCallbackPage));
    return *(volatile u32 *)(g_loaderDiagCallbackPage + DIAG_ACTIVE_FLAG_OFFSET) != 0;
}

static bool LoaderDiag_TakeDumpRequest(void)
{
    if(!g_loaderDiagInjected)
        return false;
    (void)svcInvalidateProcessDataCache(CUR_PROCESS_HANDLE,
                                        (u32)g_loaderDiagCallbackPage,
                                        sizeof(g_loaderDiagCallbackPage));
    volatile u32 *flag = (volatile u32 *)(g_loaderDiagCallbackPage + DIAG_DUMP_FLAG_OFFSET);
    if(*flag == 0)
        return false;
    *flag = 0;
    svcFlushEntireDataCache();
    return true;
}

static bool LoaderDiag_TakeRestoreRequest(void)
{
    if(!g_loaderDiagInjected)
        return false;
    (void)svcInvalidateProcessDataCache(CUR_PROCESS_HANDLE,
                                        (u32)g_loaderDiagCallbackPage,
                                        sizeof(g_loaderDiagCallbackPage));
    volatile u32 *flag = (volatile u32 *)(g_loaderDiagCallbackPage + DIAG_RESTORE_FLAG_OFFSET);
    if(*flag == 0)
        return false;
    *flag = 0;
    svcFlushEntireDataCache();
    return true;
}

/* Phase 1 of X cleanup. Restore the scanHeldKeys BL immediately so no new
   callback executions can begin. Do not touch the menu table or mapping yet:
   an already-running callback may still be inside menuShow. */
static Result LoaderDiag_RestoreXHookOnly(void)
{
    if(!g_loaderDiagInjected || g_loaderDiagXHookRestored)
        return 0;
    if(!g_loaderDiagSavedXStateValid)
        return (Result)0xD8E007ED;

    static const char rosalinaName[8] = {'r','o','s','a','l','i','n','a'};
    Handle rosalina = 0;
    Result res = LoaderDiag_OpenProcessByName(rosalinaName, &rosalina);
    if(R_FAILED(res)) return res;

    u32 localBase = 0, remoteStart = 0, textSize = 0, totalSize = 0;
    res = LoaderDiag_MapProcessImage(rosalina, &localBase, &remoteStart, &textSize, &totalSize);
    if(R_FAILED(res)) { svcCloseHandle(rosalina); return res; }

    if(g_loaderDiagSavedHookSite < remoteStart ||
       g_loaderDiagSavedHookSite - remoteStart > totalSize - 4u)
        res = (Result)0xD8E007ED;
    else
    {
        u32 *hook = (u32 *)(localBase + g_loaderDiagSavedHookSite - remoteStart);
        *hook = g_loaderDiagSavedHookInsn;
        svcFlushEntireDataCache();
        svcInvalidateEntireInstructionCache();
        if(*hook != g_loaderDiagSavedHookInsn)
            res = (Result)0xD8E007ED;
    }

    svcUnmapProcessMemoryEx(CUR_PROCESS_HANDLE, localBase, totalSize);
    svcCloseHandle(rosalina);
    if(R_SUCCEEDED(res))
    {
        g_loaderDiagXHookRestored = true;
        *(volatile u32 *)(g_loaderDiagCallbackPage + DIAG_OPEN_FLAG_OFFSET) = 0;
        svcFlushEntireDataCache();
    }
    return res;
}

/* Phase 2 runs only after the callback has reported active=0. The BL has
   already been restored, so after a short grace period nobody can still be
   executing the callback page. Restore the menu table and remove the page. */
static Result LoaderDiag_FinalizeXRestore(void)
{
    if(!g_loaderDiagInjected)
        return 0;
    if(!g_loaderDiagXHookRestored || LoaderDiag_XCallbackActive())
        return (Result)0xD8E007ED;

    svcSleepThread(20LL * 1000LL * 1000LL);
    if(LoaderDiag_XCallbackActive())
        return (Result)0xD8E007ED;

    static const char rosalinaName[8] = {'r','o','s','a','l','i','n','a'};
    Handle rosalina = 0;
    Result res = LoaderDiag_OpenProcessByName(rosalinaName, &rosalina);
    if(R_FAILED(res)) return res;

    u32 localBase = 0, remoteStart = 0, textSize = 0, totalSize = 0;
    res = LoaderDiag_MapProcessImage(rosalina, &localBase, &remoteStart, &textSize, &totalSize);
    if(R_FAILED(res)) { svcCloseHandle(rosalina); return res; }

    if(g_loaderDiagSavedMenuRemote < remoteStart ||
       g_loaderDiagSavedMenuRemote - remoteStart > totalSize - sizeof(LoaderDiagMenu))
        res = (Result)0xD8E007ED;
    else
    {
        LoaderDiagMenu *menu = (LoaderDiagMenu *)(localBase + g_loaderDiagSavedMenuRemote - remoteStart);
        *menu = g_loaderDiagSavedMenu;
        svcFlushEntireDataCache();
        if(memcmp(menu, &g_loaderDiagSavedMenu, sizeof(LoaderDiagMenu)) != 0)
            res = (Result)0xD8E007ED;
    }

    svcUnmapProcessMemoryEx(CUR_PROCESS_HANDLE, localBase, totalSize);
    if(R_SUCCEEDED(res) && g_loaderDiagCallbackRemotePage)
    {
        /* At this point the BL and menu have already been restored and the
           callback has been inactive for the grace period. Failure to remove
           an unreachable helper mapping must not strand CTGP in the gate. */
        (void)svcUnmapProcessMemoryEx(rosalina, g_loaderDiagCallbackRemotePage, 0x1000u);
    }
    svcCloseHandle(rosalina);

    if(R_SUCCEEDED(res))
    {
        g_loaderDiagInjected = false;
        g_loaderDiagSavedXStateValid = false;
        g_loaderDiagXHookRestored = false;
        g_loaderDiagSavedMenuRemote = 0;
        g_loaderDiagSavedHookSite = 0;
        g_loaderDiagSavedHookInsn = 0;
        g_loaderDiagCallbackRemotePage = 0;
        *(volatile u32 *)(g_loaderDiagCallbackPage + DIAG_RESTORE_FLAG_OFFSET) = 0;
        *(volatile u32 *)(g_loaderDiagCallbackPage + DIAG_DUMP_FLAG_OFFSET) = 0;
        *(volatile u32 *)(g_loaderDiagCallbackPage + DIAG_ACTIVE_FLAG_OFFSET) = 0;
        *(volatile u32 *)(g_loaderDiagCallbackPage + DIAG_OPEN_FLAG_OFFSET) = 0;
        svcFlushEntireDataCache();
    }
    return res;
}

static bool LoaderDiag_TrapIsActive(void)
{
    if(!g_loaderDiagTrapMapped || !g_loaderDiagTrapLocal)
        return false;
    (void)svcInvalidateProcessDataCache(CUR_PROCESS_HANDLE, g_loaderDiagTrapLocal, DIAG_TRAP_PAGE_SIZE);
    volatile u32 *ctrl = LoaderDiag_TrapControlLocal();
    return ctrl != NULL && ctrl[2] == DIAG_TRAP_MAGIC && ctrl[0] == 1u;
}

static bool LoaderDiag_TrapHasDeparted(void)
{
    if(!g_loaderDiagTrapMapped || !g_loaderDiagTrapLocal)
        return false;
    (void)svcInvalidateProcessDataCache(CUR_PROCESS_HANDLE, g_loaderDiagTrapLocal, DIAG_TRAP_PAGE_SIZE);
    volatile u32 *ctrl = LoaderDiag_TrapControlLocal();
    return ctrl != NULL && ctrl[2] == DIAG_TRAP_MAGIC && ctrl[0] == 2u;
}

static Result LoaderDiag_TryCompletePendingRelease(void)
{
    if(!g_loaderDiagReleasePending)
        return 0;

    if(g_loaderDiagInjected)
    {
        Result res = LoaderDiag_RestoreXHookOnly();
        if(R_FAILED(res)) return res;
        if(LoaderDiag_XCallbackActive())
            return 0;
        res = LoaderDiag_FinalizeXRestore();
        if(R_FAILED(res)) return res;
    }

    bool earlyActive = LoaderDiag_TrapIsActive();
    u32 installerState = LoaderDiag_LateInstallerState();
    bool keepZlForRun = earlyActive && g_loaderDiagZlArmed &&
                        g_loaderDiagLateInstallerPatched && g_loaderDiagStartupIvPatched &&
                        installerState == 0u;

    /* Staged R behavior for the intended experiment:
         Y -> CTGP held before first instruction
         ZL -> install CTGP-only 0x07000130 mid-load installer
         R -> clean/release Y, but intentionally keep the ZL installer alive
       The target-side installer self-restores 0x07000130 when it fires and
       creates the actual post-unpack hold. A later R cleans/releases that. */
    if(keepZlForRun)
    {
        Result res = LoaderDiag_RestoreYHook();
        if(R_FAILED(res)) return res;

        volatile u32 *ctrl = LoaderDiag_TrapControlLocal();
        if(ctrl == NULL || ctrl[2] != DIAG_TRAP_MAGIC)
            return (Result)0xD8E007ED;
        ctrl[1] = 1u;
        svcFlushEntireDataCache();
        if(g_loaderDiagTrapProcess && g_loaderDiagTrapRemote)
            (void)svcInvalidateProcessDataCache(g_loaderDiagTrapProcess, g_loaderDiagTrapRemote, DIAG_TRAP_PAGE_SIZE);

        g_loaderDiagYArmed = false;
        g_loaderDiagReleasePending = false;
        return 0;
    }

    /* An extra R during startup waits for the target installer to finish.
       Cancelling the IV repair after Y releases could race its consumption. */
    if(g_loaderDiagStartupIvPatched && installerState == 0u && !earlyActive)
        return 0;

    if(!earlyActive && g_loaderDiagZlArmed && installerState == 1u &&
       !LoaderDiag_LateTrapIsActive() && !LoaderDiag_LateTrapHasDeparted())
        return 0;

    Result res = LoaderDiag_RestoreLateHook();
    if(R_FAILED(res)) return res;
    res = LoaderDiag_RestoreYHook();
    if(R_FAILED(res)) return res;

    if(earlyActive)
    {
        volatile u32 *ctrl = LoaderDiag_TrapControlLocal();
        if(ctrl == NULL || ctrl[2] != DIAG_TRAP_MAGIC)
            return (Result)0xD8E007ED;
        ctrl[1] = 1u;
    }

    if(LoaderDiag_LateTrapIsActive())
    {
        volatile u32 *ctrl = LoaderDiag_LateTrapControlLocal();
        if(ctrl == NULL || ctrl[2] != DIAG_LATE_TRAP_MAGIC)
            return (Result)0xD8E007ED;
        ctrl[1] = 1u;
    }

    svcFlushEntireDataCache();
    if(g_loaderDiagTrapProcess && g_loaderDiagTrapRemote)
        (void)svcInvalidateProcessDataCache(g_loaderDiagTrapProcess, g_loaderDiagTrapRemote, DIAG_TRAP_PAGE_SIZE);
    if(g_loaderDiagLateProcess && g_loaderDiagLateTrapRemote)
        (void)svcInvalidateProcessDataCache(g_loaderDiagLateProcess, g_loaderDiagLateTrapRemote, DIAG_TRAP_PAGE_SIZE);

    if(g_loaderDiagLateTrapMapped && installerState == 0u &&
       !LoaderDiag_LateTrapIsActive() && !LoaderDiag_LateTrapHasDeparted())
        LoaderDiag_ResetLateTrapMapping(true);

    g_loaderDiagYArmed = false;
    g_loaderDiagZlArmed = false;
    g_loaderDiagReleasePending = false;
    return 0;
}

static Result LoaderDiag_RestoreAndRelease(void)
{
    /* R is the universal cleaner. Restore CTGP's launcher-modified Rosalina
       word first, then remove every hook this Loader added before releasing
       either the Y pre-entry gate or the ZL post-unpack gate. */
    Result res = LoaderDiag_SetRosalinaLauncherWord(DIAG_ROS_LAUNCH_WORD_CLEAN);
    if(R_FAILED(res))
        return res;

    g_loaderDiagReleasePending = true;
    return LoaderDiag_TryCompletePendingRelease();
}

static Result LoaderDiag_OpenDumpFile(IFile *file, const char *path)
{
    FS_Archive archive;
    Result res = FSUSER_OpenArchive(&archive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""));
    if(R_FAILED(res))
        return res;

    FSUSER_CreateDirectory(archive, fsMakePath(PATH_ASCII, "/luma"), 0);
    FSUSER_CreateDirectory(archive, fsMakePath(PATH_ASCII, "/luma/dumps"), 0);
    FSUSER_CreateDirectory(archive, fsMakePath(PATH_ASCII, "/luma/dumps/memory"), 0);

    res = IFile_OpenFromArchive(file, archive, fsMakePath(PATH_ASCII, path), FS_OPEN_WRITE | FS_OPEN_CREATE);
    FSUSER_CloseArchive(archive);
    if(R_SUCCEEDED(res))
        res = IFile_SetSize(file, 0);
    if(R_FAILED(res))
        IFile_Close(file);
    return res;
}

Result LoaderDiag_WriteDump(const char *path, const void *data, u32 size)
{
    IFile file = {0};
    Result res = LoaderDiag_OpenDumpFile(&file, path);
    if(R_FAILED(res))
        return res;

    const u8 *p = (const u8 *)data;
    u32 remaining = size;
    while(remaining != 0)
    {
        u32 chunk = remaining > 0x10000u ? 0x10000u : remaining;
        u64 written = 0;
        res = IFile_Write(&file, &written, p, chunk, remaining == chunk ? FS_WRITE_FLUSH : 0);
        if(R_FAILED(res) || written != chunk)
        {
            if(R_SUCCEEDED(res)) res = (Result)0xD9004587;
            break;
        }
        p += chunk;
        remaining -= chunk;
    }
    IFile_Close(&file);
    return res;
}

static Result LoaderDiag_Probe3gxSize(Handle process, u32 *outSize)
{
    u32 scratch = 0;
    Result res = LoaderDiag_FindFreeRange(0x1000, &scratch);
    if(R_FAILED(res))
        return res;

    u32 size = 0;
    while(size < DIAG_3GX_MAX_SIZE)
    {
        res = svcMapProcessMemoryEx(CUR_PROCESS_HANDLE, scratch, process, DIAG_3GX_BASE + size, 0x1000, 0);
        if(R_FAILED(res))
            break;
        svcUnmapProcessMemoryEx(CUR_PROCESS_HANDLE, scratch, 0x1000);
        size += 0x1000;
    }

    if(size <= DIAG_3GX_DUMP_SKIP + DIAG_3GX_DUMP_TAIL)
        return (Result)0xD8E007ED;
    *outSize = size;
    return 0;
}

static Result LoaderDiag_Dump3gx(Handle marioKart)
{
    u32 blockSize = 0;
    Result res = LoaderDiag_Probe3gxSize(marioKart, &blockSize);
    if(R_FAILED(res))
        return res;

    u32 localBase = 0;
    if(R_FAILED(res = LoaderDiag_FindFreeRange(blockSize, &localBase)))
        return res;
    if(R_FAILED(res = svcMapProcessMemoryEx(CUR_PROCESS_HANDLE, localBase, marioKart, DIAG_3GX_BASE, blockSize, 0)))
        return res;

    u32 dumpSize = blockSize - DIAG_3GX_DUMP_SKIP - DIAG_3GX_DUMP_TAIL;
    res = LoaderDiag_WriteDump("/luma/dumps/memory/snapshot3gx_loaderonly.bin", (const void *)(localBase + DIAG_3GX_DUMP_SKIP), dumpSize);
    svcUnmapProcessMemoryEx(CUR_PROCESS_HANDLE, localBase, blockSize);
    return res;
}

static Result LoaderDiag_DumpMarioKart(Handle marioKart)
{
    u32 remoteStart = 0, textSize = 0, totalSize = 0;
    Result res = LoaderDiag_GetProcessImageInfo(marioKart, &remoteStart, &textSize, &totalSize);
    if(R_FAILED(res))
        return res;

    u32 localBase = 0;
    if(R_FAILED(res = LoaderDiag_FindFreeRange(totalSize, &localBase)))
        return res;
    if(R_FAILED(res = svcMapProcessMemoryEx(CUR_PROCESS_HANDLE, localBase, marioKart, remoteStart, totalSize, 0)))
        return res;

    res = LoaderDiag_WriteDump("/luma/dumps/memory/snapshot_mk7_loaderonly.bin", (const void *)localBase, totalSize);
    svcUnmapProcessMemoryEx(CUR_PROCESS_HANDLE, localBase, totalSize);
    return res;
}

static Result LoaderDiag_DumpAll(void)
{
    static const char rosalinaName[8] = {'r','o','s','a','l','i','n','a'};
    static const char marioKartName[8] = {'M','a','r','i','o','K','a','r'};
    Handle rosalina = 0, marioKart = 0;
    Result res = LoaderDiag_OpenProcessByName(rosalinaName, &rosalina);
    if(R_FAILED(res))
        LoaderDiag_Fatal(0x2001u, res);
    res = LoaderDiag_OpenProcessByName(marioKartName, &marioKart);
    if(R_FAILED(res))
        LoaderDiag_Fatal(0x2002u, res);

    // Stock Rosalina owns the complete 3GX backing block at 0x07000000.
    // The game only has the executable and plugin heap mapped into separate VAs.
    Result gxRes = LoaderDiag_Dump3gx(rosalina);
    if(R_FAILED(gxRes))
        LoaderDiag_Fatal(0x2003u, gxRes);
    Result gameRes = LoaderDiag_DumpMarioKart(marioKart);
    if(R_FAILED(gameRes))
        LoaderDiag_Fatal(0x2004u, gameRes);
    svcCloseHandle(marioKart);
    svcCloseHandle(rosalina);
    return 0;
}

static Result LoaderDiag_GetFileSize(const char *path, u64 *size)
{
    IFile file = {0};
    Result res = IFile_Open(&file, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""),
                            fsMakePath(PATH_ASCII, path), FS_OPEN_READ);
    if(R_FAILED(res))
        return res;
    res = IFile_GetSize(&file, size);
    IFile_Close(&file);
    return res;
}

static Result LoaderDiag_ReadExactFile(const char *path, void *dst, u32 expectedSize)
{
    IFile file = {0};
    Result res = IFile_Open(&file, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""),
                            fsMakePath(PATH_ASCII, path), FS_OPEN_READ);
    if(R_FAILED(res))
        return res;

    u64 size = 0;
    res = IFile_GetSize(&file, &size);
    if(R_SUCCEEDED(res) && size != expectedSize)
        res = (Result)0xD8E007ED;

    u8 *p = (u8 *)dst;
    u32 remaining = expectedSize;
    while(R_SUCCEEDED(res) && remaining != 0)
    {
        u32 chunk = remaining > 0x10000u ? 0x10000u : remaining;
        u64 got = 0;
        res = IFile_Read(&file, &got, p, chunk);
        if(R_SUCCEEDED(res) && got != chunk)
            res = (Result)0xD9004587;
        p += chunk;
        remaining -= chunk;
    }
    IFile_Close(&file);
    return res;
}

static Result LoaderDiag_Restore3gx(Handle rosalina, u32 restoreSize)
{
    u32 blockSize = restoreSize + DIAG_3GX_DUMP_SKIP + DIAG_3GX_DUMP_TAIL;
    if(blockSize > DIAG_3GX_MAX_SIZE || restoreSize == 0)
        return (Result)0xD8E007ED;

    u32 localBase = 0;
    Result res = LoaderDiag_FindFreeRange(blockSize, &localBase);
    if(R_FAILED(res)) return res;
    res = svcMapProcessMemoryEx(CUR_PROCESS_HANDLE, localBase, rosalina, DIAG_3GX_BASE, blockSize, 0);
    if(R_FAILED(res)) return res;

    res = LoaderDiag_ReadExactFile("/luma/dumps/memory/snapshot3gx_loaderonly.bin",
                                   (void *)(localBase + DIAG_3GX_DUMP_SKIP), restoreSize);
    if(R_SUCCEEDED(res))
    {
        svcFlushEntireDataCache();
        svcInvalidateEntireInstructionCache();
    }
    svcUnmapProcessMemoryEx(CUR_PROCESS_HANDLE, localBase, blockSize);
    return res;
}

static Result LoaderDiag_RestoreMarioKart(Handle marioKart, u32 remoteStart, u32 restoreSize)
{
    if(restoreSize == 0 || (restoreSize & 0xFFFu) != 0 || restoreSize > 0x01000000u)
        return (Result)0xD8E007ED;

    u32 localBase = 0;
    Result res = LoaderDiag_FindFreeRange(restoreSize, &localBase);
    if(R_FAILED(res)) return res;
    /* Deliberately map the size stored in the snapshot, not the current
       CodeSet-reported total. The bad Rosalina experiment can report a much
       shorter image even though the saved known-good image is 0x5BF000. */
    res = svcMapProcessMemoryEx(CUR_PROCESS_HANDLE, localBase, marioKart, remoteStart, restoreSize, 0);
    if(R_FAILED(res)) return res;

    res = LoaderDiag_ReadExactFile("/luma/dumps/memory/snapshot_mk7_loaderonly.bin",
                                   (void *)localBase, restoreSize);
    if(R_SUCCEEDED(res))
    {
        svcFlushEntireDataCache();
        svcInvalidateEntireInstructionCache();
    }
    svcUnmapProcessMemoryEx(CUR_PROCESS_HANDLE, localBase, restoreSize);
    return res;
}


static Result LoaderDiag_RestoreAll(void)
{
    static const char rosalinaName[8] = {'r','o','s','a','l','i','n','a'};
    static const char marioKartName[8] = {'M','a','r','i','o','K','a','r'};
    Handle rosalina = 0, marioKart = 0;
    Result res = LoaderDiag_OpenProcessByName(rosalinaName, &rosalina);
    if(R_FAILED(res)) return res;
    res = LoaderDiag_OpenProcessByName(marioKartName, &marioKart);
    if(R_FAILED(res)) { svcCloseHandle(rosalina); return res; }

    u64 file3gx64 = 0, fileGame64 = 0;
    res = LoaderDiag_GetFileSize("/luma/dumps/memory/snapshot3gx_loaderonly.bin", &file3gx64);
    if(R_SUCCEEDED(res)) res = LoaderDiag_GetFileSize("/luma/dumps/memory/snapshot_mk7_loaderonly.bin", &fileGame64);
    if(R_SUCCEEDED(res) && (file3gx64 == 0 || file3gx64 > 0xFFFFFFFFULL ||
                            fileGame64 == 0 || fileGame64 > 0xFFFFFFFFULL))
        res = (Result)0xD8E007ED;

    u32 remoteStart = 0, textSize = 0, currentTotal = 0;
    if(R_SUCCEEDED(res))
        res = LoaderDiag_GetProcessImageInfo(marioKart, &remoteStart, &textSize, &currentTotal);
    (void)textSize;
    (void)currentTotal;

    u32 backingSize = 0;
    if(R_SUCCEEDED(res))
        res = LoaderDiag_Probe3gxSize(rosalina, &backingSize);

    u32 file3gx = (u32)file3gx64;
    u32 fileGame = (u32)fileGame64;
    u32 neededBacking = file3gx + DIAG_3GX_DUMP_SKIP + DIAG_3GX_DUMP_TAIL;
    if(R_SUCCEEDED(res) && (neededBacking > backingSize || neededBacking > DIAG_3GX_MAX_SIZE ||
                            (fileGame & 0xFFFu) != 0 || fileGame > 0x01000000u))
        res = (Result)0xD8E007ED;

    if(R_SUCCEEDED(res)) res = LoaderDiag_Restore3gx(rosalina, file3gx);
    if(R_SUCCEEDED(res)) res = LoaderDiag_RestoreMarioKart(marioKart, remoteStart, fileGame);

    svcCloseHandle(marioKart);
    svcCloseHandle(rosalina);
    return res;
}


static Result LoaderDiag_HidInitDirect(void)
{
    Handle hidHandle = 0;
    Result res = srvGetServiceHandle(&hidHandle, "hid:USER");
    if(R_FAILED(res))
        return res;

    u32 *cmdbuf = getThreadCommandBuffer();
    cmdbuf[0] = 0x000A0000u; // HIDUSER_GetHandles
    res = svcSendSyncRequest(hidHandle);
    if(R_SUCCEEDED(res))
        res = (Result)cmdbuf[1];
    if(R_FAILED(res))
    {
        svcCloseHandle(hidHandle);
        return res;
    }

    Handle memHandle = (Handle)cmdbuf[3];
    // We poll shared memory directly, so none of HID's update event handles are needed.
    for(u32 i = 4; i <= 8; i++)
        if(cmdbuf[i] != 0)
            svcCloseHandle((Handle)cmdbuf[i]);
    svcCloseHandle(hidHandle);

    if(memHandle == 0)
        return (Result)0xD8E007ED;

    u32 mapBase = 0;
    res = LoaderDiag_FindMappableRange(0x1000u, &mapBase);
    if(R_FAILED(res))
    {
        svcCloseHandle(memHandle);
        return res;
    }

    res = svcMapMemoryBlock(memHandle, mapBase, MEMPERM_READ, MEMPERM_DONTCARE);
    if(R_FAILED(res))
    {
        svcCloseHandle(memHandle);
        return res;
    }

    g_loaderDiagHidMemHandle = memHandle;
    g_loaderDiagHidMapBase = mapBase;
    g_loaderDiagHidSharedMem = (volatile u32 *)mapBase;
    return 0;
}

static u32 LoaderDiag_HidKeysHeldDirect(void)
{
    volatile u8 *mem = (volatile u8 *)g_loaderDiagHidSharedMem;
    if(mem == NULL)
        return 0;

    // HID shared memory pad section: current ring index at +0x10,
    // held-button word at +0x28 + index*0x10. Read the index twice so
    // an HID update racing this poll cannot mix two ring entries.
    for(u32 tries = 0; tries < 4; tries++)
    {
        u32 index1 = *(volatile u32 *)(mem + 0x10);
        if(index1 > 7)
            continue;
        u32 held = *(volatile u32 *)(mem + 0x28 + index1 * 0x10);
        u32 index2 = *(volatile u32 *)(mem + 0x10);
        if(index1 == index2)
            return held;
    }
    return 0;
}

static Result LoaderDiag_IrrstInitDirect(void)
{
    if(g_loaderDiagIrrstSharedMem != NULL)
        return 0;

    Handle handle = 0;
    Result res = srvGetServiceHandle(&handle, "ir:rst");
    if(R_FAILED(res))
        return res;

    u32 *cmdbuf = getThreadCommandBuffer();
    cmdbuf[0] = 0x00010000u; // IRRST_GetHandles
    res = svcSendSyncRequest(handle);
    if(R_SUCCEEDED(res))
        res = (Result)cmdbuf[1];
    if(R_FAILED(res))
    {
        svcCloseHandle(handle);
        return res;
    }

    Handle memHandle = (Handle)cmdbuf[3];
    Handle eventHandle = (Handle)cmdbuf[4];
    if(eventHandle != 0)
        svcCloseHandle(eventHandle);
    if(memHandle == 0)
    {
        svcCloseHandle(handle);
        return (Result)0xD8E007ED;
    }

    cmdbuf[0] = 0x00020080u; // IRRST_Initialize(10, 0)
    cmdbuf[1] = 10;
    cmdbuf[2] = 0;
    res = svcSendSyncRequest(handle);
    if(R_SUCCEEDED(res))
        res = (Result)cmdbuf[1];
    if(R_FAILED(res))
    {
        svcCloseHandle(memHandle);
        svcCloseHandle(handle);
        return res;
    }

    u32 mapBase = 0;
    res = LoaderDiag_FindMappableRange(0x1000u, &mapBase);
    if(R_FAILED(res) || R_FAILED(res = svcMapMemoryBlock(memHandle, mapBase, MEMPERM_READ, MEMPERM_DONTCARE)))
    {
        svcCloseHandle(memHandle);
        svcCloseHandle(handle);
        return res;
    }

    g_loaderDiagIrrstHandle = handle;
    g_loaderDiagIrrstMemHandle = memHandle;
    g_loaderDiagIrrstMapBase = mapBase;
    g_loaderDiagIrrstSharedMem = (volatile u32 *)mapBase;
    return 0;
}

static u32 LoaderDiag_IrrstKeysHeldDirect(void)
{
    volatile u32 *mem = g_loaderDiagIrrstSharedMem;
    if(mem == NULL)
        return 0;

    for(u32 tries = 0; tries < 4; tries++)
    {
        u32 index1 = mem[4];
        if(index1 > 7)
            continue;
        u32 held = mem[6 + index1 * 4u];
        u32 index2 = mem[4];
        if(index1 == index2)
            return held;
    }
    return 0;
}

static void LoaderDiag_HidExitDirect(void)
{
    if(g_loaderDiagHidMemHandle != 0 && g_loaderDiagHidMapBase != 0)
        (void)svcUnmapMemoryBlock(g_loaderDiagHidMemHandle, g_loaderDiagHidMapBase);
    if(g_loaderDiagHidMemHandle != 0)
        svcCloseHandle(g_loaderDiagHidMemHandle);

    g_loaderDiagHidMemHandle = 0;
    g_loaderDiagHidMapBase = 0;
    g_loaderDiagHidSharedMem = NULL;
}

static void LoaderDiag_IrrstExitDirect(void)
{
    /* Pair our direct IRRST_Initialize with the matching Shutdown. Holding the
       service/session/memblock open into system poweroff can delay PM teardown. */
    if(g_loaderDiagIrrstHandle != 0)
    {
        u32 *cmdbuf = getThreadCommandBuffer();
        cmdbuf[0] = 0x00030000u; // IRRST_Shutdown
        (void)svcSendSyncRequest(g_loaderDiagIrrstHandle);
    }

    if(g_loaderDiagIrrstMemHandle != 0 && g_loaderDiagIrrstMapBase != 0)
        (void)svcUnmapMemoryBlock(g_loaderDiagIrrstMemHandle, g_loaderDiagIrrstMapBase);
    if(g_loaderDiagIrrstMemHandle != 0)
        svcCloseHandle(g_loaderDiagIrrstMemHandle);
    if(g_loaderDiagIrrstHandle != 0)
        svcCloseHandle(g_loaderDiagIrrstHandle);

    g_loaderDiagIrrstHandle = 0;
    g_loaderDiagIrrstMemHandle = 0;
    g_loaderDiagIrrstMapBase = 0;
    g_loaderDiagIrrstSharedMem = NULL;
}

static void LoaderDiag_ThreadShutdown(void)
{
    /* Do not leave our duplicate application handle around either. The remote
       mapping belongs to the dying application, so closing the duplicate is the
       important shutdown action; avoid cross-process unmap work during PM exit. */
    LoaderDiag_ResetTrapMapping(false);
    LoaderDiag_ResetLateTrapMapping(false);
    LoaderDiag_IrrstExitDirect();
    LoaderDiag_HidExitDirect();
    g_loaderDiagThreadExited = true;
}

void LoaderDiag_RequestShutdown(void)
{
    if(!g_loaderDiagThreadStarted)
        return;

    g_loaderDiagShutdownRequested = true;

    /* The worker polls every 20 ms. Give it a bounded window to perform the
       service-specific teardown before Loader receives the final 0x100 exit. */
    for(u32 i = 0; i < 50 && !g_loaderDiagThreadExited; i++)
        svcSleepThread(2LL * 1000LL * 1000LL);
}

static void LoaderDiag_ThreadMain(void *arg)
{
    (void)arg;

    while(true)
    {
        if(g_loaderDiagShutdownRequested)
        {
            LoaderDiag_ThreadShutdown();
            svcExitThread();
        }

        bool registered = false;
        Result regRes = srvIsServiceRegistered(&registered, "hid:USER");
        if(R_SUCCEEDED(regRes) && registered && R_SUCCEEDED(LoaderDiag_HidInitDirect()))
            break;
        svcSleepThread(250LL * 1000LL * 1000LL);
    }

    bool xWasHeld = false;
    bool yWasHeld = false;
    bool rWasHeld = false;
    bool zlWasHeld = false;
    bool zrWasHeld = false;
    u32 irrstRetry = 0;
    while(true)
    {
        if(g_loaderDiagShutdownRequested)
        {
            LoaderDiag_ThreadShutdown();
            svcExitThread();
        }

        /* ZL/ZR are independent controls. Do not make them depend on X or Y
           having been pressed first. Retry IRRST initialization periodically
           until the New-3DS service is ready. */
        if(isN3DS && g_loaderDiagIrrstSharedMem == NULL)
        {
            if(irrstRetry == 0)
                (void)LoaderDiag_IrrstInitDirect();
            irrstRetry = (irrstRetry + 1u) % 25u;
        }
        u32 held = LoaderDiag_HidKeysHeldDirect();
        bool xHeld = (held & KEY_X) != 0;
        bool yHeld = (held & KEY_Y) != 0;
        bool rHeld = (held & KEY_R) != 0;

        /* Y only arms/cancels the one-shot pre-entry listener. It is no longer
           a release key. Initialize IRRST here so ZR/ZL are available during
           the held pre-entry experiment even if X has not been pressed yet. */
        if(yHeld && !yWasHeld)
        {
            if(!g_loaderDiagYArmed && !LoaderDiag_TrapIsActive())
            {
                Result arm = LoaderDiag_DiscoverGamePatchLiteral();
                if(R_FAILED(arm))
                    LoaderDiag_Fatal(0x3101u, arm);
                Result irrRes = LoaderDiag_IrrstInitDirect();
                if(R_FAILED(irrRes))
                    LoaderDiag_Fatal(0x1101u, irrRes);
                g_loaderDiagYArmed = true;
            }
            else if(g_loaderDiagYArmed && !g_loaderDiagTrapMapped)
            {
                g_loaderDiagYArmed = false;
            }
        }
        yWasHeld = yHeld;

        /* X keeps the established dump behavior. */
        if(xHeld && !xWasHeld && !g_loaderDiagInjected)
        {
            (void)LoaderDiag_InjectRosalinaMenu();
            Result irrRes = LoaderDiag_IrrstInitDirect();
            if(R_FAILED(irrRes))
                LoaderDiag_Fatal(0x1101u, irrRes);
        }
        xWasHeld = xHeld;

        /* R is the universal cleaner/release key for X, Y and ZL. */
        if(rHeld && !rWasHeld)
        {
            Result rel = LoaderDiag_RestoreAndRelease();
            if(R_FAILED(rel))
                LoaderDiag_Fatal(0x3104u, rel);
        }
        rWasHeld = rHeld;

        if(g_loaderDiagIrrstSharedMem != NULL)
        {
            u32 irHeld = LoaderDiag_IrrstKeysHeldDirect();
            bool zlHeld = (irHeld & KEY_ZL) != 0;
            bool zrHeld = (irHeld & KEY_ZR) != 0;

            /* ZL installs the guarded CBC-IV repair before CTGP runs, together
               with the established 0x130 installer for the later sleep gate.
               Y must be holding the first instruction for installation or
               cancellation. Both ZL writes remain entirely CTGP-side. */
            if(zlHeld && !zlWasHeld)
            {
                if(!g_loaderDiagZlArmed && !LoaderDiag_LateTrapIsActive())
                {
                    (void)LoaderDiag_InstallLateMidHook();
                }
                else if(g_loaderDiagZlArmed && LoaderDiag_LateInstallerState() == 0u &&
                        !LoaderDiag_LateTrapIsActive() && LoaderDiag_TrapIsActive())
                {
                    if(R_SUCCEEDED(LoaderDiag_RestoreLateInstaller()))
                    {
                        g_loaderDiagZlArmed = false;
                        LoaderDiag_ResetLateTrapMapping(true);
                    }
                }
            }

            /* ZR toggles only the CTGP-launcher-modified Rosalina word back to
               its stock/original BL. It does not clean X/Y hooks or release. */
            if(zrHeld && !zrWasHeld)
            {
                Result zrRes = LoaderDiag_SetRosalinaLauncherWord(DIAG_ROS_LAUNCH_WORD_ORIGINAL);
                if(R_FAILED(zrRes))
                    LoaderDiag_Fatal(0x3201u, zrRes);
            }

            zlWasHeld = zlHeld;
            zrWasHeld = zrHeld;
        }

        if(LoaderDiag_TakeDumpRequest())
            (void)LoaderDiag_DumpAll();
        if(LoaderDiag_TakeRestoreRequest())
        {
            /* A restore mismatch is an experiment result, not a reason to
               deliberately UDF Loader. Leave the held process intact so the
               user can still dump/clean/release and inspect the failure. */
            (void)LoaderDiag_RestoreAll();
        }

        /* The ZL installer runs inside MK7 itself. Once it has executed it
           has already self-restored 0x07000130 and installed the late epilogue
           branch, so Loader only mirrors that state for cleanup bookkeeping. */
        if(g_loaderDiagZlArmed && LoaderDiag_LateInstallerState() == 1u)
            g_loaderDiagLateHookPatched = true;

        if(g_loaderDiagReleasePending)
        {
            Result rel = LoaderDiag_TryCompletePendingRelease();
            if(R_FAILED(rel))
                LoaderDiag_Fatal(0x3104u, rel);
        }

        /* Keep the temporary gate mapping until Mario Kart exits. Unmapping a
           page while another core is branching away from it is needless risk,
           and this page is outside CTGP's own executable/heap mappings. */
        (void)LoaderDiag_TrapHasDeparted();
        if(g_loaderDiagTrapProcess && svcWaitSynchronization(g_loaderDiagTrapProcess, 0) == 0)
        {
            /* Process died before normal release: never leave Rosalina's stock
               gamePatchFunc literal pointing at a dead temporary page. */
            (void)LoaderDiag_RestoreYHook();
            LoaderDiag_ResetTrapMapping(false);
            g_loaderDiagYArmed = false;
            g_loaderDiagReleasePending = false;
        }

        (void)LoaderDiag_LateTrapHasDeparted();
        if(g_loaderDiagLateProcess && svcWaitSynchronization(g_loaderDiagLateProcess, 0) == 0)
        {
            LoaderDiag_ResetLateTrapMapping(false);
            g_loaderDiagZlArmed = false;
        }

        svcSleepThread(DIAG_POLL_NS);
    }
}

Result LoaderDiag_StartThread(void)
{
    if(g_loaderDiagThreadStarted)
        return 0;

    g_loaderDiagShutdownRequested = false;
    g_loaderDiagThreadExited = false;

    Handle thread = 0;
    Result res = svcCreateThread(&thread, LoaderDiag_ThreadMain, 0,
                                 (u32 *)(g_loaderDiagThreadStack + sizeof(g_loaderDiagThreadStack)),
                                 DIAG_WORKER_PRIORITY, DIAG_CORE_SYSTEM);
    if(R_SUCCEEDED(res))
    {
        g_loaderDiagThreadStarted = true;
        svcCloseHandle(thread);
    }
    return res;
}

/* One-shot live K11 snapshot for ARM9 patch-output comparison.
   Runs before CTGP starts, so it does not instrument or perturb Mario Kart. */
extern Result svcCustomBackdoorRead4(u32 out[4], const void *src);
extern u32 svcConvertVAToPA(const void *addr, u32 writeCheck);

static u8 g_loaderDiagKernelChunk[0x10000] __attribute__((aligned(0x1000)));

static Result LoaderDiag_DumpKernelRange(const char *path, u32 base, u32 size)
{
    IFile file = {0};
    Result res = LoaderDiag_OpenDumpFile(&file, path);
    if(R_FAILED(res))
        return res;

    u32 done = 0;
    while(done < size)
    {
        u32 chunk = size - done;
        if(chunk > sizeof(g_loaderDiagKernelChunk)) chunk = sizeof(g_loaderDiagKernelChunk);

        /* K11 high VAs are not directly readable from a CustomBackdoor callback
           entered from Loader. Translate each kernel page to its PA with Luma's
           svc 0x90, then read through K11's uncached direct physical alias.
           The callback returns four words in r0-r3; only after returning to user
           mode do we store those registers into Loader's buffer. */
        for(u32 pageOff = 0; pageOff < chunk; pageOff += 0x1000u)
        {
            u32 va = base + done + pageOff;
            u32 pa = svcConvertVAToPA((const void *)va, 0);
            if(pa == 0)
            {
                res = (Result)0xD900182Fu;
                break;
            }

            u32 pageSize = chunk - pageOff;
            if(pageSize > 0x1000u) pageSize = 0x1000u;
            u32 physAlias = pa | 0x80000000u;

            for(u32 off = 0; off < pageSize; off += 16u)
            {
                res = svcCustomBackdoorRead4((u32 *)(g_loaderDiagKernelChunk + pageOff + off),
                                             (const void *)(physAlias + off));
                if(R_FAILED(res))
                    break;
            }
            if(R_FAILED(res))
                break;
        }
        if(R_FAILED(res))
            break;

        u64 written = 0;
        res = IFile_Write(&file, &written, g_loaderDiagKernelChunk, chunk,
                          done + chunk == size ? FS_WRITE_FLUSH : 0);
        if(R_FAILED(res) || written != chunk)
        {
            if(R_SUCCEEDED(res)) res = (Result)0xD9004587;
            break;
        }
        done += chunk;
    }

    IFile_Close(&file);
    return res;
}

Result LoaderDiag_DumpK11Snapshot(void)
{
    Result r1 = LoaderDiag_DumpKernelRange("/luma/dumps/memory/k11_kernel_fff00000.bin", 0xFFF00000u, 0x80000u);
    Result r2 = LoaderDiag_DumpKernelRange("/luma/dumps/memory/k11_ext_70000000.bin", 0x70000000u, 0x10000u);
    return R_FAILED(r1) ? r1 : r2;
}
