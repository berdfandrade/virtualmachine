# Assembly language

Programs are written as plain text files (by convention ending in `.asm`)
and turned into the VM's integer array by the assembler in
[`mac/src/asm.c`](../mac/src/asm.c).

## Syntax

```asm
; Count down from 5 to 1
        SET A 5          ; register A = 5
loop:   LOD A
        POP              ; print A
        LOD A
        PSH 1
        SUB
        DUP
        STR A            ; A = A - 1
        JNZ loop         ; keep going while A != 0
        HLT
```

- **One instruction per line.** Blank lines are ignored.
- **Comments** start with `;` or `#` and run to the end of the line.
- **Mnemonics and register names are case-insensitive** (`psh`, `PSH`, `Psh`).
- **Operands** are separated by spaces, tabs or commas: `MOV A B` and
  `MOV A, B` are the same.
- **Numbers** are parsed with `strtol` base 0: `42`, `-7`, `+3`, `0x1F`
  (hex) and `017` (octal — beware of leading zeros). They must fit in a
  32-bit `int`.
- **Registers** are the single letters `A` to `F`.
- Lines can end in `\n` or `\r\n`.

## Labels

A label is a name followed by a colon at the start of a line. It stands for
the address of the next instruction:

```asm
start:  PSH 1
        JMP start     ; same as JMP 0
```

- A label may share the line with an instruction or sit alone on its own
  line. Several labels can point to the same place (`a1: a2: HLT`).
- Names start with a letter or `_`, followed by letters, digits or `_`.
  They are **case-sensitive**.
- A label cannot be named like a register (`A`..`F`) or an instruction
  (`add`, `JMP`, ...), so `loop`, `done`, `_end`, `L1` are fine but `a:` is not.
- Labels can be used before they are defined (forward references).
- Only jump instructions (`JMP`, `JZ`, `JNZ`) accept labels. They also accept
  a raw numeric address.

## How the assembler works

The assembler makes **two passes** over the source:

1. **Pass 1** tokenizes every line, records each label with the address it
   refers to, and adds up instruction sizes (`1 + operands`) to know the
   addresses. It also validates instruction names and operand counts.
2. **Pass 2** tokenizes again and emits the words: the opcode, then each
   operand converted to a number. Register operands become `0..5`, label
   operands are looked up in the table built by pass 1.

Two passes are what make forward references work: when `JMP end` is seen,
`end` may not have been reached yet, but after pass 1 its address is known.

Alongside the code, the assembler records the **source line of every word**
(`AsmResult.lines`). The CLI uses it to report runtime errors as
`line N`, and the debugger and disassembler show it next to each instruction.

### API

```c
#include "asm.h"

AsmResult r;
if (asm_assemble(source_text, &r) != 0) {
    fprintf(stderr, "line %d: %s\n", r.error_line, r.error);
} else {
    Vm vm;
    vm_init(&vm, r.code, r.len, stdout);
    vm_run(&vm, 0);
}
asm_free(&r);   /* always, even on error */
```

## Error messages

Assembly stops at the first error and reports the line number:

| Message | Example |
| --- | --- |
| `unknown instruction 'FOO'` | `FOO 1` |
| `PSH expects 1 operand, got 0` | `PSH` |
| `'abc' is not a number` | `PSH abc`, `PSH 99999999999` |
| `'G' is not a register (expected A-F)` | `SET G 1` |
| `undefined label 'nowhere'` | `JMP nowhere` |
| `label 'x' already defined on line 1` | two `x:` |
| `invalid label name '1abc'` | `1abc: HLT` |
| `label 'A' clashes with a register or instruction` | `A: HLT` |
| `program is empty` | only comments / labels |
| `token too long` / `line too long` / `too many tokens on one line` | limits: 63 chars per token, 1023 per line, 8 tokens per line |

Things the assembler does **not** check, because they are detected at run
time instead: a missing `HLT`, a numeric jump target outside the program,
stack underflow, and division by zero.
