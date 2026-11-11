/*
 * tui.c - a small full screen debugger drawn with ANSI escape codes.
 *
 * No curses library is needed: the terminal is put in raw mode with
 * termios, the screen is redrawn into a buffer and written in one go.
 *
 * Layout:
 *   +-- program ---------------+-- registers ---+
 *   | listing with IP marker   | A..F, IP, SP   |
 *   | and breakpoints          +-- stack -------+
 *   |                          | top values     |
 *   |                          +-- output ------+
 *   |                          | printed values |
 *   +--------------------------+----------------+
 *   status / key help
 */
#define _POSIX_C_SOURCE 200809L
#include "tui.h"
#include "vm.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#define RUN_CHUNK 1000000UL   /* max instructions per 'c' key press */

/* ---- output buffer ---------------------------------------------------- */

typedef struct {
    char  *data;
    size_t len, cap;
} Buf;

static void buf_printf(Buf *b, const char *fmt, ...)
{
    for (;;) {
        va_list ap;
        va_start(ap, fmt);
        int n = vsnprintf(b->data + b->len, b->cap - b->len, fmt, ap);
        va_end(ap);
        if (n < 0)
            return;
        if ((size_t)n < b->cap - b->len) {
            b->len += (size_t)n;
            return;
        }
        size_t cap = (b->cap + (size_t)n + 1) * 2;
        char *d = realloc(b->data, cap);
        if (!d)
            return;
        b->data = d;
        b->cap = cap;
    }
}

/* ---- terminal handling ------------------------------------------------ */

static struct termios saved_termios;
static bool raw_enabled;

static void term_restore(void)
{
    if (!raw_enabled)
        return;
    const char *s = "\x1b[0m\x1b[?25h\x1b[?1049l";
    if (write(STDOUT_FILENO, s, strlen(s)) < 0) { /* nothing to do */ }
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_termios);
    raw_enabled = false;
}

static int term_raw(void)
{
    if (tcgetattr(STDIN_FILENO, &saved_termios) < 0)
        return -1;
    struct termios raw = saved_termios;
    raw.c_iflag &= ~(unsigned)(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(unsigned)OPOST;
    raw.c_cflag |= CS8;
    raw.c_lflag &= ~(unsigned)(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0)
        return -1;
    raw_enabled = true;
    atexit(term_restore);
    const char *s = "\x1b[?1049h\x1b[?25l";  /* alternate screen, hide cursor */
    if (write(STDOUT_FILENO, s, strlen(s)) < 0)
        return -1;
    return 0;
}

static void term_size(int *rows, int *cols)
{
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row && ws.ws_col) {
        *rows = ws.ws_row;
        *cols = ws.ws_col;
    } else {
        *rows = 24;
        *cols = 80;
    }
}

enum { KEY_UP = 1000, KEY_DOWN, KEY_PGUP, KEY_PGDN, KEY_NONE };

static int read_key(void)
{
    unsigned char c;
    if (read(STDIN_FILENO, &c, 1) != 1)
        return 'q';           /* EOF or error: leave the debugger */
    if (c != 0x1b)
        return c;

    /* Escape sequence: read the rest with a short timeout. */
    struct termios t;
    tcgetattr(STDIN_FILENO, &t);
    struct termios quick = t;
    quick.c_cc[VMIN] = 0;
    quick.c_cc[VTIME] = 1;
    tcsetattr(STDIN_FILENO, TCSANOW, &quick);
    unsigned char seq[3] = {0};
    ssize_t n = read(STDIN_FILENO, seq, 2);
    if (n == 2 && seq[0] == '[' && (seq[1] == '5' || seq[1] == '6'))
        n += read(STDIN_FILENO, seq + 2, 1);
    tcsetattr(STDIN_FILENO, TCSANOW, &t);

    if (n >= 2 && seq[0] == '[') {
        switch (seq[1]) {
        case 'A': return KEY_UP;
        case 'B': return KEY_DOWN;
        case '5': return KEY_PGUP;
        case '6': return KEY_PGDN;
        }
    }
    return n == 0 ? 0x1b : KEY_NONE;
}

/* ---- debugger state --------------------------------------------------- */

typedef struct {
    const char      *title;
    const AsmResult *prog;
    Vm      vm;
    size_t *instr;        /* address of each instruction                 */
    size_t  ninstr;
    bool   *breakpoint;   /* indexed by address                          */
    size_t  cursor;       /* selected instruction (index into instr)     */
    size_t  scroll;
    char    message[128];
} Dbg;

static size_t instr_index_of(const Dbg *d, int addr)
{
    for (size_t i = 0; i < d->ninstr; i++)
        if ((int)d->instr[i] == addr)
            return i;
    return 0;
}

static void follow_ip(Dbg *d)
{
    d->cursor = instr_index_of(d, d->vm.ip);
}

