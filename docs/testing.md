# Testing

```sh
cd mac
make test        # unit tests + end-to-end CLI tests
make sanitize    # the same, built with -fsanitize=address,undefined
```

`make test` stops at the first failing suite and exits non-zero, so it can be
used in CI (see `.github/workflows/ci.yml`, which runs it with gcc and clang
on Linux and macOS, plus the sanitizer build on Linux).

## The three suites

### `tests/test_vm.c` – the machine

Programs are written directly as `int` arrays, so these tests do not depend
on the assembler. They cover:

- the result of every arithmetic, comparison and stack instruction,
- operand order (`SUB` is `second - top`),
- wrap-around on overflow and the `INT_MIN / -1` edge case,
- register reads/writes and invalid register numbers,
- stack underflow for every instruction that pops, stack overflow,
- taken and not-taken jumps, jumps outside the program, running off the end,
  truncated instructions, invalid opcodes,
- the step limit, `vm_reset`, the output ring buffer and the `FILE *` output,
- the disassembler and the metadata tables.

### `tests/test_asm.c` – the assembler

- every mnemonic, case-insensitivity, comments, commas, `\r\n` line endings,
- decimal / negative / hex numbers and 32-bit limits,
- label resolution: backward, forward, several labels on one address,
- line numbers recorded for each word,
- every error message and the line it is reported on,
- buffer growth on a 3000-word program,
- complete programs assembled **and** executed (countdown, sum 1..100, max).

### `tests/test_cli.sh` – the binary

A POSIX shell script that runs `build/mac` and compares stdout and exit
codes: all example programs, `--trace`, `--max-steps`, assembly and runtime
error messages, missing files, `disasm`, `help`, bad usage, and that `debug`
refuses to start without a terminal.

## Writing a test

```c
TEST(my_new_instruction)
{
    const int p[] = {OP_PSH, 2, OP_PSH, 3, OP_MUL, OP_POP, OP_HLT};
    Vm vm;
    vm_init(&vm, p, sizeof p / sizeof p[0], NULL);
    CHECK_EQ(vm_run(&vm, 1000), VM_HALTED);

    int v;
    CHECK(vm_output_at(&vm, 0, &v));
    CHECK_EQ(v, 6);
}
```

Then add `RUN(my_new_instruction);` to `main()` at the bottom of the file.
Failures print the file, line, expression and both values:

```
  FAIL  my_new_instruction (tests/test_vm.c:42): v == 6 (5 != 6)
```

For the CLI, add a line to `test_cli.sh`:

```sh
expect "name" "expected stdout" 0 -- "$MAC" run "$DIR/examples/file.asm"
expect_stderr "name of stderr check" 'regex to find in stderr'
```

The interactive debugger is not covered by automated tests because it needs
a real terminal; it can be exercised manually, or with `script(1)` feeding
key presses into a pseudo-terminal.
