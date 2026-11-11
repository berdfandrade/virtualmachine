/*
 * tui.h - interactive terminal debugger for the mac virtual machine.
 */
#ifndef MAC_TUI_H
#define MAC_TUI_H

#include "asm.h"

/* Open the full screen debugger on an assembled program.
 * Returns 0 on normal exit, 1 if the terminal could not be used. */
int tui_run(const char *title, const AsmResult *prog);

#endif /* MAC_TUI_H */