static void set_message(Dbg *d, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(d->message, sizeof d->message, fmt, ap);
    va_end(ap);
}

static void report_status(Dbg *d, VmStatus st)
{
    if (st == VM_HALTED)
        set_message(d, "Program halted after %lu steps. Press r to reset.", d->vm.steps);
    else if (st != VM_OK)
        set_message(d, "Runtime error at %d: %s. Press r to reset.", d->vm.ip, vm_status_str(st));
}

static void do_step(Dbg *d)
{
    if (!d->vm.running) {
        set_message(d, "Machine is stopped. Press r to reset.");
        return;
    }
    VmStatus st = vm_step(&d->vm);
    set_message(d, "Stepped.");
    report_status(d, st);
    follow_ip(d);
}

static bool at_breakpoint(const Dbg *d)
{
    return d->vm.ip >= 0 && (size_t)d->vm.ip < d->prog->len && d->breakpoint[d->vm.ip];
}

static void do_continue(Dbg *d)
{
    if (!d->vm.running) {
        set_message(d, "Machine is stopped. Press r to reset.");
        return;
    }
    VmStatus st = VM_OK;
    unsigned long n = 0;
    do {
        st = vm_step(&d->vm);
        n++;
    } while (st == VM_OK && !at_breakpoint(d) && n < RUN_CHUNK);

    if (st == VM_OK && at_breakpoint(d))
        set_message(d, "Breakpoint hit at address %d.", d->vm.ip);
    else if (st == VM_OK)
        set_message(d, "Paused after %lu steps (press c to keep going).", n);
    report_status(d, st);
    follow_ip(d);
}

/* ---- drawing ---------------------------------------------------------- */

#define C_RESET "\x1b[0m"
#define C_TITLE "\x1b[1;36m"
#define C_DIM   "\x1b[2m"
#define C_IP    "\x1b[1;30;42m"
#define C_SEL   "\x1b[7m"
#define C_BP    "\x1b[1;31m"
#define C_ERR   "\x1b[1;31m"
#define C_OK    "\x1b[1;32m"

static void draw(Dbg *d)
{
    int rows, cols;
    term_size(&rows, &cols);
    Buf b = {0};
    buf_printf(&b, "\x1b[H\x1b[2J");

    if (rows < 16 || cols < 60) {
        buf_printf(&b, "Terminal too small (need 60x16). Press q to quit.");
        if (write(STDOUT_FILENO, b.data, b.len) < 0) { /* ignore */ }
        free(b.data);
        return;
    }

    int right_w = 26;
    int left_w = cols - right_w - 1;
    int body = rows - 3;  /* header + 2 footer lines */

    /* header */
    buf_printf(&b, C_TITLE " mac debugger " C_RESET C_DIM " %.*s" C_RESET,
               cols - 16, d->title);

    /* program listing */
    if (d->cursor < d->scroll)
        d->scroll = d->cursor;
    if (d->cursor >= d->scroll + (size_t)(body - 1))
        d->scroll = d->cursor - (size_t)(body - 2);

    buf_printf(&b, "\x1b[2;1H" C_TITLE "%-*s" C_RESET, left_w, " PROGRAM");
    for (int r = 0; r < body - 1; r++) {
        size_t i = d->scroll + (size_t)r;
        buf_printf(&b, "\x1b[%d;1H", r + 3);
        if (i >= d->ninstr)
            continue;
        size_t addr = d->instr[i];
        char text[64];
        vm_disassemble(d->prog->code, d->prog->len, addr, text, sizeof text);
        bool at_ip = (int)addr == d->vm.ip;
        const char *style = at_ip ? C_IP : (i == d->cursor ? C_SEL : "");
        buf_printf(&b, "%s", d->breakpoint[addr] ? C_BP "*" C_RESET : " ");
        char line[256];
        snprintf(line, sizeof line, "%s%04zu  %-18s line %d", at_ip ? ">" : " ",
                 addr, text, d->prog->lines[addr]);
        buf_printf(&b, "%s%-*.*s" C_RESET, style, left_w - 1, left_w - 1, line);
    }

    /* right column */
    int x = left_w + 2;
    int y = 2;
    buf_printf(&b, "\x1b[%d;%dH" C_TITLE "REGISTERS" C_RESET, y++, x);
    for (int r = 0; r < REG_COUNT; r += 2)
        buf_printf(&b, "\x1b[%d;%dH%s=%-10d %s=%-10d", y++, x,
                   vm_register_name(r), d->vm.regs[r],
                   vm_register_name(r + 1), d->vm.regs[r + 1]);
    buf_printf(&b, "\x1b[%d;%dHIP=%-4d SP=%-4d", y++, x, d->vm.ip, d->vm.sp);
    buf_printf(&b, "\x1b[%d;%dHsteps=%lu", y++, x, d->vm.steps);
    const char *st_color = d->vm.status == VM_OK ? C_OK
                         : d->vm.status == VM_HALTED ? C_DIM : C_ERR;
    buf_printf(&b, "\x1b[%d;%dH%s%.*s" C_RESET, y++, x, st_color,
               right_w - 1, d->vm.running ? "running" : vm_status_str(d->vm.status));

    int remaining = body + 1 - y;         /* rows left for stack + output */
    int stack_rows = (remaining - 2) / 2;
    int out_rows = remaining - 2 - stack_rows;

    y++;
    buf_printf(&b, "\x1b[%d;%dH" C_TITLE "STACK" C_RESET C_DIM " (top first)" C_RESET, y++, x);
    if (d->vm.sp < 0)
        buf_printf(&b, "\x1b[%d;%dH" C_DIM "<empty>" C_RESET, y, x);
    for (int i = 0; i < stack_rows - 1 && d->vm.sp - i >= 0; i++)
        buf_printf(&b, "\x1b[%d;%dH%3d: %d", y + i, x, d->vm.sp - i, d->vm.stack[d->vm.sp - i]);
    y += stack_rows - 1;

    y++;
    buf_printf(&b, "\x1b[%d;%dH" C_TITLE "OUTPUT" C_RESET C_DIM " (newest first)" C_RESET, y++, x);
    if (d->vm.output_len == 0)
        buf_printf(&b, "\x1b[%d;%dH" C_DIM "<nothing yet>" C_RESET, y, x);
    for (int i = 0; i < out_rows; i++) {
        int v;
        if (!vm_output_at(&d->vm, (size_t)i, &v))
            break;
        buf_printf(&b, "\x1b[%d;%dH%d", y + i, x, v);
    }

    /* footer */
    buf_printf(&b, "\x1b[%d;1H\x1b[K %.*s", rows - 1, cols - 2, d->message);
    buf_printf(&b, "\x1b[%d;1H" C_SEL "%-*.*s" C_RESET, rows, cols - 1, cols - 1,
               " s/space:step  c:continue  b:breakpoint  j/k:move  g:goto IP  r:reset  q:quit");

    if (write(STDOUT_FILENO, b.data, b.len) < 0) { /* ignore */ }
    free(b.data);
}

