#!/bin/bash
# black-box repl tests. pipes a sequence of commands into the built
# dbengine binary via stdin and checks what comes back on stdout.
#
# run via `make test` (which builds the binary first) or directly:
#   ./dbengine  # build it first
#   bash tests/run_tests.sh

set -u

cd "$(dirname "$0")/.." || exit 1

BINARY="./dbengine"
TMP_ROOT="tests/tmp_data"
FAILURES=0

if [ ! -x "$BINARY" ]; then
    echo "dbengine binary not found - run 'make' first"
    exit 1
fi

rm -rf "$TMP_ROOT"
mkdir -p "$TMP_ROOT"

pass() {
    echo "ok - $1"
}

fail() {
    FAILURES=$((FAILURES + 1))
    echo "FAIL - $1"
}

# ---------------------------------------------------------------------
# test 1: the canonical example session, diffed exactly against what we
# expect to come out the other end.
# ---------------------------------------------------------------------
BASIC_DIR="$TMP_ROOT/basic"
mkdir -p "$BASIC_DIR"

ACTUAL_BASIC=$(printf 'CREATE TABLE users (id INTEGER, name TEXT)\nINSERT INTO users VALUES (1, "Prince")\nINSERT INTO users VALUES (2, "Rahul")\nSELECT * FROM users\nDELETE FROM users WHERE id = 1\nSELECT * FROM users\nEXIT\n' | "$BINARY" "$BASIC_DIR")

EXPECTED_BASIC="simple db engine - type EXIT to quit
using data directory: $BASIC_DIR
db> table 'users' created
db> 1 row inserted
db> 1 row inserted
db> id | name
1 | Prince
2 | Rahul
(2 rows)
db> 1 row deleted
db> id | name
2 | Rahul
(1 row)
db> "

if diff <(echo "$ACTUAL_BASIC") <(echo "$EXPECTED_BASIC") > /tmp/dbengine_test_diff.$$ 2>&1; then
    pass "basic create/insert/select/delete session matches expected output"
else
    fail "basic session output didn't match"
    cat /tmp/dbengine_test_diff.$$
fi
rm -f /tmp/dbengine_test_diff.$$

# ---------------------------------------------------------------------
# test 2: error messages for the required error cases. just checking the
# right message shows up, not the whole session, since these get mixed
# together in one run.
# ---------------------------------------------------------------------
ERR_DIR="$TMP_ROOT/errors"
mkdir -p "$ERR_DIR"

ERR_OUTPUT=$(printf 'SELECT * FROM ghost\nCREATE TABLE users (id INTEGER, name TEXT)\nCREATE TABLE users (id INTEGER)\nINSERT INTO users VALUES (1)\nINSERT INTO users VALUES ("nope", "x")\nSELECT * FROM users WHERE ghost_col = 1\nDELETE FROM users\ngarbage input here\nEXIT\n' | "$BINARY" "$ERR_DIR")

check_contains() {
    local desc="$1"
    local needle="$2"
    if echo "$ERR_OUTPUT" | grep -qF "$needle"; then
        pass "$desc"
    else
        fail "$desc (expected to find: $needle)"
    fi
}

check_contains "unknown table error"        "error: unknown table 'ghost'"
check_contains "duplicate table error"      "error: table 'users' already exists"
check_contains "wrong number of values"     "error: table 'users' has 2 column(s) but 1 value(s) were given"
check_contains "type mismatch on insert"    "error: type mismatch for column 'id'"
check_contains "unknown column in where"    "error: unknown column 'ghost_col'"
check_contains "delete without where"       "DELETE requires a WHERE clause"
check_contains "malformed command"          "malformed command"

# ---------------------------------------------------------------------
# test 3: persistence across restart. run once to insert data and exit,
# then run again against the same data directory and confirm select
# still shows the earlier rows.
# ---------------------------------------------------------------------
PERSIST_DIR="$TMP_ROOT/persist"
mkdir -p "$PERSIST_DIR"

printf 'CREATE TABLE users (id INTEGER, name TEXT)\nINSERT INTO users VALUES (1, "Prince")\nINSERT INTO users VALUES (2, "Rahul")\nEXIT\n' | "$BINARY" "$PERSIST_DIR" > /dev/null

SECOND_RUN=$(printf 'SELECT * FROM users\nEXIT\n' | "$BINARY" "$PERSIST_DIR")

if echo "$SECOND_RUN" | grep -qF "1 | Prince" && echo "$SECOND_RUN" | grep -qF "2 | Rahul"; then
    pass "data survives a process restart"
else
    fail "data did not survive a process restart"
    echo "$SECOND_RUN"
fi

# also make sure a table created in one run can be inserted into and
# deleted from in a later run against the same directory
printf 'INSERT INTO users VALUES (3, "Amit")\nDELETE FROM users WHERE id = 1\nEXIT\n' | "$BINARY" "$PERSIST_DIR" > /dev/null

THIRD_RUN=$(printf 'SELECT * FROM users\nEXIT\n' | "$BINARY" "$PERSIST_DIR")

if echo "$THIRD_RUN" | grep -qF "3 | Amit" && ! echo "$THIRD_RUN" | grep -qF "1 | Prince"; then
    pass "inserts and deletes across restarts keep accumulating correctly"
else
    fail "state across multiple restarts is wrong"
    echo "$THIRD_RUN"
fi

# ---------------------------------------------------------------------
rm -rf "$TMP_ROOT"

echo ""
if [ "$FAILURES" -eq 0 ]; then
    echo "all repl tests passed"
    exit 0
else
    echo "$FAILURES repl test(s) failed"
    exit 1
fi
