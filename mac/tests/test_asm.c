/*
 * test_asm.c - unit tests for the assembler, plus assemble-and-run tests.
 */
#include "asm.h"
#include "test.h"
#include "vm.h"

/* Assemble `src`, expecting success, and compare with `expected`. */
static void expect_code(const char *src, const int *expected, size_t n, int line)
{
    AsmResult r;
    int rc = asm_assemble(src, &r);
    test_checks++;
    if (rc != 0) {
        FAIL_AT("line %d: assembly failed: %s (line %d)", line, r.error, r.error_line);
    } else if (r.len != n) {
        FAIL_AT("line %d: expected %zu words, got %zu", line, n, r.len);
    } else {
        for (size_t i = 0; i < n; i++)
            if (r.code[i] != expected[i]) {
                FAIL_AT("line %d: word %zu is %d, expected %d", line, i, r.code[i], expected[i]);
                break;
            }
    }
    asm_free(&r);
}

#define EXPECT_CODE(src, ...) do {                                       \
        const int e_[] = {__VA_ARGS__};                                  \
        expect_code(src, e_, sizeof e_ / sizeof e_[0], __LINE__);        \
    } while (0)

/* Assemble `src`, expecting failure at `line` with a message containing `msg`. */
#define EXPECT_ERROR(src, line, msg) do {                                \
        AsmResult r_;                                                    \
        CHECK_EQ(asm_assemble(src, &r_), -1);                            \
        CHECK_EQ(r_.error_line, line);                                   \
        if (!strstr(r_.error, msg))                                      \
            FAIL_AT("error \"%s\" does not mention \"%s\"", r_.error, msg); \
        asm_free(&r_);                                                   \
    } while (0)

/* Assemble and run `src`; return status and the most recent output value. */
static VmStatus run_src(const char *src, int *last, size_t *count)
{
    AsmResult r;
    if (asm_assemble(src, &r) != 0) {
        printf("    assembly error: %s\n", r.error);
        asm_free(&r);
        return VM_ERR_BAD_OPCODE;
    }
    Vm vm;
    vm_init(&vm, r.code, r.len, NULL);
    VmStatus st = vm_run(&vm, 1000000);
    *last = 0;
    vm_output_at(&vm, 0, last);
    *count = vm.output_len;
    asm_free(&r);
    return st;
}

TEST(tutorial_program)
{
    EXPECT_CODE("PSH 5\nPSH 6\nADD\nPOP\nHLT\n",
                OP_PSH, 5, OP_PSH, 6, OP_ADD, OP_POP, OP_HLT);
}

TEST(every_instruction_assembles)
{
    EXPECT_CODE("HLT\nPOP\nADD\nSUB\nMUL\nDIV\nMOD\nDUP\nSWP\nDRP\nPRN\nEQ\nLT\nGT\nNOP",
                OP_HLT, OP_POP, OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD, OP_DUP,
                OP_SWP, OP_DRP, OP_PRN, OP_EQ, OP_LT, OP_GT, OP_NOP);
    EXPECT_CODE("SET C 7\nMOV D E\nLOD F\nSTR A\nJMP 0\nJZ 0\nJNZ 0",
                OP_SET, REG_C, 7, OP_MOV, REG_D, REG_E, OP_LOD, REG_F,
                OP_STR, REG_A, OP_JMP, 0, OP_JZ, 0, OP_JNZ, 0);
}

TEST(case_insensitive_mnemonics_and_registers)
{
    EXPECT_CODE("psh 1\nSet a 2\nmov b, A\nhlt", OP_PSH, 1, OP_SET, REG_A, 2,
                OP_MOV, REG_B, REG_A, OP_HLT);
}

TEST(comments_whitespace_and_commas)
{
    EXPECT_CODE("; full line comment\n"
                "\n"
                "   \t PSH   3   ; trailing comment\n"
                "# hash comments too\n"
                "SET A, 4\r\n"
                "HLT",
                OP_PSH, 3, OP_SET, REG_A, 4, OP_HLT);
}

TEST(number_formats)
{
    EXPECT_CODE("PSH -7\nPSH 0x10\nPSH +3\nPSH 2147483647\nPSH -2147483648",
                OP_PSH, -7, OP_PSH, 16, OP_PSH, 3, OP_PSH, 2147483647,
                OP_PSH, (int)(-2147483647 - 1));
}

TEST(labels_resolve_to_addresses)
{
    EXPECT_CODE("start: PSH 1\n"     /* 0 */
                "loop:\n"            /* 2 */
                "  JZ end\n"         /* 2 */
                "  JMP loop\n"       /* 4 */
                "end: JNZ start\n",  /* 6 */
                OP_PSH, 1, OP_JZ, 6, OP_JMP, 2, OP_JNZ, 0);
}

TEST(forward_and_multiple_labels)
{
    EXPECT_CODE("JMP y\nx: y: z: HLT", OP_JMP, 2, OP_HLT);
}

TEST(numeric_jump_target)
{
    EXPECT_CODE("JMP 0", OP_JMP, 0);
}