/* ---- main loop -------------------------------------------------------- */

int tui_run(const char *title, const AsmResult *prog)
{
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        fprintf(stderr, "mac: the debugger needs an interactive terminal\n");
        return 1;
    }

    Dbg d = {0};
    d.title = title;
    d.prog = prog;
    d.instr = calloc(prog->len, sizeof *d.instr);
    d.breakpoint = calloc(prog->len, sizeof *d.breakpoint);
    if (!d.instr || !d.breakpoint) {
        free(d.instr);
        free(d.breakpoint);
        fprintf(stderr, "mac: out of memory\n");
        return 1;
    }
    for (size_t a = 0; a < prog->len;) {
        d.instr[d.ninstr++] = a;
        const OpInfo *info = vm_opinfo(prog->code[a]);
        a += 1 + (info ? (size_t)info->operands : 0);
    }

    vm_init(&d.vm, prog->code, prog->len, NULL);  /* output shown on screen */
    set_message(&d, "Ready. Press s to execute the first instruction.");

    if (term_raw() < 0) {
        term_restore();
        free(d.instr);
        free(d.breakpoint);
        fprintf(stderr, "mac: could not configure the terminal\n");
        return 1;
    }

    for (bool quit = false; !quit;) {
        draw(&d);
        int k = read_key();
        switch (k) {
        case 'q': case 'Q': case 3 /* Ctrl-C */:
            quit = true;
            break;
        case 's': case 'n': case ' ':
            do_step(&d);
            break;
        case 'c':
            do_continue(&d);
            break;
        case 'r':
            vm_reset(&d.vm);
            follow_ip(&d);
            set_message(&d, "Machine reset.");
            break;
        case 'b': {
            size_t addr = d.instr[d.cursor];
            d.breakpoint[addr] = !d.breakpoint[addr];
            set_message(&d, "Breakpoint %s at address %zu.",
                        d.breakpoint[addr] ? "set" : "cleared", addr);
            break;
        }
        case 'j': case KEY_DOWN:
            if (d.cursor + 1 < d.ninstr)
                d.cursor++;
            break;
        case 'k': case KEY_UP:
            if (d.cursor > 0)
                d.cursor--;
            break;
        case KEY_PGDN:
            d.cursor = d.cursor + 10 < d.ninstr ? d.cursor + 10 : d.ninstr - 1;
            break;
        case KEY_PGUP:
            d.cursor = d.cursor > 10 ? d.cursor - 10 : 0;
            break;
        case 'g':
            follow_ip(&d);
            break;
        default:
            break;
        }
    }

    term_restore();
    free(d.instr);
    free(d.breakpoint);
    return 0;
}
