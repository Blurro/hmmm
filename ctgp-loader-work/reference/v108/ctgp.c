#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include "draw.h"
#include "menu.h"
#include "menus.h"
#include "process_patches.h"
#include "plugin/plgloader.h"
#include "menus/plugin_options.h"
#include "sysplugin_menu.h"

#ifdef __INTELLISENSE__
#define __attribute__(x)
#endif

#define PLUGIN_CODE_FROZEN(id) __attribute__((section(".plugin_" #id), used))
#define PLUGIN_CODE(id)   __attribute__((section(".plugin_" #id ".ext"), used))
#define PLUGIN_CODE_LATE(id) __attribute__((section(".plugin_" #id ".azahar"), used))
#define PLUGIN_MAIN(id)   __attribute__((section(".plugin_" #id "_entry"), used))
#define PLUGIN_RODATA(id) __attribute__((section(".pluginrodata_" #id), used))
#define PLUGIN_DATA(id)   __attribute__((section(".plugindata_" #id), used))
#define PLUGIN_BSS(id)    __attribute__((section(".pluginbss_" #id), used))
#define PLUGIN_BSS_LATE(id) __attribute__((section(".pluginbss_" #id ".v120"), used))

typedef void (*BlurTickFunc)(u64 delta);
typedef struct BlurFeatureRegistration {
    u32 pluginId; const char *title; void (*callback)(void); struct BlurFeatureRegistration *next;
} BlurFeatureRegistration;
typedef struct {
    u32 version;
    u32 patchMode;
} CtgpSettings;

extern bool PLUGIN_blur_AddTickFunc(BlurTickFunc func, s64 intervalNs);
extern bool PLUGIN_blur_RemoveTickFunc(BlurTickFunc func);
extern bool PLUGIN_blur_AddFeatureItem(BlurFeatureRegistration *item, u32 pluginId, const char *title, void (*callback)(void));
extern void PLUGIN_blur_DrawFeatureFrame(const char *title);
extern bool PLUGIN_MENU_MapPage(Handle sourceProcess, u32 sourceAddress, u32 *mappedBase, u32 *mappedAddress);
extern void PLUGIN_MENU_UnmapPage(u32 mappedBase);
NEXUS_PLUGIN_EXTERNAL_FUNC(PLUGIN_blur_AddTickFunc);
NEXUS_PLUGIN_EXTERNAL_FUNC(PLUGIN_blur_RemoveTickFunc);
NEXUS_PLUGIN_EXTERNAL_FUNC(PLUGIN_blur_AddFeatureItem);
NEXUS_PLUGIN_EXTERNAL_FUNC(PLUGIN_blur_DrawFeatureFrame);
NEXUS_PLUGIN_EXTERNAL_FUNC(PLUGIN_MENU_MapPage);
NEXUS_PLUGIN_EXTERNAL_FUNC(PLUGIN_MENU_UnmapPage);
extern u32 g_memBlockSize;
extern bool isN3DS;
extern PluginLoaderContext PluginLoaderCtx;
extern u64 g_titleId;
extern u32 ctgp_mount_plugin_dispatch;
extern u32 ctgp_post_game_flush;
extern u32 ctgp_mount_exe_map;
extern void ctgpAzaharHeaderHook(void);
extern void ctgpAzaharEncryptHook(void);
extern void ctgpAzaharDecryptHook(void);
extern void ctgpAzaharMiiWrap(void);
extern void ctgpAzaharMiiUnwrap(void);
extern void PLUGIN_ctgp_LoadSettingsAzahar(void);
extern void PLUGIN_ctgp_ToggleOptionsAzahar(void);
extern void PLUGIN_ctgp_OnTickAzahar(u64 delta);
extern u32 g_ctgpAzaharState_raw[14] __asm__("g_ctgpAzaharState");

PLUGIN_DATA(ctgp) void *pluginTable_ctgp[] = {
    (void*)PLUGIN_blur_AddTickFunc,
    (void*)PLUGIN_blur_AddFeatureItem,
    (void*)PLUGIN_blur_DrawFeatureFrame,
    (void*)PLUGIN_MENU_MapPage,
    (void*)PLUGIN_MENU_UnmapPage,
    (void*)PLUGIN_MENU_AddItem,
    (void*)OperateOnProcessByName,
    (void*)FSUSER_OpenArchive,
    (void*)FSUSER_CloseArchive,
    (void*)FSUSER_OpenFile,
    (void*)FSUSER_CreateDirectory,
    (void*)FSUSER_RenameFile,
    (void*)FSFILE_Read,
    (void*)FSFILE_Write,
    (void*)FSFILE_Close,
    (void*)FSFILE_GetSize,
    (void*)fsMakePath,
    (void*)svcConvertVAToPA,
    (void*)svcFlushEntireDataCache,
    (void*)svcInvalidateEntireInstructionCache,
    (void*)svcCloseHandle,
    (void*)svcGetProcessList,
    (void*)svcOpenProcess,
    (void*)svcWaitSynchronization,
    (void*)svcGetProcessInfo,
    (void*)svcGetSystemTick,
    (void*)svcQueryMemory,
    (void*)Draw_Lock,
    (void*)Draw_Unlock,
    (void*)Draw_ClearFramebuffer,
    (void*)Draw_DrawString,
    (void*)Draw_DrawFormattedString,
    (void*)Draw_FlushFramebuffer,
    (void*)waitInputWithTimeout,
    (void*)&menuShouldExit,
    (void*)menuEnter,
    (void*)PluginLoaderOptions__UpdateMenu,
    (void*)PluginWatcher__UpdateMenu,
    (void*)PluginConverter__UpdateMenu,
    (void*)menuShow,
    (void*)&rosalinaMenu,
    (void*)menuLeave,
    (void*)memcpy,
    (void*)memcmp,
    (void*)strcmp,
    (void*)strcpy,
    (void*)strcat,
    (void*)strlen,
    (void*)sprintf,
    (void*)&g_memBlockSize,
    (void*)&isN3DS,
    (void*)&PluginLoaderCtx,
    (void*)&g_titleId,
    (void*)svcMapProcessMemoryEx,
    (void*)&ctgp_mount_plugin_dispatch,
    (void*)&ctgp_post_game_flush,
    (void*)&ctgp_mount_exe_map,
    (void*)PLUGIN_MENU_SaveData,
    (void*)PLUGIN_MENU_LoadData,
    (void*)PLUGIN_MENU_OpenOnlineSource,
    (void*)FSUSER_DeleteFile,
    (void*)PLUGIN_MENU_TempAlloc,
    (void*)PLUGIN_MENU_TempFree,
    (void*)svcSendSyncRequest,
    (void*)PLUGIN_blur_RemoveTickFunc,
    (void*)PLUGIN_MENU_RegisterBridgeReceiver,
    (void*)PLUGIN_MENU_UnregisterBridgeReceiver,
    (void*)svcGetProcessId,
};

#define CTGP_BLUR__AddTickFunc ((bool(*)(BlurTickFunc,s64))pluginTable_ctgp[0])
#define CTGP_BLUR__AddFeatureItem ((bool(*)(BlurFeatureRegistration*,u32,const char*,void(*)(void)))pluginTable_ctgp[1])
#define CTGP_BLUR__DrawFeatureFrame ((void(*)(const char*))pluginTable_ctgp[2])
#define CTGP_MENU__MapPage ((bool(*)(Handle,u32,u32*,u32*))pluginTable_ctgp[3])
#define CTGP_MENU__UnmapPage ((void(*)(u32))pluginTable_ctgp[4])
#define CTGP_MENU__AddItem ((bool(*)(PluginMenuRegistration*,u32,const char*,void(*)(void),u32))pluginTable_ctgp[5])
#define CTGP_HOST__OperateOnProcessByName ((Result(*)(const char*,OperateOnProcessCb))pluginTable_ctgp[6])
#define CTGP_HOST__FSUSER_OpenArchive ((Result(*)(FS_Archive*,FS_ArchiveID,FS_Path))pluginTable_ctgp[7])
#define CTGP_HOST__FSUSER_CloseArchive ((Result(*)(FS_Archive))pluginTable_ctgp[8])
#define CTGP_HOST__FSUSER_OpenFile ((Result(*)(Handle*,FS_Archive,FS_Path,u32,u32))pluginTable_ctgp[9])
#define CTGP_HOST__FSUSER_CreateDirectory ((Result(*)(FS_Archive,FS_Path,u32))pluginTable_ctgp[10])
#define CTGP_HOST__FSUSER_RenameFile ((Result(*)(FS_Archive,FS_Path,FS_Archive,FS_Path))pluginTable_ctgp[11])
#define CTGP_HOST__FSFILE_Read ((Result(*)(Handle,u32*,u64,void*,u32))pluginTable_ctgp[12])
#define CTGP_HOST__FSFILE_Write ((Result(*)(Handle,u32*,u64,const void*,u32,u32))pluginTable_ctgp[13])
#define CTGP_HOST__FSFILE_Close ((Result(*)(Handle))pluginTable_ctgp[14])
#define CTGP_HOST__FSFILE_GetSize ((Result(*)(Handle,u64*))pluginTable_ctgp[15])
#define CTGP_HOST__fsMakePath ((FS_Path(*)(FS_PathType,const void*))pluginTable_ctgp[16])
#define CTGP_HOST__svcConvertVAToPA ((u32(*)(const void*,bool))pluginTable_ctgp[17])
#define CTGP_HOST__svcFlushEntireDataCache ((void(*)(void))pluginTable_ctgp[18])
#define CTGP_HOST__svcInvalidateEntireInstructionCache ((void(*)(void))pluginTable_ctgp[19])
#define CTGP_HOST__svcCloseHandle ((Result(*)(Handle))pluginTable_ctgp[20])
#define CTGP_HOST__svcGetProcessList ((Result(*)(s32*,u32*,s32))pluginTable_ctgp[21])
#define CTGP_HOST__svcOpenProcess ((Result(*)(Handle*,u32))pluginTable_ctgp[22])
#define CTGP_HOST__svcWaitSynchronization ((Result(*)(Handle,s64))pluginTable_ctgp[23])
#define CTGP_HOST__svcGetProcessInfo ((Result(*)(s64*,Handle,u32))pluginTable_ctgp[24])
#define CTGP_HOST__svcGetSystemTick ((u64(*)(void))pluginTable_ctgp[25])
#define CTGP_HOST__svcQueryMemory ((Result(*)(MemInfo*,PageInfo*,u32))pluginTable_ctgp[26])
#define CTGP_HOST__Draw_Lock ((void(*)(void))pluginTable_ctgp[27])
#define CTGP_HOST__Draw_Unlock ((void(*)(void))pluginTable_ctgp[28])
#define CTGP_HOST__Draw_ClearFramebuffer ((void(*)(void))pluginTable_ctgp[29])
#define CTGP_HOST__Draw_DrawString ((u32(*)(u32,u32,u32,const char*))pluginTable_ctgp[30])
#define CTGP_HOST__Draw_DrawFormattedString ((u32(*)(u32,u32,u32,const char*,...))pluginTable_ctgp[31])
#define CTGP_HOST__Draw_FlushFramebuffer ((void(*)(void))pluginTable_ctgp[32])
#define CTGP_HOST__waitInputWithTimeout ((u32(*)(s32))pluginTable_ctgp[33])
#define CTGP_HOST__menuShouldExit (*(bool*)pluginTable_ctgp[34])
#define CTGP_HOST__menuEnter ((void(*)(void))pluginTable_ctgp[35])
#define CTGP_HOST__PluginLoaderOptions__UpdateMenu ((void(*)(void))pluginTable_ctgp[36])
#define CTGP_HOST__PluginWatcher__UpdateMenu ((void(*)(void))pluginTable_ctgp[37])
#define CTGP_HOST__PluginConverter__UpdateMenu ((void(*)(void))pluginTable_ctgp[38])
#define CTGP_HOST__menuShow ((void(*)(Menu*))pluginTable_ctgp[39])
#define CTGP_HOST__rosalinaMenu ((Menu*)pluginTable_ctgp[40])
#define CTGP_HOST__menuLeave ((void(*)(void))pluginTable_ctgp[41])
#define CTGP_HOST__memcpy ((void*(*)(void*,const void*,size_t))pluginTable_ctgp[42])
#define CTGP_HOST__memcmp ((int(*)(const void*,const void*,size_t))pluginTable_ctgp[43])
#define CTGP_HOST__strcmp ((int(*)(const char*,const char*))pluginTable_ctgp[44])
#define CTGP_HOST__strcpy ((char*(*)(char*,const char*))pluginTable_ctgp[45])
#define CTGP_HOST__strcat ((char*(*)(char*,const char*))pluginTable_ctgp[46])
#define CTGP_HOST__strlen ((size_t(*)(const char*))pluginTable_ctgp[47])
#define CTGP_HOST__sprintf ((int(*)(char*,const char*,...))pluginTable_ctgp[48])
#define CTGP_HOST__g_memBlockSize (*(u32*)pluginTable_ctgp[49])
#define CTGP_HOST__isN3DS (*(bool*)pluginTable_ctgp[50])
#define CTGP_HOST__PluginLoaderCtx ((PluginLoaderContext*)pluginTable_ctgp[51])
#define CTGP_HOST__g_titleId (*(u64*)pluginTable_ctgp[52])
#define CTGP_HOST__svcMapProcessMemoryEx ((Result(*)(Handle,u32,Handle,u32,u32,MapExFlags))pluginTable_ctgp[53])
#define CTGP_HOST__marker_mount_plugin_dispatch ((u32)pluginTable_ctgp[54])
#define CTGP_HOST__marker_post_game_flush ((u32)pluginTable_ctgp[55])
#define CTGP_HOST__marker_mount_exe_map ((u32)pluginTable_ctgp[56])
#define CTGP_MENU__SaveData ((bool(*)(u32,const void*,u32))pluginTable_ctgp[57])
#define CTGP_MENU__LoadData ((bool(*)(u32,void*,u32))pluginTable_ctgp[58])
#define CTGP_MENU__OpenOnlineSource ((void(*)(const char*))pluginTable_ctgp[59])
#define CTGP_HOST__FSUSER_DeleteFile ((Result(*)(FS_Archive,FS_Path))pluginTable_ctgp[60])
#define CTGP_MENU__TempAlloc ((bool(*)(u32,u32*))pluginTable_ctgp[61])
#define CTGP_MENU__TempFree ((void(*)(u32,u32))pluginTable_ctgp[62])
#define CTGP_HOST__svcSendSyncRequest ((Result(*)(Handle))pluginTable_ctgp[63])
#define CTGP_BLUR__RemoveTickFunc ((bool(*)(BlurTickFunc))pluginTable_ctgp[64])
#define CTGP_MENU__RegisterBridgeReceiver ((bool(*)(PluginMenuBridgeRegistration*))pluginTable_ctgp[65])
#define CTGP_MENU__UnregisterBridgeReceiver ((bool(*)(PluginMenuBridgeRegistration*))pluginTable_ctgp[66])
#define CTGP_HOST__svcGetProcessId ((Result(*)(u32*,Handle))pluginTable_ctgp[67])

#define CTGP_PLUGIN_ID 0x70677463u
#define CTGP_SETTINGS_VERSION 2u
#define CTGP_AZAHAR_SETTINGS_VERSION 3u
#define CTGP_TICK_ARMED_NS (250LL * 1000LL * 1000LL)
#define CTGP_TICK_FAST_NS (1LL * 1000LL * 1000LL)
#define CTGP_EVENT_PATCH_RESULT 1u
#define TICKS_PER_SEC 268123480ULL
#define TICKS_250MS (TICKS_PER_SEC/4)
#define TICKS_10S (TICKS_PER_SEC*10)
#define TICKS_1FRAME (TICKS_PER_SEC/60)
#define MAX_DELTA_CTGP (TICKS_PER_SEC/30)

PLUGIN_RODATA(ctgp) static const char g_ctgpMimPath[] = "/CTGP-7/gamefs/Driver/mim/mim.bcmdl";
PLUGIN_RODATA(ctgp) static const char g_ctgpMimTempPath[] = "/CTGP-7/gamefs/Driver/mim/mimtemp.bcmdl";
PLUGIN_RODATA(ctgp) static const char g_ctgpMimFmtD[] = "/CTGP-7/gamefs/Driver/mim/mim%d.bcmdl";
PLUGIN_RODATA(ctgp) static const char g_ctgpMimFmtU[] = "/CTGP-7/gamefs/Driver/mim/mim%u.bcmdl";
PLUGIN_RODATA(ctgp) static const char g_ctgpCustomMenuText[] = "CTGP-7 - Blurro's Custom";
PLUGIN_RODATA(ctgp) static const char g_ctgpLangSettingPath[] = "/CTGP-7/resources/langSetting.bin";
PLUGIN_RODATA(ctgp) static const char g_ctgpLanguageBase[] = "/CTGP-7/MyStuff/Languages/Lang_XXX";
PLUGIN_RODATA(ctgp) static const char g_ctgpTextSuffix[] = "/Text.txt";
PLUGIN_RODATA(ctgp) static const char g_ctgpToxtSuffix[] = "/Toxt.txt";
PLUGIN_RODATA(ctgp) static const char g_ctgpHaxxorzText[] = "\\n\\nEnter 'haxxorz' for a cheats-enabled lobby >:D";
PLUGIN_RODATA(ctgp) static const char g_ctgpColonText[] = " ::";
PLUGIN_RODATA(ctgp) static const char g_ctgpEnterLobbyText[] = "enter_lobby_code";
PLUGIN_RODATA(ctgp) static const char g_ctgpLauncherName[8] = {'C','u','s','t','o','m',' ','T'};
PLUGIN_RODATA(ctgp) static const char g_ctgpMkName[8] = {'M','a','r','i','o','K','a','r'};
PLUGIN_RODATA(ctgp) static const char g_ctgp3gxPath[] = "/CTGP-7/resources/CTGP-7.3gx";
PLUGIN_RODATA(ctgp) static const unsigned char g_ctgpFrameworkPattern[20] = {0x43,0x54,0x52,0x50,0x6C,0x75,0x67,0x69,0x6E,0x46,0x72,0x61,0x6D,0x65,0x77,0x6F,0x72,0x6B,0x20,0x42};

PLUGIN_DATA(ctgp) bool g_dumpCTGP7 = false;
PLUGIN_DATA(ctgp) bool g_restoreCTGP7 = false;
PLUGIN_DATA(ctgp) bool g_vanillaCTGP7 = false;
PLUGIN_DATA(ctgp) bool isPlgCTGP7 = false;
PLUGIN_DATA(ctgp) bool g_CtgpMasterLoopActive = false;
PLUGIN_DATA(ctgp) FS_Archive g_ctgpSd = {0};
PLUGIN_DATA(ctgp) u64 elapsed10sMK7 = 0;
PLUGIN_DATA(ctgp) u64 elapsed1frameMK7 = 0;
PLUGIN_DATA(ctgp) u64 elapsed1frameCTGPStartup = 0;
PLUGIN_DATA(ctgp) u64 elapsed20sTimeout = 0;

extern u32 g_myHookVal_raw[2] __asm__("g_myHookVal");
extern u32 g_ctgpHookAddrs_raw[8] __asm__("g_ctgpHookAddrs");
extern u32 g_ctgpRelocScanSize_raw __asm__("g_ctgpRelocScanSize");
extern u32 g_ctgpRelocTrigger_raw[3] __asm__("g_ctgpRelocTrigger");
extern u32 g_ctgpRelocCopies_raw[17] __asm__("g_ctgpRelocCopies");
extern u32 g_privTextInstruction_raw __asm__("g_privTextInstruction");
extern u32 g_ctgpTextStrAddr_raw[2] __asm__("g_ctgpTextStrAddr");

extern void ctgpDumpWaitHook(void);
extern void ctgpDumpArmHook(void);
extern void ctgpRestoreWaitHook(void);
extern bool PLUGIN_ctgp_ArmRelocReturnHook(void);
extern u32 PLUGIN_ctgp_FindStartupEntry(const u32 *image);
extern u32 PLUGIN_ctgp_FindRuntimeEntry(const u32 *image);
extern bool PLUGIN_ctgp_FindRelocReturn(const u32*,u32,u32,u32,u32*,u32*);
extern u32 PLUGIN_ctgp_FindRelocDispatch(const u32*,u32,u32,u32);
extern u32 PLUGIN_ctgp_ValidateRelocatedEntry(u32*,u32,const u32*,u32);
extern u32 PLUGIN_ctgp_RelocRendezvous(u32*,u32,u32*,u32);
extern bool PLUGIN_ctgp_MatchTextEntry(const u32*,u32,u32,u32,u32*,u32*);
PLUGIN_CODE_FROZEN(ctgp) static bool PLUGIN_ctgp_EncodeArmBranch(u32,u32,bool,u32*);
extern void botCountHook(void);
extern void privTextHook(void);
extern u32 g_privTextReturn_raw __asm__("g_privTextReturn");
extern u32 g_privTextStackOffset_raw __asm__("g_privTextStackOffset");
extern u32 g_langWord_raw __asm__("g_langWord");
extern u32 g_privTextDiagHits_raw __asm__("g_privTextDiagHits");
extern u32 g_privTextDiagR0_raw __asm__("g_privTextDiagR0");
extern u32 g_privTextDiagPath_raw __asm__("g_privTextDiagPath");
extern u32 g_privTextDiagLang_raw __asm__("g_privTextDiagLang");

typedef struct { u32 sourcePage; u32 mappedBase; } CtgpAliasPage;
PLUGIN_BSS(ctgp) static CtgpAliasPage g_ctgpAliasPages[2];

PLUGIN_CODE_FROZEN(ctgp) static void *PLUGIN_ctgp_AsmAlias(const void *ptr)
{
    u32 address = (u32)ptr;
    u32 page = address & ~0xFFFu;
    for (u32 i = 0; i < 2; i++)
        if (g_ctgpAliasPages[i].sourcePage == page && g_ctgpAliasPages[i].mappedBase)
            return (void*)(g_ctgpAliasPages[i].mappedBase + (address & 0xFFFu));
    for (u32 i = 0; i < 2; i++)
    {
        if (!g_ctgpAliasPages[i].mappedBase)
        {
            u32 mappedBase = 0, mappedAddress = 0;
            if (!CTGP_MENU__MapPage(CUR_PROCESS_HANDLE, address, &mappedBase, &mappedAddress))
                return NULL;
            g_ctgpAliasPages[i].sourcePage = page;
            g_ctgpAliasPages[i].mappedBase = mappedBase;
            return (void*)mappedAddress;
        }
    }
    return NULL;
}



#define g_myHookVal ((volatile u32*)PLUGIN_ctgp_AsmAlias(g_myHookVal_raw))
#define g_ctgpHookAddrs ((volatile u32*)PLUGIN_ctgp_AsmAlias(g_ctgpHookAddrs_raw))
#define g_ctgpRelocScanSize (*(volatile u32*)PLUGIN_ctgp_AsmAlias(&g_ctgpRelocScanSize_raw))
#define g_ctgpRelocTrigger ((volatile u32*)PLUGIN_ctgp_AsmAlias(g_ctgpRelocTrigger_raw))
#define g_ctgpRelocCopies ((volatile u32*)PLUGIN_ctgp_AsmAlias(g_ctgpRelocCopies_raw))
#define g_privTextInstruction (*(volatile u32*)PLUGIN_ctgp_AsmAlias(&g_privTextInstruction_raw))
#define g_ctgpTextStrAddr ((volatile u32*)PLUGIN_ctgp_AsmAlias(g_ctgpTextStrAddr_raw))
#define g_privTextReturn (*(volatile u32*)PLUGIN_ctgp_AsmAlias(&g_privTextReturn_raw))
#define g_privTextStackOffset (*(volatile u32*)PLUGIN_ctgp_AsmAlias(&g_privTextStackOffset_raw))
#define g_langWord (*(volatile u32*)PLUGIN_ctgp_AsmAlias(&g_langWord_raw))
#define g_privTextDiagHits (*(volatile u32*)PLUGIN_ctgp_AsmAlias(&g_privTextDiagHits_raw))
#define g_privTextDiagR0 (*(volatile u32*)PLUGIN_ctgp_AsmAlias(&g_privTextDiagR0_raw))
#define g_privTextDiagPath (*(volatile u32*)PLUGIN_ctgp_AsmAlias(&g_privTextDiagPath_raw))
#define g_privTextDiagLang (*(volatile u32*)PLUGIN_ctgp_AsmAlias(&g_privTextDiagLang_raw))

PLUGIN_CODE_FROZEN(ctgp) u32 PLUGIN_ctgp_Phys(const void *ptr)
{
    return CTGP_HOST__svcConvertVAToPA(ptr, false) | 0x80000000u;
}

static PLUGIN_CODE_FROZEN(ctgp) bool fileExists(FS_Archive g_ctgpSd, const char* p)
{
    Handle f;
    if (R_SUCCEEDED(CTGP_HOST__FSUSER_OpenFile(&f, g_ctgpSd, CTGP_HOST__fsMakePath(PATH_ASCII, p),
        FS_OPEN_READ, 0)))
    {
        CTGP_HOST__FSFILE_Close(f);
        return true;
    }
    return false;
}

static PLUGIN_CODE_FROZEN(ctgp) u32 *PLUGIN_ctgp_CommandBufferLocal(void)
{
    u8 *tls;
    __asm__ volatile("mrc p15, 0, %0, c13, c0, 3" : "=r"(tls));
    return (u32*)(tls + 0x80);
}

static PLUGIN_CODE_FROZEN(ctgp) u32 PLUGIN_ctgp_BufferDesc(u32 size, u32 rights)
{
    return (size << 4) | 0x8u | rights;
}

static PLUGIN_CODE_FROZEN(ctgp) Result PLUGIN_ctgp_FSUSER_ControlArchiveLocal(
    Handle session,
    FS_Archive archive,
    FS_ArchiveAction action,
    void *input,
    u32 inputSize,
    void *output,
    u32 outputSize
)
{
    u32 *cmdbuf = PLUGIN_ctgp_CommandBufferLocal();
    cmdbuf[0] = 0x080D0144u;
    cmdbuf[1] = (u32)archive;
    cmdbuf[2] = (u32)(archive >> 32);
    cmdbuf[3] = action;
    cmdbuf[4] = inputSize;
    cmdbuf[5] = outputSize;
    cmdbuf[6] = PLUGIN_ctgp_BufferDesc(inputSize, 0x2u);
    cmdbuf[7] = (u32)input;
    cmdbuf[8] = PLUGIN_ctgp_BufferDesc(outputSize, 0x4u);
    cmdbuf[9] = (u32)output;

    Result res = CTGP_HOST__svcSendSyncRequest(session);
    if (R_FAILED(res))
        return res;

    return (Result)PLUGIN_ctgp_CommandBufferLocal()[1];
}

static PLUGIN_CODE_FROZEN(ctgp) u32 PLUGIN_ctgp_FindHostFsHandleObject(const void *hostFunction)
{
    // libctru's FS wrappers select between a thread-local FS override and the
    // process-wide fsuHandle. The global handle address is emitted in the same
    // literal pool as FS_OVERRIDE_MAGIC. Find that pair in a host FS wrapper
    // whose address was already repaired by 3nr, instead of adding a new HOST
    // dependency for a helper that clean Rosalina may have garbage-collected.
    const volatile u32 *words = (const volatile u32*)((u32)hostFunction & ~3u);

    for (u32 i = 0; i + 1 < 0x40u; i++)
    {
        if (words[i] != 0x21465324u) // FS_OVERRIDE_MAGIC = "!FS$"
            continue;

        u32 candidate = words[i + 1];
        if ((candidate & 3u) == 0 && candidate >= 0x10000000u && candidate < 0x18000000u)
            return candidate;
    }

    return 0;
}

