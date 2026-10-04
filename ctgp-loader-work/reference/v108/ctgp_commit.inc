// Included after the host table and before the frozen feature functions.
// No kernel identity writes or per-query host callbacks. The mapped page is
// immutable and intentionally remains allocated for the plugin's lifetime.
extern const u32 g_ctgpCommitBlob[23];
typedef struct {
    u32 state, site, branch, remote, source, pid, matches, original;
    u32 querySite, queryBranch, queryMatches;
} CtgpCommitState;
PLUGIN_BSS_LATE(ctgp) static CtgpCommitState g_ctgpCommit;
PLUGIN_BSS_LATE(ctgp) static u32 g_ctgpCommitReported;

PLUGIN_CODE_LATE(ctgp) static bool PLUGIN_ctgp_CommitTail(const volatile u32 *p)
{
    return p[1] == 0xEF00002Au &&
           (p[2] == 0xE8BD0008u || p[2] == 0xE49D3004u) &&
           p[3] == 0xE5831000u && p[4] == 0xE5832004u &&
           p[5] == 0xE12FFF1Eu;
}

PLUGIN_CODE_LATE(ctgp) static bool PLUGIN_ctgp_CommitStart(u32 x)
{
    return x == 0xE52D0004u || x == 0xE92D0001u;
}

// Recognize the complete ABI, including a branch previously installed by this
// session. A restored original wrapper is scanned again and re-patched.
PLUGIN_CODE_LATE(ctgp) static u32 PLUGIN_ctgp_FindCommitWrapper(
    const volatile u32 *image, u32 base, u32 size, u32 ownSite, u32 ownBranch,
    u32 *matches)
{
    *matches = 0;
    if (!image || (base & 3u) || (size & 3u) || size < 24u ||
        size > 0x02000000u || base + size < base) return 0;
    u32 found = 0;
    // Probe the known current location first, but still prove uniqueness over
    // the entire declared executable. Heap/backing bytes are never scanned.
    const u32 known = 0x070A8288u;
    if (known >= base && known - base <= size - 24u) {
        const volatile u32 *p = image + (known - base) / 4u;
        if (PLUGIN_ctgp_CommitTail(p) &&
            (PLUGIN_ctgp_CommitStart(p[0]) ||
             (known == ownSite && ownBranch && p[0] == ownBranch))) {
            found = known;
            *matches = 1;
        }
    }
    for (u32 off = 0; off <= size - 24u; off += 4u) {
        u32 address = base + off;
        if (address == known) continue;
        const volatile u32 *p = image + off / 4u;
        if (PLUGIN_ctgp_CommitTail(p) &&
            (PLUGIN_ctgp_CommitStart(p[0]) ||
             (address == ownSite && ownBranch && p[0] == ownBranch))) {
            found = address;
            if (++*matches > 1u) return 0;
        }
    }
    return *matches == 1u ? found : 0;
}

PLUGIN_CODE_LATE(ctgp) static u32 PLUGIN_ctgp_ArmImmediate(u32 insn)
{
    u32 x = insn & 0xFFu, shift = ((insn >> 8) & 15u) * 2u;
    return shift ? (x >> shift) | (x << (32u-shift)) : x;
}

// CTGP's actual hash check constructs a second SVC wrapper on its stack.
// Match its exact query setup and consumer, allowing changed stack offsets.
// A BL at the same site may be recognized only when owned by this session.
PLUGIN_CODE_LATE(ctgp) static u32 PLUGIN_ctgp_FindStackCommitQuery(
    const volatile u32 *image, u32 base, u32 size, u32 ownSite, u32 ownBranch,
    u32 *matches)
{
    *matches = 0;
    if (!image || (base & 3u) || (size & 3u) || size < 36u ||
        size > 0x02000000u || base+size < base) return 0;
    u32 found = 0;
    for (u32 off=0; off<=size-36u; off+=4u) {
        const volatile u32 *p = image+off/4u;
        u32 site = base+off+16u;
        if ((p[0]&0xFFFFF000u)!=0xE3A02000u ||
            PLUGIN_ctgp_ArmImmediate(p[0])!=1u ||
            (p[1]&0xFFFFF000u)!=0xE3A01000u ||
            PLUGIN_ctgp_ArmImmediate(p[1])!=0x10000u ||
            (p[2]&0xFFFFF000u)!=0xE28D0000u ||
            (p[3]&0xFFFFF000u)!=0xE28D3000u ||
            (p[4]!=0xE12FFF33u &&
             !(site==ownSite && ownBranch && p[4]==ownBranch)) ||
            (p[5]&0xFFFFF000u)!=0xE59D1000u ||
            (p[6]&0xFFFFF000u)!=0xE28D3000u ||
            p[7]!=0xE1A00001u || p[8]!=0xE12FFF33u) continue;
        u32 output = PLUGIN_ctgp_ArmImmediate(p[2]);
        u32 stub = PLUGIN_ctgp_ArmImmediate(p[3]);
        u32 compare = PLUGIN_ctgp_ArmImmediate(p[6]);
        if ((output|stub|compare)&3u || output!=(p[5]&0xFFFu) ||
            compare!=stub+24u || output<compare+24u) continue;
        found=site;
        if (++*matches>1u) return 0;
    }
    return *matches==1u ? found : 0;
}

