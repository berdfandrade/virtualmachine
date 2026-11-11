/*
 * test_vm.c - unit tests for the virtual machine core (no assembler).
 */
#include "test.h"
#include "vm.h"

#include <limits.h>

#define LEN(a) (sizeof(a) / sizeof((a)[0]))

/* Run a program to completion and return the final status. */
static VmStatus run(Vm *vm, const int *prog, size_t len)
{
    vm_init(vm, prog, len, NULL);
    return vm_run(vm, 100000);
}

static int last_output(const Vm *vm)
{
    int v = 0;
    vm_output_at(vm, 0, &v);
    return v;
}

TEST(tutorial_program_prints_eleven)
{
    const int p[] = {OP_PSH, 5, OP_PSH, 6, OP_ADD, OP_POP, OP_HLT};
    Vm vm;
    CHECK_EQ(run(&vm, p, LEN(p)), VM_HALTED);
    CHECK_EQ(vm.output_len, 1);
    CHECK_EQ(last_output(&vm), 11);
    CHECK_EQ(vm.sp, -1);
    CHECK(!vm.running);
}

TEST(init_state)
{
    const int p[] = {OP_HLT};
    Vm vm;
    vm_init(&vm, p, LEN(p), NULL);
    CHECK_EQ(vm.ip, 0);
    CHECK_EQ(vm.sp, -1);
    CHECK_EQ(vm.steps, 0);
    CHECK(vm.running);
    for (int r = 0; r < REG_COUNT; r++)
        CHECK_EQ(vm.regs[r], 0);
}

TEST(step_advances_ip_by_instruction_size)
{
    const int p[] = {OP_PSH, 1, OP_NOP, OP_SET, REG_A, 3, OP_HLT};
    Vm vm;
    vm_init(&vm, p, LEN(p), NULL);
    CHECK_EQ(vm_step(&vm), VM_OK);  CHECK_EQ(vm.ip, 2);
    CHECK_EQ(vm_step(&vm), VM_OK);  CHECK_EQ(vm.ip, 3);
    CHECK_EQ(vm_step(&vm), VM_OK);  CHECK_EQ(vm.ip, 6);
    CHECK_EQ(vm_step(&vm), VM_HALTED);
    CHECK_EQ(vm.ip, 6);             /* IP stays on HLT */
    CHECK_EQ(vm.steps, 4);
    CHECK_EQ(vm_step(&vm), VM_HALTED); /* stepping a halted VM is a no-op */
    CHECK_EQ(vm.steps, 4);
}

static int binop(int op, int b, int a, VmStatus *st)
{
    const int p[] = {OP_PSH, b, OP_PSH, a, op, OP_POP, OP_HLT};
    Vm vm;
    *st = run(&vm, p, LEN(p));
    return last_output(&vm);
}

TEST(arithmetic)
{
    VmStatus st;
    CHECK_EQ(binop(OP_ADD, 7, 3, &st), 10);   CHECK_EQ(st, VM_HALTED);
    CHECK_EQ(binop(OP_SUB, 7, 3, &st), 4);    /* second - top */
    CHECK_EQ(binop(OP_MUL, 7, 3, &st), 21);
    CHECK_EQ(binop(OP_DIV, 7, 3, &st), 2);
    CHECK_EQ(binop(OP_MOD, 7, 3, &st), 1);
    CHECK_EQ(binop(OP_DIV, -7, 2, &st), -3);  /* C truncation */
    CHECK_EQ(binop(OP_SUB, 0, 5, &st), -5);
}

TEST(arithmetic_wraps_instead_of_overflowing)
{
    VmStatus st;
    CHECK_EQ(binop(OP_ADD, INT_MAX, 1, &st), INT_MIN);
    CHECK_EQ(st, VM_HALTED);
    CHECK_EQ(binop(OP_SUB, INT_MIN, 1, &st), INT_MAX);
    CHECK_EQ(binop(OP_DIV, INT_MIN, -1, &st), INT_MIN);
    CHECK_EQ(st, VM_HALTED);
    CHECK_EQ(binop(OP_MOD, INT_MIN, -1, &st), 0);
}