static PLUGIN_CODE_FROZEN(ctgp) Handle PLUGIN_ctgp_GetHostFsSession(void)
{
    u8 *tls;
    __asm__ volatile("mrc p15, 0, %0, c13, c0, 3" : "=r"(tls));

    // Match libctru fsSession(): honour a per-thread FS override first.
    if (*(volatile u32*)(tls + 0x10) == 0x21465324u)
        return *(volatile Handle*)(tls + 0x14);

    // Cross-check two already-imported FS wrappers. On the supplied Rosalina
    // both literal pools resolve to the same fsuHandle object. Requiring the
    // match avoids blindly dereferencing a coincidental literal in host code.
    u32 fromOpen = PLUGIN_ctgp_FindHostFsHandleObject((const void*)CTGP_HOST__FSUSER_OpenArchive);
    u32 fromClose = PLUGIN_ctgp_FindHostFsHandleObject((const void*)CTGP_HOST__FSUSER_CloseArchive);

    if (!fromOpen || !fromClose || fromOpen != fromClose)
        return 0;

    return *(volatile Handle*)fromOpen;
}

static PLUGIN_CODE_FROZEN(ctgp) bool PLUGIN_ctgp_GetFileModifiedTime(
    FS_Archive archive,
    const char *path,
    u64 *timestamp
)
{
    if (!archive || !path || !timestamp)
        return false;

    u16 utf16Path[160];
    u32 length = 0;

    while (path[length] && length + 1 < (sizeof(utf16Path) / sizeof(utf16Path[0])))
    {
        utf16Path[length] = (u8)path[length];
        length++;
    }
    if (path[length])
        return false;
    utf16Path[length] = 0;

    Handle fsSession = PLUGIN_ctgp_GetHostFsSession();
    if (!fsSession)
        return false;

    Result res = PLUGIN_ctgp_FSUSER_ControlArchiveLocal(
        fsSession,
        archive,
        ARCHIVE_ACTION_GET_TIMESTAMP,
        utf16Path,
        (length + 1) * sizeof(utf16Path[0]),
        timestamp,
        sizeof(*timestamp)
    );

    return R_SUCCEEDED(res);
}
PLUGIN_DATA(ctgp) int mimCount = 0;
PLUGIN_CODE_FROZEN(ctgp) void countMimFiles()
{
    mimCount = 0;

    bool foundTemp = false;
    bool noMim = false;

    if (fileExists(g_ctgpSd, g_ctgpMimPath))
        mimCount++;
    else
        noMim = true;

    char path[48];

    for (int i = 2; i <= 100; i++)
    {
        CTGP_HOST__sprintf(path, g_ctgpMimFmtD, i);

        if (fileExists(g_ctgpSd, path))
        {
            mimCount++;
            continue;
        }

        if (noMim)
            break;

        if (!foundTemp)
        {
            if (fileExists(g_ctgpSd, g_ctgpMimTempPath))
            {
                if (R_SUCCEEDED(CTGP_HOST__FSUSER_RenameFile(
                    g_ctgpSd,
                    CTGP_HOST__fsMakePath(PATH_ASCII, g_ctgpMimTempPath),
                    g_ctgpSd,
                    CTGP_HOST__fsMakePath(PATH_ASCII, path))))
                {
                    foundTemp = true;
                    mimCount += 1;
                }
                else {
                    break;
                }
                continue;
            }
        }
        break;
    }
    if (noMim)
    {
        bool renamed = false;
        if (fileExists(g_ctgpSd, g_ctgpMimTempPath))
        {
            if (R_SUCCEEDED(CTGP_HOST__FSUSER_RenameFile(
                g_ctgpSd,
                CTGP_HOST__fsMakePath(PATH_ASCII, g_ctgpMimTempPath),
                g_ctgpSd,
                CTGP_HOST__fsMakePath(PATH_ASCII, g_ctgpMimPath))))
            {
                renamed = true;
                mimCount += 1;
            }
        }
        if (!renamed)
            mimCount = 0;
    }
}

PLUGIN_CODE_FROZEN(ctgp) bool ensureProcess(u32* pid, const char* targetName, Handle* outHandle)
{
    s32 count;
    u32 ids[64];

    if (R_FAILED(CTGP_HOST__svcGetProcessList(&count, ids, 64)))
        return false;

    // if pid is nonzero, check if it still exists and matches
    if (*pid != 0)
    {
        for (int i = 38; i < count; i++)
        {
            if (ids[i] == *pid)
            {
                Handle h;
                u64 info;

                if (R_SUCCEEDED(CTGP_HOST__svcOpenProcess(&h, *pid)))
                {
                    if (CTGP_HOST__svcWaitSynchronization(h, 0) == 0)
                    {
                        // zombie -> treat as not found
                        CTGP_HOST__svcCloseHandle(h);
                        break;
                    }

                    if (R_SUCCEEDED(CTGP_HOST__svcGetProcessInfo((s64*)&info, h, 0x10000)) &&
                        CTGP_HOST__memcmp(&info, targetName, 8) == 0)
                    {
                        if (outHandle) *outHandle = h;
                        return true;
                    }
                    CTGP_HOST__svcCloseHandle(h);
                }
                break;
            }
        }
        *pid = 0;
    }

    // search for new process
    for (int i = 38; i < count; i++)
    {
        Handle h;
        u64 info;

        if (R_SUCCEEDED(CTGP_HOST__svcOpenProcess(&h, ids[i])))
        {
            // ZOMBIE CHECK
            if (CTGP_HOST__svcWaitSynchronization(h, 0) == 0)
            {
                CTGP_HOST__svcCloseHandle(h);
                continue;
            }

            if (R_SUCCEEDED(CTGP_HOST__svcGetProcessInfo((s64*)&info, h, 0x10000)) &&
                CTGP_HOST__memcmp(&info, targetName, 8) == 0)
            {
                *pid = ids[i];
                if (outHandle) *outHandle = h;
                return true;
            }

            CTGP_HOST__svcCloseHandle(h);
        }
    }

    return false;
}

// Compact patch-health state. These values intentionally survive QuitCtgpLoop()
// so the debug page can report the last attempted launch/game patch even after a
// failed callback has stopped the polling loop. They are reset by the callbacks
// when a new patch attempt begins.
PLUGIN_DATA(ctgp) static u32 g_ctgpLauncherScanMask = 0;
PLUGIN_DATA(ctgp) static bool g_ctgpLauncherWriteOk = false;
PLUGIN_DATA(ctgp) static s32 g_ctgpLauncherPatchResult = -1;

#define CTGP_MK_SCAN_CHAR          (1u << 0)
#define CTGP_MK_SCAN_CONTROL       (1u << 1)
#define CTGP_MK_SCAN_SPINY         (1u << 2)
#define CTGP_MK_SCAN_SHELL         (1u << 3)
#define CTGP_MK_SCAN_WW            (1u << 4)
#define CTGP_MK_SCAN_TIMER         (1u << 10)
#define CTGP_MK_SCAN_CAVE          (1u << 11)
#define CTGP_MK_SCAN_MENU          (1u << 12)
#define CTGP_MK_SCAN_BOT           (1u << 13)
#define CTGP_MK_SCAN_ROOM          (1u << 14)
#define CTGP_MK_SCAN_INITIAL_EXPECTED (CTGP_MK_SCAN_CHAR | CTGP_MK_SCAN_CONTROL | CTGP_MK_SCAN_SPINY | CTGP_MK_SCAN_SHELL | CTGP_MK_SCAN_WW | CTGP_MK_SCAN_TIMER)
#define CTGP_MK_SCAN_EXPECTED      (CTGP_MK_SCAN_INITIAL_EXPECTED | CTGP_MK_SCAN_CAVE | CTGP_MK_SCAN_MENU | CTGP_MK_SCAN_BOT | CTGP_MK_SCAN_ROOM)

#define CTGP_MK_WRITE_CAVE       (1u << 0)
#define CTGP_MK_WRITE_CHAR       (1u << 1)
#define CTGP_MK_WRITE_CONTROL    (1u << 2)
#define CTGP_MK_WRITE_SHELL      (1u << 3)
#define CTGP_MK_WRITE_SPINY      (1u << 4)
#define CTGP_MK_WRITE_TRAIL_A    (1u << 5)
#define CTGP_MK_WRITE_TRAIL_B    (1u << 6)
#define CTGP_MK_WRITE_MENU       (1u << 10)
#define CTGP_MK_WRITE_EXPECTED   (CTGP_MK_WRITE_CAVE | CTGP_MK_WRITE_CHAR | CTGP_MK_WRITE_CONTROL | CTGP_MK_WRITE_SHELL | CTGP_MK_WRITE_SPINY | CTGP_MK_WRITE_TRAIL_A | CTGP_MK_WRITE_TRAIL_B | CTGP_MK_WRITE_MENU)

PLUGIN_DATA(ctgp) static u32 g_ctgpMkScanMask = 0;
PLUGIN_DATA(ctgp) static u32 g_ctgpMkWriteMask = 0;
PLUGIN_DATA(ctgp) static bool g_ctgpMkPatchAttempted = false;
PLUGIN_DATA(ctgp) static bool g_ctgpMkPatchSuccess = false;
PLUGIN_DATA(ctgp) static s32 g_ctgpMkPatchResult = -1;
PLUGIN_DATA(ctgp) static bool g_ctgpPrivatePollSeen = false;
PLUGIN_DATA(ctgp) static bool g_ctgpPrivateVerifyOk = false;

// Observe entry layout without rewriting CTGP's plaintext or encrypted word 0.
PLUGIN_DATA(ctgp) static u32 g_ctgpEntryBodyAddress = 0;
PLUGIN_DATA(ctgp) static bool g_ctgpEntryBodyKnown = false;
PLUGIN_DATA(ctgp) static u32 g_ctgpChecksumState = 0;
PLUGIN_DATA(ctgp) static u32 g_ctgpChecksumIvAddress = 0;
PLUGIN_DATA(ctgp) static u32 g_ctgpChecksumStore0 = 0;
PLUGIN_DATA(ctgp) static u32 g_ctgpChecksumStore1 = 0;
extern bool PLUGIN_ctgp_FindChecksumStores(const u32*,u32,u32,u32*);
extern bool PLUGIN_ctgp_RuntimeHeaderValid(const u32*);
PLUGIN_CODE_LATE(ctgp) static void PLUGIN_ctgp_PrepareStartup(void);
PLUGIN_CODE_LATE(ctgp) static void PLUGIN_ctgp_PublishStartup(void);
PLUGIN_CODE_LATE(ctgp) static void PLUGIN_ctgp_SaveStartupStatus(u32 phase);

PLUGIN_CODE_FROZEN(ctgp) static bool PLUGIN_ctgp_EncodeArmBranch(u32 address, u32 target, bool link, u32 *instruction)
{
    s32 delta = (s32)(target - (address + 8u));
    if ((delta & 3) != 0 || delta < -0x02000000 || delta > 0x01FFFFFC)
        return false;
    *instruction = (link ? 0xEB000000u : 0xEA000000u) | (((u32)delta >> 2) & 0x00FFFFFFu);
    return true;
}

#include "ctgp_commit.inc"
PLUGIN_BSS_LATE(ctgp) static volatile u32 g_ctgpReleaseBusy;
PLUGIN_CODE_LATE(ctgp) static bool PLUGIN_ctgp_CompletePostUnpackRelease(void);



void botCountHook(void);
void privTextHook(void);

PLUGIN_DATA(ctgp) u32 charSelectPointer = 0;
PLUGIN_DATA(ctgp) static u32 loseControlPointer = 0;
PLUGIN_DATA(ctgp) static u32 shRedPointer = 0;
PLUGIN_DATA(ctgp) static u32 shGreenPointer = 0;
PLUGIN_DATA(ctgp) static u32 shBluePointer = 0;
PLUGIN_DATA(ctgp) static u32 spRedPointer = 0;
PLUGIN_DATA(ctgp) static u32 spGreenPointer = 0;
PLUGIN_DATA(ctgp) static u32 spBluePointer = 0;
PLUGIN_DATA(ctgp) static u32 shActivePointer = 0;

// use to toggle on/off based on room code
PLUGIN_BSS(ctgp) static u32 wwDisconnInstrOG[3];
PLUGIN_DATA(ctgp) static u32 wwDisconnAddr = 0;
PLUGIN_DATA(ctgp) static u32 timer5minAddr = 0;
PLUGIN_BSS(ctgp) static u32 botCountInstrOG[3];
PLUGIN_DATA(ctgp) static u32 botCountAddr = 0;
PLUGIN_DATA(ctgp) static u32 privRoomGlobal = 0;

PLUGIN_CODE_FROZEN(ctgp) Result PatchGameCTGP7Callback(
    Handle procHandle,
    u32 textSz,
    u32 roSz,
    u32 rwSz
)
{
    g_ctgpMkPatchAttempted = true;
    g_ctgpMkPatchSuccess = false;
    g_ctgpMkPatchResult = -1;
    g_ctgpMkScanMask = 0;
    g_ctgpMkWriteMask = 0;
    g_ctgpPrivatePollSeen = false;
    g_ctgpPrivateVerifyOk = false;

    u32 baseMk = 0x00100000;
    u32 sizeMk = textSz + roSz + rwSz;

    u32 charSelectAddr = 0;
    u32 loseControlAddr = 0;
    u32 shellColorAddr = 0;
    u32 spinyColorAddr = 0;

    u8 shellPatternCount = 0;

    u32* scan = (u32*)baseMk;
    u32* end = (u32*)(baseMk + sizeMk - 0x10); // trim safety so scan[3] doesnt crash for example

    while (scan < end) // MK7 SCAN LOOP
    {
        // charSelectAddr pattern
        if (!charSelectAddr &&
            scan[0] == 0xE3500013 &&
            scan[1] == 0x0A000017 &&
            scan[2] == 0xE5840354)
        {
            charSelectAddr = (u32)scan + 0x8;
        }

        // loseControlAddr pattern
        if (!loseControlAddr &&
            scan[0] == 0xE5900010 &&
            scan[1] == 0xE5900010 &&
            scan[2] == 0xE5C01030)
        {
            loseControlAddr = (u32)scan + 0x4;
        }

        // shared shell / spiny pattern
        if (scan[0] == 0xE1A0000D &&
            scan[1] == 0xE12FFF32 &&
            scan[2] == 0xE89D000F)
        {
            shellPatternCount++;

            if (shellPatternCount == 1 && !spinyColorAddr)
                spinyColorAddr = (u32)scan + 0x8;
            else if (shellPatternCount == 2 && !shellColorAddr)
                shellColorAddr = (u32)scan + 0x8; // 0x2C707C for USA
        }

        if (!wwDisconnAddr &&
            scan[0] == 0x03100502 &&
            scan[1] == 0x03100801 &&
            scan[2] == 0x0A000008)
        {
            wwDisconnAddr = (u32)scan + 0x8;
            wwDisconnInstrOG[0] = *(u32*)(wwDisconnAddr);
            wwDisconnInstrOG[1] = *(u32*)(wwDisconnAddr + 0x74);
            wwDisconnInstrOG[2] = *(u32*)(wwDisconnAddr + 0x78);
        }

        if (!timer5minAddr &&
            scan[0] == 0x00004740)
        {
            timer5minAddr = (u32)scan;
        }

        if (charSelectAddr &&
            loseControlAddr &&
            spinyColorAddr &&
            shellColorAddr &&
            wwDisconnAddr &&
            timer5minAddr)
            break;

        scan++;
    }

    if (charSelectAddr)      g_ctgpMkScanMask |= CTGP_MK_SCAN_CHAR;
    if (loseControlAddr)     g_ctgpMkScanMask |= CTGP_MK_SCAN_CONTROL;
    if (spinyColorAddr)      g_ctgpMkScanMask |= CTGP_MK_SCAN_SPINY;
    if (shellColorAddr)      g_ctgpMkScanMask |= CTGP_MK_SCAN_SHELL;
    if (wwDisconnAddr)       g_ctgpMkScanMask |= CTGP_MK_SCAN_WW;
    if (timer5minAddr)       g_ctgpMkScanMask |= CTGP_MK_SCAN_TIMER;

    if ((g_ctgpMkScanMask & CTGP_MK_SCAN_INITIAL_EXPECTED) != CTGP_MK_SCAN_INITIAL_EXPECTED)
    {
        g_ctgpMkPatchResult = -1;
        CTGP_HOST__svcCloseHandle(procHandle);
        return -1;
    }

    u32 menuTextAddr = 0;
    u32 ctgp7HookAddr = 0;
    u32 ctgp7ContinueAddr = 0;
    u32 ctgp7HookMatches = 0;
    u32 menuTextMatches = 0;
    u32 botCountMatches = 0;
    u32 fetchPrivRoomMatches = 0;

    u32 imageSize = CTGP_HOST__g_memBlockSize;
    if (imageSize <= 0x1100)
    {
        CTGP_HOST__svcCloseHandle(procHandle);
        return -1;
    }
    u32 imageStart = 0x07000100;
    u32 imageEnd = imageStart + imageSize - 0x1100;
    scan = (u32*)imageStart;
    end = (u32*)imageEnd;

    while (scan + 14 < end) // CTGP7 SCAN LOOP
    {
        if ((scan[0] & 0xFFFFF000) == 0xE51F0000 &&
            (scan[1] & 0xFFFFF000) == 0xE58D5000 &&
            (scan[2] & 0xFF000000) == 0xEB000000 &&
            scan[3] == 0xE3A03001)
        {
            u32 candidateHook = (u32)&scan[3];
            u32 candidateContinue = 0;
            u32 continueMatches = 0;
            u32 *continuation = (u32*)(candidateHook + 0xC0);
            u32 *continuationEnd = (u32*)(candidateHook + 0x180);
            if (continuationEnd + 6 > end) continuationEnd = end - 6;

            while (continuation <= continuationEnd)
            {
                if ((continuation[0] & 0xFFFFF000) == 0xE51F0000 &&
                    (continuation[1] & 0xFF000000) == 0xEB000000 &&
                    continuation[2] == 0xE59D300C &&
                    continuation[3] == 0xE5D333B8 &&
                    continuation[4] == 0xE3530000 &&
                    (continuation[5] & 0xFF000000) == 0x0A000000)
                {
                    continueMatches++;
                    candidateContinue = (u32)continuation;
                }
                continuation++;
            }

            if (continueMatches == 1)
            {
                ctgp7HookMatches++;
                if (ctgp7HookMatches == 1)
                {
                    ctgp7HookAddr = candidateHook;
                    ctgp7ContinueAddr = candidateContinue;
                }
            }
        }

        char *b = (char*)scan;
        for (u32 o = 0; o < 4; o++)
        {
            unsigned char *p = (unsigned char*)(b + o);
            bool match = true;
            for (u32 k = 0; k < 20; k++)
            {
                if (p[k] != g_ctgpFrameworkPattern[k])
                {
                    match = false;
                    break;
                }
            }
            if (match)
            {
                menuTextMatches++;
                if (menuTextMatches == 1) menuTextAddr = (u32)p;
            }
        }

        if ((scan[0] & 0xFFFFF000) == 0xE59F7000 &&
            scan[1] == 0xE3833001 &&
            (scan[2] & 0xFFFFF000) == 0xE5C23000 &&
            (scan[3] & 0xFF000000) == 0xEB000000 &&
            scan[4] == 0xE3A02000 &&
            scan[5] == 0xE3A03000)
        {
            u32 strdIndex = 6;
            while (strdIndex <= 10 && scan[strdIndex] != 0xE1C720F0) strdIndex++;
            if (strdIndex <= 10 &&
                (scan[strdIndex + 1] & 0xFF000000) == 0xEB000000 &&
                (scan[strdIndex + 2] & 0xFF000000) == 0xEB000000 &&
                scan[strdIndex + 3] == 0xE3500000 &&
                (scan[strdIndex + 4] & 0xFF000000) == 0x0A000000)
            {
                u32 literalAddr = (u32)scan + 8 + (scan[0] & 0xFFF);
                if (literalAddr >= imageStart && literalAddr + 4 <= imageEnd)
                {
                    u32 candidateGlobal = *(u32*)literalAddr;
                    if (candidateGlobal >= imageStart && candidateGlobal + 8 <= imageEnd)
                    {
                        fetchPrivRoomMatches++;
                        if (fetchPrivRoomMatches == 1)
                        {
                            privRoomGlobal = candidateGlobal;
                        }
                    }
                }
            }
        }

        if (scan[0] == 0xE3A03001 &&
            scan[1] == 0xE8940006 &&
            scan[2] == 0xE5850000)
        {
            botCountMatches++;
            if (botCountMatches == 1)
            {
                botCountAddr = (u32)scan;
                botCountInstrOG[0] = scan[0];
                botCountInstrOG[1] = scan[1];
                botCountInstrOG[2] = scan[2];
            }
        }

        scan++;
    }

    if (ctgp7HookMatches == 1)     g_ctgpMkScanMask |= CTGP_MK_SCAN_CAVE;
    if (menuTextMatches == 1)      g_ctgpMkScanMask |= CTGP_MK_SCAN_MENU;
    if (botCountMatches == 1)      g_ctgpMkScanMask |= CTGP_MK_SCAN_BOT;
    if (fetchPrivRoomMatches == 1) g_ctgpMkScanMask |= CTGP_MK_SCAN_ROOM;

    if (g_ctgpMkScanMask != CTGP_MK_SCAN_EXPECTED)
    {
        g_ctgpMkPatchResult = -2;
        CTGP_HOST__svcCloseHandle(procHandle);
        return -1;
    }

    u32 caveBranch = 0;
    if (!PLUGIN_ctgp_EncodeArmBranch(ctgp7HookAddr, ctgp7ContinueAddr, false, &caveBranch))
    {
        g_ctgpMkPatchResult = -3;
        CTGP_HOST__svcCloseHandle(procHandle);
        return -1;
    }

    u32* p = (u32*)charSelectAddr;
    u32* c = (u32*)loseControlAddr;
    u32* h = (u32*)ctgp7HookAddr;
    char* m = (char*)menuTextAddr;
    u32* s = (u32*)shellColorAddr;
    u32* b = (u32*)spinyColorAddr;

    // --- hook beginning ---
    int i = 0;
    h[i++] = caveBranch;

    // ---- character/model-swap state ----
    u32 hook1Off = i * 4;
    h[i++] = p[0]; // 0x7048E10, hook 1 start, og str r0
    // hook logic
    h[i++] = 0xE58F0008; // store r0 to word below
    // hook end and return
    h[i++] = p[1]; // og clobbers r0 so must do at end
    h[i++] = 0xE51FF004; // change pc to address following
    h[i++] = charSelectAddr + 0x8;
    charSelectPointer = ctgp7HookAddr + i * 4;
    h[i++] = 0x0; // store r0 here

    // hook 2 start
    u32 hook2Off = i * 4;
    h[i++] = c[0];
    h[i++] = c[1]; // og strb r1
    h[i++] = 0xE58F1004; // store r1
    h[i++] = 0xE51FF004; // change pc to address following
    h[i++] = loseControlAddr + 0x8;
    loseControlPointer = ctgp7HookAddr + i * 4;
    h[i++] = 0x0; // store r1 here

    // jump to hook for slot selection
    p[0] = 0xE51FF004; // used to be STR 0 instruction
    p[1] = ctgp7HookAddr + hook1Off; // used to be LDR 0

    c[0] = 0xE51FF004;
    c[1] = ctgp7HookAddr + hook2Off;

    // ---- shell stuff ----
    u32 hook3Off = i * 4;
    h[i++] = s[0]; // og sets r0-r3

    // if redshell trigger r->g->b-> shift
    h[i++] = 0xEE001A10;
    h[i++] = 0xEDDF0A11;
    h[i++] = 0xEEB40A60;
    h[i++] = 0xEEF1FA10;

    // ldr custom rgba floats
    h[i++] = 0xE59F0028;
    h[i++] = 0xE59F1028;
    h[i++] = 0xE59F2028;
    h[i++] = 0xE59F3028;

    // str alpha at flag value
    h[i++] = 0xE58F302C;

    // from redshell check cycle regs r0-r2 using r12 as temp
    h[i++] = 0xB1A0C000;
    h[i++] = 0xB1A00002;
    h[i++] = 0xB1A02001;
    h[i++] = 0xB1A0100C;

    h[i++] = s[1]; // og clobbers r12
    h[i++] = 0xE51FF004; // change pc to address following
    h[i++] = shellColorAddr + 0x8;

    // rgba floats
    shRedPointer = ctgp7HookAddr + i * 4;
    h[i++] = 0x3F800000;
    shGreenPointer = ctgp7HookAddr + i * 4;
    h[i++] = 0x3F800000;
    shBluePointer = ctgp7HookAddr + i * 4;
    h[i++] = 0x3F800000;
    h[i++] = 0x3F800000; // 1.0f alpha
    h[i++] = 0x3F000000; // 0.5f check for green/red shell
    shActivePointer = ctgp7HookAddr + i * 4;
    h[i++] = 0; // isShellActive flag

    // --- blue shell ---
    u32 hook4Off = i * 4;
    h[i++] = b[0]; // og sets r0-r3

    // ldr custom rgba floats
    h[i++] = 0xE59F0018;
    h[i++] = 0xE59F1018;
    h[i++] = 0xE59F2018;
    h[i++] = 0xE51F3024; // alpha from above
    // str alpha at same flag value
    h[i++] = 0xE50F3020;

    h[i++] = b[1]; // og clobbers r12
    h[i++] = 0xE51FF004; // change pc to address following
    h[i++] = spinyColorAddr + 0x8;

    // rgba floats
    spRedPointer = ctgp7HookAddr + i * 4; h[i++] = 0x3F800000;
    spGreenPointer = ctgp7HookAddr + i * 4; h[i++] = 0x3F800000;
    spBluePointer = ctgp7HookAddr + i * 4; h[i++] = 0x3F800000;

    // pretty much no space left in this hook

    // jump to hook for shell color compute
    s[0] = 0xE51FF004;
    s[1] = ctgp7HookAddr + hook3Off;

    b[0] = 0xE51FF004;
    b[1] = ctgp7HookAddr + hook4Off;

    // everlasting trail patch
    *(u32*)(shellColorAddr - 0x104) = 0xEA00001E;
    *(u32*)(shellColorAddr - 0x74) = 0xE320F000;

    // --------- menu text patch last as a sort of confirmation ---------
    const char *newstr = g_ctgpCustomMenuText;
    size_t newstrLen = CTGP_HOST__strlen(newstr);
    for (size_t j = 0; j <= newstrLen; j++)
        m[j] = newstr[j];

    // Read back every externally visible patch site.  The generated cave body
    // itself is constructed above; these checks verify that all entry points,
    // persistent instruction patches and the menu text landed in live memory.
    if (h[0] == caveBranch)
        g_ctgpMkWriteMask |= CTGP_MK_WRITE_CAVE;
    if (p[0] == 0xE51FF004 && p[1] == ctgp7HookAddr + hook1Off)
        g_ctgpMkWriteMask |= CTGP_MK_WRITE_CHAR;
    if (c[0] == 0xE51FF004 && c[1] == ctgp7HookAddr + hook2Off)
        g_ctgpMkWriteMask |= CTGP_MK_WRITE_CONTROL;
    if (s[0] == 0xE51FF004 && s[1] == ctgp7HookAddr + hook3Off)
        g_ctgpMkWriteMask |= CTGP_MK_WRITE_SHELL;
    if (b[0] == 0xE51FF004 && b[1] == ctgp7HookAddr + hook4Off)
        g_ctgpMkWriteMask |= CTGP_MK_WRITE_SPINY;
    if (*(u32*)(shellColorAddr - 0x104) == 0xEA00001E)
        g_ctgpMkWriteMask |= CTGP_MK_WRITE_TRAIL_A;
    if (*(u32*)(shellColorAddr - 0x74) == 0xE320F000)
        g_ctgpMkWriteMask |= CTGP_MK_WRITE_TRAIL_B;
    if (CTGP_HOST__memcmp(m, newstr, newstrLen + 1) == 0)
        g_ctgpMkWriteMask |= CTGP_MK_WRITE_MENU;

    g_ctgpMkPatchSuccess = g_ctgpMkWriteMask == CTGP_MK_WRITE_EXPECTED;
    g_ctgpMkPatchResult = g_ctgpMkPatchSuccess ? 0 : -4;

    CTGP_HOST__svcFlushEntireDataCache();
    CTGP_HOST__svcInvalidateEntireInstructionCache();
    CTGP_HOST__svcCloseHandle(procHandle);
    return g_ctgpMkPatchSuccess ? 0 : -1;
}

