# Architecture

`mac` is a **stack-based virtual machine**: most instructions take their
inputs from the top of a stack and push their result back onto it. A handful
of registers exist too, so values can be stored without juggling the stack.

The design follows the
[Virtual Machine in C](https://web.archive.org/web/20200121100942/https://blog.felixangell.com/virtual-machine-in-c/)
tutorial and extends it with arithmetic, comparisons, jumps, an assembler and
a debugger.

## Machine state

All state lives in one struct, `Vm`, declared in
[`mac/include/vm.h`](../mac/include/vm.h):

| Field | Meaning |
| --- | --- |
| `program`, `program_len` | The program: a read-only array of `int`s. The VM never writes to it. |
| `stack[256]` | The data stack. |
| `sp` | Stack pointer: index of the **top** element. `-1` means the stack is empty. |
| `ip` | Instruction pointer: index in `program` of the next instruction to execute. |
| `regs[6]` | General purpose registers `A`, `B`, `C`, `D`, `E`, `F`. All start at 0. |
| `running` | `true` until `HLT` executes or an error occurs. |
| `status` | The last result of `vm_step` (`VM_OK`, `VM_HALTED` or an error). |
| `steps` | How many instructions have been executed. |
| `output[64]`, `output_len` | Ring buffer of the values printed by `POP`/`PRN`. |
| `out` | Optional `FILE *`; printed values are also written there (one per line). |

Because the state is a plain struct with no globals, you can run several
machines at once, copy one, or inspect it from a test or the debugger.

## Program encoding

A program is a flat array of `int`. Each instruction is one opcode word
followed by zero, one or two operand words:

```
PSH 5      ->  [OP_PSH, 5]
ADD        ->  [OP_ADD]
SET A 42   ->  [OP_SET, 0, 42]      ; registers are encoded as 0..5
JMP loop   ->  [OP_JMP, 3]          ; labels become absolute addresses
```

Addresses are indices into this array, so `JMP 0` jumps to the first word.
The number of operands of each opcode is stored in a table (`vm_opinfo`) that
the VM, the assembler and the disassembler all share, so they cannot disagree.

## The instruction cycle

`vm_step()` in [`mac/src/vm.c`](../mac/src/vm.c) executes exactly one
instruction:

1. **Fetch** – check that `ip` is inside the program and read `program[ip]`.
2. **Decode** – look the opcode up in the opcode table. Unknown opcodes are an
   error. Check that all of its operands are inside the program.
3. **Execute** – perform the operation. The next `ip` is computed up front as
   `ip + 1 + operands`; jump instructions simply overwrite it.

`vm_run()` calls `vm_step()` in a loop until the machine stops, with an
optional step limit to protect against infinite loops.

```
          +---------+     +----------+     +-----------+
  ip ---> |  fetch  | --> |  decode  | --> |  execute  | --+
          +---------+     +----------+     +-----------+   |
               ^                                           |
               +-------------- ip = next ------------------+
```

## Stack discipline

Binary operations pop two values: `a` is the **top** of the stack and `b` the
value below it. The result is `b <op> a`, so the operand order reads naturally
in assembly:

```
PSH 10
PSH 3
SUB        ; 10 - 3 = 7
```

Before touching the stack every instruction checks that enough values are
present (`NEED`) and that there is room for the result (`ROOM`). If a check
fails the instruction does nothing at all: the stack is left exactly as it
was, which makes errors easy to inspect in the debugger.

## Errors

Instead of crashing or reading out of bounds, every problem becomes a
`VmStatus`:

| Status | Cause |
| --- | --- |
| `VM_ERR_STACK_OVERFLOW` | Pushing onto a full stack (256 values). |
| `VM_ERR_STACK_UNDERFLOW` | Popping more values than the stack holds. |
| `VM_ERR_DIV_BY_ZERO` | `DIV` or `MOD` with a zero divisor. |
| `VM_ERR_BAD_OPCODE` | The word at `ip` is not a valid opcode. |
| `VM_ERR_BAD_REGISTER` | A register operand is not 0..5. |
| `VM_ERR_BAD_ADDRESS` | `ip` or a jump target is outside the program, or an instruction's operands run past the end (e.g. a program without `HLT`). |
| `VM_ERR_STEP_LIMIT` | `vm_run` reached its `max_steps` limit (the machine is still runnable). |

On an error the machine stops (`running = false`), `status` keeps the error
and **`ip` is left pointing at the faulting instruction**. Calling `vm_step`
again just returns the same error; `vm_reset` starts over.

## Integer semantics

Values are 32-bit `int`s. Signed overflow is undefined behaviour in C, so
`ADD`, `SUB` and `MUL` are computed on `unsigned` and converted back, which
gives predictable two's complement wrap-around (`INT_MAX + 1 == INT_MIN`).
`DIV` truncates toward zero like C, and the one overflowing case
`INT_MIN / -1` is defined to give `INT_MIN` (and `INT_MIN % -1` gives `0`).

## Output

`POP` (which prints the value it removes, as in the original tutorial) and
`PRN` (prints without removing) send values to two places:

- the in-memory ring buffer, read with `vm_output_at(vm, n, &value)` where
  `n = 0` is the most recent value. Tests and the debugger use this.
- the `FILE *out` given to `vm_init`, if not `NULL`. The CLI passes `stdout`.
