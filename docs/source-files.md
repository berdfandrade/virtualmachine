# What each file does

```
.
├── README.md                    project overview
├── docs/                        this documentation
│   └── notes/                   original study notes from the tutorial
├── .github/workflows/ci.yml     GitHub Actions: build + test on Linux and macOS
├── mac/                         the C implementation
│   ├── Makefile
│   ├── include/
│   │   ├── vm.h                 public API of the virtual machine
│   │   ├── asm.h                public API of the assembler
│   │   └── tui.h                entry point of the debugger
│   ├── src/
│   │   ├── vm.c                 the machine itself
│   │   ├── asm.c                text -> bytecode
│   │   ├── tui.c                interactive terminal debugger
│   │   └── main.c               the `mac` command line tool
│   ├── examples/                sample programs
│   └── tests/                   unit and end-to-end tests
├── virtualMachinePython.py      unfinished Python sketch of the same VM
└── push.sh                      old helper script: add, commit, push
```

## `mac/include/vm.h` and `mac/src/vm.c`

The core. Defines the opcodes (`Opcode`), registers (`Register`), status
codes (`VmStatus`) and the machine state (`Vm`). Functions:

- `vm_init` / `vm_reset` – set up or restart a machine.
- `vm_step` – run one instruction (fetch, decode, execute).
- `vm_run` – loop `vm_step` until halt, error or a step limit.
- `vm_output_at` – read printed values back from the ring buffer.
- `vm_opinfo` – name and operand count of an opcode (the single source of
  truth shared by assembler and disassembler).
- `vm_disassemble` – turn one encoded instruction back into text.
- `vm_status_str`, `vm_register_name` – human readable names.

This file has no dependencies besides the C standard library and does no
I/O except writing printed values to the optional `FILE *`.

## `mac/include/asm.h` and `mac/src/asm.c`

The assembler. `asm_assemble` converts source text to an `AsmResult`
holding the code, the source line of each word and, on failure, an error
message with its line. `asm_read_file` loads a file into memory and
`asm_free` releases the result. See [assembly-language.md](assembly-language.md).

## `mac/include/tui.h` and `mac/src/tui.c`

The interactive debugger, drawn with raw ANSI escape codes. Its only public
function is `tui_run`. See [debugger.md](debugger.md).

## `mac/src/main.c`

Parses the command line and dispatches to `run`, `debug` or `disasm`.
Implements `--trace` and `--max-steps` and formats error messages with file
names and line numbers. See [cli.md](cli.md).

## `mac/Makefile`

| Target | What it does |
| --- | --- |
| `make` | Build `build/mac`. |
| `make test` | Build and run all tests. |
| `make sanitize` | Rebuild and run the tests with AddressSanitizer and UndefinedBehaviorSanitizer. |
| `make examples` | Run every program in `examples/`. |
| `make clean` | Delete `build/`. |

Compiler flags `-std=c11 -Wall -Wextra -Wpedantic` are always applied; you
can override `CC`, `CFLAGS` and `LDFLAGS` (`make CC=clang CFLAGS=-O0`).

## `mac/examples/`

| File | Shows | Prints |
| --- | --- | --- |
| `add.asm` | The tutorial's original program | `11` |
| `registers.asm` | `SET`, `MOV`, `LOD`, `STR` | `34` |
| `countdown.asm` | A loop with `DUP` + `JNZ` | `5 4 3 2 1` |
| `factorial.asm` | Loop with `JZ`, accumulator in a register | `3628800` |
| `fibonacci.asm` | Two registers updated together | first 10 Fibonacci numbers |
| `gcd.asm` | `MOD` and Euclid's algorithm | `21` |

## `mac/tests/`

| File | What it tests |
| --- | --- |
| `test.h` | Minimal test framework (`TEST`, `RUN`, `CHECK`, `CHECK_EQ`, `CHECK_STR`). |
| `test_vm.c` | Every instruction and every error status, using hand-encoded programs. |
| `test_asm.c` | Parsing, labels, number formats, error messages, and assemble-and-run programs. |
| `test_cli.sh` | Runs the real `mac` binary: example outputs, exit codes, error messages, flags. |

See [testing.md](testing.md).

## `docs/notes/`

The notes written while reading the tutorial: an introduction to the
instruction set, the fetch/decode/execute cycle and an explanation (in
Portuguese) of what a stack is, including the tutorial's first version of the
VM in C.

## `virtualMachinePython.py`

An early, unfinished attempt to port the tutorial VM to Python: its `eval`
function is still empty, so it is not a working implementation. It is not
used by the C project.