PLUGIN_BSS(ctgp) static u32 g_ctgpTextHostAddr[2];
PLUGIN_DATA(ctgp) static bool g_ctgpToxtTargetFound = false;
PLUGIN_DATA(ctgp) static bool g_ctgpToxtHookInstalled = false;
PLUGIN_DATA(ctgp) static bool g_ctgpToxtRedirected = false;
// Exact instructions replaced by privTextHook. These are used only to make
// DUMP snapshots self-contained instead of serialising a pointer into this 3NX.
PLUGIN_DATA(ctgp) static u32 g_ctgpToxtOrigWord0 = 0;
PLUGIN_DATA(ctgp) static u32 g_ctgpToxtOrigWord1 = 0;
PLUGIN_DATA(ctgp) static bool g_ctgpToxtOrigValid = false;
PLUGIN_DATA(ctgp) static bool g_ctgpToxtScanPending = false;
PLUGIN_DATA(ctgp) static u64 g_ctgpToxtScanElapsed = 0;
PLUGIN_DATA(ctgp) static u64 g_ctgpToxtFullScanElapsed = 0;

// Probe the mapped header for a relocated runtime entry. Translation sites may
// move between versions; installation still requires one validated /Text.txt
// construction in the complete image.
#define CTGP_TOXT_KNOWN_PROBE 0x07000100u

// Toxt diagnostics are retained for the compact CTGP debug page.  They are
// observational only: a failed Toxt stage no longer deliberately crashes the
// console.  The mid-load installer keeps retrying until CTGP leaves its window.
PLUGIN_BSS(ctgp) static u32 g_toxtDiagStage;
PLUGIN_BSS(ctgp) static u32 g_toxtDiagFail;
PLUGIN_BSS(ctgp) static u32 g_toxtDiagScanCalls;
PLUGIN_BSS(ctgp) static u32 g_toxtDiagCtx;
PLUGIN_BSS(ctgp) static u32 g_toxtDiagHostBase;
PLUGIN_BSS(ctgp) static u32 g_toxtDiagImageSize;
PLUGIN_BSS(ctgp) static u32 g_toxtDiagMemSize;
PLUGIN_BSS(ctgp) static u32 g_toxtDiagSig0;
PLUGIN_BSS(ctgp) static u32 g_toxtDiagSig3;
PLUGIN_BSS(ctgp) static u32 g_toxtDiagSig11;
PLUGIN_BSS(ctgp) static u32 g_toxtDiagLiteralRange;
PLUGIN_BSS(ctgp) static u32 g_toxtDiagPtrRange;
PLUGIN_BSS(ctgp) static u32 g_toxtDiagTextExact;
PLUGIN_BSS(ctgp) static u32 g_toxtDiagMatches;
PLUGIN_BSS(ctgp) static u32 g_toxtDiagScanResult;
PLUGIN_BSS(ctgp) static u32 g_toxtDiagPatchResult;
PLUGIN_BSS(ctgp) static u32 g_toxtDiagArchiveResult;
PLUGIN_BSS(ctgp) static u32 g_toxtDiagLangOpenResult;
PLUGIN_BSS(ctgp) static u32 g_toxtDiagLangReadResult;
PLUGIN_BSS(ctgp) static u32 g_toxtDiagLangReadBytes;
PLUGIN_BSS(ctgp) static u32 g_toxtDiagToxtExists;
PLUGIN_BSS(ctgp) static u32 g_toxtDiagHookWord0;
PLUGIN_BSS(ctgp) static u32 g_toxtDiagHookWord1;
PLUGIN_BSS(ctgp) static u32 g_toxtDiagProbeWord;
PLUGIN_BSS(ctgp) static u32 g_toxtDiagQuickTriggers;
PLUGIN_BSS(ctgp) static u32 g_toxtDiagPendingTicks;

PLUGIN_DATA(ctgp) static u32 g_ctgpLegacyPatchAddr0 = 0;
PLUGIN_DATA(ctgp) static u32 g_ctgpLegacyPatchAddr1 = 0;
PLUGIN_DATA(ctgp) static bool g_ctgpLegacyPatch0Applied = false;
PLUGIN_DATA(ctgp) static bool g_ctgpLegacyPatch1Applied = false;

PLUGIN_CODE_FROZEN(ctgp) static bool PLUGIN_ctgp_IsWritableAddress(u32 address);

// Compatibility is installed only while the final runtime is held at the
// post-unpack gate. Leave MCU and runtime integrity instructions intact.
PLUGIN_CODE_FROZEN(ctgp) static bool PLUGIN_ctgp_ApplyLegacySafetyPatches(void)
{
    return PLUGIN_ctgp_InstallCommitCompat();
}

PLUGIN_CODE_FROZEN(ctgp) Result scanPreloadAnyCTGP()
{
    // IMPORTANT: this scans the live 3GX mapping, not the initial loader image.
    // The original working Toxt patch only ran after CTGP had progressed into
    // startup. At PostGameFlush this region does not yet contain the final code.
    g_toxtDiagStage = 0x31;
    g_toxtDiagFail = 0;
    g_toxtDiagScanCalls++;
    g_toxtDiagSig0 = g_toxtDiagSig3 = g_toxtDiagSig11 = 0;
    g_toxtDiagLiteralRange = g_toxtDiagPtrRange = g_toxtDiagTextExact = 0;
    g_toxtDiagMatches = 0;
    g_toxtDiagScanResult = (u32)-1;

    const u32 imageSize = CTGP_HOST__g_memBlockSize;
    const u32 imageStart = 0x07000100u;
    if (imageSize <= 0x1100u)
    {
        g_toxtDiagFail = 3;
        g_toxtDiagScanResult = (u32)-0x103;
        return (Result)g_toxtDiagScanResult;
    }
    const u32 imageEnd = imageStart + imageSize - 0x1100u;
    g_toxtDiagCtx = (u32)CTGP_HOST__PluginLoaderCtx;
    g_toxtDiagHostBase = imageStart;
    g_toxtDiagImageSize = imageEnd - imageStart;
    g_toxtDiagMemSize = imageSize;

    MemInfo mi;
    PageInfo pi;
    if (R_FAILED(CTGP_HOST__svcQueryMemory(&mi, &pi, imageStart)) ||
        mi.state == MEMSTATE_FREE || !(mi.perm & MEMPERM_READ))
    {
        g_toxtDiagFail = 2;
        g_toxtDiagScanResult = (u32)-0x102;
        return (Result)g_toxtDiagScanResult;
    }

    u32 *scan = (u32*)imageStart;
    u32 *end = (u32*)imageEnd;
    u32 textMatches = 0;
    u32 textAddr = 0;
    u32 textHookAddr = 0;
    u32 textStackOffset = 0;
    u32 legacyMatches[2] = {0, 0};
    u32 legacy0Body = 0;
    u32 legacy0BodyMatches = 0;

    // Once one of the legacy safety patches has been applied its signature is
    // intentionally gone, so later Toxt rescans must not erase the resolved
    // address/state. ResetCtgpState() clears these between CTGP sessions.
    if (!g_ctgpLegacyPatch0Applied) g_ctgpLegacyPatchAddr0 = 0;
    if (!g_ctgpLegacyPatch1Applied) g_ctgpLegacyPatchAddr1 = 0;
    g_ctgpTextStrAddr[0] = 0;
    g_ctgpTextStrAddr[1] = 0;
    g_ctgpTextHostAddr[0] = 0;
    g_ctgpTextHostAddr[1] = 0;
    g_privTextStackOffset = 0;
    g_ctgpToxtTargetFound = false;

    while (scan + 10 < end)
    {
        if ((scan[0] & 0x0FFFFFFFu) == 0x03E0603Au &&
            scan[1] == 0xE92D4080u &&
            (scan[2] & 0x0FFFF000u) == 0x059F4000u)
        {
            bool hasLoopSetup = false;
            for (u32 j = 3; j < 8; j++)
            {
                if (scan[j] == 0xE3A05008u && scan[j + 1] == 0xE3A0804Au)
                {
                    hasLoopSetup = true;
                    break;
                }
            }
            if (hasLoopSetup)
            {
                legacy0BodyMatches++;
                legacy0Body = (u32)scan;
            }
        }

        if (scan[0] == 0xE92D407Fu &&
            scan[1] == 0xEE1D3F70u &&
            (scan[6] & 0xFF000000u) == 0xEB000000u)
        {
            if (++legacyMatches[1] == 1) g_ctgpLegacyPatchAddr1 = (u32)scan + 0x18u;
        }

        bool c0 = scan[0] == 0xE1A01000u;
        bool c1 = (scan[1] & 0xFFFFF000u) == 0xE28D0000u ||
                  ((scan[1] & 0xFFFFFFF0u) == 0xE1A00000u && (scan[1] & 15u) >= 4u && (scan[1] & 15u) <= 11u);
        bool c2 = (scan[2] & 0xFF000000u) == 0xEB000000u;
        if (c0) g_toxtDiagSig0++;
        if (c0 && c1 && c2) g_toxtDiagSig3++;

        u32 candidateText = 0, decoded = 0;
        if (c0 && c1 && c2 && PLUGIN_ctgp_MatchTextEntry((const u32*)imageStart,
            imageStart,imageEnd-imageStart,(u32)scan-imageStart,&candidateText,&decoded))
        {
            g_toxtDiagSig11++; g_toxtDiagLiteralRange++; g_toxtDiagPtrRange++; g_toxtDiagTextExact++;
            textMatches++; textAddr = candidateText; textHookAddr = (u32)scan; textStackOffset = decoded;
        }
        scan++;
    }

    if (legacy0BodyMatches == 1)
    {
        u32 entry = 0;
        bool ambiguous = false;
        u32 searchStart = legacy0Body >= imageStart + 0x40u ? legacy0Body - 0x40u : imageStart;
        for (u32 *call = (u32*)imageStart; call < end; call++)
        {
            u32 instruction = *call;
            if ((instruction & 0xFF000000u) != 0xEB000000u) continue;
            s32 immediate = (s32)(instruction & 0x00FFFFFFu);
            if (immediate & 0x00800000u) immediate |= (s32)0xFF000000u;
            u32 target = (u32)((s32)((u32)call + 8u) + immediate * 4);
            if (target < searchStart || target > legacy0Body) continue;
            if (!entry) entry = target;
            else if (entry != target) ambiguous = true;
        }
        if (entry && !ambiguous)
        {
            g_ctgpLegacyPatchAddr0 = entry;
            legacyMatches[0] = 1;
        }
    }

    g_toxtDiagMatches = textMatches;
    if (textMatches == 1)
    {
        g_ctgpTextStrAddr[0] = textAddr;
        g_ctgpTextStrAddr[1] = textHookAddr;
        // Mid-load scanning is against the live mapping, so host/target are the
        // same Rosalina VA here. Keep both fields for the diagnostic page.
        g_ctgpTextHostAddr[0] = textAddr;
        g_ctgpTextHostAddr[1] = textHookAddr;
        g_privTextStackOffset = textStackOffset;
        g_ctgpToxtTargetFound = true;
        g_toxtDiagStage = 0x32;
        g_toxtDiagScanResult = 0;
        return 0;
    }

    g_toxtDiagFail = 4;
    g_toxtDiagStage = 0x34;
    g_toxtDiagScanResult = (u32)-0x104;
    return (Result)g_toxtDiagScanResult;
}

PLUGIN_CODE_FROZEN(ctgp) static bool PLUGIN_ctgp_ToxtQuickProbeReady(void)
{
    MemInfo mi;
    PageInfo pi;
    if (R_FAILED(CTGP_HOST__svcQueryMemory(&mi, &pi, CTGP_TOXT_KNOWN_PROBE)) ||
        mi.state == MEMSTATE_FREE || !(mi.perm & MEMPERM_READ))
    {
        g_toxtDiagProbeWord = 0xFFFFFFFFu;
        return false;
    }

    volatile u32 *p = (volatile u32*)CTGP_TOXT_KNOWN_PROBE;
    g_toxtDiagProbeWord = p[0];
    if (PLUGIN_ctgp_FindRuntimeEntry((const u32*)p) == 0xFFFFFFFFu)
        return false;

    g_toxtDiagQuickTriggers++;
    return true;
}

PLUGIN_CODE_FROZEN(ctgp) void patchTextFile() {
    CTGP_HOST__FSUSER_OpenArchive(&g_ctgpSd, ARCHIVE_SDMC, CTGP_HOST__fsMakePath(PATH_EMPTY, NULL));

    const char* add = g_ctgpHaxxorzText;
    char base[sizeof(g_ctgpLanguageBase)];
    CTGP_HOST__strcpy(base, g_ctgpLanguageBase);
    // overwrite XXX with lang from g_langWord
    base[sizeof(base) - 4] = (g_langWord >> 16) & 0xFF;
    base[sizeof(base) - 3] = (g_langWord >> 8) & 0xFF;
    base[sizeof(base) - 2] = (g_langWord >> 0) & 0xFF;

    char path[160];
    char textPath[160];
    char toxtPath[160];

    CTGP_HOST__strcpy(textPath, base);
    CTGP_HOST__strcat(textPath, g_ctgpTextSuffix);
    CTGP_HOST__strcpy(toxtPath, base);
    CTGP_HOST__strcat(toxtPath, g_ctgpToxtSuffix);

    // check Toxt.txt exists
    CTGP_HOST__strcpy(path, toxtPath);
    if (fileExists(g_ctgpSd, path)) {
        // change Text.txt to Toxt.txt in CTGP's real language-import literal
        *((char*)(g_ctgpTextStrAddr[0] + 2)) = 'o';
        g_ctgpToxtRedirected = true;
        g_toxtDiagToxtExists = 1;
        CTGP_HOST__svcFlushEntireDataCache();
        CTGP_HOST__svcInvalidateEntireInstructionCache();
        CTGP_HOST__FSUSER_CloseArchive(g_ctgpSd);
        return;
    }

    Handle in, out;
    CTGP_HOST__strcpy(path, textPath);
    if (R_SUCCEEDED(CTGP_HOST__FSUSER_OpenFile(&in, g_ctgpSd, CTGP_HOST__fsMakePath(PATH_ASCII, path), FS_OPEN_READ, 0))) {
        // The Toxt rewrite can run from Blur's callback worker. Keep the large
        // streaming buffer off that thread's stack and borrow temporary pages
        // from MENU instead. 0x2000 leaves room for a 0x1000 read plus overlap.
        u32 scratchBase = 0;
        if (CTGP_MENU__TempAlloc(0x2000u, &scratchBase)) {
            char *buf = (char*)scratchBase;

            // open/create Toxt.txt only after scratch allocation succeeds, so a
            // temporary allocation failure cannot leave an empty generated file.
            CTGP_HOST__strcpy(path, toxtPath);
            if (R_SUCCEEDED(CTGP_HOST__FSUSER_OpenFile(&out, g_ctgpSd, CTGP_HOST__fsMakePath(PATH_ASCII, path), FS_OPEN_WRITE | FS_OPEN_CREATE, 0))) {

                const char* key = g_ctgpEnterLobbyText;
                const char* sep = g_ctgpColonText;

                u32 keyLen = CTGP_HOST__strlen(key), sepLen = CTGP_HOST__strlen(sep), addLen = CTGP_HOST__strlen(add);
                u32 overlap = (keyLen > sepLen ? keyLen : sepLen) - 1;
                u32 carry = 0;

                u64 off = 0, size = 0, outOff = 0;
                CTGP_HOST__FSFILE_GetSize(in, &size);

                bool foundKey = false, inserted = false;
                int sepCount = 0;

                while (off < size) {
                    u32 r;
                    CTGP_HOST__FSFILE_Read(in, &r, off, buf + carry, 0x1000);
                    off += r;
                    r += carry;

                    u32 limit = (r > overlap) ? r - overlap : 0;
                    u32 start = 0;

                    for (u32 i = 0; i < limit; i++) {

                        if (!foundKey && i + keyLen <= r && !CTGP_HOST__memcmp(buf + i, key, keyLen)) {
                            foundKey = true;
                            i += keyLen - 1;
                            continue;
                        }

                        if (foundKey && !inserted && i + sepLen <= r && !CTGP_HOST__memcmp(buf + i, sep, sepLen)) {
                            sepCount++;

                            if (sepCount == 2) {
                                u32 w;

                                if (i > start) {
                                    CTGP_HOST__FSFILE_Write(out, &w, outOff, buf + start, i - start, FS_WRITE_UPDATE_TIME);
                                    outOff += w;
                                }

                                CTGP_HOST__FSFILE_Write(out, &w, outOff, add, addLen, FS_WRITE_UPDATE_TIME);
                                outOff += w;

                                start = i;
                                inserted = true;
                            }

                            i += sepLen - 1;
                        }
                    }

                    if (limit > start) {
                        u32 w;
                        CTGP_HOST__FSFILE_Write(out, &w, outOff, buf + start, limit - start, FS_WRITE_UPDATE_TIME);
                        outOff += w;
                    }

                    // keep overlap bytes
                    carry = r - limit;
                    CTGP_HOST__memcpy(buf, buf + limit, carry);
                }

                // flush remaining
                if (carry) {
                    u32 w;
                    CTGP_HOST__FSFILE_Write(out, &w, outOff, buf, carry, FS_WRITE_UPDATE_TIME);
                    outOff += w;
                }

                CTGP_HOST__FSFILE_Close(out);

                if (inserted) {
                    // change Text.txt to Toxt.txt only when the requested entry
                    // was actually found and extended successfully.
                    *((char*)(g_ctgpTextStrAddr[0] + 2)) = 'o';
                    g_ctgpToxtRedirected = true;
                    g_toxtDiagToxtExists = 1;
                } else {
                    // Do not preserve a generated file with no injected entry.
                    (void)CTGP_HOST__FSUSER_DeleteFile(
                        g_ctgpSd,
                        CTGP_HOST__fsMakePath(PATH_ASCII, toxtPath)
                    );
                }
            }

            CTGP_MENU__TempFree(scratchBase, 0x2000u);
        }

        CTGP_HOST__FSFILE_Close(in);
    }

    CTGP_HOST__FSUSER_CloseArchive(g_ctgpSd);
}

PLUGIN_CODE_FROZEN(ctgp) Result patchPreloadCustomCTGP()
{
    g_toxtDiagStage = 0x10;
    g_toxtDiagPatchResult = (u32)-1;
    g_toxtDiagArchiveResult = (u32)-1;
    g_toxtDiagLangOpenResult = (u32)-1;
    g_toxtDiagLangReadResult = (u32)-1;
    g_toxtDiagLangReadBytes = 0;
    g_toxtDiagToxtExists = 0;
    g_toxtDiagHookWord0 = g_toxtDiagHookWord1 = 0;

    if (!g_ctgpTextStrAddr[0] || !g_ctgpTextStrAddr[1] || !g_ctgpTextHostAddr[0] || !g_ctgpTextHostAddr[1])
    {
        g_toxtDiagFail = 5;
        g_toxtDiagPatchResult = (u32)-0x105;
        return (Result)g_toxtDiagPatchResult;
    }

    Result ar = CTGP_HOST__FSUSER_OpenArchive(&g_ctgpSd, ARCHIVE_SDMC, CTGP_HOST__fsMakePath(PATH_EMPTY, NULL));
    g_toxtDiagArchiveResult = (u32)ar;
    if (R_FAILED(ar))
    {
        g_toxtDiagFail = 6;
        g_toxtDiagPatchResult = (u32)-0x106;
        return (Result)g_toxtDiagPatchResult;
    }

    char base[sizeof(g_ctgpLanguageBase)];
    CTGP_HOST__strcpy(base, g_ctgpLanguageBase);
    Handle f = 0;
    Result lr = CTGP_HOST__FSUSER_OpenFile(&f, g_ctgpSd,
        CTGP_HOST__fsMakePath(PATH_ASCII, g_ctgpLangSettingPath), FS_OPEN_READ, 0);
    g_toxtDiagLangOpenResult = (u32)lr;
    if (R_SUCCEEDED(lr))
    {
        u8 buf[3];
        u32 br = 0;
        Result rr = CTGP_HOST__FSFILE_Read(f, &br, 4, buf, 3);
        g_toxtDiagLangReadResult = (u32)rr;
        g_toxtDiagLangReadBytes = br;
        if (R_SUCCEEDED(rr) && br == 3)
        {
            base[sizeof(base) - 4] = buf[0];
            base[sizeof(base) - 3] = buf[1];
            base[sizeof(base) - 2] = buf[2];
        }
        CTGP_HOST__FSFILE_Close(f);
    }

    char path[160];
    CTGP_HOST__strcpy(path, base);
    CTGP_HOST__strcat(path, g_ctgpToxtSuffix);
    if (fileExists(g_ctgpSd, path))
    {
        bool useExistingToxt = true;
        char textPath[160];
        u64 textModified = 0;
        u64 toxtModified = 0;

        CTGP_HOST__strcpy(textPath, base);
        CTGP_HOST__strcat(textPath, g_ctgpTextSuffix);

        if (PLUGIN_ctgp_GetFileModifiedTime(g_ctgpSd, textPath, &textModified) &&
            PLUGIN_ctgp_GetFileModifiedTime(g_ctgpSd, path, &toxtModified) &&
            textModified > toxtModified)
        {
            if (R_SUCCEEDED(CTGP_HOST__FSUSER_DeleteFile(
                    g_ctgpSd,
                    CTGP_HOST__fsMakePath(PATH_ASCII, path))))
            {
                useExistingToxt = false;
            }
        }

        if (useExistingToxt)
        {
            g_toxtDiagToxtExists = 1;
            *((char*)(g_ctgpTextStrAddr[0] + 2)) = 'o';
            g_langWord = 2;
            g_ctgpToxtRedirected = true;
        }
    }
    CTGP_HOST__FSUSER_CloseArchive(g_ctgpSd);

    const u32 hookWord0 = 0xE51FF004;
    const u32 hookWord1 = (u32)PLUGIN_ctgp_Phys(privTextHook);
    if (!g_ctgpToxtHookInstalled)
    {
        g_ctgpToxtOrigWord0 = *(u32*)g_ctgpTextStrAddr[1];
        g_ctgpToxtOrigWord1 = *(u32*)(g_ctgpTextStrAddr[1] + 4);
        g_ctgpToxtOrigValid = true;
    }
    g_privTextInstruction = g_ctgpToxtOrigWord1;
    *(u32*)g_ctgpTextStrAddr[1] = hookWord0;
    *(u32*)(g_ctgpTextStrAddr[1] + 4) = hookWord1;
    g_privTextReturn = g_ctgpTextStrAddr[1] + 0x8;
    g_ctgpToxtHookInstalled = true;

    CTGP_HOST__svcFlushEntireDataCache();
    CTGP_HOST__svcInvalidateEntireInstructionCache();

    g_toxtDiagHookWord0 = *(u32*)g_ctgpTextStrAddr[1];
    g_toxtDiagHookWord1 = *(u32*)(g_ctgpTextStrAddr[1] + 4);
    if (g_toxtDiagHookWord0 != hookWord0 || g_toxtDiagHookWord1 != hookWord1)
    {
        g_toxtDiagFail = 7;
        g_toxtDiagPatchResult = (u32)-0x107;
        return (Result)g_toxtDiagPatchResult;
    }

    g_toxtDiagStage = 0x20;
    g_toxtDiagPatchResult = 0;
    return 0;
}

PLUGIN_DATA(ctgp) static bool cheatsActive = false;

PLUGIN_CODE_FROZEN(ctgp) static bool PLUGIN_ctgp_VerifyPrivateRoomPatch(bool enabled)
{
    if (!wwDisconnAddr || !botCountAddr || !timer5minAddr || !privRoomGlobal)
        return false;

    if (*(u32*)wwDisconnAddr != (enabled ? 0xE320F000 : wwDisconnInstrOG[0])) return false;
    if (*(u32*)(wwDisconnAddr + 0x74) != (enabled ? 0xE320F000 : wwDisconnInstrOG[1])) return false;
    if (*(u32*)(wwDisconnAddr + 0x78) != (enabled ? 0xE320F000 : wwDisconnInstrOG[2])) return false;
    if (*(u32*)botCountAddr != (enabled ? 0xE1A0300F : botCountInstrOG[0])) return false;
    if (*(u32*)(botCountAddr + 0x4) != (enabled ? 0xE51FF004 : botCountInstrOG[1])) return false;
    if (*(u32*)(botCountAddr + 0x8) != (enabled ? (u32)PLUGIN_ctgp_Phys(botCountHook) : botCountInstrOG[2])) return false;
    if (*(u32*)timer5minAddr != (enabled ? 0x0000E0D3 : 0x00004740)) return false;
    return true;
}

PLUGIN_CODE_FROZEN(ctgp) Result pollToggleHackerLobbyCallback(
    Handle procHandle,
    u32 textSz,
    u32 roSz,
    u32 rwSz
)
{
    (void)textSz;
    (void)roSz;
    (void)rwSz;

    bool en = (*(u64*)privRoomGlobal == 0x26c7cca200dc715); // 'haxxorz' lobby hash
    if (en == cheatsActive) {
        g_ctgpPrivatePollSeen = true;
        g_ctgpPrivateVerifyOk = PLUGIN_ctgp_VerifyPrivateRoomPatch(en);
        CTGP_HOST__svcCloseHandle(procHandle); // remember every return requires handle close else leak and zombie process
        return 0;
    }

    // anti disconnect
    *(u32*)(wwDisconnAddr) = en ? 0xE320F000 : wwDisconnInstrOG[0]; // baseMk + 0x3623D8
    *(u32*)(wwDisconnAddr + 0x74) = en ? 0xE320F000 : wwDisconnInstrOG[1]; // baseMk + 0x36244C
    *(u32*)(wwDisconnAddr + 0x78) = en ? 0xE320F000 : wwDisconnInstrOG[2]; // baseMk + 0x362450

    // remove bots
    *(u32*)(botCountAddr) = en ? 0xE1A0300F : botCountInstrOG[0]; // mov r3, pc (pc is botCountAddr + 0x8 at this moment)
    *(u32*)(botCountAddr + 0x4) = en ? 0xE51FF004 : botCountInstrOG[1];
    *(u32*)(botCountAddr + 0x8) = en ? (u32)PLUGIN_ctgp_Phys(botCountHook) : botCountInstrOG[2];

    // max timer 15 min instead of 5 min
    *(u32*)(timer5minAddr) = en ? 0x0000E0D3 : 0x00004740;

    cheatsActive = en;
    g_ctgpPrivatePollSeen = true;
    g_ctgpPrivateVerifyOk = PLUGIN_ctgp_VerifyPrivateRoomPatch(en);

    CTGP_HOST__svcFlushEntireDataCache();
    CTGP_HOST__svcInvalidateEntireInstructionCache();
    CTGP_HOST__svcCloseHandle(procHandle);
    return 0;
}

PLUGIN_BSS(ctgp) static u32 slotVal, controlVal;
PLUGIN_DATA(ctgp) static bool waitingForMii = true;
PLUGIN_DATA(ctgp) static bool lostControl = false;
PLUGIN_CODE_FROZEN(ctgp) Result swapMimFiles() {
    // read value stored by hook
    slotVal = *(u32*)charSelectPointer;
    controlVal = *(u32*)loseControlPointer;

    // race end detection
    if (controlVal == 1 && !lostControl)
    {
        lostControl = true;
        waitingForMii = true;
        *(u32*)loseControlPointer = 0;
        controlVal = 0;
    }
    else if (controlVal == 0 && lostControl)
    {
        lostControl = false;
    }


    // logic for mii variant swap
    if ((slotVal != 9 && !lostControl) || (slotVal == 9 && lostControl)) {
        if (waitingForMii)
            waitingForMii = false;
        else {
            return 0;
        }
    }
    else {
        waitingForMii = true;
        return 0;
    }

    FS_Path mim = CTGP_HOST__fsMakePath(PATH_ASCII, g_ctgpMimPath);
    FS_Path mimTemp = CTGP_HOST__fsMakePath(PATH_ASCII, g_ctgpMimTempPath);

    char path[48];
    u64 tick = CTGP_HOST__svcGetSystemTick();
    union { u64 value; u32 word[2]; } tickParts = { tick };
    u32 div = (u32)(mimCount - 1);
    u32 rem = 0;
    u32 hi = tickParts.word[1];
    u32 lo = tickParts.word[0];
    for (u32 bit = 0; bit < 32; bit++) { rem = (rem << 1) | (hi >> 31); hi <<= 1; if (rem >= div) rem -= div; }
    for (u32 bit = 0; bit < 32; bit++) { rem = (rem << 1) | (lo >> 31); lo <<= 1; if (rem >= div) rem -= div; }
    u32 r = 2 + rem;
    CTGP_HOST__sprintf(path, g_ctgpMimFmtU, (unsigned int)r);
    FS_Path mimSwap = CTGP_HOST__fsMakePath(PATH_ASCII, path);

    CTGP_HOST__FSUSER_OpenArchive(&g_ctgpSd, ARCHIVE_SDMC, CTGP_HOST__fsMakePath(PATH_EMPTY, NULL));
    Result res;
    CTGP_HOST__FSUSER_RenameFile(g_ctgpSd, mimSwap, g_ctgpSd, mimTemp);
    CTGP_HOST__FSUSER_RenameFile(g_ctgpSd, mim, g_ctgpSd, mimSwap);
    res = CTGP_HOST__FSUSER_RenameFile(g_ctgpSd, mimTemp, g_ctgpSd, mim);
    if (R_FAILED(res)) {
        countMimFiles(); // fixes temp
    }
    CTGP_HOST__FSUSER_CloseArchive(g_ctgpSd);
    return 0;
}

