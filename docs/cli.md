# Command line: `mac`

The `mac` binary (built into `mac/build/mac`) is the front end that ties
the assembler, the VM and the debugger together. Its code is in
[`mac/src/main.c`](../mac/src/main.c).

```
usage: mac <command> [options] <file.asm>
```

## `mac run`

Assembles the file and executes it. Every value printed by `POP`/`PRN` goes
to **stdout**, one per line.

```console
$ ./build/mac run examples/factorial.asm
3628800
```

Options:

| Option | Effect |
| --- | --- |
| `--trace` | Before each instruction, print the address, the disassembled instruction, `sp` and the bottom of the stack to **stderr**. stdout stays clean so you can still pipe the program's output. |
| `--max-steps N` | Abort with an error after `N` instructions. Useful for programs that may loop forever. |

Trace example:

```console
$ ./build/mac run --trace examples/add.asm
[0000] PSH 5          sp=-1  stack:
[0002] PSH 6          sp=0   stack: 5
[0004] ADD            sp=1   stack: 5 6
[0005] POP            sp=0   stack: 11
11
[0006] HLT            sp=-1  stack:
```

## `mac debug`

Opens the interactive full-screen debugger. See [debugger.md](debugger.md).
It requires a real terminal on stdin and stdout.

## `mac disasm`

Assembles the file and prints each instruction with its address and source
line — handy to see what addresses your labels turned into.

```console
$ ./build/mac disasm examples/countdown.asm
0000  SET A 5          ; line 2
0003  LOD A            ; line 3
...
0014  JNZ 3            ; line 10
0016  HLT              ; line 11
```

## `mac help`

Prints the usage text (also `-h` / `--help`).

## Exit codes

| Code | Meaning |
| --- | --- |
| `0` | Program halted normally with `HLT` (or `disasm`/`help`/`debug` finished). |
| `1` | The file could not be read, assembly failed, or a runtime error happened. |
| `2` | Bad command line usage. |

## Error messages

Errors go to stderr and include the location:

```console
$ ./build/mac run bad.asm
bad.asm:2: error: unknown instruction 'BOGUS'

$ ./build/mac run div.asm
mac: runtime error at address 4 (line 3): division by zero
```
