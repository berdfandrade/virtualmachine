/*
 * main.c - command line front end for the mac virtual machine.
 *
 *   mac run    [--trace] [--max-steps N] file.asm
 *   mac debug  file.asm
 *   mac disasm file.asm
 */
#include "asm.h"
#include "tui.h"
#include "vm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(FILE *f)
{
    fprintf(f,
        "usage: mac <command> [options] <file.asm>\n"
        "\n"
        "commands:\n"
        "  run     assemble and execute a program\n"
        "            --trace          print every instruction as it runs\n"
        "            --max-steps N    abort after N instructions\n"
        "  debug   open the interactive terminal debugger\n"
        "  disasm  print the assembled program with addresses\n"
        "  help    show this message\n");
}

static int load(const char *path, AsmResult *prog)
{
    char *src = asm_read_file(path);
    if (!src) {
        fprintf(stderr, "mac: cannot read '%s'\n", path);
        return -1;
    }
    int rc = asm_assemble(src, prog);
    free(src);
    if (rc) {
        if (prog->error_line)
            fprintf(stderr, "%s:%d: error: %s\n", path, prog->error_line, prog->error);
        else
            fprintf(stderr, "%s: error: %s\n", path, prog->error);
        asm_free(prog);
    }
    return rc;
}

static void print_trace(const Vm *vm)
{
    char text[64];
    vm_disassemble(vm->program, vm->program_len, (size_t)vm->ip, text, sizeof text);
    fprintf(stderr, "[%04d] %-14s sp=%-3d stack:", vm->ip, text, vm->sp);
    for (int i = 0; i <= vm->sp && i < 8; i++)
        fprintf(stderr, " %d", vm->stack[i]);
    if (vm->sp >= 8)
        fprintf(stderr, " ...");
    fprintf(stderr, "\n");
}

static int cmd_run(int argc, char **argv)
{
    int trace = 0;
    unsigned long max_steps = 0;
    const char *path = NULL;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--trace") == 0) {
            trace = 1;
        } else if (strcmp(argv[i], "--max-steps") == 0 && i + 1 < argc) {
            char *end;
            max_steps = strtoul(argv[++i], &end, 10);
            if (*end) {
                fprintf(stderr, "mac: invalid --max-steps value\n");
                return 2;
            }
        } else if (!path && argv[i][0] != '-') {
            path = argv[i];
        } else {
            fprintf(stderr, "mac: unexpected argument '%s'\n", argv[i]);
            return 2;
        }
    }
    if (!path) {
        usage(stderr);
        return 2;
    }

    AsmResult prog;
    if (load(path, &prog))
        return 1;

    Vm vm;
    vm_init(&vm, prog.code, prog.len, stdout);
    VmStatus st = VM_OK;
    while (vm.running) {
        if (max_steps && vm.steps >= max_steps) {
            st = VM_ERR_STEP_LIMIT;
            break;
        }
        if (trace)
            print_trace(&vm);
        st = vm_step(&vm);
    }

    int rc = 0;
    if (st != VM_HALTED) {
        int line = (vm.ip >= 0 && (size_t)vm.ip < prog.len) ? prog.lines[vm.ip] : 0;
        fprintf(stderr, "mac: runtime error at address %d (line %d): %s\n",
                vm.ip, line, vm_status_str(st));
        rc = 1;
    }
    asm_free(&prog);
    return rc;
}

static int cmd_disasm(const char *path)
{
    AsmResult prog;
    if (load(path, &prog))
        return 1;
    char text[64];
    for (size_t a = 0; a < prog.len;) {
        size_t next = vm_disassemble(prog.code, prog.len, a, text, sizeof text);
        printf("%04zu  %-16s ; line %d\n", a, text, prog.lines[a]);
        a = next;
    }
    asm_free(&prog);
    return 0;
}

static int cmd_debug(const char *path)
{
    AsmResult prog;
    if (load(path, &prog))
        return 1;
    int rc = tui_run(path, &prog);
    asm_free(&prog);
    return rc;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        usage(stderr);
        return 2;
    }
    const char *cmd = argv[1];
    if (!strcmp(cmd, "help") || !strcmp(cmd, "-h") || !strcmp(cmd, "--help")) {
        usage(stdout);
        return 0;
    }
    if (!strcmp(cmd, "run"))
        return cmd_run(argc - 2, argv + 2);
    if ((!strcmp(cmd, "disasm") || !strcmp(cmd, "debug")) && argc == 3)
        return cmd[1] == 'i' ? cmd_disasm(argv[2]) : cmd_debug(argv[2]);

    usage(stderr);
    return 2;
}