PLUGIN_CODE_LATE(ctgp) static bool PLUGIN_ctgp_CommitFail(u32 state)
{
    g_ctgpCommit.state = state;
    return false;
}

PLUGIN_CODE_LATE(ctgp) static void PLUGIN_ctgp_ResetCommitMount(void)
{
    // Keep the immutable source page. Never dismantle an old process's hook
    // during VANILLA feature shutdown. Each new mount gets its own mapping.
    g_ctgpCommit.state = g_ctgpCommit.site = g_ctgpCommit.branch = 0;
    g_ctgpCommit.remote = g_ctgpCommit.pid = g_ctgpCommit.matches = 0;
    g_ctgpCommit.original = g_ctgpCommitReported = 0;
    g_ctgpCommit.querySite = g_ctgpCommit.queryBranch = g_ctgpCommit.queryMatches = 0;
}

PLUGIN_CODE_LATE(ctgp) static bool PLUGIN_ctgp_InstallCommitCompat(void)
{
    const u32 base = 0x07000100u;
    PluginLoaderContext *ctx = CTGP_HOST__PluginLoaderCtx;
    if (!isPlgCTGP7 || !ctx || !ctx->target || !CTGP_MENU__TempAlloc ||
        !CTGP_MENU__MapPage || !CTGP_MENU__UnmapPage || !CTGP_HOST__svcGetProcessId)
        return PLUGIN_ctgp_CommitFail(0xE1u);
    u32 exeSize = ctx->header.exeSize;
    if (exeSize < 0x1000u || (exeSize & 0xFFFu) ||
        exeSize > CTGP_HOST__g_memBlockSize || exeSize > 0x01F00000u)
        return PLUGIN_ctgp_CommitFail(0xE1u);
    MemInfo mi; PageInfo pi;
    if (R_FAILED(CTGP_HOST__svcQueryMemory(&mi, &pi, base)) ||
        mi.state == MEMSTATE_FREE || !(mi.perm & MEMPERM_READ) ||
        mi.base_addr > base || mi.size < base - mi.base_addr ||
        mi.size - (base - mi.base_addr) < exeSize - 0x100u)
        return PLUGIN_ctgp_CommitFail(0xE2u);
    if (!PLUGIN_ctgp_RuntimeHeaderValid((const u32*)base))
        return PLUGIN_ctgp_CommitFail(0xE3u);
    u32 pid;
    if (R_FAILED(CTGP_HOST__svcGetProcessId(&pid, ctx->target)) || !pid)
        return PLUGIN_ctgp_CommitFail(0xE5u);
    if (g_ctgpCommit.pid && g_ctgpCommit.pid != pid) {
        PLUGIN_ctgp_ResetCommitMount();
    }
    g_ctgpCommit.pid = pid;
    u32 site = PLUGIN_ctgp_FindCommitWrapper((const volatile u32*)base,
        base, exeSize - 0x100u, g_ctgpCommit.site, g_ctgpCommit.branch,
        &g_ctgpCommit.matches);
    if (!site) return PLUGIN_ctgp_CommitFail(g_ctgpCommit.matches > 1u ? 0xE4u : 0xE3u);
    // MapPage supplies exactly one page with unmapped neighbors. Never read
    // the six-word wrapper validation across its trailing guard.
    if ((site & 0xFFFu) > 0xFE8u) return PLUGIN_ctgp_CommitFail(0xE9u);
    u32 querySite = PLUGIN_ctgp_FindStackCommitQuery((const volatile u32*)base,
        base, exeSize-0x100u, g_ctgpCommit.querySite, g_ctgpCommit.queryBranch,
        &g_ctgpCommit.queryMatches);
    if (!querySite) return PLUGIN_ctgp_CommitFail(0xEBu);
    u32 queryBranch;
    if (!PLUGIN_ctgp_EncodeArmBranch(querySite,site,true,&queryBranch))
        return PLUGIN_ctgp_CommitFail(0xEBu);

    if (!g_ctgpCommit.source) {
        u32 source = 0;
        if (!CTGP_MENU__TempAlloc(0x1000u, &source) || !source || (source & 0xFFFu))
            return PLUGIN_ctgp_CommitFail(0xE6u);
        volatile u32 *p = (volatile u32*)source;
        for (u32 i = 0; i < 23u; i++) p[i] = g_ctgpCommitBlob[i];
        for (u32 i = 0; i < 23u; i++)
            if (p[i] != g_ctgpCommitBlob[i]) return PLUGIN_ctgp_CommitFail(0xE8u);
        g_ctgpCommit.source = source;
        CTGP_HOST__svcFlushEntireDataCache();
    }
    if (!g_ctgpCommit.remote) {
        // One free page within ARM B range. Never borrow CTGP BSS, zero-filled
        // heap, or unpacker's scratch space. MapProcessMemoryEx rejects occupied
        // destinations and supplies the same executable alias as the 3GX map.
        for (u32 address = 0x08E00000u; address >= 0x07110000u; address -= 0x10000u) {
            u32 branch;
            if (!PLUGIN_ctgp_EncodeArmBranch(site, address, false, &branch)) continue;
            if (R_SUCCEEDED(CTGP_HOST__svcMapProcessMemoryEx(ctx->target, address,
                CUR_PROCESS_HANDLE, g_ctgpCommit.source, 0x1000u, (MapExFlags)0))) {
                g_ctgpCommit.remote = address;
                break;
            }
        }
        if (!g_ctgpCommit.remote) return PLUGIN_ctgp_CommitFail(0xE7u);
    }
    u32 branch;
    if (!PLUGIN_ctgp_EncodeArmBranch(site, g_ctgpCommit.remote, false, &branch))
        return PLUGIN_ctgp_CommitFail(0xE7u);
    // Read the actual target alias through MENU's guarded mapping before
    // publishing the branch. This also catches lost/swap-removed mappings.
    u32 alias = 0, mapped = 0;
    if (!CTGP_MENU__MapPage(ctx->target, g_ctgpCommit.remote, &alias, &mapped))
        return PLUGIN_ctgp_CommitFail(0xE8u);
    bool bodyOk = true;
    const volatile u32 *body = (const volatile u32*)mapped;
    for (u32 i = 0; i < 23u; i++)
        if (body[i] != g_ctgpCommitBlob[i]) bodyOk = false;
    CTGP_MENU__UnmapPage(alias);
    if (!bodyOk) return PLUGIN_ctgp_CommitFail(0xE8u);
    // Scan reads the existing Rosalina backing; the guarded write alias is the
    // game mapping. Verify they agree and change one instruction atomically.
    if (!CTGP_MENU__MapPage(ctx->target, site, &alias, &mapped))
        return PLUGIN_ctgp_CommitFail(0xE9u);
    volatile u32 *p = (volatile u32*)mapped;
    u32 original = p[0];
    bool oldHook = site == g_ctgpCommit.site && original == g_ctgpCommit.branch &&
                   original == branch;
    bool ok = PLUGIN_ctgp_CommitTail(p) &&
              (PLUGIN_ctgp_CommitStart(original) || oldHook) &&
              original == *(const volatile u32*)site;
    u32 queryAlias=0, queryMapped=0;
    if (!ok || !CTGP_MENU__MapPage(ctx->target,querySite,&queryAlias,&queryMapped)) {
        CTGP_MENU__UnmapPage(alias);
        return PLUGIN_ctgp_CommitFail(0xECu);
    }
    volatile u32 *q = (volatile u32*)queryMapped;
    u32 queryOriginal = q[0];
    bool oldQuery = querySite==g_ctgpCommit.querySite &&
                    queryOriginal==g_ctgpCommit.queryBranch && queryOriginal==queryBranch;
    ok = (queryOriginal==0xE12FFF33u || oldQuery) &&
         queryOriginal==*(const volatile u32*)querySite;
    if (ok) {
        if (!oldHook) {
            g_ctgpCommit.original = original;
            p[0] = branch;
        }
        if (!oldQuery) q[0]=queryBranch;
        CTGP_HOST__svcFlushEntireDataCache();
        CTGP_HOST__svcInvalidateEntireInstructionCache();
        ok = p[0] == branch && *(const volatile u32*)site == branch &&
             PLUGIN_ctgp_CommitTail(p) && q[0]==queryBranch &&
             *(const volatile u32*)querySite==queryBranch;
        if (!ok) {
            if (!oldHook) p[0] = original;
            if (!oldQuery) q[0] = queryOriginal;
            CTGP_HOST__svcFlushEntireDataCache();
            CTGP_HOST__svcInvalidateEntireInstructionCache();
        }
    }
    CTGP_MENU__UnmapPage(queryAlias);
    CTGP_MENU__UnmapPage(alias);
    if (!ok) return PLUGIN_ctgp_CommitFail(0xE9u);
    g_ctgpCommit.site = site;
    g_ctgpCommit.branch = branch;
    g_ctgpCommit.querySite=querySite;
    g_ctgpCommit.queryBranch=queryBranch;
    g_ctgpCommit.state = 2u;
    return true;
}

PLUGIN_CODE_LATE(ctgp) static void PLUGIN_ctgp_ReportCommitStatus(void)
{
    // One transition breadcrumb, never any logging in the query trampoline.
    if (g_ctgpCommitReported == g_ctgpCommit.state) return;
    g_ctgpCommitReported = g_ctgpCommit.state;
    PLUGIN_ctgp_SaveStartupStatus(g_ctgpCommit.state == 2u ? 5u : 6u);
}