TEST(line_numbers_are_recorded)
{
    AsmResult r;
    CHECK_EQ(asm_assemble("; comment\nPSH 1\n\nADD\n", &r), 0);
    CHECK_EQ(r.len, 3);
    CHECK_EQ(r.lines[0], 2);
    CHECK_EQ(r.lines[1], 2);
    CHECK_EQ(r.lines[2], 4);
    asm_free(&r);
}

TEST(errors)
{
    EXPECT_ERROR("PSH 1\nFOO\n", 2, "unknown instruction 'FOO'");
    EXPECT_ERROR("PSH\n", 1, "PSH expects 1 operand, got 0");
    EXPECT_ERROR("ADD 1\n", 1, "ADD expects 0 operands, got 1");
    EXPECT_ERROR("SET A\n", 1, "expects 2 operands");
    EXPECT_ERROR("PSH abc\n", 1, "'abc' is not a number");
    EXPECT_ERROR("PSH 99999999999\n", 1, "is not a number");
    EXPECT_ERROR("PSH 12x\n", 1, "is not a number");
    EXPECT_ERROR("SET G 1\n", 1, "'G' is not a register");
    EXPECT_ERROR("LOD 1\n", 1, "is not a register");
    EXPECT_ERROR("MOV A AB\n", 1, "is not a register");
    EXPECT_ERROR("NOP\nJMP nowhere\n", 2, "undefined label 'nowhere'");
    EXPECT_ERROR("x: NOP\nx: HLT\n", 2, "already defined on line 1");
    EXPECT_ERROR("1abc: HLT\n", 1, "invalid label name");
    EXPECT_ERROR("A: HLT\n", 1, "clashes");
    EXPECT_ERROR("add: HLT\n", 1, "clashes");
    EXPECT_ERROR("; nothing here\n\n", 0, "program is empty");
    EXPECT_ERROR("lonely:\n", 0, "program is empty");
}

TEST(long_token_is_rejected)
{
    char src[200];
    memset(src, 'x', sizeof src);
    memcpy(src, "PSH ", 4);
    src[sizeof src - 1] = '\0';
    EXPECT_ERROR(src, 1, "token too long");
}

TEST(large_program_grows_buffer)
{
    /* 1000 x "PSH 1 / DRP" + HLT: forces several reallocations */
    static char src[16 * 1024];
    size_t n = 0;
    for (int i = 0; i < 1000; i++)
        n += (size_t)snprintf(src + n, sizeof src - n, "PSH %d\nDRP\n", i);
    snprintf(src + n, sizeof src - n, "HLT\n");
    AsmResult r;
    CHECK_EQ(asm_assemble(src, &r), 0);
    CHECK_EQ(r.len, 3001);
    CHECK_EQ(r.code[2998], 999);
    CHECK_EQ(r.code[3000], OP_HLT);
    asm_free(&r);
}

TEST(run_countdown)
{
    int last; size_t count;
    VmStatus st = run_src("      SET A 3\n"
                          "loop: LOD A\n"
                          "      PRN\n"
                          "      PSH 1\n"
                          "      SUB\n"
                          "      DUP\n"
                          "      STR A\n"
                          "      JNZ loop\n"
                          "      HLT\n", &last, &count);
    CHECK_EQ(st, VM_HALTED);
    CHECK_EQ(count, 3);   /* 3, 2, 1 */
    CHECK_EQ(last, 1);
}

TEST(run_sum_1_to_100)
{
    int last; size_t count;
    VmStatus st = run_src("      SET A 100   ; counter\n"
                          "      SET B 0     ; sum\n"
                          "loop: LOD A\n"
                          "      JZ done\n"
                          "      LOD B\n"
                          "      LOD A\n"
                          "      ADD\n"
                          "      STR B\n"
                          "      LOD A\n"
                          "      PSH 1\n"
                          "      SUB\n"
                          "      STR A\n"
                          "      JMP loop\n"
                          "done: LOD B\n"
                          "      POP\n"
                          "      HLT\n", &last, &count);
    CHECK_EQ(st, VM_HALTED);
    CHECK_EQ(count, 1);
    CHECK_EQ(last, 5050);
}

TEST(run_max_of_two)
{
    int last; size_t count;
    VmStatus st = run_src("      PSH 17\n"
                          "      PSH 42\n"
                          "      STR B\n"
                          "      STR A\n"
                          "      LOD A\n"
                          "      LOD B\n"
                          "      GT\n"
                          "      JNZ a_big\n"
                          "      LOD B\n"
                          "      JMP out\n"
                          "a_big: LOD A\n"
                          "out:  POP\n"
                          "      HLT\n", &last, &count);
    CHECK_EQ(st, VM_HALTED);
    CHECK_EQ(last, 42);
}

int main(void)
{
    printf("test_asm\n");
    RUN(tutorial_program);
    RUN(every_instruction_assembles);
    RUN(case_insensitive_mnemonics_and_registers);
    RUN(comments_whitespace_and_commas);
    RUN(number_formats);
    RUN(labels_resolve_to_addresses);
    RUN(forward_and_multiple_labels);
    RUN(numeric_jump_target);
    RUN(line_numbers_are_recorded);
    RUN(errors);
    RUN(long_token_is_rejected);
    RUN(large_program_grows_buffer);
    RUN(run_countdown);
    RUN(run_sum_1_to_100);
    RUN(run_max_of_two);
    return test_summary("test_asm");
}
