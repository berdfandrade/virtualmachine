/*
 * asm.c - two pass assembler.
 *
 * Pass 1 walks the source, records the address of every label and counts
 * how many words each instruction needs. Pass 2 walks it again and emits
 * the words, resolving label references with the table from pass 1.
 */
#include "asm.h"
#include "vm.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TOKEN   64
#define MAX_LABELS  512
#define MAX_TOKENS  8

typedef struct {
    char name[MAX_TOKEN];
    int  addr;
    int  line;
} Label;

typedef struct {
    Label  labels[MAX_LABELS];
    size_t nlabels;
    AsmResult *out;
    size_t cap;
} Ctx;

static int fail(Ctx *c, int line, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(c->out->error, sizeof c->out->error, fmt, ap);
    va_end(ap);
    c->out->error_line = line;
    return -1;
}

static int ci_equal(const char *a, const char *b)
{
    for (; *a && *b; a++, b++)
        if (toupper((unsigned char)*a) != toupper((unsigned char)*b))
            return 0;
    return *a == *b;
}

static int find_opcode(const char *name)
{
    for (int op = 0; op < OP_COUNT; op++)
        if (ci_equal(vm_opinfo(op)->name, name))
            return op;
    return -1;
}

static int parse_register(const char *s)
{
    if (s[0] && !s[1]) {
        int r = toupper((unsigned char)s[0]) - 'A';
        if (r >= 0 && r < REG_COUNT)
            return r;
    }
    return -1;
}

static int parse_number(const char *s, int *value)
{
    char *end;
    errno = 0;
    long v = strtol(s, &end, 0);   /* accepts 10, -3, 0x1F */
    if (end == s || *end || errno || v < INT_MIN || v > INT_MAX)
        return -1;
    *value = (int)v;
    return 0;
}

static int valid_label_name(const char *s)
{
    if (!(isalpha((unsigned char)*s) || *s == '_'))
        return 0;
    for (s++; *s; s++)
        if (!(isalnum((unsigned char)*s) || *s == '_'))
            return 0;
    return 1;
}

static Label *find_label(Ctx *c, const char *name)
{
    for (size_t i = 0; i < c->nlabels; i++)
        if (strcmp(c->labels[i].name, name) == 0)
            return &c->labels[i];
    return NULL;
}

/* Split one line into tokens. Commas and whitespace separate tokens and
 * everything after ';' or '#' is a comment. */
static int tokenize(Ctx *c, int lineno, const char *line,
                    char tok[MAX_TOKENS][MAX_TOKEN], int *ntok)
{
    *ntok = 0;
    const char *p = line;
    for (;;) {
        while (*p == ' ' || *p == '\t' || *p == ',' || *p == '\r')
            p++;
        if (!*p || *p == '\n' || *p == ';' || *p == '#')
            return 0;
        if (*ntok == MAX_TOKENS)
            return fail(c, lineno, "too many tokens on one line");
        size_t n = 0;
        while (*p && !strchr(" \t,\r\n;#", *p)) {
            if (n + 1 >= MAX_TOKEN)
                return fail(c, lineno, "token too long");
            tok[*ntok][n++] = *p++;
        }
        tok[*ntok][n] = '\0';
        (*ntok)++;
    }
}

static int emit(Ctx *c, int word, int line)
{
    AsmResult *o = c->out;
    if (o->len == c->cap) {
        size_t cap = c->cap ? c->cap * 2 : 64;
        int *code = realloc(o->code, cap * sizeof *code);
        if (!code)
            return fail(c, line, "out of memory");
        o->code = code;
        int *lines = realloc(o->lines, cap * sizeof *lines);
        if (!lines)
            return fail(c, line, "out of memory");
        o->lines = lines;
        c->cap = cap;
    }
    o->code[o->len] = word;
    o->lines[o->len] = line;
    o->len++;
    return 0;
}

/* Is operand `i` of opcode `op` a register (as opposed to a number/label)? */
static int operand_is_register(int op, int i)
{
    return (op == OP_SET && i == 0) || op == OP_MOV || op == OP_LOD || op == OP_STR;
}

static int operand_is_address(int op)
{
    return op == OP_JMP || op == OP_JZ || op == OP_JNZ;
}

