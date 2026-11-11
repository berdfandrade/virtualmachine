/*
 * vm.c - fetch / decode / execute loop of the mac virtual machine.
 */
#include "vm.h"

#include <limits.h>
#include <string.h>

static const OpInfo OPS[OP_COUNT] = {
    [OP_HLT] = {"HLT", 0}, [OP_PSH] = {"PSH", 1}, [OP_POP] = {"POP", 0},
    [OP_ADD] = {"ADD", 0}, [OP_SUB] = {"SUB", 0}, [OP_MUL] = {"MUL", 0},
    [OP_DIV] = {"DIV", 0}, [OP_MOD] = {"MOD", 0}, [OP_SET] = {"SET", 2},
    [OP_MOV] = {"MOV", 2}, [OP_LOD] = {"LOD", 1}, [OP_STR] = {"STR", 1},
    [OP_DUP] = {"DUP", 0}, [OP_SWP] = {"SWP", 0}, [OP_DRP] = {"DRP", 0},
    [OP_PRN] = {"PRN", 0}, [OP_EQ]  = {"EQ", 0},  [OP_LT]  = {"LT", 0},
    [OP_GT]  = {"GT", 0},  [OP_JMP] = {"JMP", 1}, [OP_JZ]  = {"JZ", 1},
    [OP_JNZ] = {"JNZ", 1}, [OP_NOP] = {"NOP", 0},
};

static const char *REG_NAMES[REG_COUNT] = {"A", "B", "C", "D", "E", "F"};

const OpInfo *vm_opinfo(int opcode)
{
    if (opcode < 0 || opcode >= OP_COUNT)
        return NULL;
    return &OPS[opcode];
}

const char *vm_register_name(int reg)
{
    return (reg >= 0 && reg < REG_COUNT) ? REG_NAMES[reg] : "?";
}

const char *vm_status_str(VmStatus s)
{
    switch (s) {
    case VM_OK:                  return "ok";
    case VM_HALTED:              return "halted";
    case VM_ERR_STACK_OVERFLOW:  return "stack overflow";
    case VM_ERR_STACK_UNDERFLOW: return "stack underflow";
    case VM_ERR_DIV_BY_ZERO:     return "division by zero";
    case VM_ERR_BAD_OPCODE:      return "invalid opcode";
    case VM_ERR_BAD_REGISTER:    return "invalid register";
    case VM_ERR_BAD_ADDRESS:     return "address out of program bounds";
    case VM_ERR_STEP_LIMIT:      return "step limit reached";
    }
    return "unknown status";
}

void vm_init(Vm *vm, const int *program, size_t program_len, FILE *out)
{
    memset(vm, 0, sizeof *vm);
    vm->program = program;
    vm->program_len = program_len;
    vm->out = out;
    vm_reset(vm);
}

void vm_reset(Vm *vm)
{
    memset(vm->stack, 0, sizeof vm->stack);
    memset(vm->regs, 0, sizeof vm->regs);
    memset(vm->output, 0, sizeof vm->output);
    vm->output_len = 0;
    vm->sp = -1;
    vm->ip = 0;
    vm->steps = 0;
    vm->running = true;
    vm->status = VM_OK;
}

bool vm_output_at(const Vm *vm, size_t n, int *value)
{
    if (n >= vm->output_len || n >= VM_OUTPUT_SIZE)
        return false;
    *value = vm->output[(vm->output_len - 1 - n) % VM_OUTPUT_SIZE];
    return true;
}

/* ---- helpers ---------------------------------------------------------- */

static void emit(Vm *vm, int value)
{
    vm->output[vm->output_len % VM_OUTPUT_SIZE] = value;
    vm->output_len++;
    if (vm->out) {
        fprintf(vm->out, "%d\n", value);
        fflush(vm->out);
    }
}

#define NEED(n) do { if (vm->sp + 1 < (n)) return VM_ERR_STACK_UNDERFLOW; } while (0)
#define ROOM(n) do { if (vm->sp + (n) >= VM_STACK_SIZE) return VM_ERR_STACK_OVERFLOW; } while (0)

static bool valid_reg(int r) { return r >= 0 && r < REG_COUNT; }

/* Arithmetic on unsigned to get well defined wrap-around instead of UB. */
static int wrap_add(int a, int b) { return (int)((unsigned)a + (unsigned)b); }
static int wrap_sub(int a, int b) { return (int)((unsigned)a - (unsigned)b); }
static int wrap_mul(int a, int b) { return (int)((unsigned)a * (unsigned)b); }

/* Decode and execute one instruction. `next` is the address right after the
 * instruction and its operands; jumps overwrite it. */
