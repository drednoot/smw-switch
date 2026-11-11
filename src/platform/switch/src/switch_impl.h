#pragma once

void SwitchImpl_Init();
void SwitchImpl_Exit();
void PrintErrorMessageToScreen(const char *str, ...);

// Error messages

void ThrowMissingROM();