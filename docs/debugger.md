# Interactive debugger (TUI)

`mac debug file.asm` opens a full-screen terminal interface that lets you
watch the machine execute one instruction at a time.

```
 mac debugger  examples/countdown.asm
 PROGRAM                                                    REGISTERS
  0000  SET A 5            line 2                           A=4          B=0
  0003  LOD A              line 3                           C=0          D=0
  0005  POP                line 4                           E=0          F=0
*>0006  LOD A              line 5                           IP=6    SP=-1
  0008  PSH 1              line 6                           steps=11
  0010  SUB                line 7                           running
  0011  DUP                line 8
  0012  STR A              line 9                           STACK (top first)
  0014  JNZ 3              line 10                          <empty>
  0016  HLT                line 11
                                                            OUTPUT (newest first)
                                                            4
                                                            5
 Breakpoint hit at address 6.
 s/space:step  c:continue  b:breakpoint  j/k:move  g:goto IP  r:reset  q:quit
```

## Panels

- **Program** – every instruction with its address and source line. The
  instruction that will run next (`IP`) is highlighted in green and marked
  with `>`. The selected line (cursor) is shown in reverse video. A red `*`
  marks a breakpoint.
- **Registers** – `A`..`F`, `IP`, `SP`, the number of executed steps and the
  machine status (`running`, `halted` or the error message).
- **Stack** – the values on the stack, top first, with their index.
- **Output** – values printed by `POP`/`PRN`, newest first (the last 64 are kept).
- **Status line** – the result of the last action, e.g. a breakpoint hit or
  a runtime error.

## Keys

| Key | Action |
| --- | --- |
| `s`, `n` or `Space` | Execute one instruction. |
| `c` | Continue until `HLT`, an error, or a breakpoint. Pauses after 1,000,000 steps so infinite loops cannot freeze the interface. |
| `b` | Toggle a breakpoint on the selected instruction. |
| `j` / `↓`, `k` / `↑` | Move the cursor. |
| `PgDn` / `PgUp` | Move the cursor by 10 instructions. |
| `g` | Move the cursor back to the current `IP`. |
| `r` | Reset the machine (registers, stack and output). Breakpoints are kept. |
| `q` or `Ctrl-C` | Quit. |

The terminal must be at least 60 columns by 16 rows.

## How it is implemented

[`mac/src/tui.c`](../mac/src/tui.c) uses no external library (no ncurses):

1. **Raw mode** – `termios` is used to turn off line buffering, echo and
   signal keys, so every key press reaches the program immediately. The
   original settings are saved and restored on exit (also via `atexit`).
2. **Alternate screen** – the escape code `ESC[?1049h` switches to a separate
   screen buffer, so your shell scrollback is untouched when you quit.
3. **Drawing** – each frame is built in a memory buffer using ANSI escape
   codes for cursor positioning (`ESC[row;colH`) and colours (`ESC[...m`),
   then sent with a single `write()` to avoid flicker.
4. **Input** – `read()` one byte at a time; arrow and page keys arrive as
   escape sequences (`ESC [ A`, `ESC [ 5 ~`, ...) which are decoded with a
   short timeout.
5. **Running** – the debugger owns a normal `Vm` and calls the same
   `vm_step()` as the CLI, so what you see is exactly what `mac run` does.
   The VM's `out` stream is `NULL`; printed values are read from its
   output ring buffer instead.

The debugger refuses to start when stdin or stdout is not a terminal (for
example when piped), and prints an error instead of corrupting the output.