TEST(comparisons)
{
    VmStatus st;
    CHECK_EQ(binop(OP_EQ, 4, 4, &st), 1);
    CHECK_EQ(binop(OP_EQ, 4, 5, &st), 0);
    CHECK_EQ(binop(OP_LT, 4, 5, &st), 1);
    CHECK_EQ(binop(OP_LT, 5, 4, &st), 0);
    CHECK_EQ(binop(OP_GT, 5, 4, &st), 1);
    CHECK_EQ(binop(OP_GT, 4, 4, &st), 0);
}

TEST(division_by_zero_is_an_error)
{
    VmStatus st;
    binop(OP_DIV, 1, 0, &st);
    CHECK_EQ(st, VM_ERR_DIV_BY_ZERO);
    binop(OP_MOD, 1, 0, &st);
    CHECK_EQ(st, VM_ERR_DIV_BY_ZERO);
}

TEST(registers)
{
    const int p[] = {
        OP_SET, REG_A, 42,
        OP_MOV, REG_F, REG_A,   /* F = A */
        OP_LOD, REG_F,          /* push F */
        OP_PSH, 1, OP_ADD,
        OP_STR, REG_B,          /* B = 43 */
        OP_HLT,
    };
    Vm vm;
    CHECK_EQ(run(&vm, p, LEN(p)), VM_HALTED);
    CHECK_EQ(vm.regs[REG_A], 42);
    CHECK_EQ(vm.regs[REG_F], 42);
    CHECK_EQ(vm.regs[REG_B], 43);
    CHECK_EQ(vm.sp, -1);
}

TEST(invalid_register_is_an_error)
{
    const int p1[] = {OP_SET, 6, 1, OP_HLT};
    const int p2[] = {OP_MOV, REG_A, -1, OP_HLT};
    const int p3[] = {OP_LOD, 99, OP_HLT};
    const int p4[] = {OP_PSH, 1, OP_STR, 7, OP_HLT};
    Vm vm;
    CHECK_EQ(run(&vm, p1, LEN(p1)), VM_ERR_BAD_REGISTER);
    CHECK_EQ(vm.ip, 0);  /* IP left on the faulting instruction */
    CHECK_EQ(run(&vm, p2, LEN(p2)), VM_ERR_BAD_REGISTER);
    CHECK_EQ(run(&vm, p3, LEN(p3)), VM_ERR_BAD_REGISTER);
    CHECK_EQ(run(&vm, p4, LEN(p4)), VM_ERR_BAD_REGISTER);
}

TEST(stack_manipulation)
{
    const int p[] = {
        OP_PSH, 1, OP_PSH, 2,
        OP_SWP,         /* 2 1 */
        OP_DUP,         /* 2 1 1 */
        OP_PRN,         /* prints 1, keeps it */
        OP_DRP,         /* 2 1 */
        OP_POP,         /* prints 1 */
        OP_POP,         /* prints 2 */
        OP_HLT,
    };
    Vm vm;
    CHECK_EQ(run(&vm, p, LEN(p)), VM_HALTED);
    CHECK_EQ(vm.output_len, 3);
    int v;
    CHECK(vm_output_at(&vm, 0, &v)); CHECK_EQ(v, 2);
    CHECK(vm_output_at(&vm, 1, &v)); CHECK_EQ(v, 1);
    CHECK(vm_output_at(&vm, 2, &v)); CHECK_EQ(v, 1);
    CHECK(!vm_output_at(&vm, 3, &v));
    CHECK_EQ(vm.sp, -1);
}

TEST(stack_underflow)
{
    const int ops[] = {OP_POP, OP_PRN, OP_DRP, OP_DUP, OP_ADD, OP_SWP, OP_JZ};
    for (size_t i = 0; i < LEN(ops); i++) {
        int p[] = {ops[i], 0, OP_HLT};
        Vm vm;
        CHECK_EQ(run(&vm, p, LEN(p)), VM_ERR_STACK_UNDERFLOW);
    }
    /* binary ops need two values */
    const int p[] = {OP_PSH, 1, OP_MUL, OP_HLT};
    Vm vm;
    CHECK_EQ(run(&vm, p, LEN(p)), VM_ERR_STACK_UNDERFLOW);
    CHECK_EQ(vm.ip, 2);
    CHECK_EQ(vm.sp, 0);  /* the stack is left untouched */
}