PLUGIN_DATA(ctgp) static float shRed = 0.4f;
PLUGIN_DATA(ctgp) static float shGreen = 1.0f;
PLUGIN_DATA(ctgp) static float shBlue = 0.0f;
PLUGIN_DATA(ctgp) static float spRed = 0.0f;
PLUGIN_DATA(ctgp) static float spGreen = 0.4f;
PLUGIN_DATA(ctgp) static float spBlue = 1.0f;
PLUGIN_DATA(ctgp) static bool shActiveLastFrame = false;
PLUGIN_DATA(ctgp) static float colSpeed = 0.01f;
PLUGIN_DATA(ctgp) static int colPhase = 2; // start finishing red->green
PLUGIN_DATA(ctgp) static float colSpeedSpiny = 0.1f;
PLUGIN_DATA(ctgp) static int colPhaseSpiny = 0;
PLUGIN_DATA(ctgp) static int colDirSpiny = 1; // 1 forward, -1 backward
PLUGIN_RODATA(ctgp) static const float maxG = 0.6f;

PLUGIN_CODE_FROZEN(ctgp) Result calcRGBShells() {
    float* shActive = (float*)shActivePointer;
    bool shellThisFrame = (*shActive > 0.f);
    if (shellThisFrame) {
        *shActive = 0.f;
    }
    bool updateCol = shellThisFrame || shActiveLastFrame;
    shActiveLastFrame = shellThisFrame; // 1 frame grace window

    if (updateCol) {
        // red/green shell anim
        if (colPhase == 0) {
            // phase: green -> blue
            if (shBlue < 1.f) {
                shBlue += colSpeed;
                if (shBlue > 1.f) shBlue = 1.f;
            }
            else if (shGreen > 0.f) {
                shGreen -= colSpeed;
                if (shGreen < 0.f) shGreen = 0.f;
            }
            else {
                colPhase = 1;
            }
        }
        else if (colPhase == 1) {
            // phase: blue -> red
            if (shRed < 1.f) {
                shRed += colSpeed;
                if (shRed > 1.f) shRed = 1.f;
            }
            else if (shBlue > 0.f) {
                shBlue -= colSpeed;
                if (shBlue < 0.f) shBlue = 0.f;
            }
            else {
                colPhase = 2;
            }
        }
        else {
            // phase: red -> green
            if (shGreen < 1.f) {
                shGreen += colSpeed;
                if (shGreen > 1.f) shGreen = 1.f;
            }
            else if (shRed > 0.f) {
                shRed -= colSpeed;
                if (shRed < 0.f) shRed = 0.f;
            }
            else {
                colPhase = 0;
            }
        }

        // spiny anim
        if (colDirSpiny > 0) {
            // forward direction
            if (colPhaseSpiny == 0) {
                // g maxG -> 0
                if (spGreen > 0.f) {
                    spGreen -= colSpeedSpiny;
                    if (spGreen < 0.f) spGreen = 0.f;
                }
                else colPhaseSpiny = 1;
            }
            else if (colPhaseSpiny == 1) {
                // r 0 -> 1
                if (spRed < 1.f) {
                    spRed += colSpeedSpiny;
                    if (spRed > 1.f) spRed = 1.f;
                }
                else colPhaseSpiny = 2;
            }
            else if (colPhaseSpiny == 2) {
                // b 1 -> 0
                if (spBlue > 0.f) {
                    spBlue -= colSpeedSpiny;
                    if (spBlue < 0.f) spBlue = 0.f;
                }
                else colPhaseSpiny = 3;
            }
            else {
                // g 0 -> (maxG/3)*2 (on red side so we dont get yellow)
                if (spGreen < (maxG / 3) * 2) {
                    spGreen += colSpeedSpiny;
                    if (spGreen > (maxG / 3) * 2) spGreen = (maxG / 3) * 2;
                }
                else {
                    colPhaseSpiny = 2;
                    colDirSpiny = -1;
                }
            }
        }
        else {
            // backward direction
            if (colPhaseSpiny == 2) {
                // g halfG -> 0
                if (spGreen > 0.f) {
                    spGreen -= colSpeedSpiny;
                    if (spGreen < 0.f) spGreen = 0.f;
                }
                else colPhaseSpiny = 1;
            }
            else if (colPhaseSpiny == 1) {
                // b 0 -> 1
                if (spBlue < 1.f) {
                    spBlue += colSpeedSpiny;
                    if (spBlue > 1.f) spBlue = 1.f;
                }
                else colPhaseSpiny = 0;
            }
            else if (colPhaseSpiny == 0) {
                // r 1 -> 0
                if (spRed > 0.f) {
                    spRed -= colSpeedSpiny;
                    if (spRed < 0.f) spRed = 0.f;
                }
                else colPhaseSpiny = 3;
            }
            else {
                // g 0 -> maxG (blue == 1 side)
                if (spGreen < maxG) {
                    spGreen += colSpeedSpiny;
                    if (spGreen > maxG) spGreen = maxG;
                }
                else {
                    colPhaseSpiny = 0;
                    colDirSpiny = 1;
                }
            }
        }

        // clamp safety
        if (shRed < 0.f) shRed = 0.f;
        if (shGreen < 0.f) shGreen = 0.f;
        if (shBlue < 0.f) shBlue = 0.f;
        if (shRed > 1.f) shRed = 1.f;
        if (shGreen > 1.f) shGreen = 1.f;
        if (shBlue > 1.f) shBlue = 1.f;
        if (spRed < 0.f) spRed = 0.f;
        if (spGreen < 0.f) spGreen = 0.f;
        if (spBlue < 0.f) spBlue = 0.f;
        if (spRed > 1.f) spRed = 1.f;
        if (spGreen > maxG) spGreen = maxG;
        if (spBlue > 1.f) spBlue = 1.f;

        // write to game
        *(float*)shRedPointer = shRed;
        *(float*)shGreenPointer = shGreen;
        *(float*)shBluePointer = shBlue;
        *(float*)spRedPointer = spRed;
        *(float*)spGreenPointer = spGreen;
        *(float*)spBluePointer = spBlue;
    }
    return 0;
}

PLUGIN_DATA(ctgp) static Handle proc = 0;

PLUGIN_DATA(ctgp) bool mk7Loop = false;
PLUGIN_DATA(ctgp) bool ctgpStartLoop = false;
PLUGIN_DATA(ctgp) bool patchedMK7Hook = false;
PLUGIN_DATA(ctgp) bool ctgpWaitLoadPostLauncher = false;

PLUGIN_CODE_FROZEN(ctgp) static void PLUGIN_ctgp_UnregisterTick(void);

PLUGIN_CODE_FROZEN(ctgp) static void PLUGIN_ctgp_CleanupTransientStartupHooks(void)
{
    bool changed = false;
    u32 state = g_ctgpHookAddrs[7];

    if (state == 0x10u && g_ctgpRelocTrigger[0] &&
        PLUGIN_ctgp_IsWritableAddress(g_ctgpRelocTrigger[0]))
    {
        volatile u32 *trigger = (volatile u32*)g_ctgpRelocTrigger[0];
        if (trigger[0] == 0xE51FF004u && trigger[1] == PLUGIN_ctgp_Phys(ctgpDumpArmHook))
        {
            trigger[0] = g_ctgpRelocTrigger[1]; trigger[1] = g_ctgpRelocTrigger[2];
            changed = true;
        }
    }
    else if ((state == 0x11u || state == 0x12u) && g_ctgpHookAddrs[0] &&
             PLUGIN_ctgp_IsWritableAddress(g_ctgpHookAddrs[0]))
    {
        volatile u32 *site = (volatile u32*)g_ctgpHookAddrs[0];
        if (site[0] == 0xE51FF004u && site[1] == PLUGIN_ctgp_Phys(ctgpDumpWaitHook))
        {
            site[0] = g_ctgpHookAddrs[3];
            site[1] = g_ctgpHookAddrs[4];
            changed = true;
        }
        u32 count = g_ctgpRelocCopies[0];
        if (count <= 8u)
            for (u32 i = 0; i < count; i++)
            {
                u32 address = g_ctgpRelocCopies[1u + i * 2u];
                if (!PLUGIN_ctgp_IsWritableAddress(address)) continue;
                volatile u32 *copy = (volatile u32*)address;
                if (copy[0] != 0xE51FF004u || copy[1] != PLUGIN_ctgp_Phys(ctgpDumpWaitHook)) continue;
                copy[0] = g_ctgpHookAddrs[3];
                copy[1] = g_ctgpRelocCopies[2u + i * 2u];
                changed = true;
            }
    }
    else if (state == 0x20u && g_ctgpHookAddrs[0] && PLUGIN_ctgp_IsWritableAddress(g_ctgpHookAddrs[0]))
    {
        volatile u32 *site = (volatile u32*)g_ctgpHookAddrs[0];
        if (site[0] == 0xE51FF004u && site[1] == PLUGIN_ctgp_Phys(ctgpRestoreWaitHook))
        {
            site[0] = g_ctgpHookAddrs[5];
            site[1] = g_ctgpHookAddrs[6];
            changed = true;
        }
    }

    if (changed)
    {
        CTGP_HOST__svcFlushEntireDataCache();
        CTGP_HOST__svcInvalidateEntireInstructionCache();
    }
}

PLUGIN_CODE_FROZEN(ctgp) void ResetCtgpState(void)
{
    isPlgCTGP7 = false;
    g_myHookVal[0] = 0;
    proc = 0;
    elapsed10sMK7 = 0;
    elapsed1frameMK7 = 0;
    elapsed1frameCTGPStartup = 0;
    elapsed20sTimeout = 0;
    g_ctgpTextHostAddr[0] = 0;
    g_ctgpTextHostAddr[1] = 0;
    g_ctgpToxtTargetFound = false;
    g_ctgpToxtHookInstalled = false;
    g_ctgpToxtRedirected = false;
    g_ctgpToxtOrigWord0 = 0;
    g_ctgpToxtOrigWord1 = 0;
    g_ctgpToxtOrigValid = false;
    g_ctgpToxtScanPending = false;
    g_ctgpToxtScanElapsed = 0;
    g_ctgpToxtFullScanElapsed = 0;
    g_toxtDiagStage = 0;
    g_toxtDiagFail = 0;
    g_toxtDiagScanCalls = 0;
    g_toxtDiagCtx = 0;
    g_toxtDiagHostBase = 0;
    g_toxtDiagImageSize = 0;
    g_toxtDiagMemSize = 0;
    g_toxtDiagSig0 = g_toxtDiagSig3 = g_toxtDiagSig11 = 0;
    g_toxtDiagLiteralRange = g_toxtDiagPtrRange = g_toxtDiagTextExact = 0;
    g_toxtDiagMatches = 0;
    g_toxtDiagScanResult = 0;
    g_toxtDiagPatchResult = 0;
    g_toxtDiagArchiveResult = 0;
    g_toxtDiagLangOpenResult = 0;
    g_toxtDiagLangReadResult = 0;
    g_toxtDiagLangReadBytes = 0;
    g_toxtDiagToxtExists = 0;
    g_toxtDiagHookWord0 = g_toxtDiagHookWord1 = 0;
    g_toxtDiagProbeWord = 0;
    g_toxtDiagQuickTriggers = 0;
    g_toxtDiagPendingTicks = 0;
    g_ctgpLegacyPatchAddr0 = 0;
    g_ctgpLegacyPatchAddr1 = 0;
    g_ctgpLegacyPatch0Applied = false;
    g_ctgpLegacyPatch1Applied = false;
    g_privTextDiagHits = 0;
    g_privTextDiagR0 = 0;
    g_privTextDiagPath = 0;
    g_privTextDiagLang = 0;
    g_langWord = 0;
    ctgpStartLoop = false;
    mk7Loop = false;
    patchedMK7Hook = false;
}

PLUGIN_CODE_FROZEN(ctgp) void QuitCtgpLoop(void)
{
    PLUGIN_ctgp_CleanupTransientStartupHooks();
    ResetCtgpState();
    for (u32 i = 0; i < 8; i++) g_ctgpHookAddrs[i] = 0;
    g_CtgpMasterLoopActive = false;
    PLUGIN_ctgp_UnregisterTick();
}
PLUGIN_RODATA(ctgp) static const char g_ctgpOptionsTitle[] = "CTGP-7 Options";
PLUGIN_RODATA(ctgp) static const char g_ctgpToggleTitle[] = "Toggle CTGP-7 options";
PLUGIN_RODATA(ctgp) static const char g_ctgpMiiFitTitle[] = "Download Blurro's Mii Fit";
PLUGIN_RODATA(ctgp) static const char g_ctgpMiiFitPageTitle[] = "Download Mii Fit";
PLUGIN_RODATA(ctgp) static const char g_ctgpMiiFitBody[] =
    "When PATCHES are enabled, this will cycle\n"
    "between every 'mimX.bcmdl' file present on the\n"
    "SD card at /CTGP-7/gamefs/Driver/mim/\n"
    "\n"
    "Here you can download my custom Mii drip, and\n"
    "you will see it change slightly every time you\n"
    "reselect the Mii (male only currently)\n"
    "\n"
    "You can preview it on GameBanana.";
PLUGIN_RODATA(ctgp) static const char g_ctgpMiiFitPressA[] = "Press A to download";
PLUGIN_RODATA(ctgp) static const char g_ctgpMiiFitUrl[] = "https://blurro.github.io/sysplugins/online_mii/tempmiidownload.3on";
PLUGIN_RODATA(ctgp) static const char g_ctgpDump3gxTitle[] = "Dump 3GX memory";
PLUGIN_RODATA(ctgp) static const char g_ctgpDumpMk7Title[] = "Dump MK7 memory";
PLUGIN_RODATA(ctgp) static const char g_ctgpRestoreTitle[] = "Restore Snapshots";
PLUGIN_RODATA(ctgp) static const char g_ctgpDebugTitle[] = "CTGP-7 Debug";
PLUGIN_RODATA(ctgp) static const char g_ctgpLauncherHealthFmt[] = "Launcher patch: %s scan:%X wr:%s";
PLUGIN_RODATA(ctgp) static const char g_ctgpEntryHealthFmt[] = "Checksum:%lu IV:%08X";
PLUGIN_RODATA(ctgp) static const char g_ctgpToxtHealthFmt[] = "Toxt target:%s hook:%s hit:%u redir:%s";
PLUGIN_RODATA(ctgp) static const char g_ctgpFreezeHealthFmt[] = "Freeze %s st:%02X wait:%08X";
PLUGIN_RODATA(ctgp) static const char g_ctgpFreezeNone[] = "NONE";
PLUGIN_RODATA(ctgp) static const char g_ctgpFreezeDump[] = "DUMP";
PLUGIN_RODATA(ctgp) static const char g_ctgpFreezeRestore[] = "REST";
PLUGIN_RODATA(ctgp) static const char g_ctgpCoreHealthFmt[] = "Core char:%s ctrl:%s shell:%s spiny:%s";
PLUGIN_RODATA(ctgp) static const char g_ctgpPrivScanFmt[] = "Priv ww:%s bot:%s timer:%s room:%s";
PLUGIN_RODATA(ctgp) static const char g_ctgpCtgpScanFmt[] = "CTGP cave:%s menu:%s";
PLUGIN_RODATA(ctgp) static const char g_ctgpWriteHealthFmt[] = "Writes %03X/%03X patch:%s";
PLUGIN_RODATA(ctgp) static const char g_ctgpPrivateHealthFmt[] = "Private active:%s verify:%s";
PLUGIN_RODATA(ctgp) static const char g_ctgpMimCountFmt[] = "Mii files: %d";
PLUGIN_RODATA(ctgp) static const char g_ctgpHealthFmt[] = "Patch health: %s";
PLUGIN_RODATA(ctgp) static const char g_ctgpSkipVanilla[] = "Feature patches: SKIP (VANILLA)";
PLUGIN_RODATA(ctgp) static const char g_ctgpY[] = "Y";
PLUGIN_RODATA(ctgp) static const char g_ctgpN[] = "N";
PLUGIN_RODATA(ctgp) static const char g_ctgpWait[] = "WAIT";
PLUGIN_RODATA(ctgp) static const char g_ctgpHealthWaitingCtgp[] = "WAITING FOR CTGP";
PLUGIN_RODATA(ctgp) static const char g_ctgpHealthWaitingMk[] = "WAITING FOR MK7";
PLUGIN_RODATA(ctgp) static const char g_ctgpHealthHostFail[] = "HOST HOOK FAIL";
PLUGIN_RODATA(ctgp) static const char g_ctgpHealthLauncherFail[] = "LAUNCHER PATCH FAIL";
PLUGIN_RODATA(ctgp) static const char g_ctgpHealthEntryFail[] = "CTGP CHECKSUM FAIL";
PLUGIN_RODATA(ctgp) static const char g_ctgpHealthToxtFail[] = "TOXT PATCH FAIL";
PLUGIN_RODATA(ctgp) static const char g_ctgpHealthFreezeFail[] = "DUMP/RESTORE FREEZE FAIL";
PLUGIN_RODATA(ctgp) static const char g_ctgpHealthMkFail[] = "MK7 PATCH FAIL";
PLUGIN_RODATA(ctgp) static const char g_ctgpHealthPrivFail[] = "PRIVATE VERIFY FAIL";
PLUGIN_RODATA(ctgp) static const char g_ctgpHealthVanillaOk[] = "VANILLA OK";
PLUGIN_RODATA(ctgp) static const char g_ctgpHealthAllOk[] = "ALL PATCHES OK";
PLUGIN_RODATA(ctgp) static const char g_ctgpCurrentVanilla[] = "Current: VANILLA";
PLUGIN_RODATA(ctgp) static const char g_ctgpCurrentPatches[] = "Current: PATCHES";
PLUGIN_RODATA(ctgp) static const char g_ctgpDumpRestoreNone[] = "Dump/Restore: NONE";
PLUGIN_RODATA(ctgp) static const char g_ctgpDumpRestoreDump[] = "Dump/Restore: DUMP";
PLUGIN_RODATA(ctgp) static const char g_ctgpDumpRestoreRestore[] = "Dump/Restore: RESTORE";
PLUGIN_RODATA(ctgp) static const char g_ctgpCycleDumpRestore[] = "Press Y to cycle Dump/Restore.";
PLUGIN_RODATA(ctgp) static const char g_ctgpTogglePatchMode[] = "Press X to toggle Patches/Vanilla.";
PLUGIN_RODATA(ctgp) static const char g_ctgpAzaharOn[] = "Toggle Azahar Servers: ON";
PLUGIN_RODATA(ctgp) static const char g_ctgpAzaharOff[] = "Toggle Azahar Servers: OFF";
PLUGIN_RODATA(ctgp) static const char g_ctgpToggleAzahar[] = "Press A to toggle Azahar Servers.";
PLUGIN_RODATA(ctgp) static const char g_ctgpBack[] = "Press B to go back.";
PLUGIN_RODATA(ctgp) static const char g_ctgpRestoreReady[] = "Restore ready!";
PLUGIN_RODATA(ctgp) static const char g_ctgpDumpReady[] = "Dump ready!";
PLUGIN_RODATA(ctgp) static const char g_ctgpDump3gxHeader[] = "Dump full 3GX runtime memory";
PLUGIN_RODATA(ctgp) static const char g_ctgpDump3gxBaseFmt[] = "Base: 0x07000100\nSize: %lu bytes";
PLUGIN_RODATA(ctgp) static const char g_ctgpPressDump[] = "Press A to dump.";
PLUGIN_RODATA(ctgp) static const char g_ctgpDump3gxOk[] = "Dump successful: snapshot3gx.bin\n(existing old is renamed to dump_runtime)";
PLUGIN_RODATA(ctgp) static const char g_ctgpDumpMk7Header[] = "Dump MK7 memory";
PLUGIN_RODATA(ctgp) static const char g_ctgpDumpMk7Base[] = "Base: 0x00100000";
PLUGIN_RODATA(ctgp) static const char g_ctgpDumpMk7Ok[] = "Dump successful: snapshot.bin\n(existing old is renamed to dump_mariokar)";
PLUGIN_RODATA(ctgp) static const char g_ctgpWorking[] = "Working...";
PLUGIN_RODATA(ctgp) static const char g_ctgpErrorFmt[] = "Error: 0x%08lx";
PLUGIN_RODATA(ctgp) static const char g_ctgpRestoreHeader[] = "Restore MK7 + 3GX snapshots";
PLUGIN_RODATA(ctgp) static const char g_ctgpRestoreMk[] = "snapshot.bin -> MK7";
PLUGIN_RODATA(ctgp) static const char g_ctgpRestore3gx[] = "snapshot3gx.bin -> 3GX";
PLUGIN_RODATA(ctgp) static const char g_ctgpPressRestore[] = "Press A to restore.";
PLUGIN_RODATA(ctgp) static const char g_ctgpRestoreMkSkipped[] = "MK7 not restored";
PLUGIN_RODATA(ctgp) static const char g_ctgpRestoreMkOk[] = "MK7 restore OK";
PLUGIN_RODATA(ctgp) static const char g_ctgpRestore3gxOk[] = "3GX restore OK";
PLUGIN_RODATA(ctgp) static const char g_ctgpRestoreMkErr[] = "MK7 err: 0x%08lx";
PLUGIN_RODATA(ctgp) static const char g_ctgpRestore3gxErr[] = "3GX err: 0x%08lx";
PLUGIN_RODATA(ctgp) static const char g_ctgpSnapshot3gx[] = "/luma/dumps/memory/snapshot3gx.bin";
PLUGIN_RODATA(ctgp) static const char g_ctgpSnapshotMk[] = "/luma/dumps/memory/snapshot.bin";
PLUGIN_RODATA(ctgp) static const char g_ctgpDumpRuntimeFmt[] = "/luma/dumps/memory/dump_runtime%lu.bin";
PLUGIN_RODATA(ctgp) static const char g_ctgpDumpMkFmt[] = "/luma/dumps/memory/dump_mariokar%lu.bin";
PLUGIN_RODATA(ctgp) static const char g_ctgpLumaDir[] = "/luma";
PLUGIN_RODATA(ctgp) static const char g_ctgpDumpsDir[] = "/luma/dumps";
PLUGIN_RODATA(ctgp) static const char g_ctgpMemoryDir[] = "/luma/dumps/memory";
PLUGIN_RODATA(ctgp) static const char g_ctgpIntervalFast[] = "Interval: 1 ms";
PLUGIN_RODATA(ctgp) static const char g_ctgpIntervalArmed[] = "Interval: 250 ms";
PLUGIN_RODATA(ctgp) static const char g_ctgpIntervalOff[] = "Interval: off";
PLUGIN_RODATA(ctgp) static const char g_ctgpLauncherDetectedFmt[] = "CTGP-7 launcher detected: %s";
PLUGIN_RODATA(ctgp) static const char g_ctgpLauncherPatchedFmt[] = "CTGP-7 launcher patched: %s";
PLUGIN_RODATA(ctgp) static const char g_ctgpMkEnteredFmt[] = "MK7 game entered: %s";
PLUGIN_RODATA(ctgp) static const char g_ctgpHostHooksFmt[] = "Host hooks installed: %s";
PLUGIN_RODATA(ctgp) static const char g_ctgpPasswordFmt[] = "Console password: %s";
PLUGIN_RODATA(ctgp) static const char g_ctgpPasswordUnavailable[] = "unavailable";
PLUGIN_RODATA(ctgp) static const char g_ctgpToxtTargetFmt[] = "Toxt target/hook: %s/%s";
PLUGIN_RODATA(ctgp) static const char g_ctgpToxtRedirectFmt[] = "Toxt redirected: %s";
PLUGIN_RODATA(ctgp) static const char g_ctgpHexAlphabet[] = "0123456789ABCDEF";
PLUGIN_RODATA(ctgp) static const char g_ctgpYes[] = "YES";
PLUGIN_RODATA(ctgp) static const char g_ctgpNo[] = "NO";
PLUGIN_RODATA(ctgp) static const char g_ctgpFrameCorner[] = "+";
PLUGIN_RODATA(ctgp) static const char g_ctgpFrameSide[] = "|";
PLUGIN_RODATA(ctgp) static const char g_ctgpFrameRail[] = "----------------------";
PLUGIN_RODATA(ctgp) static const char g_ctgpSelectedFmt[] = "> %s";
PLUGIN_RODATA(ctgp) static const char g_ctgpUnselectedFmt[] = "  %s";
PLUGIN_RODATA(ctgp) static const char g_ctgpClearRow[] = "                                                ";

PLUGIN_BSS(ctgp) static PluginMenuRegistration g_ctgpMenuRegistration;
PLUGIN_BSS(ctgp) static BlurFeatureRegistration g_ctgpDebugRegistration;
PLUGIN_DATA(ctgp) static s64 g_ctgpCurrentIntervalNs = 0;
PLUGIN_DATA(ctgp) static bool g_ctgpTickRegistered = false;
PLUGIN_DATA(ctgp) static volatile u32 g_ctgpWakeGeneration = 0;
PLUGIN_DATA(ctgp) static u32 g_ctgpHandledGeneration = 0;
PLUGIN_DATA(ctgp) static bool g_ctgpLauncherDetected = false;
PLUGIN_DATA(ctgp) static bool g_ctgpLauncherPatched = false;
PLUGIN_DATA(ctgp) static bool g_ctgpMk7Entered = false;
PLUGIN_DATA(ctgp) static bool g_ctgpHostHooksInstalled = false;
PLUGIN_DATA(ctgp) static bool g_ctgpAlive = false;
PLUGIN_DATA(ctgp) static bool g_ctgpWaitLauncherAppear = false;
PLUGIN_DATA(ctgp) static bool g_ctgpMkAlive = false;
PLUGIN_DATA(ctgp) static u32 g_ctgpPid = 0;
PLUGIN_DATA(ctgp) static u32 g_ctgpMkPid = 0;
PLUGIN_DATA(ctgp) static u64 g_ctgpElapsed250ms = 0;
PLUGIN_DATA(ctgp) static u32 g_ctgpConsolePasswordAddr = 0;
PLUGIN_BSS(ctgp) static bool g_ctgpAzaharEnabled;
PLUGIN_BSS(ctgp) static bool g_ctgpAzaharRuntimeActive;
PLUGIN_BSS(ctgp) static bool g_ctgpAzaharRuntimeSeen;
PLUGIN_BSS(ctgp) static bool g_ctgpDumpRestoreUnlocked;
PLUGIN_BSS(ctgp) static u32 g_ctgpAzaharFail;
PLUGIN_BSS(ctgp) static volatile u32 g_ctgpAzaharSites[5];
PLUGIN_BSS(ctgp) static volatile u32 g_ctgpAzaharOriginal[8];

#define CTGP_FRAME_COLOR COLOR_WHITE
#define CTGP_TITLE_COLOR RGB565(31, 0, 0)
#define CTGP_ROOT_COLOR RGB565(31, 23, 11)

