#!/bin/sh
# End-to-end tests for the `mac` command line tool.
# Usage: tests/test_cli.sh path/to/mac
set -u

MAC=${1:-build/mac}
DIR=$(cd "$(dirname "$0")/.." && pwd)
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

pass=0
fail=0

ok()   { pass=$((pass + 1)); printf '  ok    %s\n' "$1"; }
bad()  { fail=$((fail + 1)); printf '  FAIL  %s\n' "$1"; }

# expect_output NAME EXPECTED_STDOUT EXPECTED_EXIT -- command...
expect() {
    name=$1; want_out=$2; want_rc=$3; shift 4
    out=$("$@" 2>"$TMP/stderr"); rc=$?
    if [ "$out" = "$want_out" ] && [ "$rc" -eq "$want_rc" ]; then
        ok "$name"
    else
        bad "$name (exit $rc, want $want_rc)"
        printf '        got:  %s\n' "$out" | head -5
        printf '        want: %s\n' "$want_out" | head -5
        sed 's/^/        stderr: /' "$TMP/stderr" | head -5
    fi
}

# expect_stderr NAME PATTERN -- checks the stderr of the previous expect
expect_stderr() {
    if grep -q "$2" "$TMP/stderr"; then ok "$1"; else bad "$1"; sed 's/^/        stderr: /' "$TMP/stderr"; fi
}

echo "test_cli"

expect "add.asm"       "11"        0 -- "$MAC" run "$DIR/examples/add.asm"
expect "registers.asm" "34"        0 -- "$MAC" run "$DIR/examples/registers.asm"
expect "countdown.asm" "5
4
3
2
1"                                 0 -- "$MAC" run "$DIR/examples/countdown.asm"
expect "factorial.asm" "3628800"   0 -- "$MAC" run "$DIR/examples/factorial.asm"
expect "fibonacci.asm" "0
1
1
2
3
5
8
13
21
34"                                0 -- "$MAC" run "$DIR/examples/fibonacci.asm"
expect "gcd.asm"       "21"        0 -- "$MAC" run "$DIR/examples/gcd.asm"

expect "--trace keeps stdout clean" "11" 0 -- "$MAC" run --trace "$DIR/examples/add.asm"
expect_stderr "--trace writes to stderr" '\[0004\] ADD'

printf 'PSH 1\nPSH 0\nDIV\nHLT\n' > "$TMP/div.asm"
expect "runtime error exits 1" "" 1 -- "$MAC" run "$TMP/div.asm"
expect_stderr "runtime error message" 'address 4 (line 3): division by zero'

printf 'PSH 1\nBOGUS 2\n' > "$TMP/bad.asm"
expect "assembly error exits 1" "" 1 -- "$MAC" run "$TMP/bad.asm"
expect_stderr "assembly error message" 'bad.asm:2: error: unknown instruction'

printf 'loop: JMP loop\n' > "$TMP/loop.asm"
expect "--max-steps stops infinite loops" "" 1 -- "$MAC" run --max-steps 100 "$TMP/loop.asm"
expect_stderr "step limit message" 'step limit reached'

printf 'PSH 1\n' > "$TMP/nohlt.asm"
expect "missing HLT is reported" "" 1 -- "$MAC" run "$TMP/nohlt.asm"
expect_stderr "missing HLT message" 'out of program bounds'

expect "missing file" "" 1 -- "$MAC" run "$TMP/does-not-exist.asm"
expect_stderr "missing file message" 'cannot read'

expect "disasm" "0000  PSH 5            ; line 2
0002  PSH 6            ; line 3
0004  ADD              ; line 4
0005  POP              ; line 5
0006  HLT              ; line 6" 0 -- "$MAC" disasm "$DIR/examples/add.asm"

expect "debug refuses a non-terminal" "" 1 -- "$MAC" debug "$DIR/examples/add.asm"
expect_stderr "debug message" 'interactive terminal'

expect "no arguments shows usage" "" 2 -- "$MAC"
expect "unknown command" "" 2 -- "$MAC" frobnicate x.asm
expect "run without file" "" 2 -- "$MAC" run
expect "bad --max-steps" "" 2 -- "$MAC" run --max-steps abc "$DIR/examples/add.asm"
"$MAC" help > "$TMP/help" 2>&1 && grep -q "usage: mac" "$TMP/help" && ok "help" || bad "help"

echo "test_cli: $((pass + fail)) checks, $fail failures"
[ "$fail" -eq 0 ]
