#include <3ds.h>
#include <assert.h>
#include "memory.h"
#include "patcher.h"
#include "ifile.h"
#include "util.h"
#include "loader.h"
#include "service_manager.h"
#include "3dsx.h"
#include "hbldr.h"

u32 config, multiConfig, bootConfig;
bool isN3DS, isSdMode, nextGamePatchDisabled, isLumaWithKext;

#define ARM9INFO_MAGIC 0x49394D41u /* AM9I */

typedef struct
{
    u32 type;
    u32 param;
    s32 result;
    u32 reserved;
    s64 value;
} Arm9InfoQuery;

typedef struct
{
    u32 magic;
    u32 version;
    u32 loaderConfig;
    u32 loaderMultiConfig;
    u32 loaderBootConfig;
    u32 loaderIsN3DS;
    u32 loaderIsSdMode;
    u32 stockNewCpuView;
    u32 nexusNewCpuView;
    u32 count;
    Arm9InfoQuery q[48];
} Arm9InfoDump;

static Arm9InfoDump g_arm9Info;

static void arm9InfoAdd(u32 type, u32 param)
{
    if(g_arm9Info.count >= 48) return;
    Arm9InfoQuery *q = &g_arm9Info.q[g_arm9Info.count++];
    s64 out = (s64)0x1122334455667788LL;
    Result res = svcGetSystemInfo(&out, (s32)type, (s32)param);
    q->type = type;
    q->param = param;
    q->result = res;
    q->reserved = 0;
    q->value = out;
}

static void arm9InfoCapture(void)
{
    g_arm9Info.magic = ARM9INFO_MAGIC;
    g_arm9Info.version = 1;
    g_arm9Info.loaderConfig = config;
    g_arm9Info.loaderMultiConfig = multiConfig;
    g_arm9Info.loaderBootConfig = bootConfig;
    g_arm9Info.loaderIsN3DS = isN3DS;
    g_arm9Info.loaderIsSdMode = isSdMode;
    g_arm9Info.stockNewCpuView = (multiConfig >> (2 * 4)) & 3;
    g_arm9Info.nexusNewCpuView = (multiConfig >> (2 * 5)) & 3;
    g_arm9Info.count = 0;

    static const u32 p10000[] = {
        0,1,2,3,4,5,6,7,0x10,0x11,0x80,0x100,0x101,0x102,0x103,0x104,0x105,0x106,0x107,
        0x108,0x109,0x10A,0x10B,0x10C,0x10D,0x10E,0x180,0x181,0x182,0x183,0x184,0x185,0x186,0x187,
        0x200,0x201,0x202,0x203,0x300,0x301
    };
    for(u32 i = 0; i < sizeof(p10000)/sizeof(p10000[0]); i++) arm9InfoAdd(0x10000, p10000[i]);
    arm9InfoAdd(0x10001, 0);
    arm9InfoAdd(0x10001, 1);
    arm9InfoAdd(0x10001, 2);
    arm9InfoAdd(0x10002, 0);
    arm9InfoAdd(26, 0);
    arm9InfoAdd(0x20000, 0);
}

extern Result LoaderDiag_WriteDump(const char *path, const void *data, u32 size);
extern Result LoaderDiag_DumpK11Snapshot(void);

// MAKE SURE fsreg has been init before calling this
static Result fsldrPatchPermissions(void)
{
    u32 pid;
    Result res = 0;
    FS_ProgramInfo info;
    ExHeader_Arm11StorageInfo storageInfo = {
        .fs_access_info = FSACCESS_NANDRW | FSACCESS_NANDRO_RO | FSACCESS_SDMC_RW,
    };

    info.programId = 0x0004013000001302LL; // loader PID
    info.mediaType = MEDIATYPE_NAND;
    TRY(svcGetProcessId(&pid, CUR_PROCESS_HANDLE));
    return FSREG_Register(pid, 0xFFFF000000000000LL, &info, &storageInfo);
}

