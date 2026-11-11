/*
 * asm.h - assembler for the mac virtual machine.
 *
 * Turns source text such as
 *
 *     PSH 5        ; comment
 *     loop:  JMP loop
 *
 * into the int array the VM executes. See docs/assembly-language.md.
 */
#ifndef MAC_ASM_H
#define MAC_ASM_H

#include <stddef.h>

typedef struct {
    int    *code;        /* assembled program (heap allocated)           */
    size_t  len;
    int    *lines;       /* source line of each word in `code`           */
    char    error[256];  /* message when assembly fails                  */
    int     error_line;  /* 1-based, 0 when not tied to a line           */
} AsmResult;

/* Assemble `source`. Returns 0 on success, -1 on error (see out->error).
 * Always call asm_free afterwards. */
int asm_assemble(const char *source, AsmResult *out);

/* Read a whole file into a NUL terminated heap buffer (NULL on error). */
char *asm_read_file(const char *path);

void asm_free(AsmResult *r);

#endif /* MAC_ASM_H */