static int process(Ctx *c, const char *src, int pass)
{
    int addr = 0;
    int lineno = 0;
    const char *p = src;

    while (*p) {
        lineno++;
        const char *eol = strchr(p, '\n');
        size_t linelen = eol ? (size_t)(eol - p) : strlen(p);
        char line[1024];
        if (linelen >= sizeof line)
            return fail(c, lineno, "line too long");
        memcpy(line, p, linelen);
        line[linelen] = '\0';
        p += linelen + (eol ? 1 : 0);

        char tok[MAX_TOKENS][MAX_TOKEN];
        int ntok, t = 0;
        if (tokenize(c, lineno, line, tok, &ntok))
            return -1;

        /* leading labels, e.g. "loop:" (several are allowed) */
        while (t < ntok) {
            size_t n = strlen(tok[t]);
            if (n == 0 || tok[t][n - 1] != ':')
                break;
            tok[t][n - 1] = '\0';
            if (pass == 1) {
                if (!valid_label_name(tok[t]))
                    return fail(c, lineno, "invalid label name '%s'", tok[t]);
                if (parse_register(tok[t]) >= 0 || find_opcode(tok[t]) >= 0)
                    return fail(c, lineno, "label '%s' clashes with a register or instruction", tok[t]);
                Label *prev = find_label(c, tok[t]);
                if (prev)
                    return fail(c, lineno, "label '%s' already defined on line %d", tok[t], prev->line);
                if (c->nlabels == MAX_LABELS)
                    return fail(c, lineno, "too many labels");
                Label *l = &c->labels[c->nlabels++];
                snprintf(l->name, sizeof l->name, "%s", tok[t]);
                l->addr = addr;
                l->line = lineno;
            }
            t++;
        }
        if (t == ntok)
            continue;

        int op = find_opcode(tok[t]);
        if (op < 0)
            return fail(c, lineno, "unknown instruction '%s'", tok[t]);
        const OpInfo *info = vm_opinfo(op);
        int nargs = ntok - t - 1;
        if (nargs != info->operands)
            return fail(c, lineno, "%s expects %d operand%s, got %d", info->name,
                        info->operands, info->operands == 1 ? "" : "s", nargs);

        if (pass == 2) {
            if (emit(c, op, lineno))
                return -1;
            for (int i = 0; i < nargs; i++) {
                const char *arg = tok[t + 1 + i];
                int value;
                if (operand_is_register(op, i)) {
                    value = parse_register(arg);
                    if (value < 0)
                        return fail(c, lineno, "'%s' is not a register (expected A-F)", arg);
                } else if (parse_number(arg, &value) == 0) {
                    /* literal number (or absolute address) */
                } else if (operand_is_address(op) && find_label(c, arg)) {
                    value = find_label(c, arg)->addr;
                } else if (operand_is_address(op)) {
                    return fail(c, lineno, "undefined label '%s'", arg);
                } else {
                    return fail(c, lineno, "'%s' is not a number", arg);
                }
                if (emit(c, value, lineno))
                    return -1;
            }
        }
        addr += 1 + info->operands;
    }
    return 0;
}

int asm_assemble(const char *source, AsmResult *out)
{
    memset(out, 0, sizeof *out);
    Ctx *c = calloc(1, sizeof *c);
    if (!c) {
        snprintf(out->error, sizeof out->error, "out of memory");
        return -1;
    }
    c->out = out;
    int rc = process(c, source, 1);
    if (rc == 0)
        rc = process(c, source, 2);
    if (rc == 0 && out->len == 0) {
        snprintf(out->error, sizeof out->error, "program is empty");
        rc = -1;
    }
    free(c);
    return rc;
}

char *asm_read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    size_t cap = 4096, len = 0;
    char *buf = malloc(cap);
    while (buf) {
        size_t n = fread(buf + len, 1, cap - len - 1, f);
        len += n;
        if (n == 0)
            break;
        if (len + 1 == cap) {
            char *nb = realloc(buf, cap * 2);
            if (!nb) {
                free(buf);
                buf = NULL;
                break;
            }
            buf = nb;
            cap *= 2;
        }
    }
    if (buf && ferror(f)) {
        free(buf);
        buf = NULL;
    }
    fclose(f);
    if (buf)
        buf[len] = '\0';
    return buf;
}

void asm_free(AsmResult *r)
{
    free(r->code);
    free(r->lines);
    r->code = NULL;
    r->lines = NULL;
    r->len = 0;
}