PLUGIN_CODE_FROZEN(ctgp) static void PLUGIN_ctgp_DrawFrame(const char *title)
{
    CTGP_HOST__Draw_DrawString(10, 8, CTGP_FRAME_COLOR, g_ctgpFrameCorner);
    CTGP_HOST__Draw_DrawString(148, 8, CTGP_FRAME_COLOR, g_ctgpFrameCorner);
    CTGP_HOST__Draw_DrawString(10, 24, CTGP_FRAME_COLOR, g_ctgpFrameCorner);
    CTGP_HOST__Draw_DrawString(148, 24, CTGP_FRAME_COLOR, g_ctgpFrameCorner);
    CTGP_HOST__Draw_DrawString(16, 8, CTGP_FRAME_COLOR, g_ctgpFrameRail);
    CTGP_HOST__Draw_DrawString(16, 24, CTGP_FRAME_COLOR, g_ctgpFrameRail);
    CTGP_HOST__Draw_DrawString(10, 16, CTGP_FRAME_COLOR, g_ctgpFrameSide);
    CTGP_HOST__Draw_DrawString(148, 16, CTGP_FRAME_COLOR, g_ctgpFrameSide);
    CTGP_HOST__Draw_DrawString(20, 16, CTGP_TITLE_COLOR, title);
}

PLUGIN_CODE_FROZEN(ctgp) static void PLUGIN_ctgp_DrawFullPage(const char *title)
{
    CTGP_HOST__Draw_Lock();
    CTGP_HOST__Draw_ClearFramebuffer();
    PLUGIN_ctgp_DrawFrame(title);
}

PLUGIN_CODE_FROZEN(ctgp) static void PLUGIN_ctgp_EndFullPage(void)
{
    CTGP_HOST__Draw_FlushFramebuffer();
    CTGP_HOST__Draw_Unlock();
}

PLUGIN_CODE_FROZEN(ctgp) static Result PLUGIN_ctgp_DumpMarioKart7Callback(Handle procHandle, u32 textSz, u32 roSz, u32 rwSz)
{
    Result res = 0;
    u32 base = 0x00100000;
    u32 size = textSz + roSz + rwSz;
    FS_Archive archive;
    Handle f;
    char path[128];
    u32 index = 0;

    if (R_SUCCEEDED(res = CTGP_HOST__FSUSER_OpenArchive(&archive, ARCHIVE_SDMC, CTGP_HOST__fsMakePath(PATH_EMPTY, NULL))))
    {
        CTGP_HOST__FSUSER_CreateDirectory(archive, CTGP_HOST__fsMakePath(PATH_ASCII, g_ctgpLumaDir), 0);
        CTGP_HOST__FSUSER_CreateDirectory(archive, CTGP_HOST__fsMakePath(PATH_ASCII, g_ctgpDumpsDir), 0);
        CTGP_HOST__FSUSER_CreateDirectory(archive, CTGP_HOST__fsMakePath(PATH_ASCII, g_ctgpMemoryDir), 0);
        if (R_SUCCEEDED(CTGP_HOST__FSUSER_OpenFile(&f, archive, CTGP_HOST__fsMakePath(PATH_ASCII, g_ctgpSnapshotMk), FS_OPEN_READ, 0)))
        {
            CTGP_HOST__FSFILE_Close(f);
            for (index = 0; index < 1000; index++)
            {
                CTGP_HOST__sprintf(path, g_ctgpDumpMkFmt, index);
                if (R_FAILED(CTGP_HOST__FSUSER_OpenFile(&f, archive, CTGP_HOST__fsMakePath(PATH_ASCII, path), FS_OPEN_READ, 0))) break;
                CTGP_HOST__FSFILE_Close(f);
            }
            CTGP_HOST__FSUSER_RenameFile(archive, CTGP_HOST__fsMakePath(PATH_ASCII, g_ctgpSnapshotMk), archive, CTGP_HOST__fsMakePath(PATH_ASCII, path));
        }
        if (R_SUCCEEDED(res = CTGP_HOST__FSUSER_OpenFile(&f, archive, CTGP_HOST__fsMakePath(PATH_ASCII, g_ctgpSnapshotMk), FS_OPEN_WRITE | FS_OPEN_CREATE, 0)))
        {
            u32 written = 0;
            res = CTGP_HOST__FSFILE_Write(f, &written, 0, (void*)base, size, FS_WRITE_FLUSH);
            CTGP_HOST__FSFILE_Close(f);
            if (R_SUCCEEDED(res) && written != size) res = (Result)-0x225;
        }
        CTGP_HOST__FSUSER_CloseArchive(archive);
    }
    CTGP_HOST__svcCloseHandle(procHandle);
    return res;
}

PLUGIN_CODE_FROZEN(ctgp) static Result PLUGIN_ctgp_RestoreMarioKart7Callback(Handle procHandle, u32 textSz, u32 roSz, u32 rwSz)
{
    Result res = 0;
    u32 size = textSz + roSz + rwSz;
    FS_Archive archive;
    Handle f;
    u32 read = 0;
    if (R_SUCCEEDED(res = CTGP_HOST__FSUSER_OpenArchive(&archive, ARCHIVE_SDMC, CTGP_HOST__fsMakePath(PATH_EMPTY, NULL))))
    {
        if (R_SUCCEEDED(res = CTGP_HOST__FSUSER_OpenFile(&f, archive, CTGP_HOST__fsMakePath(PATH_ASCII, g_ctgpSnapshotMk), FS_OPEN_READ, 0)))
        {
            u64 fileSize = 0;
            res = CTGP_HOST__FSFILE_GetSize(f,&fileSize);
            if (R_SUCCEEDED(res) && fileSize != size) res = (Result)-0x224;
            if (R_SUCCEEDED(res)) res = CTGP_HOST__FSFILE_Read(f,&read,0,(void*)0x00100000,size);
            CTGP_HOST__FSFILE_Close(f);
            if (R_SUCCEEDED(res) && read != size) res = (Result)-0x224;
        }
        CTGP_HOST__FSUSER_CloseArchive(archive);
        CTGP_HOST__svcFlushEntireDataCache();
        CTGP_HOST__svcInvalidateEntireInstructionCache();
    }
    CTGP_HOST__svcCloseHandle(procHandle);
    return res;
}

PLUGIN_CODE_FROZEN(ctgp) static void PLUGIN_ctgp_SaveSettings(void)
{
    if (!CTGP_MENU__SaveData) return;
    CtgpSettings settings;
    settings.version = CTGP_SETTINGS_VERSION;
    settings.patchMode = g_vanillaCTGP7 ? 1u : 0u;
    (void)CTGP_MENU__SaveData(CTGP_PLUGIN_ID, &settings, sizeof(settings));
}

PLUGIN_CODE_FROZEN(ctgp) static void PLUGIN_ctgp_LoadSettings(void)
{
    // Dump/restore are intentionally session-only. Every boot begins at NONE.
    g_dumpCTGP7 = false;
    g_restoreCTGP7 = false;
    g_vanillaCTGP7 = false;
    if (!CTGP_MENU__LoadData) return;

    CtgpSettings settings;
    if (!CTGP_MENU__LoadData(CTGP_PLUGIN_ID, &settings, sizeof(settings))) return;

    if (settings.version == CTGP_SETTINGS_VERSION && settings.patchMode <= 1u)
        g_vanillaCTGP7 = settings.patchMode == 1u;
    else if (settings.version == 1u && settings.patchMode <= 3u)
        // Migrate the old combined selector: only VANILLA survives; old DUMP/RESTORE do not.
        g_vanillaCTGP7 = settings.patchMode == 1u;
}

PLUGIN_CODE_FROZEN(ctgp) static void PLUGIN_ctgp_DrawToggleOptions(void)
{
    const char *dumpRestore = g_restoreCTGP7 ? g_ctgpDumpRestoreRestore :
                              g_dumpCTGP7 ? g_ctgpDumpRestoreDump : g_ctgpDumpRestoreNone;

    PLUGIN_ctgp_DrawFullPage(g_ctgpOptionsTitle);
    CTGP_HOST__Draw_DrawString(20, 45, COLOR_WHITE, g_vanillaCTGP7 ? g_ctgpCurrentVanilla : g_ctgpCurrentPatches);
    CTGP_HOST__Draw_DrawString(20, 70, COLOR_WHITE, dumpRestore);
    CTGP_HOST__Draw_DrawString(20, 105, COLOR_WHITE, g_ctgpCycleDumpRestore);
    CTGP_HOST__Draw_DrawString(20, 125, COLOR_WHITE, g_ctgpTogglePatchMode);
    CTGP_HOST__Draw_DrawString(20, 160, COLOR_GRAY, g_ctgpBack);
    PLUGIN_ctgp_EndFullPage();
}

PLUGIN_CODE_FROZEN(ctgp) static void PLUGIN_ctgp_ToggleOptions(void)
{
    PLUGIN_ctgp_DrawToggleOptions();
    for (;;)
    {
        u32 pressed = CTGP_HOST__waitInputWithTimeout(50);
        bool changed = false;

        if (pressed & KEY_Y)
        {
            if (!g_dumpCTGP7 && !g_restoreCTGP7)
                g_dumpCTGP7 = true;                     // NONE -> DUMP
            else if (g_dumpCTGP7)
            {
                g_dumpCTGP7 = false;
                g_restoreCTGP7 = true;                  // DUMP -> RESTORE
            }
            else
                g_restoreCTGP7 = false;                 // RESTORE -> NONE
            changed = true;
        }
        else if (pressed & KEY_X)
        {
            g_vanillaCTGP7 = !g_vanillaCTGP7;
            changed = true;
        }
        else if ((pressed & KEY_B) || CTGP_HOST__menuShouldExit)
        {
            // Persist only PATCHES/VANILLA. Dump/Restore always returns to NONE next boot.
            PLUGIN_ctgp_SaveSettings();
            return;
        }

        if (changed) PLUGIN_ctgp_DrawToggleOptions();
    }
}

typedef struct
{
    bool waitSiteCleaned;
    bool toxtHookCleaned;
    bool toxtStringCleaned;
} CtgpDumpCleanState;

PLUGIN_CODE_FROZEN(ctgp) static bool PLUGIN_ctgp_IsWritableAddress(u32 address)
{
    MemInfo mi;
    PageInfo pi;
    return R_SUCCEEDED(CTGP_HOST__svcQueryMemory(&mi, &pi, address)) &&
           mi.state != MEMSTATE_FREE && (mi.perm & MEMPERM_WRITE);
}

PLUGIN_CODE_FROZEN(ctgp) static void PLUGIN_ctgp_PrepareClean3gxDump(CtgpDumpCleanState *state)
{
    state->waitSiteCleaned = false;
    state->toxtHookCleaned = false;
    state->toxtStringCleaned = false;

    // DUMP freezes by replacing the late E8BDDFFF epilogue with a pointer into
    // this sysplugin. Never serialize that transient veneer into snapshot3gx.
    if (g_ctgpHookAddrs[7] == 0x12u && g_ctgpHookAddrs[0] &&
        PLUGIN_ctgp_IsWritableAddress(g_ctgpHookAddrs[0]))
    {
        volatile u32 *wait = (volatile u32*)g_ctgpHookAddrs[0];
        if (wait[0] == 0xE51FF004u && wait[1] == PLUGIN_ctgp_Phys(ctgpDumpWaitHook))
        {
            wait[0] = g_ctgpHookAddrs[3];
            wait[1] = g_ctgpHookAddrs[4];
            state->waitSiteCleaned = true;
        }
    }

    // Likewise, snapshot the original CTGP language-import instructions rather
    // than a physical pointer to privTextHook. Reinstall it for the live
    // session after the file is written.
    if (g_ctgpToxtHookInstalled && g_ctgpToxtOrigValid && g_ctgpTextStrAddr[1] &&
        PLUGIN_ctgp_IsWritableAddress(g_ctgpTextStrAddr[1]))
    {
        volatile u32 *hook = (volatile u32*)g_ctgpTextStrAddr[1];
        if (hook[0] == 0xE51FF004u && hook[1] == PLUGIN_ctgp_Phys(privTextHook))
        {
            hook[0] = g_ctgpToxtOrigWord0;
            hook[1] = g_ctgpToxtOrigWord1;
            state->toxtHookCleaned = true;
        }
    }

    if (g_ctgpToxtRedirected && g_ctgpTextStrAddr[0] &&
        PLUGIN_ctgp_IsWritableAddress(g_ctgpTextStrAddr[0]))
    {
        volatile char *text = (volatile char*)g_ctgpTextStrAddr[0];
        if (text[2] == 'o')
        {
            text[2] = 'e';
            state->toxtStringCleaned = true;
        }
    }

    CTGP_HOST__svcFlushEntireDataCache();
    CTGP_HOST__svcInvalidateEntireInstructionCache();
}

PLUGIN_CODE_FROZEN(ctgp) static void PLUGIN_ctgp_RestoreLiveAfter3gxDump(const CtgpDumpCleanState *state)
{
    // The active DUMP wait invocation is already executing in the sysplugin, so
    // leave its callsite clean. It will return through the original epilogue.
    // Only restore Toxt for the still-running CTGP session.
    if (state->toxtHookCleaned && g_ctgpTextStrAddr[1] &&
        PLUGIN_ctgp_IsWritableAddress(g_ctgpTextStrAddr[1]))
    {
        volatile u32 *hook = (volatile u32*)g_ctgpTextStrAddr[1];
        hook[0] = 0xE51FF004u;
        hook[1] = PLUGIN_ctgp_Phys(privTextHook);
    }
    if (state->toxtStringCleaned && g_ctgpTextStrAddr[0] &&
        PLUGIN_ctgp_IsWritableAddress(g_ctgpTextStrAddr[0]))
        ((volatile char*)g_ctgpTextStrAddr[0])[2] = 'o';

    CTGP_HOST__svcFlushEntireDataCache();
    CTGP_HOST__svcInvalidateEntireInstructionCache();
}

PLUGIN_CODE_FROZEN(ctgp) static void PLUGIN_ctgp_DrawDump3gx(Result res, bool show, bool working)
{
    PLUGIN_ctgp_DrawFullPage(g_ctgpOptionsTitle);
    CTGP_HOST__Draw_DrawString(20, 40, COLOR_WHITE, g_ctgpDump3gxHeader);
    CTGP_HOST__Draw_DrawFormattedString(20, 60, COLOR_WHITE, g_ctgpDump3gxBaseFmt, (unsigned long)(CTGP_HOST__g_memBlockSize - 0x1100));
    CTGP_HOST__Draw_DrawString(20, 90, COLOR_WHITE, g_ctgpPressDump);
    if (show) { if (R_SUCCEEDED(res)) CTGP_HOST__Draw_DrawString(20, 110, COLOR_GREEN, g_ctgpDump3gxOk); else CTGP_HOST__Draw_DrawFormattedString(20, 110, COLOR_RED, g_ctgpErrorFmt, (unsigned long)res); }
    else if (working) CTGP_HOST__Draw_DrawString(20, 110, COLOR_GRAY, g_ctgpWorking);
    CTGP_HOST__Draw_DrawString(20, 140, COLOR_GRAY, g_ctgpBack);
    PLUGIN_ctgp_EndFullPage();
}

PLUGIN_CODE_FROZEN(ctgp) static void PLUGIN_ctgp_Dump3gxMemory(void)
{
    Result res = 0;
    PLUGIN_ctgp_DrawDump3gx(res, false, false);
    bool requested = false;
    for (;;)
    {
        u32 pressed = CTGP_HOST__waitInputWithTimeout(50);
        if (pressed & KEY_A) requested = true;
        if ((pressed & KEY_B) || CTGP_HOST__menuShouldExit) return;
        if (requested)
        {
            if (__sync_lock_test_and_set(&g_ctgpReleaseBusy,1u)) continue;
            requested = false;
            g_myHookVal[1] = 0;
            __sync_synchronize();
            PLUGIN_ctgp_DrawDump3gx(res, false, true);
            FS_Archive archive; Handle f; char path[128]; u32 index;
            CtgpDumpCleanState cleanState;
            PLUGIN_ctgp_PrepareClean3gxDump(&cleanState);

            if (R_SUCCEEDED(res = CTGP_HOST__FSUSER_OpenArchive(&archive, ARCHIVE_SDMC, CTGP_HOST__fsMakePath(PATH_EMPTY, NULL))))
            {
                CTGP_HOST__FSUSER_CreateDirectory(archive, CTGP_HOST__fsMakePath(PATH_ASCII, g_ctgpLumaDir), 0);
                CTGP_HOST__FSUSER_CreateDirectory(archive, CTGP_HOST__fsMakePath(PATH_ASCII, g_ctgpDumpsDir), 0);
                CTGP_HOST__FSUSER_CreateDirectory(archive, CTGP_HOST__fsMakePath(PATH_ASCII, g_ctgpMemoryDir), 0);
                if (R_SUCCEEDED(CTGP_HOST__FSUSER_OpenFile(&f, archive, CTGP_HOST__fsMakePath(PATH_ASCII, g_ctgpSnapshot3gx), FS_OPEN_READ, 0)))
                {
                    CTGP_HOST__FSFILE_Close(f);
                    for (index = 0; index < 1000; index++)
                    {
                        CTGP_HOST__sprintf(path, g_ctgpDumpRuntimeFmt, index);
                        if (R_FAILED(CTGP_HOST__FSUSER_OpenFile(&f, archive, CTGP_HOST__fsMakePath(PATH_ASCII, path), FS_OPEN_READ, 0))) break;
                        CTGP_HOST__FSFILE_Close(f);
                    }
                    CTGP_HOST__FSUSER_RenameFile(archive, CTGP_HOST__fsMakePath(PATH_ASCII, g_ctgpSnapshot3gx), archive, CTGP_HOST__fsMakePath(PATH_ASCII, path));
                }
                if (R_SUCCEEDED(res = CTGP_HOST__FSUSER_OpenFile(&f, archive, CTGP_HOST__fsMakePath(PATH_ASCII, g_ctgpSnapshot3gx), FS_OPEN_WRITE | FS_OPEN_CREATE, 0)))
                {
                    u32 written = 0;
                    const u32 size = CTGP_HOST__g_memBlockSize - 0x1100u;
                    res = CTGP_HOST__FSFILE_Write(f, &written, 0, (void*)0x07000100, size, FS_WRITE_FLUSH);
                    CTGP_HOST__FSFILE_Close(f);
                    if (R_SUCCEEDED(res) && written != size) res = (Result)-0x221;
                }
                CTGP_HOST__FSUSER_CloseArchive(archive);
            }

            PLUGIN_ctgp_RestoreLiveAfter3gxDump(&cleanState);
            __sync_lock_release(&g_ctgpReleaseBusy);
            if (R_SUCCEEDED(res)) {
                __sync_synchronize();
                g_myHookVal[1] = 2; // request only; 1 is the actual unlock
                (void)PLUGIN_ctgp_CompletePostUnpackRelease();
                if (g_myHookVal[1] != 1 && g_ctgpCommit.state >= 0xE0u)
                    res = (Result)-0x227;
            }
            PLUGIN_ctgp_DrawDump3gx(res, true, false);
        }
    }
}

PLUGIN_CODE_FROZEN(ctgp) static void PLUGIN_ctgp_DrawDumpGame(Result res, bool show, bool working)
{
    PLUGIN_ctgp_DrawFullPage(g_ctgpOptionsTitle);
    CTGP_HOST__Draw_DrawString(20, 40, COLOR_WHITE, g_ctgpDumpMk7Header);
    CTGP_HOST__Draw_DrawString(20, 70, COLOR_WHITE, g_ctgpDumpMk7Base);
    CTGP_HOST__Draw_DrawString(20, 90, COLOR_WHITE, g_ctgpPressDump);
    if (show) { if (R_SUCCEEDED(res)) CTGP_HOST__Draw_DrawString(20, 110, COLOR_GREEN, g_ctgpDumpMk7Ok); else CTGP_HOST__Draw_DrawFormattedString(20, 110, COLOR_RED, g_ctgpErrorFmt, (unsigned long)res); }
    else if (working) CTGP_HOST__Draw_DrawString(20, 110, COLOR_GRAY, g_ctgpWorking);
    CTGP_HOST__Draw_DrawString(20, 140, COLOR_GRAY, g_ctgpBack);
    PLUGIN_ctgp_EndFullPage();
}

PLUGIN_CODE_FROZEN(ctgp) static void PLUGIN_ctgp_DumpGameMemory(void)
{
    Result res = 0;
    PLUGIN_ctgp_DrawDumpGame(res, false, false);
    bool requested = false;
    for (;;)
    {
        u32 pressed = CTGP_HOST__waitInputWithTimeout(50);
        if (pressed & KEY_A) requested = true;
        if ((pressed & KEY_B) || CTGP_HOST__menuShouldExit) return;
        if (requested)
        {
            if (__sync_lock_test_and_set(&g_ctgpReleaseBusy,1u)) continue;
            requested = false;
            g_myHookVal[1] = 0;
            __sync_synchronize();
            PLUGIN_ctgp_DrawDumpGame(res, false, true);
            res = CTGP_HOST__OperateOnProcessByName(g_ctgpMkName, PLUGIN_ctgp_DumpMarioKart7Callback);
            __sync_lock_release(&g_ctgpReleaseBusy);
            // Never release the late DUMP freeze after a short/failed MK7
            // snapshot. Keep the game frozen so A can retry.
            if (R_SUCCEEDED(res)) {
                __sync_synchronize();
                g_myHookVal[1] = 2;
                (void)PLUGIN_ctgp_CompletePostUnpackRelease();
                if (g_myHookVal[1] != 1 && g_ctgpCommit.state >= 0xE0u)
                    res = (Result)-0x227;
            }
            PLUGIN_ctgp_DrawDumpGame(res, true, false);
        }
    }
}

PLUGIN_CODE_FROZEN(ctgp) static void PLUGIN_ctgp_DrawRestore(Result resMk, Result res3gx, bool show, bool working)
{
    PLUGIN_ctgp_DrawFullPage(g_ctgpOptionsTitle);
    CTGP_HOST__Draw_DrawString(20, 40, COLOR_WHITE, g_ctgpRestoreHeader);
    CTGP_HOST__Draw_DrawString(20, 70, COLOR_WHITE, g_ctgpRestoreMk);
    CTGP_HOST__Draw_DrawString(20, 85, COLOR_WHITE, g_ctgpRestore3gx);
    CTGP_HOST__Draw_DrawString(20, 105, COLOR_WHITE, g_ctgpPressRestore);
    if (show)
    {
        if (resMk == (Result)-0x226) CTGP_HOST__Draw_DrawString(20,125,COLOR_WHITE,g_ctgpRestoreMkSkipped);
        else if (R_SUCCEEDED(resMk)) CTGP_HOST__Draw_DrawString(20, 125, COLOR_GREEN, g_ctgpRestoreMkOk); else CTGP_HOST__Draw_DrawFormattedString(20, 125, COLOR_RED, g_ctgpRestoreMkErr, (unsigned long)resMk);
        if (R_SUCCEEDED(res3gx)) CTGP_HOST__Draw_DrawString(20, 140, COLOR_GREEN, g_ctgpRestore3gxOk); else CTGP_HOST__Draw_DrawFormattedString(20, 140, COLOR_RED, g_ctgpRestore3gxErr, (unsigned long)res3gx);
    }
    else if (working) CTGP_HOST__Draw_DrawString(20, 125, COLOR_GRAY, g_ctgpWorking);
    CTGP_HOST__Draw_DrawString(20, 165, COLOR_GRAY, g_ctgpBack);
    PLUGIN_ctgp_EndFullPage();
}

PLUGIN_CODE_FROZEN(ctgp) static void PLUGIN_ctgp_RestoreSnapshots(void)
{
    Result resMk = 0, res3gx = 0;
    PLUGIN_ctgp_DrawRestore(resMk, res3gx, false, false);
    bool requested = false;
    for (;;)
    {
        u32 pressed = CTGP_HOST__waitInputWithTimeout(50);
        if (pressed & KEY_A) requested = true;
        if ((pressed & KEY_B) || CTGP_HOST__menuShouldExit) return;
        if (requested)
        {
            if (__sync_lock_test_and_set(&g_ctgpReleaseBusy,1u)) continue;
            requested = false;
            g_myHookVal[1] = 0;
            __sync_synchronize();
            PLUGIN_ctgp_DrawRestore(resMk, res3gx, false, true);
            resMk = (Result)-0x226;
            FS_Archive archive; Handle f; u32 read = 0;
            const u32 size = CTGP_HOST__g_memBlockSize - 0x1100u;
            if (R_SUCCEEDED(res3gx = CTGP_HOST__FSUSER_OpenArchive(&archive, ARCHIVE_SDMC, CTGP_HOST__fsMakePath(PATH_EMPTY, NULL))))
            {
                if (R_SUCCEEDED(res3gx = CTGP_HOST__FSUSER_OpenFile(&f, archive, CTGP_HOST__fsMakePath(PATH_ASCII, g_ctgpSnapshot3gx), FS_OPEN_READ, 0)))
                {
                    u64 fileSize = 0; u32 header[64];
                    res3gx = CTGP_HOST__FSFILE_GetSize(f,&fileSize);
                    if (R_SUCCEEDED(res3gx) && fileSize != size) res3gx = (Result)-0x222;
                    if (R_SUCCEEDED(res3gx))
                    {
                        res3gx = CTGP_HOST__FSFILE_Read(f,&read,0,header,sizeof(header));
                        if (R_SUCCEEDED(res3gx) && read != sizeof(header)) res3gx = (Result)-0x222;
                        if (R_SUCCEEDED(res3gx) && !PLUGIN_ctgp_RuntimeHeaderValid(header)) res3gx = (Result)-0x223;
                    }
                    if (R_SUCCEEDED(res3gx))
                    {
                        resMk = CTGP_HOST__OperateOnProcessByName(g_ctgpMkName,PLUGIN_ctgp_RestoreMarioKart7Callback);
                        if (R_SUCCEEDED(resMk)) res3gx = CTGP_HOST__FSFILE_Read(f,&read,0,(void*)0x07000100u,size);
                    }
                    CTGP_HOST__FSFILE_Close(f);
                    if (R_SUCCEEDED(res3gx) && read != size) res3gx = (Result)-0x222;
                }
                CTGP_HOST__FSUSER_CloseArchive(archive);
                CTGP_HOST__svcFlushEntireDataCache();
                CTGP_HOST__svcInvalidateEntireInstructionCache();
            }

            if (R_SUCCEEDED(resMk) && R_SUCCEEDED(res3gx))
            {
                volatile u32 *entry = (volatile u32*)0x07000100u;
                if (!PLUGIN_ctgp_RuntimeHeaderValid((const u32*)entry)) res3gx = (Result)-0x223;
                else g_ctgpHookAddrs[1] = 0x07000100u;
            }
            CTGP_HOST__svcFlushEntireDataCache(); CTGP_HOST__svcInvalidateEntireInstructionCache();

            __sync_lock_release(&g_ctgpReleaseBusy);
            // Keep the game frozen on any failure so A can retry. Only a
            // complete, validated pair advances the release handshake.
            if (R_SUCCEEDED(resMk) && R_SUCCEEDED(res3gx)) {
                __sync_synchronize();
                g_myHookVal[1] = 2; // request only; 1 is the actual unlock
                (void)PLUGIN_ctgp_CompletePostUnpackRelease();
                if (g_myHookVal[1] != 1 && g_ctgpCommit.state >= 0xE0u)
                    res3gx = (Result)-0x227;
            }
            PLUGIN_ctgp_DrawRestore(resMk, res3gx, true, false);
        }
    }
}

PLUGIN_CODE(ctgp) static void PLUGIN_ctgp_OpenMiiFitDownload(void)
{
    PLUGIN_ctgp_DrawFullPage(g_ctgpMiiFitPageTitle);
    CTGP_HOST__Draw_DrawString(20, 40, COLOR_WHITE, g_ctgpMiiFitBody);
    CTGP_HOST__Draw_DrawString(20, 170, RGB565(20, 63, 21), g_ctgpMiiFitPressA);
    CTGP_HOST__Draw_DrawString(20, 190, COLOR_GRAY, g_ctgpBack);
    PLUGIN_ctgp_EndFullPage();

    for (;;)
    {
        u32 pressed = CTGP_HOST__waitInputWithTimeout(50);
        if (pressed & KEY_A)
        {
            if (pluginTable_ctgp[59])
                CTGP_MENU__OpenOnlineSource(g_ctgpMiiFitUrl);
            return;
        }
        if ((pressed & KEY_B) || CTGP_HOST__menuShouldExit) return;
    }
}

