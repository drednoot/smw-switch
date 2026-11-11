#pragma once

// Called when the app is sent to the background (HOME menu, sleep), so the
// autosave can be written before the process is potentially terminated.
typedef void (*SwitchImplAutosaveCallback)(void);

void SwitchImpl_Init();
void SwitchImpl_Exit();
void SwitchImpl_SetAutosaveCallback(SwitchImplAutosaveCallback callback);
void PrintErrorMessageToScreen(const char *str, ...);

// Error messages

void ThrowMissingROM();