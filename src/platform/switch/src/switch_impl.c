#include "switch_impl.h"

#include <switch.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

static bool DirectoryIsWritable(void) {
    static const char kProbe[] = ".__smw_write_test";
    FILE *f = fopen(kProbe, "wb");
    if (!f)
        return false;
    fclose(f);
    remove(kProbe);
    return true;
}

static SwitchImplAutosaveCallback g_autosave_callback;
static AppletHookCookie g_applet_hook_cookie;

static void AppletHookCallback(AppletHookType hook, void *param) {
    (void)param;
    // Save when the app is sent to the background. HOME/sleep are the last
    // reliable moments before hbmenu, a forwarder or the system can terminate
    // the process without letting the main loop finish. (Exit requests while
    // still in focus are handled by the exit lock and the normal shutdown
    // path in main().)
    if (g_autosave_callback != NULL &&
        hook == AppletHookType_OnFocusState &&
        appletGetFocusState() == AppletFocusState_Background)
        g_autosave_callback();
}

void SwitchImpl_SetAutosaveCallback(SwitchImplAutosaveCallback callback) {
    g_autosave_callback = callback;
}

void SwitchImpl_Init() {
    // Without this, the system force-terminates the process as soon as an exit
    // request arrives. That skipped the autosave (and other cleanup) at the end
    // of main(), which is why autosave did not work on Switch. The 15 second
    // timeout leaves plenty of time to write the snapshot.
    Result rc = appletLockExit();
    if (R_FAILED(rc))
        fprintf(stderr, "appletLockExit failed: 0x%x\n", rc);

    appletHook(&g_applet_hook_cookie, AppletHookCallback, NULL);

    // Mount the romfs embedded in the .nro. smw_assets.dat and smw.ini live
    // there so the game works no matter what the current directory is.
    rc = romfsInit();
    if (R_FAILED(rc))
        fprintf(stderr, "romfsInit failed: 0x%x\n", rc);

    // Saves and options are written to the current directory. Applet launches
    // through hbmenu usually already point at the directory containing the
    // .nro, which is writable, but forwarders launch the game as an NSO where
    // the cwd may be missing or read-only. Fall back to a known writable
    // location on the SD card in that case.
    if (envIsNso() || !DirectoryIsWritable()) {
        mkdir("sdmc:/switch", 0755);
        mkdir("sdmc:/switch/smw", 0755);
        if (chdir("sdmc:/switch/smw") != 0)
            fprintf(stderr, "Unable to change to sdmc:/switch/smw\n");
    }

    appletSetFocusHandlingMode(AppletFocusHandlingMode_NoSuspend);
    socketInitializeDefault();
#ifdef DEBUG
    nxlinkStdio();
#endif
}

void SwitchImpl_Exit() {
    appletUnhook(&g_applet_hook_cookie);
    g_autosave_callback = NULL;
    socketExit();
    romfsExit();
    // Must be called before main() returns. If an exit was requested this is
    // where the system is finally allowed to terminate the process, after the
    // autosave and the rest of the cleanup have completed.
    appletUnlockExit();
}

// Prints a message on the console screen. Must only be used before SDL has
// initialized its video subsystem (SDL takes over the display afterwards).
void PrintErrorMessageToScreen(const char* str, ...) {
    consoleInit(NULL);

    va_list args;
    va_start(args, str);
    vprintf(str, args);
    va_end(args);

    while (appletMainLoop()) {
        consoleUpdate(NULL);
        svcSleepThread(16000000ULL);
    }

    consoleExit(NULL);
}

// Error messages

void ThrowMissingROM() {
    PrintErrorMessageToScreen(
        "\x1b[2;2HThe game data could not be loaded."
        "\x1b[4;2HPlease reinstall the app, smw_assets.dat is embedded in the .nro."
        "\x1b[6;2HIf this is a self-built .nro, rebuild it without errors."
        "\x1b[8;2HPress HOME to exit."
    );
}