PLUGIN_CODE_FROZEN(ctgp) static u32 PLUGIN_ctgp_BuildOptions(const char **titles, void (**callbacks)(void))
{
    u32 count = 0;
    titles[count] = g_ctgpToggleTitle; callbacks[count++] = PLUGIN_ctgp_ToggleOptionsAzahar;
    titles[count] = g_ctgpMiiFitTitle; callbacks[count++] = PLUGIN_ctgp_OpenMiiFitDownload;
    if (g_dumpCTGP7) { titles[count] = g_ctgpDump3gxTitle; callbacks[count++] = PLUGIN_ctgp_Dump3gxMemory; titles[count] = g_ctgpDumpMk7Title; callbacks[count++] = PLUGIN_ctgp_DumpGameMemory; }
    if (g_restoreCTGP7) { titles[count] = g_ctgpRestoreTitle; callbacks[count++] = PLUGIN_ctgp_RestoreSnapshots; }
    return count;
}

PLUGIN_CODE_FROZEN(ctgp) static void PLUGIN_ctgp_DrawOptionRow(u32 index, bool selected, const char *title)
{
    CTGP_HOST__Draw_DrawFormattedString(20, 45 + index * 15, COLOR_WHITE, selected ? g_ctgpSelectedFmt : g_ctgpUnselectedFmt, title);
}

PLUGIN_RODATA(ctgp) static const char g_ctgpCompatWaiting[] = "Applying CTGP compatibility...";
PLUGIN_RODATA(ctgp) static const char g_ctgpCompatGateFailed[] = "CTGP startup gate failed.";
PLUGIN_RODATA(ctgp) static const char g_ctgpCompatPatchFailed[] = "CTGP compatibility failed: %02lX";

PLUGIN_CODE_LATE(ctgp) static u32 PLUGIN_ctgp_StartupWaitUiState(void)
{
    if (!isPlgCTGP7) return 0;
    if (g_ctgpHookAddrs[7] >= 0xE0u) return 0x100u | g_ctgpHookAddrs[7];
    if (g_myHookVal[0] < 1 || g_myHookVal[1] == 1) return 0;
    if (g_ctgpCommit.state >= 0xE0u) return 0x200u | g_ctgpCommit.state;
    return g_dumpCTGP7 ? 1u : g_restoreCTGP7 ? 2u : 3u;
}

PLUGIN_CODE_LATE(ctgp) static void PLUGIN_ctgp_DrawStartupWaitStatus(void)
{
    u32 state = PLUGIN_ctgp_StartupWaitUiState();
    if (state & 0x100u)
        CTGP_HOST__Draw_DrawString(20,145,COLOR_RED,g_ctgpCompatGateFailed);
    else if (state & 0x200u)
        CTGP_HOST__Draw_DrawFormattedString(20,145,COLOR_RED,g_ctgpCompatPatchFailed,
            (unsigned long)(state & 0xFFu));
    else if (state)
        CTGP_HOST__Draw_DrawString(20,145,COLOR_PURPLE,state == 1u ? g_ctgpDumpReady :
            state == 2u ? g_ctgpRestoreReady : g_ctgpCompatWaiting);
}

PLUGIN_CODE_FROZEN(ctgp) static void PLUGIN_ctgp_DrawOptionsPage(const char **titles, u32 count, u32 selected)
{
    PLUGIN_ctgp_DrawFullPage(g_ctgpOptionsTitle);
    for (u32 i = 0; i < count; i++) PLUGIN_ctgp_DrawOptionRow(i, i == selected, titles[i]);
    PLUGIN_ctgp_DrawStartupWaitStatus();
    CTGP_HOST__Draw_DrawString(20, 170, COLOR_GRAY, g_ctgpBack);
    PLUGIN_ctgp_EndFullPage();
}

PLUGIN_CODE_FROZEN(ctgp) static void PLUGIN_ctgp_RedrawReady(void)
{
    CTGP_HOST__Draw_Lock();
    CTGP_HOST__Draw_DrawString(20, 145, COLOR_BLACK, g_ctgpClearRow);
    PLUGIN_ctgp_DrawStartupWaitStatus();
    CTGP_HOST__Draw_FlushFramebuffer();
    CTGP_HOST__Draw_Unlock();
}

PLUGIN_CODE_FROZEN(ctgp) static void PLUGIN_ctgp_RedrawOptionCursor(const char **titles, u32 oldSelected, u32 newSelected)
{
    CTGP_HOST__Draw_Lock();
    CTGP_HOST__Draw_DrawString(20, 45 + oldSelected * 15, COLOR_BLACK, g_ctgpClearRow);
    CTGP_HOST__Draw_DrawString(20, 45 + newSelected * 15, COLOR_BLACK, g_ctgpClearRow);
    PLUGIN_ctgp_DrawOptionRow(oldSelected, false, titles[oldSelected]);
    PLUGIN_ctgp_DrawOptionRow(newSelected, true, titles[newSelected]);
    CTGP_HOST__Draw_FlushFramebuffer();
    CTGP_HOST__Draw_Unlock();
}

PLUGIN_CODE_FROZEN(ctgp) void PLUGIN_ctgp_OpenOptions(void)
{
    u32 selected = 0;
    const char *titles[5]; void (*callbacks[5])(void);
    u32 count = PLUGIN_ctgp_BuildOptions(titles, callbacks);
    (void)PLUGIN_ctgp_CompletePostUnpackRelease();
    u32 oldReady = PLUGIN_ctgp_StartupWaitUiState();
    PLUGIN_ctgp_DrawOptionsPage(titles, count, selected);
    for (;;)
    {
        u32 pressed = CTGP_HOST__waitInputWithTimeout(50);
        (void)PLUGIN_ctgp_CompletePostUnpackRelease();
        u32 oldSelected = selected;
        if ((pressed & KEY_DUP) && selected) selected--;
        else if ((pressed & KEY_DDOWN) && selected + 1 < count) selected++;
        else if ((pressed & KEY_A) && count)
        {
            callbacks[selected]();
            count = PLUGIN_ctgp_BuildOptions(titles, callbacks);
            if (selected >= count) selected = count ? count - 1 : 0;
            PLUGIN_ctgp_DrawOptionsPage(titles, count, selected);
            continue;
        }
        else if ((pressed & KEY_B) || CTGP_HOST__menuShouldExit) return;
        if (selected != oldSelected) PLUGIN_ctgp_RedrawOptionCursor(titles, oldSelected, selected);
        u32 ready = PLUGIN_ctgp_StartupWaitUiState();
        if (ready != oldReady)
        {
            oldReady = ready;
            PLUGIN_ctgp_RedrawReady();
        }
    }
}

PLUGIN_CODE_FROZEN(ctgp) static bool PLUGIN_ctgp_IsUpperHex(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F');
}

PLUGIN_CODE_FROZEN(ctgp) static bool PLUGIN_ctgp_IsConsolePasswordCandidate(const char *p)
{
    for (u32 i = 0; i < 16; i++)
        if (!PLUGIN_ctgp_IsUpperHex(p[i])) return false;
    if (p[16] != 0) return false;
    return CTGP_HOST__memcmp(p, g_ctgpHexAlphabet, 16) != 0;
}

PLUGIN_CODE_FROZEN(ctgp) static bool PLUGIN_ctgp_ReadConsolePassword(char out[17])
{
    out[0] = 0;

    u32 imageSize = CTGP_HOST__g_memBlockSize;
    if (imageSize <= 0x1100) return false;

    u32 imageStart = 0x07000100;
    u32 imageEnd = imageStart + imageSize - 0x1100;
    if (imageEnd <= imageStart) return false;

    MemInfo mi;
    PageInfo pi;
    if (R_FAILED(CTGP_HOST__svcQueryMemory(&mi, &pi, imageStart)) ||
        mi.state == MEMSTATE_FREE || !(mi.perm & MEMPERM_READ))
    {
        g_ctgpConsolePasswordAddr = 0;
        return false;
    }

    if (g_ctgpConsolePasswordAddr >= imageStart &&
        g_ctgpConsolePasswordAddr + 17 <= imageEnd &&
        PLUGIN_ctgp_IsConsolePasswordCandidate((const char*)g_ctgpConsolePasswordAddr))
    {
        CTGP_HOST__memcpy(out, (const void*)g_ctgpConsolePasswordAddr, 16);
        out[16] = 0;
        return true;
    }

    u32 match = 0;
    u32 matches = 0;
    for (u32 addr = imageStart; addr + 17 <= imageEnd; addr++)
    {
        const char *candidate = (const char*)addr;
        if (!PLUGIN_ctgp_IsConsolePasswordCandidate(candidate)) continue;
        if (addr > imageStart && PLUGIN_ctgp_IsUpperHex(candidate[-1])) continue;

        match = addr;
        if (++matches > 1) break;
    }

    if (matches != 1)
    {
        g_ctgpConsolePasswordAddr = 0;
        return false;
    }

    g_ctgpConsolePasswordAddr = match;
    CTGP_HOST__memcpy(out, (const void*)match, 16);
    out[16] = 0;
    return true;
}

PLUGIN_CODE_FROZEN(ctgp) static const char *PLUGIN_ctgp_YN(bool value)
{
    return value ? g_ctgpY : g_ctgpN;
}

PLUGIN_CODE_FROZEN(ctgp) static const char *PLUGIN_ctgp_HealthText(u32 *color)
{
    *color = COLOR_WHITE;
    if (!g_ctgpHostHooksInstalled)
    {
        *color = COLOR_RED;
        return g_ctgpHealthHostFail;
    }
    if (!g_ctgpLauncherDetected)
    {
        *color = COLOR_GRAY;
        return g_ctgpHealthWaitingCtgp;
    }
    if (!g_ctgpLauncherPatched || g_ctgpLauncherScanMask != 0x7u || !g_ctgpLauncherWriteOk)
    {
        *color = COLOR_RED;
        return g_ctgpHealthLauncherFail;
    }
    if (isPlgCTGP7 && g_ctgpChecksumState == 4u)
    {
        *color = COLOR_RED;
        return g_ctgpHealthEntryFail;
    }
    if ((g_dumpCTGP7 || g_restoreCTGP7) && g_ctgpHookAddrs[7] >= 0xE0u)
    {
        *color = COLOR_RED;
        return g_ctgpHealthFreezeFail;
    }
    if (!g_ctgpMk7Entered)
    {
        *color = COLOR_GRAY;
        return g_ctgpHealthWaitingMk;
    }
    if (g_vanillaCTGP7)
    {
        *color = COLOR_GREEN;
        return g_ctgpHealthVanillaOk;
    }
    // If the MK7 callback actually ran and failed, report that first. Its scan
    // and write masks survive QuitCtgpLoop(), whereas transient Toxt state is
    // reset with the runtime loop.
    if (g_ctgpMkPatchAttempted && (!g_ctgpMkPatchSuccess ||
        g_ctgpMkScanMask != CTGP_MK_SCAN_EXPECTED ||
        g_ctgpMkWriteMask != CTGP_MK_WRITE_EXPECTED))
    {
        *color = COLOR_RED;
        return g_ctgpHealthMkFail;
    }
    if (!g_ctgpToxtTargetFound || !g_ctgpToxtHookInstalled || !g_ctgpToxtRedirected || g_privTextDiagHits == 0)
    {
        *color = COLOR_RED;
        return g_ctgpHealthToxtFail;
    }
    if (!g_ctgpMkPatchAttempted)
    {
        *color = COLOR_GRAY;
        return g_ctgpHealthWaitingMk;
    }
    if (g_ctgpPrivatePollSeen && !g_ctgpPrivateVerifyOk)
    {
        *color = COLOR_RED;
        return g_ctgpHealthPrivFail;
    }
    *color = COLOR_GREEN;
    return g_ctgpHealthAllOk;
}

PLUGIN_CODE_FROZEN(ctgp) static void PLUGIN_ctgp_DrawDebugPage(void)
{
    char password[17];
    bool hasPassword = false;
    if (g_ctgpDumpRestoreUnlocked)
        hasPassword = PLUGIN_ctgp_ReadConsolePassword(password);
    const char *privateVerify = g_ctgpPrivatePollSeen ? PLUGIN_ctgp_YN(g_ctgpPrivateVerifyOk) : g_ctgpWait;
    u32 healthColor = COLOR_WHITE;
    const char *health = PLUGIN_ctgp_HealthText(&healthColor);

    CTGP_HOST__Draw_Lock();
    CTGP_HOST__Draw_ClearFramebuffer();
    CTGP_BLUR__DrawFeatureFrame(g_ctgpDebugTitle);

    // The original diagnostic page, compressed to an 11px baseline so the
    // patch-health rows fit without turning it back into the temporary wall.
    const char *intervalText = !g_ctgpTickRegistered ? g_ctgpIntervalOff :
        (g_ctgpCurrentIntervalNs == CTGP_TICK_FAST_NS ?
            g_ctgpIntervalFast : g_ctgpIntervalArmed);
    CTGP_HOST__Draw_DrawString(12, 34, COLOR_WHITE, intervalText);
    CTGP_HOST__Draw_DrawFormattedString(12, 45, COLOR_WHITE, g_ctgpLauncherDetectedFmt,
        g_ctgpLauncherDetected ? g_ctgpYes : g_ctgpNo);
    CTGP_HOST__Draw_DrawFormattedString(12, 56,
        (g_ctgpLauncherDetected && !g_ctgpLauncherPatched) ? COLOR_RED : COLOR_WHITE,
        g_ctgpLauncherHealthFmt, g_ctgpLauncherPatched ? g_ctgpYes : g_ctgpNo,
        g_ctgpLauncherScanMask, PLUGIN_ctgp_YN(g_ctgpLauncherWriteOk));
    CTGP_HOST__Draw_DrawFormattedString(12, 67, COLOR_WHITE, g_ctgpMkEnteredFmt,
        g_ctgpMk7Entered ? g_ctgpYes : g_ctgpNo);
    CTGP_HOST__Draw_DrawFormattedString(12, 78,
        g_ctgpHostHooksInstalled ? COLOR_WHITE : COLOR_RED, g_ctgpHostHooksFmt,
        g_ctgpHostHooksInstalled ? g_ctgpYes : g_ctgpNo);
    if (g_ctgpDumpRestoreUnlocked)
        CTGP_HOST__Draw_DrawFormattedString(12, 89, COLOR_WHITE, g_ctgpPasswordFmt,
            hasPassword ? password : g_ctgpPasswordUnavailable);

    CTGP_HOST__Draw_DrawFormattedString(12, 100,
        g_ctgpChecksumState == 4u ? COLOR_RED : COLOR_WHITE,
        g_ctgpEntryHealthFmt, (unsigned long)g_ctgpChecksumState,
        (unsigned long)g_ctgpChecksumIvAddress);

    if (g_vanillaCTGP7)
    {
        CTGP_HOST__Draw_DrawString(12, 111, COLOR_GRAY, g_ctgpSkipVanilla);
    }
    else
    {
        bool toxtOk = g_ctgpToxtTargetFound && g_ctgpToxtHookInstalled &&
                      g_ctgpToxtRedirected && g_privTextDiagHits != 0;
        CTGP_HOST__Draw_DrawFormattedString(12, 111,
            (g_ctgpMk7Entered && !toxtOk) ? COLOR_RED : COLOR_WHITE,
            g_ctgpToxtHealthFmt,
            PLUGIN_ctgp_YN(g_ctgpToxtTargetFound), PLUGIN_ctgp_YN(g_ctgpToxtHookInstalled),
            g_privTextDiagHits, PLUGIN_ctgp_YN(g_ctgpToxtRedirected));
        CTGP_HOST__Draw_DrawFormattedString(12, 122, COLOR_WHITE, g_ctgpCoreHealthFmt,
            PLUGIN_ctgp_YN(g_ctgpMkScanMask & CTGP_MK_SCAN_CHAR),
            PLUGIN_ctgp_YN(g_ctgpMkScanMask & CTGP_MK_SCAN_CONTROL),
            PLUGIN_ctgp_YN(g_ctgpMkScanMask & CTGP_MK_SCAN_SHELL),
            PLUGIN_ctgp_YN(g_ctgpMkScanMask & CTGP_MK_SCAN_SPINY));
        CTGP_HOST__Draw_DrawFormattedString(12, 133, COLOR_WHITE, g_ctgpPrivScanFmt,
            PLUGIN_ctgp_YN(g_ctgpMkScanMask & CTGP_MK_SCAN_WW),
            PLUGIN_ctgp_YN(g_ctgpMkScanMask & CTGP_MK_SCAN_BOT),
            PLUGIN_ctgp_YN(g_ctgpMkScanMask & CTGP_MK_SCAN_TIMER),
            PLUGIN_ctgp_YN(g_ctgpMkScanMask & CTGP_MK_SCAN_ROOM));
        CTGP_HOST__Draw_DrawFormattedString(12, 144, COLOR_WHITE, g_ctgpCtgpScanFmt,
            PLUGIN_ctgp_YN(g_ctgpMkScanMask & CTGP_MK_SCAN_CAVE),
            PLUGIN_ctgp_YN(g_ctgpMkScanMask & CTGP_MK_SCAN_MENU));
        CTGP_HOST__Draw_DrawFormattedString(12, 155,
            (g_ctgpMkPatchAttempted && !g_ctgpMkPatchSuccess) ? COLOR_RED : COLOR_WHITE,
            g_ctgpWriteHealthFmt, g_ctgpMkWriteMask, CTGP_MK_WRITE_EXPECTED,
            PLUGIN_ctgp_YN(g_ctgpMkPatchSuccess));
        CTGP_HOST__Draw_DrawFormattedString(12, 166,
            (g_ctgpPrivatePollSeen && !g_ctgpPrivateVerifyOk) ? COLOR_RED : COLOR_WHITE,
            g_ctgpPrivateHealthFmt, PLUGIN_ctgp_YN(cheatsActive), privateVerify);
    }

    const char *freezeMode = g_restoreCTGP7 ? g_ctgpFreezeRestore :
                             g_dumpCTGP7 ? g_ctgpFreezeDump : g_ctgpFreezeNone;
    CTGP_HOST__Draw_DrawFormattedString(12, 177,
        (g_ctgpHookAddrs[7] >= 0xE0u) ? COLOR_RED : COLOR_WHITE,
        g_ctgpFreezeHealthFmt, freezeMode, g_ctgpHookAddrs[7], g_ctgpHookAddrs[0]);
    CTGP_HOST__Draw_DrawFormattedString(12, 188, COLOR_WHITE, g_ctgpMimCountFmt, mimCount);
    CTGP_HOST__Draw_DrawFormattedString(12, 199, healthColor, g_ctgpHealthFmt, health);
    CTGP_HOST__Draw_DrawString(12, 228, COLOR_GRAY, g_ctgpBack);
    CTGP_HOST__Draw_FlushFramebuffer();
    CTGP_HOST__Draw_Unlock();
}

PLUGIN_CODE_FROZEN(ctgp) static u32 PLUGIN_ctgp_DebugStateSignature(void)
{
    // Poll input normally, but redraw only when a visible field changed.
    u32 h = 0x43544750u;
#define CTGP_DEBUG_MIX(v) do { h ^= (u32)(v) + 0x9E3779B9u + (h << 6) + (h >> 2); } while (0)
    CTGP_DEBUG_MIX((u32)g_ctgpCurrentIntervalNs);
    CTGP_DEBUG_MIX(g_ctgpLauncherDetected);
    CTGP_DEBUG_MIX(g_ctgpLauncherPatched);
    CTGP_DEBUG_MIX(g_ctgpLauncherScanMask);
    CTGP_DEBUG_MIX(g_ctgpLauncherWriteOk);
    CTGP_DEBUG_MIX(g_ctgpMk7Entered);
    CTGP_DEBUG_MIX(g_ctgpHostHooksInstalled);
    CTGP_DEBUG_MIX(g_ctgpConsolePasswordAddr);
    CTGP_DEBUG_MIX(g_ctgpEntryBodyKnown);
    CTGP_DEBUG_MIX(g_ctgpChecksumState);
    CTGP_DEBUG_MIX(g_vanillaCTGP7);
    CTGP_DEBUG_MIX(g_dumpCTGP7);
    CTGP_DEBUG_MIX(g_restoreCTGP7);
    CTGP_DEBUG_MIX(g_ctgpToxtTargetFound);
    CTGP_DEBUG_MIX(g_ctgpToxtHookInstalled);
    CTGP_DEBUG_MIX(g_ctgpToxtRedirected);
    CTGP_DEBUG_MIX(g_privTextDiagHits);
    CTGP_DEBUG_MIX(g_ctgpMkScanMask);
    CTGP_DEBUG_MIX(g_ctgpMkWriteMask);
    CTGP_DEBUG_MIX(g_ctgpMkPatchAttempted);
    CTGP_DEBUG_MIX(g_ctgpMkPatchSuccess);
    CTGP_DEBUG_MIX(g_ctgpPrivatePollSeen);
    CTGP_DEBUG_MIX(g_ctgpPrivateVerifyOk);
    CTGP_DEBUG_MIX(cheatsActive);
    CTGP_DEBUG_MIX(g_ctgpHookAddrs[7]);
    CTGP_DEBUG_MIX(g_ctgpHookAddrs[0]);
    CTGP_DEBUG_MIX((u32)mimCount);
#undef CTGP_DEBUG_MIX
    return h;
}

PLUGIN_CODE_FROZEN(ctgp) void PLUGIN_ctgp_OpenDebug(void)
{
    PLUGIN_ctgp_DrawDebugPage();
    u32 drawnState = PLUGIN_ctgp_DebugStateSignature();
    for (;;)
    {
        u32 pressed = CTGP_HOST__waitInputWithTimeout(100);
        if ((pressed & KEY_B) || CTGP_HOST__menuShouldExit) return;

        u32 state = PLUGIN_ctgp_DebugStateSignature();
        if (state != drawnState)
        {
            PLUGIN_ctgp_DrawDebugPage();
            drawnState = PLUGIN_ctgp_DebugStateSignature();
        }
    }
}

PLUGIN_CODE_FROZEN(ctgp) void PLUGIN_ctgp_OnTick(u64 delta);

PLUGIN_CODE_FROZEN(ctgp) static void PLUGIN_ctgp_SetInterval(s64 interval)
{
    if (g_ctgpTickRegistered && g_ctgpCurrentIntervalNs == interval)
        return;

    if (CTGP_BLUR__AddTickFunc(PLUGIN_ctgp_OnTickAzahar, interval))
    {
        g_ctgpTickRegistered = true;
        g_ctgpCurrentIntervalNs = interval;
    }
}

PLUGIN_CODE_FROZEN(ctgp) static void PLUGIN_ctgp_UnregisterTick(void)
{
    if (g_ctgpTickRegistered)
        (void)CTGP_BLUR__RemoveTickFunc(PLUGIN_ctgp_OnTickAzahar);

    g_ctgpTickRegistered = false;
    g_ctgpCurrentIntervalNs = 0;
}

PLUGIN_CODE_FROZEN(ctgp) static bool PLUGIN_ctgp_QueueLaunchWake(void)
{
    __sync_synchronize();
    g_ctgpWakeGeneration++;
    __sync_synchronize();

    // Wake the Blur worker promptly for the first state transition. OnTick
    // drops back to the 250 ms armed cadence until CTGP startup needs 1 ms.
    PLUGIN_ctgp_SetInterval(CTGP_TICK_FAST_NS);
    return g_ctgpTickRegistered;
}

typedef struct
{
    u32 titleIdLow;
    u32 titleIdHigh;
    u32 scanMask;
    u32 writeOk;
    s32 patchResult;
} CtgpBridgePatchResult;

PLUGIN_BSS(ctgp) static PluginMenuBridgeRegistration g_ctgpBridgeRegistration;
PLUGIN_BSS(ctgp) static volatile bool g_ctgpBridgeReady;

PLUGIN_CODE_FROZEN(ctgp) static bool PLUGIN_ctgp_OnBridgeMessage(
    u32 command,
    const void *payload,
    u32 payloadSize
)
{
    const CtgpBridgePatchResult *message;

    if (!g_ctgpBridgeReady)
        return false;

    // Unknown CTGP bridge messages are consumed rather than retried forever.
    if (command != CTGP_EVENT_PATCH_RESULT || !payload || payloadSize != sizeof(CtgpBridgePatchResult))
        return true;

    message = (const CtgpBridgePatchResult *)payload;
    if (message->titleIdLow != 0x03070C00u || message->titleIdHigh != 0x00040000u)
        return true;

    g_ctgpLauncherDetected = true;
    g_ctgpLauncherScanMask = message->scanMask;
    g_ctgpLauncherWriteOk = message->writeOk == 1u;
    g_ctgpLauncherPatchResult = message->patchResult;
    g_ctgpLauncherPatched = g_ctgpLauncherScanMask == 0x7u &&
        g_ctgpLauncherWriteOk && g_ctgpLauncherPatchResult == 0;

    if (!g_ctgpLauncherPatched)
        return true;

    return PLUGIN_ctgp_QueueLaunchWake();
}

PLUGIN_CODE_FROZEN(ctgp) static void PLUGIN_ctgp_ConsumeLaunchWake(void)
{
    u32 generation = g_ctgpWakeGeneration;
    __sync_synchronize();
    if (generation == g_ctgpHandledGeneration)
        return;

    PLUGIN_ctgp_CleanupTransientStartupHooks();
    ResetCtgpState();
    for (u32 i = 0; i < 8; i++)
        g_ctgpHookAddrs[i] = 0;

    g_ctgpAlive = false;
    g_ctgpPid = 0;
    g_ctgpMkAlive = false;
    g_ctgpMkPid = 0;
    g_ctgpMk7Entered = false;
    g_ctgpWaitLauncherAppear = true;
    ctgpWaitLoadPostLauncher = true;
    elapsed20sTimeout = 0;
    g_ctgpElapsed250ms = 0;
    g_CtgpMasterLoopActive = true;
    g_ctgpHandledGeneration = generation;
}

PLUGIN_CODE_LATE(ctgp) static bool PLUGIN_ctgp_CompletePostUnpackRelease(void)
{
    if (!isPlgCTGP7) return false;
    if (g_myHookVal[1] == 1u) return true;
    if (g_myHookVal[0] < 1u ||
        ((g_dumpCTGP7 || g_restoreCTGP7) && g_myHookVal[1] != 2u)) return false;
    if (__sync_lock_test_and_set(&g_ctgpReleaseBusy,1u)) return false;
    bool ok = false;
    // Recheck after acquiring ownership. Worker and menu use this same path.
    if (g_myHookVal[1] == 1u) ok = true;
    else if (g_myHookVal[0] >= 1u &&
             ((!g_dumpCTGP7 && !g_restoreCTGP7) || g_myHookVal[1] == 2u)) {
        // Feature preparation is after the clean dump/full restore, before
        // the final compatibility write. A later monitor can retry Toxt.
        if (!g_vanillaCTGP7) {
            if (!g_ctgpToxtHookInstalled) {
                Result scan = scanPreloadAnyCTGP();
                Result patch = R_FAILED(scan) ? scan : patchPreloadCustomCTGP();
                if (R_FAILED(patch)) {
                    g_ctgpToxtScanPending = true;
                    g_ctgpToxtScanElapsed = 0;
                    g_ctgpToxtFullScanElapsed = 0;
                }
            }
            ctgpStartLoop = true;
        }
        if (PLUGIN_ctgp_ApplyLegacySafetyPatches()) {
            g_CtgpMasterLoopActive = true;
            mk7Loop = true;
            elapsed10sMK7 = 0;
            ctgpWaitLoadPostLauncher = false;
            elapsed20sTimeout = 0;
            __sync_synchronize();
            g_myHookVal[1] = 1u;
            g_myHookVal[0] = 0u;
            __sync_synchronize();
            CTGP_HOST__svcFlushEntireDataCache();
            ok = true;
        }
        PLUGIN_ctgp_ReportCommitStatus();
    }
    __sync_lock_release(&g_ctgpReleaseBusy);
    return ok;
}