static VmStatus execute(Vm *vm, int op, const int *arg, int *next)
{
    switch (op) {
    case OP_HLT:
        vm->running = false;
        return VM_HALTED;

    case OP_NOP:
        return VM_OK;

    case OP_PSH:
        ROOM(1);
        vm->stack[++vm->sp] = arg[0];
        return VM_OK;

    case OP_POP:
        NEED(1);
        emit(vm, vm->stack[vm->sp--]);
        return VM_OK;

    case OP_PRN:
        NEED(1);
        emit(vm, vm->stack[vm->sp]);
        return VM_OK;

    case OP_DRP:
        NEED(1);
        vm->sp--;
        return VM_OK;

    case OP_DUP:
        NEED(1);
        ROOM(1);
        vm->stack[vm->sp + 1] = vm->stack[vm->sp];
        vm->sp++;
        return VM_OK;

    case OP_SWP: {
        NEED(2);
        int t = vm->stack[vm->sp];
        vm->stack[vm->sp] = vm->stack[vm->sp - 1];
        vm->stack[vm->sp - 1] = t;
        return VM_OK;
    }

    case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD:
    case OP_EQ:  case OP_LT:  case OP_GT: {
        NEED(2);
        int a = vm->stack[vm->sp];      /* top    */
        int b = vm->stack[vm->sp - 1];  /* second */
        int r = 0;
        switch (op) {
        case OP_ADD: r = wrap_add(b, a); break;
        case OP_SUB: r = wrap_sub(b, a); break;
        case OP_MUL: r = wrap_mul(b, a); break;
        case OP_DIV:
        case OP_MOD:
            if (a == 0)
                return VM_ERR_DIV_BY_ZERO;
            if (b == INT_MIN && a == -1)   /* would overflow in C */
                r = (op == OP_DIV) ? INT_MIN : 0;
            else
                r = (op == OP_DIV) ? b / a : b % a;
            break;
        case OP_EQ: r = b == a; break;
        case OP_LT: r = b < a;  break;
        case OP_GT: r = b > a;  break;
        }
        vm->sp--;
        vm->stack[vm->sp] = r;
        return VM_OK;
    }

    case OP_SET:
        if (!valid_reg(arg[0]))
            return VM_ERR_BAD_REGISTER;
        vm->regs[arg[0]] = arg[1];
        return VM_OK;

    case OP_MOV:
        if (!valid_reg(arg[0]) || !valid_reg(arg[1]))
            return VM_ERR_BAD_REGISTER;
        vm->regs[arg[0]] = vm->regs[arg[1]];
        return VM_OK;

    case OP_LOD:
        if (!valid_reg(arg[0]))
            return VM_ERR_BAD_REGISTER;
        ROOM(1);
        vm->stack[++vm->sp] = vm->regs[arg[0]];
        return VM_OK;

    case OP_STR:
        if (!valid_reg(arg[0]))
            return VM_ERR_BAD_REGISTER;
        NEED(1);
        vm->regs[arg[0]] = vm->stack[vm->sp--];
        return VM_OK;

    case OP_JMP:
    case OP_JZ:
    case OP_JNZ: {
        int target = arg[0];
        if (target < 0 || (size_t)target >= vm->program_len)
            return VM_ERR_BAD_ADDRESS;
        bool take = true;
        if (op != OP_JMP) {
            NEED(1);
            int v = vm->stack[vm->sp--];
            take = (op == OP_JZ) ? v == 0 : v != 0;
        }
        if (take)
            *next = target;
        return VM_OK;
    }
    }
    return VM_ERR_BAD_OPCODE;
}

VmStatus vm_step(Vm *vm)
{
    if (!vm->running)
        return vm->status == VM_OK ? VM_HALTED : vm->status;

    VmStatus st;
    const OpInfo *info;

    /* fetch */
    if (vm->ip < 0 || (size_t)vm->ip >= vm->program_len) {
        st = VM_ERR_BAD_ADDRESS;
        goto fail;
    }
    int op = vm->program[vm->ip];

    /* decode */
    info = vm_opinfo(op);
    if (!info) {
        st = VM_ERR_BAD_OPCODE;
        goto fail;
    }
    if ((size_t)vm->ip + (size_t)info->operands >= vm->program_len) {
        st = VM_ERR_BAD_ADDRESS;  /* operands run past the end */
        goto fail;
    }

    /* execute */
    int next = vm->ip + 1 + info->operands;
    st = execute(vm, op, &vm->program[vm->ip + 1], &next);
    if (st != VM_OK && st != VM_HALTED)
        goto fail;

    vm->steps++;
    vm->status = st;
    if (st == VM_OK)
        vm->ip = next;
    return st;

fail:
    /* Leave IP on the faulting instruction so it can be inspected. */
    vm->running = false;
    vm->status = st;
    return st;
}

VmStatus vm_run(Vm *vm, unsigned long max_steps)
{
    VmStatus st = VM_OK;
    unsigned long n = 0;
    while (vm->running) {
        if (max_steps && n++ >= max_steps)
            return VM_ERR_STEP_LIMIT;
        st = vm_step(vm);
    }
    return vm->status == VM_OK ? st : vm->status;
}

size_t vm_disassemble(const int *program, size_t len, size_t addr,
                      char *buf, size_t bufsize)
{
    if (addr >= len) {
        snprintf(buf, bufsize, "<end>");
        return addr + 1;
    }
    int op = program[addr];
    const OpInfo *info = vm_opinfo(op);
    if (!info) {
        snprintf(buf, bufsize, ".word %d", op);
        return addr + 1;
    }
    int n = snprintf(buf, bufsize, "%s", info->name);
    for (int i = 0; i < info->operands; i++) {
        size_t a = addr + 1 + (size_t)i;
        if (n < 0 || (size_t)n >= bufsize)
            break;
        if (a >= len) {
            n += snprintf(buf + n, bufsize - (size_t)n, " ?");
            continue;
        }
        int v = program[a];
        bool is_reg = op == OP_SET ? i == 0
                    : op == OP_MOV || op == OP_LOD || op == OP_STR;
        if (is_reg)
            n += snprintf(buf + n, bufsize - (size_t)n, " %s", vm_register_name(v));
        else
            n += snprintf(buf + n, bufsize - (size_t)n, " %d", v);
    }
    return addr + 1 + (size_t)info->operands;
}
