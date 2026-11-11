# Documentation

Each page explains one part of the project. Start with the architecture if
you want to understand how the machine works, or with the assembly language
page if you just want to write programs.

| Page | What it covers |
| --- | --- |
| [architecture.md](architecture.md) | The machine's state, the fetch/decode/execute cycle, error handling |
| [instruction-set.md](instruction-set.md) | Every instruction: encoding, operands, stack effect, errors |
| [assembly-language.md](assembly-language.md) | Syntax of `.asm` files and how the two-pass assembler works |
| [cli.md](cli.md) | The `mac` command: `run`, `debug`, `disasm`, flags and exit codes |
| [debugger.md](debugger.md) | The interactive terminal interface (TUI) and how it is drawn |
| [source-files.md](source-files.md) | What every file in the repository does |
| [testing.md](testing.md) | How the tests are organised and how to add new ones |

The [notes/](notes/) folder keeps the original study notes written while
following the tutorial (one of them, `oqueEhUmaStack.md`, is in Portuguese).