PLUGIN_CODE_FROZEN(ctgp) void PLUGIN_ctgp_OnTick(u64 delta)
{
    PLUGIN_ctgp_ConsumeLaunchWake();

    if (g_CtgpMasterLoopActive &&
        g_ctgpCurrentIntervalNs == CTGP_TICK_FAST_NS &&
        delta > MAX_DELTA_CTGP)
    {
        delta = MAX_DELTA_CTGP;
    }
    if (g_CtgpMasterLoopActive)
    {
        g_ctgpElapsed250ms += delta;
        if (ctgpStartLoop) elapsed1frameCTGPStartup += delta;
        if (mk7Loop) { if (patchedMK7Hook) elapsed1frameMK7 += delta; elapsed10sMK7 += delta; }
        if (ctgpWaitLoadPostLauncher) elapsed20sTimeout += delta;
    }

    // The original working Toxt patch was intentionally installed in the
    // middle of CTGP startup, after the runtime image had materialised. Do not
    // scan at PostGameFlush: hardware proved that is too early (zero signature
    // words). Instead cheaply watch the known current semantic site every tick
    // and do a full semantic scan as soon as it appears. A 250ms full-scan
    // fallback keeps this version tolerant of an address shift.
    if (g_CtgpMasterLoopActive && g_ctgpToxtScanPending &&
        (!g_dumpCTGP7 || g_myHookVal[0] == 0u) &&
        (!g_restoreCTGP7 || g_ctgpHookAddrs[7] == 0x22u) &&
        !__sync_lock_test_and_set(&g_ctgpReleaseBusy,1u))
    {
        g_ctgpToxtScanElapsed += delta;
        g_ctgpToxtFullScanElapsed += delta;
        g_toxtDiagPendingTicks++;

        bool quickReady = PLUGIN_ctgp_ToxtQuickProbeReady();
        bool fallbackScan = g_ctgpToxtFullScanElapsed >= TICKS_250MS;
        if (quickReady || fallbackScan)
        {
            if (fallbackScan) g_ctgpToxtFullScanElapsed = 0;
            Result scanRes = scanPreloadAnyCTGP();
            if (R_SUCCEEDED(scanRes))
            {
                if (g_vanillaCTGP7)
                {
                    g_ctgpToxtScanPending = false;
                    g_toxtDiagStage = 0x40;
                }
                else
                {
                    Result patchRes = patchPreloadCustomCTGP();
                    if (R_SUCCEEDED(patchRes))
                    {
                        g_ctgpToxtScanPending = false;
                        g_toxtDiagStage = 0x40;
                    }
                    // On failure, keep the mid-load monitor armed and retry while
                    // CTGP is still starting. The debug page records the stage.
                }
            }
        }
        __sync_lock_release(&g_ctgpReleaseBusy);
    }

    if (g_CtgpMasterLoopActive && g_ctgpElapsed250ms >= TICKS_250MS)
    {
        g_ctgpElapsed250ms -= TICKS_250MS;
        if (g_ctgpWaitLauncherAppear)
        {
            if (ensureProcess(&g_ctgpPid, g_ctgpLauncherName, &proc))
            {
                g_ctgpLauncherDetected = true;
                g_ctgpAlive = true;
                g_ctgpWaitLauncherAppear = false;
                ctgpWaitLoadPostLauncher = false;
                elapsed20sTimeout = 0;
                CTGP_HOST__FSUSER_OpenArchive(&g_ctgpSd, ARCHIVE_SDMC, CTGP_HOST__fsMakePath(PATH_EMPTY, NULL));
                countMimFiles();
                CTGP_HOST__FSUSER_CloseArchive(g_ctgpSd);
            }
        }
        else if (g_ctgpAlive)
        {
            if (!ensureProcess(&g_ctgpPid, g_ctgpLauncherName, &proc))
            {
                g_ctgpAlive = false;
                ctgpWaitLoadPostLauncher = true;
                elapsed20sTimeout = 0;
            }
        }
        if (proc) { CTGP_HOST__svcCloseHandle(proc); proc = 0; }

        if (patchedMK7Hook && mimCount >= 2)
        {
            MemInfo mi; PageInfo pi;
            if (R_FAILED(CTGP_HOST__svcQueryMemory(&mi, &pi, 0x070F0000)) || mi.state == MEMSTATE_FREE || !(mi.perm & MEMPERM_READWRITE))
            {
                // This is a real MK7 teardown, not the intentional VANILLA
                // early-stop.  Clear the session-facing latch before resetting
                // transient Toxt state so the debug page does not report a
                // dead game as entered and then mislabel that teardown as a
                // Toxt patch failure.
                g_ctgpMk7Entered = false;
                QuitCtgpLoop();
            }
            else swapMimFiles();
        }
        (void)PLUGIN_ctgp_CompletePostUnpackRelease();

    }

    if (g_CtgpMasterLoopActive && elapsed20sTimeout >= TICKS_10S * 2) { elapsed20sTimeout = 0; QuitCtgpLoop(); }
    if (g_CtgpMasterLoopActive && elapsed10sMK7 >= TICKS_10S)
    {
        elapsed10sMK7 -= TICKS_10S;
        MemInfo mi; PageInfo pi;
        if (!patchedMK7Hook)
        {
            g_ctgpMkAlive = ensureProcess(&g_ctgpMkPid, g_ctgpMkName, &proc);
            if (g_ctgpMkAlive)
            {
                g_ctgpMk7Entered = true;
                // By the time MK7 exists, the mid-load Toxt window has ended.
                // Do not crash here: the compact debug page now reports target,
                // hook, hit and redirect state directly.
            }
            if (proc) { CTGP_HOST__svcCloseHandle(proc); proc = 0; }
            if (!g_ctgpMkAlive || R_FAILED(CTGP_HOST__svcQueryMemory(&mi, &pi, 0x070F0000)) || mi.state == MEMSTATE_FREE || !(mi.perm & MEMPERM_READWRITE))
            {
                g_ctgpMk7Entered = false;
                QuitCtgpLoop();
            }
            else if (g_vanillaCTGP7) QuitCtgpLoop();
            else { Result res = CTGP_HOST__OperateOnProcessByName(g_ctgpMkName, PatchGameCTGP7Callback); if (R_FAILED(res)) QuitCtgpLoop(); else patchedMK7Hook = true; }
        }
        else
        {
            if (R_FAILED(CTGP_HOST__svcQueryMemory(&mi, &pi, 0x070F0000)) || mi.state == MEMSTATE_FREE || !(mi.perm & MEMPERM_READWRITE))
            {
                g_ctgpMk7Entered = false;
                QuitCtgpLoop();
            }
            else CTGP_HOST__OperateOnProcessByName(g_ctgpMkName, pollToggleHackerLobbyCallback);
        }
    }
    if (g_CtgpMasterLoopActive && elapsed1frameMK7 >= TICKS_1FRAME)
    {
        elapsed1frameMK7 -= TICKS_1FRAME;
        MemInfo mi; PageInfo pi;
        if (R_FAILED(CTGP_HOST__svcQueryMemory(&mi, &pi, 0x070F0000)) || mi.state == MEMSTATE_FREE || !(mi.perm & MEMPERM_READWRITE))
        {
            g_ctgpMk7Entered = false;
            QuitCtgpLoop();
        }
        else calcRGBShells();
        u32 pressed = CTGP_HOST__waitInputWithTimeout(16.6);
        if ((pressed & (KEY_X | KEY_Y)) == (KEY_X | KEY_Y)) { CTGP_HOST__menuEnter(); CTGP_HOST__PluginLoaderOptions__UpdateMenu(); CTGP_HOST__PluginWatcher__UpdateMenu(); CTGP_HOST__PluginConverter__UpdateMenu(); CTGP_HOST__menuShow(CTGP_HOST__rosalinaMenu); CTGP_HOST__menuLeave(); }
    }
    if (g_CtgpMasterLoopActive && elapsed1frameCTGPStartup >= TICKS_1FRAME)
    {
        elapsed1frameCTGPStartup -= TICKS_1FRAME;
        if ((g_langWord >> 24) == 1) { patchTextFile(); g_langWord &= 0x00FFFFFF; ctgpStartLoop = false; }
        if (g_langWord == 2) ctgpStartLoop = false;
    }
    if (!g_CtgpMasterLoopActive)
        PLUGIN_ctgp_UnregisterTick();
    else if (ctgpStartLoop || mk7Loop || g_ctgpToxtScanPending || patchedMK7Hook)
        PLUGIN_ctgp_SetInterval(CTGP_TICK_FAST_NS);
    else
        PLUGIN_ctgp_SetInterval(CTGP_TICK_ARMED_NS);
}

PLUGIN_CODE_FROZEN(ctgp) void *PLUGIN_ctgp_UserConfigCopyWrapper(void *dst, const void *src, size_t size)
{
    void *ret = CTGP_HOST__memcpy(dst, src, size);
    const char *path = CTGP_HOST__PluginLoaderCtx->pluginPath;
    if (path && CTGP_HOST__strcmp(path, g_ctgp3gxPath) == 0) isPlgCTGP7 = true;
    return ret;
}

// This path is only for a rejected unpacker signature. It cannot resume into
// encrypted startup or advertise that a snapshot is ready to restore.
PLUGIN_CODE_LATE(ctgp) __attribute__((naked, noinline)) static void PLUGIN_ctgp_StartupGateFailed(void)
{
    __asm__ volatile(
        "1: ldr r0, 2f\n"
        "mov r1, #0\n"
        "svc 0x0A\n"
        "b 1b\n"
        "2: .word 1000000\n"
    );
}

PLUGIN_CODE_FROZEN(ctgp) void PLUGIN_ctgp_PostGameFlushWrapper(void)
{
    if (isPlgCTGP7)
    {
        // Repair the checksum-derived IV before CTGP executes its startup code.
        PLUGIN_ctgp_PrepareStartup();

        g_myHookVal[0] = 0;
        g_myHookVal[1] = 0;
        for (u32 i = 0; i < 8; i++) g_ctgpHookAddrs[i] = 0;

        // Keep the original custom language monitor. Compatibility itself
        // is installed at the final unpack return, before runtime startup.
        if (!g_restoreCTGP7)
        {
            g_ctgpToxtScanPending = true;
            g_ctgpToxtScanElapsed = 0;
            g_ctgpToxtFullScanElapsed = TICKS_250MS;
            g_toxtDiagStage = 0x30;
            g_toxtDiagFail = 0;
            g_toxtDiagProbeWord = 0;
            g_toxtDiagQuickTriggers = 0;
            g_toxtDiagPendingTicks = 0;
            if (!g_vanillaCTGP7) ctgpStartLoop = true;
        }

        if (g_restoreCTGP7)
        {
            if (!g_ctgpEntryBodyKnown) { g_ctgpHookAddrs[7] = 0xE0u; CTGP_HOST__svcFlushEntireDataCache(); PLUGIN_ctgp_PublishStartup(); return; }
            volatile u32 *site = (volatile u32*)g_ctgpEntryBodyAddress;
            g_ctgpHookAddrs[0] = g_ctgpEntryBodyAddress;
            g_ctgpHookAddrs[1] = g_ctgpEntryBodyAddress;
            g_ctgpHookAddrs[5] = site[0]; g_ctgpHookAddrs[6] = site[1];
            g_ctgpHookAddrs[7] = 0x20u;
            site[1] = PLUGIN_ctgp_Phys(ctgpRestoreWaitHook); site[0] = 0xE51FF004u;
            CTGP_HOST__svcFlushEntireDataCache(); CTGP_HOST__svcInvalidateEntireInstructionCache();
        }
        else
        {
            bool armed = PLUGIN_ctgp_ArmRelocReturnHook();
            if (!armed && g_ctgpEntryBodyKnown) {
                // Unsupported unpacker: hold rather than run past the first
                // commit query without a verified compatibility hook.
                volatile u32 *site = (volatile u32*)g_ctgpEntryBodyAddress;
                g_ctgpHookAddrs[0] = g_ctgpEntryBodyAddress;
                g_ctgpHookAddrs[1] = g_ctgpEntryBodyAddress;
                g_ctgpHookAddrs[5] = site[0]; g_ctgpHookAddrs[6] = site[1];
                g_ctgpHookAddrs[7] = 0xE0u;
                g_ctgpCommit.state = 0xEAu;
                site[1] = PLUGIN_ctgp_Phys(PLUGIN_ctgp_StartupGateFailed);
                site[0] = 0xE51FF004u;
                CTGP_HOST__svcFlushEntireDataCache();
                CTGP_HOST__svcInvalidateEntireInstructionCache();
                PLUGIN_ctgp_ReportCommitStatus();
            }
        }

        ctgpWaitLoadPostLauncher = false;
        elapsed20sTimeout = 0;
    }
    CTGP_HOST__svcFlushEntireDataCache();
    if (isPlgCTGP7) PLUGIN_ctgp_PublishStartup();
}

PLUGIN_CODE_FROZEN(ctgp) Result PLUGIN_ctgp_MapExeWrapper(Handle dstProcess, u32 dstAddr, Handle srcProcess, u32 srcAddr, u32 size, MapExFlags flags)
{
    if (CTGP_HOST__isN3DS && isPlgCTGP7 && g_restoreCTGP7) size = CTGP_HOST__g_memBlockSize - 0x1000;
    return CTGP_HOST__svcMapProcessMemoryEx(dstProcess, dstAddr, srcProcess, srcAddr, size, flags);
}

extern bool PLUGIN_ctgp_InstallHostHooks(void);

PLUGIN_MAIN(ctgp) bool PLUGIN_ctgp_Main(void)
{
    if (!PLUGIN_ctgp_AsmAlias(g_myHookVal_raw) || !PLUGIN_ctgp_AsmAlias(&g_langWord_raw)) return false;
    if (!CTGP_MENU__SaveData || !CTGP_MENU__LoadData ||
        !CTGP_BLUR__AddTickFunc || !CTGP_BLUR__RemoveTickFunc ||
        !CTGP_MENU__RegisterBridgeReceiver || !CTGP_MENU__UnregisterBridgeReceiver) return false;

    PLUGIN_ctgp_LoadSettingsAzahar();
    g_ctgpBridgeReady = false;
    g_ctgpBridgeRegistration.pluginId = CTGP_PLUGIN_ID;
    g_ctgpBridgeRegistration.callback = PLUGIN_ctgp_OnBridgeMessage;
    g_ctgpBridgeRegistration.next = NULL;
    if (!CTGP_MENU__RegisterBridgeReceiver(&g_ctgpBridgeRegistration)) return false;

    g_ctgpHostHooksInstalled = PLUGIN_ctgp_InstallHostHooks();
    if (!g_ctgpHostHooksInstalled)
    {
        (void)CTGP_MENU__UnregisterBridgeReceiver(&g_ctgpBridgeRegistration);
        return false;
    }

    (void)CTGP_MENU__AddItem(&g_ctgpMenuRegistration, CTGP_PLUGIN_ID, g_ctgpOptionsTitle, PLUGIN_ctgp_OpenOptions, CTGP_ROOT_COLOR);
    (void)CTGP_BLUR__AddFeatureItem(&g_ctgpDebugRegistration, CTGP_PLUGIN_ID, g_ctgpDebugTitle, PLUGIN_ctgp_OpenDebug);
    g_ctgpBridgeReady = true;
    return true;
}

PLUGIN_BSS_LATE(ctgp) static volatile bool g_ctgpMountWakePending;
PLUGIN_BSS_LATE(ctgp) static volatile bool g_ctgpMountInProgress;
PLUGIN_BSS_LATE(ctgp) static u32 g_ctgpStartupStatus[32];
PLUGIN_RODATA(ctgp) static const char g_ctgpStartupStatusPath[] = "/luma/dumps/memory/ctgp-startup-v108-commit.bin";

PLUGIN_CODE_LATE(ctgp) void PLUGIN_ctgp_LatchMountIdentity(void)
{
    const char *path = CTGP_HOST__PluginLoaderCtx->pluginPath;
    isPlgCTGP7 = path && CTGP_HOST__strcmp(path, g_ctgp3gxPath) == 0;
    // Existing launcher callbacks can already be scheduled. Block them until
    // post-mount checksum, dump/restore and scan state has been published.
    g_ctgpMountInProgress = isPlgCTGP7;
    g_ctgpMountWakePending = false;
    __sync_synchronize();
    if (isPlgCTGP7)
    {
        PLUGIN_ctgp_ResetCommitMount();
        g_ctgpChecksumState = 0;
        g_ctgpChecksumIvAddress = g_ctgpChecksumStore0 = g_ctgpChecksumStore1 = 0;
        g_ctgpEntryBodyKnown = false;
        g_ctgpEntryBodyAddress = 0;
        for (u32 i=9; i<32u; i++) ((volatile u32*)g_ctgpStartupStatus)[i]=0;
        PLUGIN_ctgp_SaveStartupStatus(1u);
    }
}


// Preserve the encrypted loader's timing: intercept dispatch after init, then
// arm the decoded copy return. The arm hook restores dispatch before replay.
PLUGIN_CODE_LATE(ctgp) bool PLUGIN_ctgp_ArmRelocReturnHook(void)
{
    if (!isPlgCTGP7 || g_restoreCTGP7 || !g_ctgpEntryBodyKnown || CTGP_HOST__g_memBlockSize <= 0x1100u) return false;
    // The loader maps only header.exeSize at 0x07000000 in the game. The
    // allocation's remaining bytes are the separately mapped 0x06000000 heap.
    // Both physical rendezvous stages must stop at that executable boundary.
    u32 exeSize = CTGP_HOST__PluginLoaderCtx->header.exeSize;
    if (exeSize < 0x1000u || (exeSize & 0xFFFu) || exeSize > CTGP_HOST__g_memBlockSize)
    { g_ctgpHookAddrs[7] = 0xE0u; return false; }
    u32 size = exeSize - 0x100u, offset = 0, pcOffset = 0;
    if (!PLUGIN_ctgp_FindRelocReturn((const u32*)0x07000100u,0x07000100u,size,
        g_ctgpEntryBodyAddress-0x07000100u,&offset,&pcOffset) || offset > size-8u)
    { g_ctgpHookAddrs[7] = 0xE0u; return false; }
    u32 triggerOffset = PLUGIN_ctgp_FindRelocDispatch((const u32*)0x07000100u,
        0x07000100u,size,offset);
    if (triggerOffset == 0xFFFFFFFFu || triggerOffset > size - 8u)
    { g_ctgpHookAddrs[7] = 0xE0u; return false; }
    volatile u32 *site = (volatile u32*)(0x07000100u + offset);
    volatile u32 *trigger = (volatile u32*)(0x07000100u + triggerOffset);
    g_ctgpHookAddrs[0] = (u32)site;
    g_ctgpRelocCopies[0] = 0;
    g_ctgpHookAddrs[1] = PLUGIN_ctgp_Phys(g_ctgpRelocCopies_raw);
    g_ctgpHookAddrs[2] = PLUGIN_ctgp_Phys(PLUGIN_ctgp_RelocRendezvous);
    g_ctgpHookAddrs[3] = site[0]; g_ctgpHookAddrs[4] = site[1];
    g_ctgpHookAddrs[5] = 0u; // Both modes resume the original saved-PC frame.
    g_ctgpHookAddrs[6] = 0x3Cu + pcOffset; g_ctgpHookAddrs[7] = 0x10u;
    g_ctgpRelocScanSize = size;
    g_ctgpRelocTrigger[0] = (u32)trigger;
    g_ctgpRelocTrigger[1] = trigger[0]; g_ctgpRelocTrigger[2] = trigger[1];
    trigger[1] = PLUGIN_ctgp_Phys(ctgpDumpArmHook); trigger[0] = 0xE51FF004u;
    CTGP_HOST__svcFlushEntireDataCache(); CTGP_HOST__svcInvalidateEntireInstructionCache();
    return true;
}

typedef struct
{
    u32 headerSite;
    u32 encryptSite;
    u32 decryptSite;
    u32 headerFunc;
    u32 serializer;
    u32 rng;
    u32 consoleDecrypt;
    u32 networkDecryptReturn;
    u32 miiWrapLiteral;
    u32 miiUnwrapLiteral;
    u32 original[8];
} CtgpAzaharResolved;

PLUGIN_CODE_LATE(ctgp) static u32 PLUGIN_ctgp_AzaharDecodeBL(u32 site, u32 instr)
{
    if ((instr & 0xFF000000u) != 0xEB000000u) return 0;
    s32 imm24 = (s32)(instr & 0x00FFFFFFu);
    if (imm24 & 0x00800000) imm24 |= (s32)0xFF000000u;
    return site + 8u + (u32)(imm24 * 4);
}

PLUGIN_CODE_LATE(ctgp) static u32 PLUGIN_ctgp_AzaharDecodeB(u32 site, u32 instr)
{
    if ((instr & 0xFF000000u) != 0xEA000000u) return 0;
    s32 imm24 = (s32)(instr & 0x00FFFFFFu);
    if (imm24 & 0x00800000) imm24 |= (s32)0xFF000000u;
    return site + 8u + (u32)(imm24 * 4);
}

PLUGIN_CODE_LATE(ctgp) static bool PLUGIN_ctgp_AzaharInImage(u32 address, u32 start, u32 end)
{
    return address >= start && address + 12u <= end && (address & 3u) == 0;
}

PLUGIN_CODE_LATE(ctgp) static bool PLUGIN_ctgp_AzaharMatchHeader(const u32 *p)
{
    return p[0] == 0xE59F23F8u && p[1] == 0xE59F13F8u &&
           p[2] == 0xE28D0030u && (p[3] & 0xFF000000u) == 0xEB000000u &&
           p[4] == 0xE2504000u && (p[5] & 0xFF000000u) == 0xBA000000u;
}

PLUGIN_CODE_LATE(ctgp) static bool PLUGIN_ctgp_AzaharMatchEncrypt(const u32 *p)
{
    return p[0] == 0xE59D5000u && p[1] == 0xE1A01000u &&
           p[2] == 0xE3550000u && p[3] == 0x15952000u &&
           p[4] == 0x01A02005u && p[5] == 0xE28D0014u &&
           (p[6] & 0xFF000000u) == 0xEB000000u && p[7] == 0xE3500000u &&
           p[8] == 0x13A03001u;
}

PLUGIN_CODE_LATE(ctgp) static bool PLUGIN_ctgp_AzaharMatchDecrypt(const u32 *p)
{
    return p[0] == 0xE0801005u && p[1] == 0xE2800008u &&
           (p[2] & 0xFF000000u) == 0xEB000000u && p[3] == 0xE1560000u &&
           (p[4] & 0xFF000000u) == 0x1A000000u && p[5] == 0xE1A0000Du;
}

PLUGIN_CODE_LATE(ctgp) static bool PLUGIN_ctgp_AzaharMatchMiiPair(u32 address, u32 imageStart, u32 imageEnd, CtgpAzaharResolved *out)
{
    const u32 *p = (const u32*)address;
    if (p[0] != 0xE92D4010u || p[1] != 0xE2401004u || p[2] != 0xE59F2010u || p[3] != 0xE59F0010u ||
        (p[4] & 0xFF000000u) != 0xEB000000u || p[5] != 0xE8BD4010u || p[6] != 0xE59F0004u ||
        (p[7] & 0xFF000000u) != 0xEA000000u ||
        p[10] != 0xE92D4010u || p[11] != 0xE2401004u || p[12] != 0xE59F2010u || p[13] != 0xE59F0010u ||
        (p[14] & 0xFF000000u) != 0xEB000000u || p[15] != 0xE8BD4010u || p[16] != 0xE59F0004u ||
        (p[17] & 0xFF000000u) != 0xEA000000u)
        return false;

    const u32 initTarget = PLUGIN_ctgp_AzaharDecodeBL(address + 0x10u, p[4]);
    const u32 initTarget2 = PLUGIN_ctgp_AzaharDecodeBL(address + 0x38u, p[14]);
    const u32 tailTarget = PLUGIN_ctgp_AzaharDecodeB(address + 0x1Cu, p[7]);
    const u32 tailTarget2 = PLUGIN_ctgp_AzaharDecodeB(address + 0x44u, p[17]);
    if (!initTarget || !tailTarget || initTarget != initTarget2 || tailTarget != tailTarget2)
        return false;

    const u32 wrap = p[8];
    const u32 wrapHook = p[9];
    const u32 unwrap = p[18];
    const u32 unwrapHook = p[19];
    if (!PLUGIN_ctgp_AzaharInImage(wrap, imageStart, imageEnd) ||
        !PLUGIN_ctgp_AzaharInImage(unwrap, imageStart, imageEnd) ||
        unwrapHook != wrapHook + 0x18u)
        return false;

    const u32 *wrapCode = (const u32*)wrap;
    const u32 *unwrapCode = (const u32*)unwrap;
    if (wrapCode[0] != 0xE59F3000u || wrapCode[1] != 0xE12FFF13u || wrapCode[2] != wrapHook + 8u ||
        unwrapCode[0] != 0xE59F3000u || unwrapCode[1] != 0xE12FFF13u || unwrapCode[2] != unwrapHook + 8u)
        return false;

    out->miiWrapLiteral = wrap + 8u;
    out->miiUnwrapLiteral = unwrap + 8u;
    out->original[6] = wrapCode[2];
    out->original[7] = unwrapCode[2];
    return true;
}

PLUGIN_CODE_LATE(ctgp) static Result PLUGIN_ctgp_AzaharResolve(CtgpAzaharResolved *out)
{
    const u32 imageStart = 0x07000100u;
    const u32 imageSize = CTGP_HOST__g_memBlockSize;
    if (imageSize < 0x100000u) return (Result)-0x301;
    const u32 imageEnd = imageStart + imageSize - 0x1100u;

    MemInfo mi;
    PageInfo pi;
    if (R_FAILED(CTGP_HOST__svcQueryMemory(&mi, &pi, imageStart)) ||
        mi.state == MEMSTATE_FREE || !(mi.perm & MEMPERM_READ))
        return (Result)-0x302;

    u32 headerMatch = 0, encryptMatch = 0, decryptMatch = 0;
    u32 headerCount = 0, encryptCount = 0, decryptCount = 0, miiCount = 0;

    for (u32 address = imageStart; address + 0x50u < imageEnd; address += 4u)
    {
        const u32 *p = (const u32*)address;
        if (PLUGIN_ctgp_AzaharMatchHeader(p)) { headerMatch = address; headerCount++; }
        if (PLUGIN_ctgp_AzaharMatchEncrypt(p)) { encryptMatch = address; encryptCount++; }
        if (PLUGIN_ctgp_AzaharMatchDecrypt(p)) { decryptMatch = address; decryptCount++; }
        if (PLUGIN_ctgp_AzaharMatchMiiPair(address, imageStart, imageEnd, out))
            miiCount++;
    }

    if (headerCount != 1u || encryptCount != 1u || decryptCount != 1u || miiCount != 1u)
        return (Result)-0x310;

    out->headerSite = headerMatch + 0x0Cu;
    out->encryptSite = encryptMatch + 0x18u;
    out->decryptSite = decryptMatch + 0x08u;
    out->headerFunc = PLUGIN_ctgp_AzaharDecodeBL(out->headerSite, *(u32*)out->headerSite);
    const u32 consoleEncrypt = PLUGIN_ctgp_AzaharDecodeBL(out->encryptSite, *(u32*)out->encryptSite);
    out->consoleDecrypt = PLUGIN_ctgp_AzaharDecodeBL(out->decryptSite, *(u32*)out->decryptSite);
    if (!out->headerFunc || !consoleEncrypt || !out->consoleDecrypt) return (Result)-0x303;

    const u32 commonDecrypt = decryptMatch - 0x50u;
    const u32 *commonDec = (const u32*)commonDecrypt;
    if (commonDec[0] != 0xE92D4070u || commonDec[1] != 0xE3520007u ||
        commonDec[2] != 0xE1A04000u || commonDec[3] != 0xE24DD020u ||
        PLUGIN_ctgp_AzaharDecodeBL(commonDecrypt + 0x58u, commonDec[22]) != out->consoleDecrypt)
        return (Result)-0x309;

    u32 networkDecryptCall = 0;
    u32 networkDecryptCount = 0;
    const u32 netScanEnd = headerMatch + 0x400u;
    for (u32 address = headerMatch; address + 4u <= netScanEnd && address + 4u <= imageEnd; address += 4u)
    {
        if (PLUGIN_ctgp_AzaharDecodeBL(address, *(const u32*)address) == commonDecrypt)
        {
            networkDecryptCall = address;
            networkDecryptCount++;
        }
    }
    if (networkDecryptCount != 1u) return (Result)-0x320;
    out->networkDecryptReturn = networkDecryptCall + 4u;

    const u32 *enc = (const u32*)consoleEncrypt;
    if (enc[0] != 0xE92D4FF8u || enc[1] != 0xE2423004u || enc[2] != 0xE1A07002u || enc[3] != 0xE3A02000u ||
        enc[4] != 0xE0818003u || enc[5] != 0xE7812003u || enc[6] != 0xE1A05001u || enc[7] != 0xE3E03000u ||
        enc[8] != 0xE247200Cu || enc[9] != 0xE281100Cu || (enc[10] & 0xFF000000u) != 0xEB000000u ||
        enc[11] != 0xE2506000u || (enc[17] & 0xFF000000u) != 0xEB000000u ||
        enc[65] != 0x60A2BA27u || enc[66] != 0x93949F81u || enc[67] != 0x232D7AC5u)
        return (Result)-0x304;

    out->serializer = PLUGIN_ctgp_AzaharDecodeBL(consoleEncrypt + 0x28u, enc[10]);
    out->rng = PLUGIN_ctgp_AzaharDecodeBL(consoleEncrypt + 0x44u, enc[17]);
    if (!out->serializer || !out->rng) return (Result)-0x305;

    if (*(u32*)(out->headerSite + 4u) != 0xE2504000u ||
        *(u32*)(out->encryptSite + 4u) != 0xE3500000u ||
        *(u32*)(out->decryptSite + 4u) != 0xE1560000u)
        return (Result)-0x306;

    out->original[0] = *(u32*)out->headerSite;
    out->original[1] = *(u32*)(out->headerSite + 4u);
    out->original[2] = *(u32*)out->encryptSite;
    out->original[3] = *(u32*)(out->encryptSite + 4u);
    out->original[4] = *(u32*)out->decryptSite;
    out->original[5] = *(u32*)(out->decryptSite + 4u);

    if (!PLUGIN_ctgp_IsWritableAddress(out->headerSite) || !PLUGIN_ctgp_IsWritableAddress(out->headerSite + 4u) ||
        !PLUGIN_ctgp_IsWritableAddress(out->encryptSite) || !PLUGIN_ctgp_IsWritableAddress(out->encryptSite + 4u) ||
        !PLUGIN_ctgp_IsWritableAddress(out->decryptSite) || !PLUGIN_ctgp_IsWritableAddress(out->decryptSite + 4u) ||
        !PLUGIN_ctgp_IsWritableAddress(out->miiWrapLiteral) || !PLUGIN_ctgp_IsWritableAddress(out->miiUnwrapLiteral))
        return (Result)-0x307;

    return 0;
}

