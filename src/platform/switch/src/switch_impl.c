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

void SwitchImpl_Init() {
    // Mount the romfs embedded in the .nro. smw_assets.dat and smw.ini live
    // there so the game works no matter what the current directory is.
    Result rc = romfsInit();
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
    socketExit();
    romfsExit();
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
