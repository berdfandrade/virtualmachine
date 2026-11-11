# Instruction set

There are 23 instructions. Mnemonics are three letters (two for the
comparisons) and are case-insensitive in assembly.

Notation for the stack effect column: `( before -- after )`, with the top of
the stack on the **right**. `a` is always the top value and `b` the one below.

## Reference

| Opcode | Mnemonic | Operands | Stack effect | Description |
| ---: | --- | --- | --- | --- |
| 0 | `HLT` | – | `( -- )` | Stop the machine. |
| 1 | `PSH n` | number | `( -- n )` | Push the literal `n`. |
| 2 | `POP` | – | `( a -- )` | Remove the top value **and print it**. |
| 3 | `ADD` | – | `( b a -- b+a )` | Addition (wraps on overflow). |
| 4 | `SUB` | – | `( b a -- b-a )` | Subtraction (wraps on overflow). |
| 5 | `MUL` | – | `( b a -- b*a )` | Multiplication (wraps on overflow). |
| 6 | `DIV` | – | `( b a -- b/a )` | Integer division, truncates toward zero. Error if `a == 0`. |
| 7 | `MOD` | – | `( b a -- b%a )` | Remainder (sign follows `b`). Error if `a == 0`. |
| 8 | `SET r n` | register, number | `( -- )` | `r = n`. |
| 9 | `MOV d s` | register, register | `( -- )` | `d = s` (copy register `s` into `d`). |
| 10 | `LOD r` | register | `( -- r )` | Push the value of register `r`. |
| 11 | `STR r` | register | `( a -- )` | Pop the top value into register `r`. |
| 12 | `DUP` | – | `( a -- a a )` | Duplicate the top value. |
| 13 | `SWP` | – | `( b a -- a b )` | Swap the two top values. |
| 14 | `DRP` | – | `( a -- )` | Discard the top value without printing it. |
| 15 | `PRN` | – | `( a -- a )` | Print the top value, leaving it on the stack. |
| 16 | `EQ` | – | `( b a -- b==a )` | Push `1` if equal, else `0`. |
| 17 | `LT` | – | `( b a -- b<a )` | Push `1` if `b < a`, else `0`. |
| 18 | `GT` | – | `( b a -- b>a )` | Push `1` if `b > a`, else `0`. |
| 19 | `JMP addr` | address | `( -- )` | Continue execution at `addr`. |
| 20 | `JZ addr` | address | `( a -- )` | Pop; jump to `addr` if the value is `0`. |
| 21 | `JNZ addr` | address | `( a -- )` | Pop; jump to `addr` if the value is not `0`. |
| 22 | `NOP` | – | `( -- )` | Do nothing. |

Registers are encoded as `A=0, B=1, C=2, D=3, E=4, F=5`.

## Notes

- **`MOV` is destination first**, the same order as `SET` (`SET A 5` writes
  `A`, `MOV A B` writes `A`). This is the opposite of the example in the
  original tutorial's text but keeps every register-writing instruction
  consistent.
- **Conditional jumps consume their condition.** This is why loops often use
  `DUP` before `JNZ` when they still need the value afterwards (see
  `examples/countdown.asm`).
- **Jump targets are checked when the jump executes**: a jump outside the
  program fails with `VM_ERR_BAD_ADDRESS` instead of jumping into garbage.
- **There is no fall-through past the end.** A program must stop with `HLT`;
  running off the end is a `VM_ERR_BAD_ADDRESS` error.
- `POP` prints because that is how the original tutorial lets you see
  results. Use `DRP` when you want to throw a value away silently.

## Common idioms

```asm
; increment register A
LOD A
PSH 1
ADD
STR A

; if (A == 10) goto done
LOD A
PSH 10
EQ
JNZ done

; while (A != 0) { ... A = A - 1 }
loop:   LOD A
        JZ end
        ; ... body ...
        LOD A
        PSH 1
        SUB
        STR A
        JMP loop
end:    HLT

; logical NOT of the top value
PSH 0
EQ
```

## Adding an instruction

1. Add the opcode to the `Opcode` enum in `mac/include/vm.h` (before `OP_COUNT`).
2. Add its name and operand count to the `OPS` table in `mac/src/vm.c`.
3. Implement it in `execute()` in `mac/src/vm.c`.
4. If an operand is a register or an address, update
   `operand_is_register` / `operand_is_address` in `mac/src/asm.c` and the
   `is_reg` check in `vm_disassemble`.
5. Add tests in `mac/tests/test_vm.c` and document it on this page.