PLUGIN_CODE_LATE(ctgp) static void PLUGIN_ctgp_AzaharForgetRuntime(void)
{
    g_ctgpAzaharRuntimeActive = false;
    for (u32 i = 0; i < 5; i++) g_ctgpAzaharSites[i] = 0;
    for (u32 i = 0; i < 8; i++) g_ctgpAzaharOriginal[i] = 0;
}

PLUGIN_CODE_LATE(ctgp) static bool PLUGIN_ctgp_AzaharWriteHookState(const CtgpAzaharResolved *r)
{
    u32 values[14] = {
        r->headerFunc,
        r->headerSite + 8u,
        r->serializer,
        r->rng,
        r->encryptSite + 8u,
        r->decryptSite + 8u,
        r->headerSite,
        r->encryptSite,
        r->decryptSite,
        0,
        0,
        0,
        r->consoleDecrypt,
        r->networkDecryptReturn,
    };

    for (u32 i = 0; i < 14; i++)
    {
        volatile u32 *word = (volatile u32*)PLUGIN_ctgp_AsmAlias(&g_ctgpAzaharState_raw[i]);
        if (!word)
            return false;
        *word = values[i];
    }

    CTGP_HOST__svcFlushEntireDataCache();
    return true;
}

PLUGIN_CODE_LATE(ctgp) static Result PLUGIN_ctgp_EnableAzaharRuntime(void)
{
    if (g_ctgpAzaharRuntimeActive) return 0;
    if (g_vanillaCTGP7 || g_dumpCTGP7 || g_restoreCTGP7) return (Result)-0x330;

    CtgpAzaharResolved r;
    Result res = PLUGIN_ctgp_AzaharResolve(&r);
    if (R_FAILED(res))
    {
        g_ctgpAzaharFail = (u32)res;
        return res;
    }

    if (!PLUGIN_ctgp_AzaharWriteHookState(&r))
    {
        g_ctgpAzaharFail = 0xFFFFFFFFu;
        return (Result)-0x331;
    }

    const u32 headerHook = PLUGIN_ctgp_Phys(ctgpAzaharHeaderHook);
    const u32 encryptHook = PLUGIN_ctgp_Phys(ctgpAzaharEncryptHook);
    const u32 decryptHook = PLUGIN_ctgp_Phys(ctgpAzaharDecryptHook);
    const u32 miiWrap = PLUGIN_ctgp_Phys(ctgpAzaharMiiWrap);
    const u32 miiUnwrap = PLUGIN_ctgp_Phys(ctgpAzaharMiiUnwrap);

    volatile u32 *h = (volatile u32*)r.headerSite;
    volatile u32 *e = (volatile u32*)r.encryptSite;
    volatile u32 *d = (volatile u32*)r.decryptSite;
    volatile u32 *mw = (volatile u32*)r.miiWrapLiteral;
    volatile u32 *mu = (volatile u32*)r.miiUnwrapLiteral;

    h[0] = 0xE51FF004u; h[1] = headerHook;
    e[0] = 0xE51FF004u; e[1] = encryptHook;
    d[0] = 0xE51FF004u; d[1] = decryptHook;
    *mw = miiWrap;
    *mu = miiUnwrap;
    CTGP_HOST__svcFlushEntireDataCache();
    CTGP_HOST__svcInvalidateEntireInstructionCache();

    if (h[0] != 0xE51FF004u || h[1] != headerHook ||
        e[0] != 0xE51FF004u || e[1] != encryptHook ||
        d[0] != 0xE51FF004u || d[1] != decryptHook ||
        *mw != miiWrap || *mu != miiUnwrap)
    {
        h[0] = r.original[0]; h[1] = r.original[1];
        e[0] = r.original[2]; e[1] = r.original[3];
        d[0] = r.original[4]; d[1] = r.original[5];
        *mw = r.original[6];
        *mu = r.original[7];
        CTGP_HOST__svcFlushEntireDataCache();
        CTGP_HOST__svcInvalidateEntireInstructionCache();
        g_ctgpAzaharFail = 0xFFFFFFFEu;
        return (Result)-0x308;
    }

    g_ctgpAzaharSites[0] = r.headerSite;
    g_ctgpAzaharSites[1] = r.encryptSite;
    g_ctgpAzaharSites[2] = r.decryptSite;
    g_ctgpAzaharSites[3] = r.miiWrapLiteral;
    g_ctgpAzaharSites[4] = r.miiUnwrapLiteral;
    for (u32 i = 0; i < 8; i++) g_ctgpAzaharOriginal[i] = r.original[i];
    g_ctgpAzaharRuntimeActive = true;
    g_ctgpAzaharFail = 0;
    return 0;
}

PLUGIN_CODE_LATE(ctgp) static Result PLUGIN_ctgp_DisableAzaharRuntime(void)
{
    if (!g_ctgpAzaharRuntimeActive) return 0;

    volatile u32 *h = (volatile u32*)g_ctgpAzaharSites[0];
    volatile u32 *e = (volatile u32*)g_ctgpAzaharSites[1];
    volatile u32 *d = (volatile u32*)g_ctgpAzaharSites[2];
    volatile u32 *mw = (volatile u32*)g_ctgpAzaharSites[3];
    volatile u32 *mu = (volatile u32*)g_ctgpAzaharSites[4];
    const u32 headerHook = PLUGIN_ctgp_Phys(ctgpAzaharHeaderHook);
    const u32 encryptHook = PLUGIN_ctgp_Phys(ctgpAzaharEncryptHook);
    const u32 decryptHook = PLUGIN_ctgp_Phys(ctgpAzaharDecryptHook);
    const u32 miiWrap = PLUGIN_ctgp_Phys(ctgpAzaharMiiWrap);
    const u32 miiUnwrap = PLUGIN_ctgp_Phys(ctgpAzaharMiiUnwrap);

    if (!h || !e || !d || !mw || !mu ||
        !PLUGIN_ctgp_IsWritableAddress((u32)h) || !PLUGIN_ctgp_IsWritableAddress((u32)e) ||
        !PLUGIN_ctgp_IsWritableAddress((u32)d) || !PLUGIN_ctgp_IsWritableAddress((u32)mw) ||
        !PLUGIN_ctgp_IsWritableAddress((u32)mu) ||
        h[0] != 0xE51FF004u || h[1] != headerHook ||
        e[0] != 0xE51FF004u || e[1] != encryptHook ||
        d[0] != 0xE51FF004u || d[1] != decryptHook ||
        *mw != miiWrap || *mu != miiUnwrap)
    {
        g_ctgpAzaharFail = 0xFFFFFFFDu;
        return (Result)-0x332;
    }

    h[0] = g_ctgpAzaharOriginal[0]; h[1] = g_ctgpAzaharOriginal[1];
    e[0] = g_ctgpAzaharOriginal[2]; e[1] = g_ctgpAzaharOriginal[3];
    d[0] = g_ctgpAzaharOriginal[4]; d[1] = g_ctgpAzaharOriginal[5];
    *mw = g_ctgpAzaharOriginal[6];
    *mu = g_ctgpAzaharOriginal[7];
    CTGP_HOST__svcFlushEntireDataCache();
    CTGP_HOST__svcInvalidateEntireInstructionCache();

    if (h[0] != g_ctgpAzaharOriginal[0] || h[1] != g_ctgpAzaharOriginal[1] ||
        e[0] != g_ctgpAzaharOriginal[2] || e[1] != g_ctgpAzaharOriginal[3] ||
        d[0] != g_ctgpAzaharOriginal[4] || d[1] != g_ctgpAzaharOriginal[5] ||
        *mw != g_ctgpAzaharOriginal[6] || *mu != g_ctgpAzaharOriginal[7])
    {
        g_ctgpAzaharFail = 0xFFFFFFFCu;
        return (Result)-0x333;
    }

    PLUGIN_ctgp_AzaharForgetRuntime();
    g_ctgpAzaharFail = 0;
    return 0;
}

PLUGIN_CODE_LATE(ctgp) static bool PLUGIN_ctgp_AzaharRuntimeReady(void)
{
    return !g_vanillaCTGP7 && !g_dumpCTGP7 && !g_restoreCTGP7 &&
           isPlgCTGP7 && g_ctgpToxtHookInstalled && !g_ctgpToxtScanPending;
}

static void PLUGIN_ctgp_SyncAzaharRuntimeImpl(void);

PLUGIN_CODE_LATE(ctgp) __attribute__((naked, used, noinline)) static void PLUGIN_ctgp_SyncAzaharRuntime(void)
{
    /* Keep the exact v115 0xAC-byte slot so every established callback after
       this function retains its hardware-proven address. The real v116 logic
       is appended after the old late callback block. */
    __asm__ volatile(
        "b PLUGIN_ctgp_SyncAzaharRuntimeImpl\n"
        ".space 0xA8, 0\n"
    );
}

PLUGIN_CODE_LATE(ctgp) static void PLUGIN_ctgp_SaveSettingsAzahar(void)
{
    if (!CTGP_MENU__SaveData) return;
    CtgpSettings settings;
    settings.version = CTGP_AZAHAR_SETTINGS_VERSION;
    settings.patchMode = (g_vanillaCTGP7 ? 1u : 0u) | (g_ctgpAzaharEnabled ? 2u : 0u);
    (void)CTGP_MENU__SaveData(CTGP_PLUGIN_ID, &settings, sizeof(settings));
}

PLUGIN_CODE_LATE(ctgp) void PLUGIN_ctgp_LoadSettingsAzahar(void)
{
    g_dumpCTGP7 = false;
    g_restoreCTGP7 = false;
    g_vanillaCTGP7 = false;
    g_ctgpAzaharEnabled = false;
    g_ctgpAzaharRuntimeSeen = false;
    g_ctgpAzaharFail = 0;
    PLUGIN_ctgp_AzaharForgetRuntime();
    if (!CTGP_MENU__LoadData) return;

    CtgpSettings settings;
    if (!CTGP_MENU__LoadData(CTGP_PLUGIN_ID, &settings, sizeof(settings))) return;

    if (settings.version == CTGP_AZAHAR_SETTINGS_VERSION && (settings.patchMode & ~3u) == 0)
    {
        g_vanillaCTGP7 = (settings.patchMode & 1u) != 0;
        g_ctgpAzaharEnabled = (settings.patchMode & 2u) != 0;
    }
    else if (settings.version == CTGP_SETTINGS_VERSION && settings.patchMode <= 1u)
        g_vanillaCTGP7 = settings.patchMode == 1u;
    else if (settings.version == 1u && settings.patchMode <= 3u)
        g_vanillaCTGP7 = settings.patchMode == 1u;
}

PLUGIN_CODE_LATE(ctgp) static void PLUGIN_ctgp_DrawToggleOptionsAzahar(void)
{
    const char *dumpRestore = g_restoreCTGP7 ? g_ctgpDumpRestoreRestore :
                              g_dumpCTGP7 ? g_ctgpDumpRestoreDump : g_ctgpDumpRestoreNone;
    u32 azaharColor = g_ctgpAzaharEnabled ? (g_ctgpAzaharFail ? COLOR_RED : COLOR_GREEN) : COLOR_WHITE;

    PLUGIN_ctgp_DrawFullPage(g_ctgpOptionsTitle);
    CTGP_HOST__Draw_DrawString(20, 45, COLOR_WHITE, g_vanillaCTGP7 ? g_ctgpCurrentVanilla : g_ctgpCurrentPatches);
    if (g_ctgpDumpRestoreUnlocked)
        CTGP_HOST__Draw_DrawString(20, 65, COLOR_WHITE, dumpRestore);
    CTGP_HOST__Draw_DrawString(20, 85, azaharColor, g_ctgpAzaharEnabled ? g_ctgpAzaharOn : g_ctgpAzaharOff);
    CTGP_HOST__Draw_DrawString(20, 110, COLOR_WHITE, g_ctgpToggleAzahar);
    if (g_ctgpDumpRestoreUnlocked)
        CTGP_HOST__Draw_DrawString(20, 125, COLOR_WHITE, g_ctgpCycleDumpRestore);
    CTGP_HOST__Draw_DrawString(20, 140, COLOR_WHITE, g_ctgpTogglePatchMode);
    CTGP_HOST__Draw_DrawString(20, 165, COLOR_GRAY, g_ctgpBack);
    PLUGIN_ctgp_EndFullPage();
}

PLUGIN_CODE_LATE(ctgp) __attribute__((naked, used, noinline)) static void PLUGIN_ctgp_AzaharApplyMenuState(void)
{
    /*
     * v109 compatibility-sized no-op.  Azahar runtime changes are forbidden
     * from the menu once a session exists; keeping this exact 0x94-byte slot
     * also preserves the established late function addresses referenced by
     * the sacred pre-Azahar code.
     */
    __asm__ volatile(
        "bx lr\n"
        ".space 0x90, 0\n"
    );
}

PLUGIN_CODE_LATE(ctgp) void PLUGIN_ctgp_ToggleOptionsAzahar(void)
{
    u32 unlockProgress = 0;
    PLUGIN_ctgp_DrawToggleOptionsAzahar();
    for (;;)
    {
        u32 pressed = CTGP_HOST__waitInputWithTimeout(50);
        bool changed = false;

        if (!g_ctgpDumpRestoreUnlocked && pressed)
        {
            u32 expected = unlockProgress < 2 ? KEY_DUP :
                           unlockProgress < 4 ? KEY_DDOWN :
                           unlockProgress == 4 || unlockProgress == 6 ? KEY_DLEFT : KEY_DRIGHT;
            if (pressed == expected)
            {
                unlockProgress++;
                if (unlockProgress == 8)
                {
                    g_ctgpDumpRestoreUnlocked = true;
                    changed = true;
                }
            }
            else
                unlockProgress = 0;
        }

        if (pressed & KEY_A)
        {
            // Master-loop active is the source of the 1 ms mode. Once that
            // live session has started, the saved Azahar choice is frozen.
            if (!g_CtgpMasterLoopActive)
            {
                g_ctgpAzaharEnabled = !g_ctgpAzaharEnabled;
                g_ctgpAzaharFail = 0;
                PLUGIN_ctgp_AzaharApplyMenuState();
                changed = true;
            }
        }
        else if (g_ctgpDumpRestoreUnlocked && (pressed & KEY_Y))
        {
            if (!g_dumpCTGP7 && !g_restoreCTGP7)
                g_dumpCTGP7 = true;
            else if (g_dumpCTGP7)
            {
                g_dumpCTGP7 = false;
                g_restoreCTGP7 = true;
            }
            else
                g_restoreCTGP7 = false;
            changed = true;
        }
        else if (pressed & KEY_X)
        {
            g_vanillaCTGP7 = !g_vanillaCTGP7;
            changed = true;
        }
        else if ((pressed & KEY_B) || CTGP_HOST__menuShouldExit)
        {
            PLUGIN_ctgp_SaveSettingsAzahar();
            return;
        }

        if (changed) PLUGIN_ctgp_DrawToggleOptionsAzahar();
    }
}

PLUGIN_CODE_LATE(ctgp) void PLUGIN_ctgp_OnTickAzahar(u64 delta)
{
    if (g_ctgpMountInProgress) return;
    __sync_synchronize();
    if (g_ctgpMountWakePending)
    {
        g_ctgpMountWakePending = false;
        if (isPlgCTGP7)
        {
            // Consume this signal on the worker thread. The live image may
            // already have its startup/Dump hook, so do not reset that state.
            // A launcher wake already queued before this mount is satisfied.
            g_ctgpHandledGeneration = g_ctgpWakeGeneration;
            __sync_synchronize();
            g_ctgpWaitLauncherAppear = false;
            g_ctgpAlive = false;
            g_ctgpPid = 0;
            ctgpWaitLoadPostLauncher = false;
            elapsed20sTimeout = 0;
            g_ctgpElapsed250ms = 0;
            g_CtgpMasterLoopActive = true;
            PLUGIN_ctgp_SaveStartupStatus(4u);
        }
    }
    PLUGIN_ctgp_OnTick(delta);
    PLUGIN_ctgp_SyncAzaharRuntime();
}

PLUGIN_CODE_LATE(ctgp) static void PLUGIN_ctgp_SyncAzaharRuntimeImpl(void)
{
    /*
     * Startup-selected Azahar mode. PATCHES and VANILLA may both arm the
     * bridge, but DUMP/RESTORE never do. Once published, the bridge is left
     * untouched for the lifetime of the running MK7 process. In particular,
     * VANILLA is allowed to quit the 1 ms master loop without clearing the
     * hook state underneath the still-running game.
     *
     * A later CTGP launch is detected while the master loop is active but the
     * new CTGP 3GX has not yet latched isPlgCTGP7. At that point only our
     * bookkeeping is cleared; the shared assembly state is deliberately not
     * zeroed. The next successful enable overwrites all state before publishing
     * any hook.
     */
    if (!g_CtgpMasterLoopActive)
        return;

    if (g_ctgpAzaharRuntimeSeen && !isPlgCTGP7)
    {
        g_ctgpAzaharRuntimeActive = false;
        for (u32 i = 0; i < 5; i++) g_ctgpAzaharSites[i] = 0;
        for (u32 i = 0; i < 8; i++) g_ctgpAzaharOriginal[i] = 0;
        g_ctgpAzaharRuntimeSeen = false;
        g_ctgpAzaharFail = 0;
        return;
    }

    if (g_ctgpAzaharRuntimeSeen || g_dumpCTGP7 || g_restoreCTGP7 ||
        !isPlgCTGP7 || g_ctgpToxtScanPending || !g_ctgpToxtTargetFound)
        return;

    /* PATCHES keeps the original v115 Toxt-hook readiness gate. VANILLA does
       not install that feature hook, so its equivalent late rendezvous is the
       successful Toxt target scan with no scan pending. */
    if (!g_vanillaCTGP7 && !g_ctgpToxtHookInstalled)
        return;

    g_ctgpAzaharFail = 0;
    g_ctgpAzaharRuntimeSeen = true;

    if (g_ctgpAzaharEnabled)
    {
        /* Keep the proven v115 resolver/installer bytecode untouched. It only
           rejects VANILLA via its entry guard, so temporarily present this one
           late install call as PATCHES and restore the mode immediately. No
           startup code observes this because Sync runs after the established
           CTGP tick path. */
        bool vanilla = g_vanillaCTGP7;
        g_vanillaCTGP7 = false;
        (void)PLUGIN_ctgp_EnableAzaharRuntime();
        g_vanillaCTGP7 = vanilla;
    }
}


// The host owns this RW allocation. Avoid a new alias/unmap during launch;
// the target game's executable mapping refers to these same physical pages.
PLUGIN_CODE_LATE(ctgp) static void PLUGIN_ctgp_PrepareStartup(void)
{
    const u32 base = 0x07000100u;
    const u32 *image = (const u32*)base;
    u32 exe = CTGP_HOST__PluginLoaderCtx->header.exeSize;
    g_ctgpChecksumState = 4u;
    g_ctgpEntryBodyKnown = false;
    g_ctgpEntryBodyAddress = 0;
    g_ctgpChecksumIvAddress = g_ctgpChecksumStore0 = g_ctgpChecksumStore1 = 0;
    if ((u32)CTGP_HOST__PluginLoaderCtx->memblock.memblock + 0x100u != base ||
        exe < 0x1000u || (exe & 0xFFFu) || exe > CTGP_HOST__g_memBlockSize) return;
    u32 body = PLUGIN_ctgp_FindStartupEntry(image);
    g_ctgpEntryBodyKnown = body != 0xFFFFFFFFu;
    g_ctgpEntryBodyAddress = g_ctgpEntryBodyKnown ? base + body : 0u;
    if (!g_ctgpEntryBodyKnown)
    {
        if (PLUGIN_ctgp_RuntimeHeaderValid(image)) g_ctgpChecksumState = 3u;
        return;
    }
    u32 plan[10];
    if (!PLUGIN_ctgp_FindChecksumStores(image,base,exe-0x100u,plan)) return;
    g_ctgpChecksumIvAddress = plan[4];
    g_ctgpChecksumStore0 = base + plan[0]; g_ctgpChecksumStore1 = base + plan[1];
    g_ctgpStartupStatus[9] = plan[7];
    g_ctgpStartupStatus[12] = image[plan[8]/4u];
    if (plan[9] == 2u) { g_ctgpChecksumState=2u; return; }
    // Both pages must still be readable/writable host backing pages before
    // either edit. No target process or arbitrary unmapped range is touched.
    for (u32 i=0; i<2u; i++)
    {
        MemInfo info; PageInfo page;
        u32 address=base+plan[i];
        if (R_FAILED(CTGP_HOST__svcQueryMemory(&info,&page,address)) ||
            (info.perm & MEMPERM_READWRITE) != MEMPERM_READWRITE ||
            info.state == MEMSTATE_FREE || info.base_addr > address ||
            info.size < 4u || address-info.base_addr > info.size-4u) return;
    }
    volatile u32 *first=(volatile u32*)(base+plan[0]), *second=(volatile u32*)(base+plan[1]);
    if (*first != plan[2] || *second != plan[3]) return;
    g_ctgpStartupStatus[10] = *first; g_ctgpStartupStatus[11] = *second;
    PLUGIN_ctgp_SaveStartupStatus(2u);
    *first=plan[7]; *second=plan[6];
    CTGP_HOST__svcFlushEntireDataCache(); CTGP_HOST__svcInvalidateEntireInstructionCache();
    bool ok = *first == plan[7] && *second == plan[6];
    if (!ok)
    {
        *first=plan[2]; *second=plan[3];
        CTGP_HOST__svcFlushEntireDataCache(); CTGP_HOST__svcInvalidateEntireInstructionCache();
    }
    g_ctgpStartupStatus[10] = *first; g_ctgpStartupStatus[11] = *second;
    if (ok) g_ctgpChecksumState=1u;
    // Never restore startup words over the decrypted runtime image.
}

PLUGIN_CODE_LATE(ctgp) static void PLUGIN_ctgp_SaveStartupStatus(u32 phase)
{
    // One fixed-size breadcrumb, flushed/closed while in Rosalina user mode.
    // No loop writes, kernel instrumentation, or dependency on opening a menu.
    g_ctgpStartupStatus[0]=0x38475443u; // CTG8
    g_ctgpStartupStatus[1]=2u; g_ctgpStartupStatus[2]=6u;
    g_ctgpStartupStatus[3]=phase; g_ctgpStartupStatus[4]=g_ctgpChecksumState;
    g_ctgpStartupStatus[5]=CTGP_HOST__PluginLoaderCtx->header.exeSize;
    g_ctgpStartupStatus[6]=g_ctgpEntryBodyAddress;
    g_ctgpStartupStatus[7]=g_ctgpChecksumStore0;
    g_ctgpStartupStatus[8]=g_ctgpChecksumStore1;
    g_ctgpStartupStatus[13]=(u32)g_CtgpMasterLoopActive;
    g_ctgpStartupStatus[14]=g_ctgpWakeGeneration;
    g_ctgpStartupStatus[15]=g_ctgpHandledGeneration;
    g_ctgpStartupStatus[16]=g_ctgpCommit.state;
    g_ctgpStartupStatus[17]=g_ctgpCommit.site;
    g_ctgpStartupStatus[18]=g_ctgpCommit.branch;
    g_ctgpStartupStatus[19]=g_ctgpCommit.remote;
    g_ctgpStartupStatus[20]=g_ctgpCommit.source;
    g_ctgpStartupStatus[21]=g_ctgpCommit.pid;
    g_ctgpStartupStatus[22]=0xB3282131u;
    g_ctgpStartupStatus[23]=g_ctgpCommit.matches;
    g_ctgpStartupStatus[24]=g_ctgpHookAddrs[7];
    g_ctgpStartupStatus[25]=g_ctgpCommit.querySite;
    g_ctgpStartupStatus[26]=g_ctgpCommit.queryBranch;
    g_ctgpStartupStatus[27]=g_ctgpCommit.queryMatches;
    g_ctgpStartupStatus[31]=(u32)g_dumpCTGP7 | ((u32)g_restoreCTGP7 << 1);
    FS_Archive archive; Handle f;
    if (R_FAILED(CTGP_HOST__FSUSER_OpenArchive(&archive,ARCHIVE_SDMC,
        CTGP_HOST__fsMakePath(PATH_EMPTY,NULL)))) return;
    CTGP_HOST__FSUSER_CreateDirectory(archive,CTGP_HOST__fsMakePath(PATH_ASCII,g_ctgpDumpsDir),0);
    CTGP_HOST__FSUSER_CreateDirectory(archive,CTGP_HOST__fsMakePath(PATH_ASCII,g_ctgpMemoryDir),0);
    if (R_SUCCEEDED(CTGP_HOST__FSUSER_OpenFile(&f,archive,
        CTGP_HOST__fsMakePath(PATH_ASCII,g_ctgpStartupStatusPath),FS_OPEN_WRITE|FS_OPEN_CREATE,0)))
    {
        u32 written=0;
        (void)CTGP_HOST__FSFILE_Write(f,&written,0,g_ctgpStartupStatus,sizeof(g_ctgpStartupStatus),FS_WRITE_FLUSH|FS_WRITE_UPDATE_TIME);
        CTGP_HOST__FSFILE_Close(f);
    }
    CTGP_HOST__FSUSER_CloseArchive(archive);
}

PLUGIN_CODE_LATE(ctgp) static void PLUGIN_ctgp_PublishStartup(void)
{
    PLUGIN_ctgp_SaveStartupStatus(3u);
    g_ctgpMountWakePending=true;
    __sync_synchronize();
    g_ctgpMountInProgress=false;
    __sync_synchronize();
    PLUGIN_ctgp_SetInterval(CTGP_TICK_FAST_NS);
}