TEST(stack_overflow)
{
    /* loop: PSH 1 ; JMP loop */
    const int p[] = {OP_PSH, 1, OP_JMP, 0};
    Vm vm;
    CHECK_EQ(run(&vm, p, LEN(p)), VM_ERR_STACK_OVERFLOW);
    CHECK_EQ(vm.sp, VM_STACK_SIZE - 1);
}

TEST(jumps)
{
    /* 0: PSH 0  2: JZ 7  4: PSH 99  6: POP  7: PSH 1  9: JNZ 13  11: JMP 0  13: HLT */
    const int p[] = {OP_PSH, 0, OP_JZ, 7, OP_PSH, 99, OP_POP,
                     OP_PSH, 1, OP_JNZ, 13, OP_JMP, 0, OP_HLT};
    Vm vm;
    CHECK_EQ(run(&vm, p, LEN(p)), VM_HALTED);
    CHECK_EQ(vm.output_len, 0);   /* the PSH 99 / POP block was skipped */
    CHECK_EQ(vm.ip, 13);
    CHECK_EQ(vm.sp, -1);          /* conditional jumps pop their value */
}

TEST(jump_not_taken_falls_through)
{
    const int p[] = {OP_PSH, 5, OP_JZ, 0, OP_PSH, 0, OP_JNZ, 0, OP_HLT};
    Vm vm;
    CHECK_EQ(run(&vm, p, LEN(p)), VM_HALTED);
    CHECK_EQ(vm.ip, 8);
}

TEST(bad_addresses)
{
    const int jump_out[] = {OP_JMP, 100};
    const int jump_neg[] = {OP_JMP, -1};
    const int fall_off[] = {OP_NOP};
    const int truncated[] = {OP_NOP, OP_PSH};  /* operand missing */
    Vm vm;
    CHECK_EQ(run(&vm, jump_out, LEN(jump_out)), VM_ERR_BAD_ADDRESS);
    CHECK_EQ(run(&vm, jump_neg, LEN(jump_neg)), VM_ERR_BAD_ADDRESS);
    CHECK_EQ(run(&vm, fall_off, LEN(fall_off)), VM_ERR_BAD_ADDRESS);
    CHECK_EQ(vm.ip, 1);
    CHECK_EQ(run(&vm, truncated, LEN(truncated)), VM_ERR_BAD_ADDRESS);
    CHECK_EQ(vm.ip, 1);
}

TEST(bad_opcode)
{
    const int p1[] = {OP_COUNT};
    const int p2[] = {-3};
    Vm vm;
    CHECK_EQ(run(&vm, p1, LEN(p1)), VM_ERR_BAD_OPCODE);
    CHECK_EQ(run(&vm, p2, LEN(p2)), VM_ERR_BAD_OPCODE);
    CHECK_EQ(vm_step(&vm), VM_ERR_BAD_OPCODE);  /* error is sticky */
}

TEST(step_limit)
{
    const int p[] = {OP_JMP, 0};   /* infinite loop */
    Vm vm;
    vm_init(&vm, p, LEN(p), NULL);
    CHECK_EQ(vm_run(&vm, 50), VM_ERR_STEP_LIMIT);
    CHECK_EQ(vm.steps, 50);
    CHECK(vm.running);             /* can be resumed */
}

TEST(reset_restores_initial_state)
{
    const int p[] = {OP_SET, REG_C, 9, OP_PSH, 4, OP_POP, OP_HLT};
    Vm vm;
    CHECK_EQ(run(&vm, p, LEN(p)), VM_HALTED);
    vm_reset(&vm);
    CHECK_EQ(vm.ip, 0);
    CHECK_EQ(vm.sp, -1);
    CHECK_EQ(vm.regs[REG_C], 0);
    CHECK_EQ(vm.output_len, 0);
    CHECK(vm.running);
    CHECK_EQ(vm_run(&vm, 0), VM_HALTED);  /* and it runs again */
    CHECK_EQ(last_output(&vm), 4);
}

