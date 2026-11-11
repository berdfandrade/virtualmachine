# mac — a tiny stack-based virtual machine in C

[![CI](https://github.com/berdfandrade/virtualmachine/actions/workflows/ci.yml/badge.svg)](https://github.com/berdfandrade/virtualmachine/actions/workflows/ci.yml)

`mac` (short for *máquina*) is a small, readable virtual machine written in
C11, together with an **assembler**, a **command line runner** and an
**interactive terminal debugger**. It started as an implementation of the
[Virtual Machine in C](https://web.archive.org/web/20200121100942/https://blog.felixangell.com/virtual-machine-in-c/)
tutorial and grew from there.

```asm
; examples/factorial.asm — compute 10!
        SET A 10        ; counter
        SET B 1         ; accumulator
loop:   LOD A
        JZ done         ; stop when the counter reaches 0
        LOD B
        LOD A
        MUL
        STR B           ; B = B * A
        LOD A
        PSH 1
        SUB
        STR A           ; A = A - 1
        JMP loop
done:   LOD B
        POP             ; prints 3628800
        HLT
```

```console
$ ./build/mac run examples/factorial.asm
3628800
```

## Features

- **Stack machine** with a 256-entry stack and six general purpose registers
  (`A`–`F`).
- **23 instructions**: arithmetic, comparisons, stack manipulation,
  registers, conditional and unconditional jumps.
- **Safe by design**: stack overflow/underflow, division by zero, invalid
  opcodes, registers and jump targets are reported as errors — the VM never
  reads or writes out of bounds, and integer overflow wraps instead of being
  undefined behaviour.
- **Two-pass assembler** with labels (including forward references),
  comments, hex numbers and precise error messages with line numbers.
- **Terminal debugger** to step through programs, set breakpoints and watch
  the registers, stack and output change — no ncurses required.
- **Tracing** (`--trace`) and **step limits** (`--max-steps`) in the runner.
- **No dependencies** beyond a C11 compiler and a POSIX system.
- **Tested**: 37 unit tests, end-to-end CLI tests, sanitizer builds, CI on
  Linux and macOS with gcc and clang.

## Quick start

Requirements: a C compiler (`gcc` or `clang`), `make`, and Linux or macOS
(or WSL on Windows).

```sh
git clone https://github.com/berdfandrade/virtualmachine.git
cd virtualmachine/mac
make            # builds build/mac
make test       # runs the whole test suite
make examples   # runs every program in examples/
```

Run a program:

```sh
./build/mac run examples/fibonacci.asm
```

Watch it run, one instruction at a time:

```sh
./build/mac debug examples/countdown.asm
```

## The debugger

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

| Key | Action |
| --- | --- |
| `s` / `Space` | step one instruction |
| `c` | continue to the next breakpoint, `HLT` or error |
| `b` | toggle a breakpoint on the selected line |
| `j` `k` / arrows | move the selection |
| `r` | reset the machine |
| `q` | quit |

More in [docs/debugger.md](docs/debugger.md).

## Command line

```
mac run    [--trace] [--max-steps N] file.asm   assemble and execute
mac debug  file.asm                             interactive debugger
mac disasm file.asm                             show addresses and encoding
mac help
```

Exit codes: `0` success, `1` read/assembly/runtime error, `2` bad usage.
Errors point to the source line:

```console
$ ./build/mac run div.asm
mac: runtime error at address 4 (line 3): division by zero
```

More in [docs/cli.md](docs/cli.md).

## Instruction set

`a` is the top of the stack, `b` the value below it.

| Instruction | Effect | | Instruction | Effect |
| --- | --- | --- | --- | --- |
| `PSH n` | push `n` | | `SET r n` | `r = n` |
| `POP` | pop and **print** | | `MOV d s` | `d = s` |
| `DRP` | pop silently | | `LOD r` | push `r` |
| `PRN` | print top, keep it | | `STR r` | pop into `r` |
| `DUP` | duplicate top | | `EQ` | push `b == a` |
| `SWP` | swap top two | | `LT` | push `b < a` |
| `ADD` | push `b + a` | | `GT` | push `b > a` |
| `SUB` | push `b - a` | | `JMP addr` | jump |
| `MUL` | push `b * a` | | `JZ addr` | pop, jump if zero |
| `DIV` | push `b / a` | | `JNZ addr` | pop, jump if not zero |
| `MOD` | push `b % a` | | `NOP` | nothing |
| `HLT` | stop | | | |

Full reference with encodings and stack effects:
[docs/instruction-set.md](docs/instruction-set.md). Assembly syntax
(labels, comments, numbers): [docs/assembly-language.md](docs/assembly-language.md).

## How it works

A program is a flat array of integers — an opcode followed by its operands.
The VM repeats the classic **fetch → decode → execute** cycle:

```c
int op = program[ip];                         // fetch
const OpInfo *info = vm_opinfo(op);           // decode
int next = ip + 1 + info->operands;
execute(vm, op, &program[ip + 1], &next);     // execute (jumps change next)
ip = next;
```

The whole machine state is one struct (`Vm`), so it can be embedded in any C
program:

```c
#include "asm.h"
#include "vm.h"

AsmResult prog;
if (asm_assemble("PSH 2\nPSH 3\nMUL\nPOP\nHLT\n", &prog) == 0) {
    Vm vm;
    vm_init(&vm, prog.code, prog.len, stdout);
    VmStatus st = vm_run(&vm, 0);            // prints 6
    if (st != VM_HALTED)
        fprintf(stderr, "error: %s\n", vm_status_str(st));
}
asm_free(&prog);
```

Read [docs/architecture.md](docs/architecture.md) for the details: state,
encoding, operand order, error handling and integer semantics.

## Project layout

```
mac/
├── include/     vm.h, asm.h, tui.h — public headers
├── src/         vm.c (machine), asm.c (assembler), tui.c (debugger), main.c (CLI)
├── examples/    sample .asm programs
├── tests/       unit tests (C) and end-to-end tests (shell)
└── Makefile
docs/            documentation for every part of the project
```

What every file does: [docs/source-files.md](docs/source-files.md).

## Testing

```sh
cd mac
make test        # unit + end-to-end tests
make sanitize    # same, with AddressSanitizer + UndefinedBehaviorSanitizer
```

The suites cover every instruction, every error status, assembler parsing
and error messages, and the real binary's output and exit codes. See
[docs/testing.md](docs/testing.md).

## Documentation

| Page | Topic |
| --- | --- |
| [docs/architecture.md](docs/architecture.md) | How the machine works |
| [docs/instruction-set.md](docs/instruction-set.md) | Instruction reference |
| [docs/assembly-language.md](docs/assembly-language.md) | Writing programs, how the assembler works |
| [docs/cli.md](docs/cli.md) | The `mac` command |
| [docs/debugger.md](docs/debugger.md) | The terminal debugger |
| [docs/source-files.md](docs/source-files.md) | What each file does |
| [docs/testing.md](docs/testing.md) | Running and writing tests |
| [docs/notes/](docs/notes/) | Original study notes from the tutorial |

## Ideas for extending it

- `CALL` / `RET` with a return-address stack, to write subroutines
- memory instructions (`LDM addr` / `STM addr`) for a data segment
- reading input (`INP`) so programs can be interactive
- saving assembled programs to a binary file and loading them back
- showing label names in the disassembler and debugger

## Credits

Based on Felix Angell's
[Virtual Machine in C](https://web.archive.org/web/20200121100942/https://blog.felixangell.com/virtual-machine-in-c/)
tutorial.
