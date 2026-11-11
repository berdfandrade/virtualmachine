/*
 * vm.h - core of the "mac" stack-based virtual machine.
 *
 * The machine has:
 *   - a read-only program (an array of ints: opcodes followed by operands)
 *   - a stack of VM_STACK_SIZE ints
 *   - six general purpose registers A..F
 *   - an instruction pointer (IP) and a stack pointer (SP)
 *
 * Execution follows the classic fetch -> decode -> execute cycle; see
 * docs/architecture.md for the details.
 */
#ifndef MAC_VM_H
#define MAC_VM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#define VM_STACK_SIZE   256
#define VM_OUTPUT_SIZE  64   /* how many printed values the VM remembers */

typedef enum {
    OP_HLT = 0, /* HLT          stop the machine                         */
    OP_PSH,     /* PSH n        push the literal n                       */
    OP_POP,     /* POP          pop the top value and print it           */
    OP_ADD,     /* ADD          pop a, pop b, push b + a                 */
    OP_SUB,     /* SUB          pop a, pop b, push b - a                 */
    OP_MUL,     /* MUL          pop a, pop b, push b * a                 */
    OP_DIV,     /* DIV          pop a, pop b, push b / a                 */
    OP_MOD,     /* MOD          pop a, pop b, push b % a                 */
    OP_SET,     /* SET r n      register r = n                           */
    OP_MOV,     /* MOV d s      register d = register s                  */
    OP_LOD,     /* LOD r        push the value of register r             */
    OP_STR,     /* STR r        pop a value into register r              */
    OP_DUP,     /* DUP          push a copy of the top value             */
    OP_SWP,     /* SWP          swap the two top values                  */
    OP_DRP,     /* DRP          pop and discard the top value            */
    OP_PRN,     /* PRN          print the top value without popping it   */
    OP_EQ,      /* EQ           pop a, pop b, push (b == a)              */
    OP_LT,      /* LT           pop a, pop b, push (b < a)               */
    OP_GT,      /* GT           pop a, pop b, push (b > a)               */
    OP_JMP,     /* JMP addr     jump to addr                             */
    OP_JZ,      /* JZ addr      pop; jump to addr if the value is 0      */
    OP_JNZ,     /* JNZ addr     pop; jump to addr if the value is not 0  */
    OP_NOP,     /* NOP          do nothing                               */
    OP_COUNT
} Opcode;

typedef enum { REG_A = 0, REG_B, REG_C, REG_D, REG_E, REG_F, REG_COUNT } Register;

typedef enum {
    VM_OK = 0,           /* instruction executed, machine still running   */
    VM_HALTED,           /* HLT executed (or machine already halted)      */
    VM_ERR_STACK_OVERFLOW,
    VM_ERR_STACK_UNDERFLOW,
    VM_ERR_DIV_BY_ZERO,
    VM_ERR_BAD_OPCODE,
    VM_ERR_BAD_REGISTER,
    VM_ERR_BAD_ADDRESS,  /* IP or jump target outside the program         */
    VM_ERR_STEP_LIMIT    /* vm_run gave up after max_steps                */
} VmStatus;

typedef struct {
    const int *program;
    size_t     program_len;

    int  stack[VM_STACK_SIZE];
    int  sp;                  /* index of the top element, -1 when empty  */
    int  ip;                  /* index of the next instruction to fetch   */
    int  regs[REG_COUNT];
    bool running;
    VmStatus status;          /* last status returned by vm_step          */
    unsigned long steps;      /* instructions executed so far             */

    /* Values produced by POP/PRN. They are always recorded here and are
     * additionally written to `out` when it is not NULL. */
    int    output[VM_OUTPUT_SIZE];
    size_t output_len;        /* total values printed (may exceed size)   */
    FILE  *out;
} Vm;

/* Information about one opcode, used by the assembler and disassembler. */
typedef struct {
    const char *name;
    int         operands;     /* number of ints following the opcode      */
} OpInfo;

/* Prepare a machine to run `program`. Output goes to `out` (may be NULL). */
void vm_init(Vm *vm, const int *program, size_t program_len, FILE *out);

/* Restore the initial state (keeps the program and output stream). */
void vm_reset(Vm *vm);

/* Execute exactly one instruction. */
VmStatus vm_step(Vm *vm);

/* Run until halt or error. max_steps == 0 means no limit. */
VmStatus vm_run(Vm *vm, unsigned long max_steps);

/* The n-th printed value counting from the most recent (0 = last). */
bool vm_output_at(const Vm *vm, size_t n_from_last, int *value);

const OpInfo *vm_opinfo(int opcode);       /* NULL for unknown opcodes */
const char   *vm_status_str(VmStatus status);
const char   *vm_register_name(int reg);    /* "A".."F" or "?" */

/* Write a human readable form of the instruction at `addr` into buf.
 * Returns the address of the next instruction. */
size_t vm_disassemble(const int *program, size_t len, size_t addr,
                      char *buf, size_t bufsize);

#endif /* MAC_VM_H */