TEST(output_ring_buffer_keeps_most_recent)
{
    /* print 0..99 with a loop: A counts up */
    const int p[] = {
        /* 0 */ OP_LOD, REG_A, OP_POP,
        /* 3 */ OP_LOD, REG_A, OP_PSH, 1, OP_ADD, OP_DUP, OP_STR, REG_A,
        /* 11 */ OP_PSH, 100, OP_LT, OP_JNZ, 0,
        /* 16 */ OP_HLT,
    };
    Vm vm;
    CHECK_EQ(run(&vm, p, LEN(p)), VM_HALTED);
    CHECK_EQ(vm.output_len, 100);
    int v;
    CHECK(vm_output_at(&vm, 0, &v));  CHECK_EQ(v, 99);
    CHECK(vm_output_at(&vm, VM_OUTPUT_SIZE - 1, &v));
    CHECK_EQ(v, 100 - VM_OUTPUT_SIZE);
    CHECK(!vm_output_at(&vm, VM_OUTPUT_SIZE, &v));
}

TEST(output_is_written_to_stream)
{
    FILE *f = tmpfile();
    CHECK(f != NULL);
    if (!f)
        return;
    const int p[] = {OP_PSH, -12, OP_PRN, OP_POP, OP_HLT};
    Vm vm;
    vm_init(&vm, p, LEN(p), f);
    CHECK_EQ(vm_run(&vm, 0), VM_HALTED);
    rewind(f);
    char buf[64] = {0};
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    buf[n] = '\0';
    CHECK_STR(buf, "-12\n-12\n");
    fclose(f);
}

TEST(disassembler)
{
    const int p[] = {OP_PSH, -4, OP_SET, REG_B, 7, OP_MOV, REG_A, REG_F,
                     OP_JMP, 0, OP_ADD, 77, OP_PSH};
    size_t len = LEN(p);
    char buf[64];
    size_t a = 0;
    a = vm_disassemble(p, len, a, buf, sizeof buf); CHECK_STR(buf, "PSH -4");
    a = vm_disassemble(p, len, a, buf, sizeof buf); CHECK_STR(buf, "SET B 7");
    a = vm_disassemble(p, len, a, buf, sizeof buf); CHECK_STR(buf, "MOV A F");
    a = vm_disassemble(p, len, a, buf, sizeof buf); CHECK_STR(buf, "JMP 0");
    a = vm_disassemble(p, len, a, buf, sizeof buf); CHECK_STR(buf, "ADD");
    a = vm_disassemble(p, len, a, buf, sizeof buf); CHECK_STR(buf, ".word 77");
    a = vm_disassemble(p, len, a, buf, sizeof buf); CHECK_STR(buf, "PSH ?");
    CHECK_EQ(a, len + 1);
}

TEST(metadata)
{
    CHECK_STR(vm_opinfo(OP_PSH)->name, "PSH");
    CHECK_EQ(vm_opinfo(OP_SET)->operands, 2);
    CHECK(vm_opinfo(OP_COUNT) == NULL);
    CHECK(vm_opinfo(-1) == NULL);
    for (int op = 0; op < OP_COUNT; op++)
        CHECK(vm_opinfo(op)->name != NULL);
    CHECK_STR(vm_register_name(REG_F), "F");
    CHECK_STR(vm_register_name(6), "?");
    CHECK_STR(vm_status_str(VM_ERR_DIV_BY_ZERO), "division by zero");
}

int main(void)
{
    printf("test_vm\n");
    RUN(tutorial_program_prints_eleven);
    RUN(init_state);
    RUN(step_advances_ip_by_instruction_size);
    RUN(arithmetic);
    RUN(arithmetic_wraps_instead_of_overflowing);
    RUN(comparisons);
    RUN(division_by_zero_is_an_error);
    RUN(registers);
    RUN(invalid_register_is_an_error);
    RUN(stack_manipulation);
    RUN(stack_underflow);
    RUN(stack_overflow);
    RUN(jumps);
    RUN(jump_not_taken_falls_through);
    RUN(bad_addresses);
    RUN(bad_opcode);
    RUN(step_limit);
    RUN(reset_restores_initial_state);
    RUN(output_ring_buffer_keeps_most_recent);
    RUN(output_is_written_to_stream);
    RUN(disassembler);
    RUN(metadata);
    return test_summary("test_vm");
}