static inline void loadCFWInfo(void)
{
    s64 out;
    u64 hbldrTid = 0;

    isLumaWithKext = svcGetSystemInfo(&out, 0x20000, 0) == 1;
    if (isLumaWithKext)
    {
        svcGetSystemInfo(&out, 0x10000, 3);
        config = (u32)out;
        svcGetSystemInfo(&out, 0x10000, 4);
        multiConfig = (u32)out;
        svcGetSystemInfo(&out, 0x10000, 5);
        bootConfig = (u32)out;

        svcGetSystemInfo(&out, 0x10000, 0x100);
        hbldrTid = (u64)out;
        svcGetSystemInfo(&out, 0x10000, 0x201);
        isN3DS = (bool)out;
        svcGetSystemInfo(&out, 0x10000, 0x203);
        isSdMode = (bool)out;
    }
    else
    {
        // Try to support non-Luma or builds where kext is disabled
        s64 numKips = 0;
        svcGetSystemInfo(&numKips, 26, 0);

        if (numKips >= 6)
            panic(0xDEADCAFE);

#ifndef BUILD_FOR_EXPLOIT_DEV
        // Most options 0, except select ones
        config = BIT(PATCHVERSTRING) | BIT(PATCHGAMES) | BIT(LOADEXTFIRMSANDMODULES);
#else
        config = 0;
#endif
        multiConfig = 0;
        bootConfig = 0;
        isN3DS = OS_KernelConfig->app_memtype >= 6;
        isSdMode = true;
    }

    hbldrTid = hbldrTid == 0 ? HBLDR_DEFAULT_3DSX_TID : hbldrTid;
    Luma_SharedConfig->hbldr_3dsx_tid = hbldrTid;
    Luma_SharedConfig->selected_hbldr_3dsx_tid = hbldrTid;
    Luma_SharedConfig->use_hbldr = true;
}

void __ctru_exit(int rc) { (void)rc; } // needed to avoid linking error

// this is called after main exits
void __wrap_exit(int rc)
{
    (void)rc;
    // Not supposed to terminate... kernel will clean up the handles if it does happen anyway
    svcExitProcess();
}

void __sync_init();
void __libc_init_array(void);

// called before main
void initSystem(void)
{
    __sync_init();
    loadCFWInfo();
    arm9InfoCapture();

    Result res;
    for(res = 0xD88007FA; res == (Result)0xD88007FA; svcSleepThread(500 * 1000LL))
    {
        res = srvInit();
        if(R_FAILED(res) && res != (Result)0xD88007FA)
            panic(res);
    }

    assertSuccess(fsRegInit());
    assertSuccess(fsldrPatchPermissions());

    //fsldrInit();
    assertSuccess(srvGetServiceHandle(fsGetSessionHandle(), "fs:LDR"));

    // Hackjob
    assertSuccess(FSUSER_InitializeWithSdkVersion(*fsGetSessionHandle(), 0x70200C8));
    assertSuccess(FSUSER_SetPriority(0));

    /* One-shot ARM9/K11 handoff probe. Failure is intentionally non-fatal. */
    LoaderDiag_WriteDump("/luma/dumps/memory/arm9_cfwinfo.bin", &g_arm9Info, sizeof(g_arm9Info));
    LoaderDiag_DumpK11Snapshot();

    assertSuccess(pxiPmInit());

    //__libc_init_array();
}

extern void LoaderDiag_RequestShutdown(void);

static void loaderDiagHandlePreTerm(u32 notificationId)
{
    (void)notificationId;
    LoaderDiag_RequestShutdown();
}

static const ServiceManagerServiceEntry services[] = {
    { "Loader", 2, loaderHandleCommands, false },
    { "hb:ldr", 2, hbldrHandleCommands,  true  },
    { NULL },
};

static const ServiceManagerNotificationEntry notifications[] = {
    /* PM pre-termination: close the diagnostic input service/memblock handles
       before the normal Loader termination notification arrives. */
    { 0x2000, loaderDiagHandlePreTerm },
    { 0x100,  loaderDiagHandlePreTerm },
    { 0x000, NULL },
};

static u8 CTR_ALIGN(4) staticBufferForHbldr[0x400];
static_assert(ARGVBUF_SIZE > 2 * PATH_MAX, "Wrong 3DSX argv buffer size");

int main(void)
{
    nextGamePatchDisabled = false;

    // Loader doesn't use any input static buffer, so we should be fine
    u32 *sbuf = getThreadStaticBuffers();
    sbuf[0] = IPC_Desc_StaticBuffer(sizeof(staticBufferForHbldr), 0);
    sbuf[1] = (u32)staticBufferForHbldr;
    sbuf[2] = IPC_Desc_StaticBuffer(sizeof(staticBufferForHbldr), 1);
    sbuf[3] = (u32)staticBufferForHbldr;

    assertSuccess(ServiceManager_Run(services, notifications, NULL));
    return 0;
}