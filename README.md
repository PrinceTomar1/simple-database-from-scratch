# simple-database-from-scratch

A tiny database engine written from scratch in C++17. Built this to
actually understand how databases store and query data under the hood,
instead of just using one. No SQLite, no LevelDB, no existing storage
engine anywhere in here - the parser, the table format, the on-disk
persistence, and the query execution are all my own code.

It's a REPL. You type SQL-ish commands, it parses them by hand (no
parser generator, no regex hacks), runs them against an in-memory
table, and saves the result to disk so it's still there next time you
launch it.

## What it can do

```
CREATE TABLE users (id INTEGER, name TEXT)
INSERT INTO users VALUES (1, "Prince")
SELECT * FROM users
SELECT * FROM users WHERE id = 1
DELETE FROM users WHERE id = 1
EXIT
```

Two column types: `INTEGER` and `TEXT`. That's it, on purpose - keeping
the type system tiny meant I could spend the time on parsing/storage
instead. `CREATE TABLE` rejects a table with two columns of the same
name, since that would make `WHERE`/`SELECT` ambiguous about which one
you meant.

## Architecture

### Tokenizing + parsing (`source/lexer.*`, `source/parser.*`)

The lexer turns a line of input into a flat list of tokens: identifiers
(keywords and names), numbers, quoted strings, and a handful of
symbols (`( ) , = *`). Nothing recursive here, just a single pass over
the characters.

The parser is keyword dispatch, not a proper grammar with a parse
tree. It looks at the first token to figure out which statement it's
looking at (`CREATE`, `INSERT`, `SELECT`, `DELETE`, `EXIT`), then each
statement has its own straightforward left-to-right parse function
that walks the token list and fills in a `Statement` struct. If
anything doesn't match what's expected, parsing bails out immediately
with a specific error message saying what it expected instead - no
silent failures, no crashes on garbage input. A single trailing `;` is
tolerated (since typing one is muscle memory from real SQL clients),
but it's dropped before parsing rather than treated as a statement
separator - `SELECT * FROM t; SELECT * FROM t2` is still a parse
error, not two statements.

### Tables (`source/table.*`)

A `Table` is a name, a schema (`vector<ColumnDef>`, each with a name
and a type), and a `vector<Row>` where each `Row` is just a
`vector<Value>` in schema order. `Value` is a small tagged struct
(either holds a `long long` or a `std::string`, plus which type it
is) rather than `std::variant`, mostly because it reads more plainly.

Every table also keeps a small hash index: for each column, a map from
that column's value to the row positions that have it
(`unordered_map<int, unordered_map<string, vector<size_t>>>`). This is
what makes `WHERE col = value` an O(1) average lookup instead of
scanning every row. It's rebuilt from scratch after a delete (since
row positions shift), which is a fine trade-off at the scale this
project runs at - definitely not a B-tree, just a hash map.

### Persistence (`source/storage.*`)

Each table gets its own file: `<data-dir>/<table>.tbl`, plain text so
you can open it and read it:

```
TABLE users
COLUMNS id:INTEGER,name:TEXT
1,"Prince"
2,"Rahul"
```

Line 1 is the table name, line 2 is the column list (`name:TYPE` pairs,
comma separated), and every line after that is one row - values comma
separated, text values double-quoted with `\"` and `\\` escaping so a
comma or quote inside a string doesn't break the format.

Writes go to a `.tmp` file first and then get renamed into place, so a
crash mid-write can't leave a half-written table file sitting there.
On startup, the database scans the data directory for `*.tbl` files
and loads every one it finds. Every `INSERT`, `CREATE TABLE`, and
`DELETE` rewrites the whole table file immediately - there's no
write-ahead log or batching, it's just "always fully synced after the
statement returns," which was simple to get right and good enough for
what this is.

I actually tested this by hand: create a table, insert a couple of
rows, `EXIT`, relaunch the binary pointed at the same directory, and
`SELECT * FROM users` still shows the rows. There's also an automated
version of that exact check in `tests/run_tests.sh`.

### Query execution (`source/database.*`)

`Database` owns an `unordered_map<string, Table>` and a data directory. It takes
an already-parsed `Statement` and dispatches to a handler per
statement type, each of which does its own validation (table exists,
column exists, types line up, right number of values) before touching
any actual data, then calls into `Table` to do the work and `storage`
to persist it. Query results come back as a plain string the REPL just
prints.

## What's genuinely NOT here

Being upfront about the scope, since it'd be easy to overstate this:

- **No joins.** One table at a time, always.
- **No transactions.** Every statement takes effect (and gets synced
  to disk) immediately - there's no BEGIN/COMMIT/ROLLBACK.
- **WHERE is equality-only, on a single column.** No `<`, `>`, `AND`,
  `OR`, `LIKE`, nothing. `WHERE id = 1` works, `WHERE id > 1` doesn't
  parse.
- **No NULLs.** Every column in every row has to have a real value of
  the right type.
- **No ALTER TABLE / DROP TABLE** exposed in the REPL (the storage
  layer has a `removeTableFile` helper but nothing wires it up).
- **No UPDATE.** Only `INSERT`, `SELECT`, and `DELETE`.
- **Only two types**, `INTEGER` (a `long long`) and `TEXT`. No floats,
  no dates, no booleans.
- **The index isn't persisted** - it's rebuilt in memory from the row
  data every time a table loads. That's cheap since tables are small,
  but it means index-build time is O(rows) on every startup.
- **No concurrency.** Single process, single-threaded REPL. Pointing
  two instances at the same data directory at once isn't safe - there's
  no file locking, so whichever one saves last wins.
- **No rollback on a failed save.** Every write goes through the
  save-to-tmp-then-rename path, so a crash mid-write can't corrupt a
  table file, but if the disk write itself fails (disk full, permission
  denied), the in-memory table has already changed and the error just
  gets reported - it doesn't undo the insert/delete in memory.

## Building

Needs a C++17 compiler (developed against g++/clang with `-std=c++17`)
and `make`. No external dependencies.

```
make
```

This builds a `dbengine` binary in the project root.

## Running

```
./dbengine [data-directory]
```

`data-directory` defaults to `./data` if you don't pass one. Point it
at whatever directory you want your tables to live in - it gets
created automatically if it doesn't exist yet.

## Testing

```
make test
```

This builds `dbengine` and a separate `unit_tests` binary (which links
the same source files, minus `main.cpp`, directly against
`tests/unit_tests.cpp`), runs the unit tests, then runs
`tests/run_tests.sh`, which pipes real command sequences into the
built `dbengine` binary over stdin and checks the output - including
an explicit restart test: insert rows, exit, relaunch pointed at the
same data directory, confirm `SELECT` still returns them.

## Example session

This is a real captured run (not hand-edited), showing the exact
example from the top of this file:

```
$ ./dbengine mydata
simple db engine - type EXIT to quit
using data directory: mydata
db> CREATE TABLE users (id INTEGER, name TEXT)
table 'users' created
db> INSERT INTO users VALUES (1, "Prince")
1 row inserted
db> INSERT INTO users VALUES (2, "Rahul")
1 row inserted
db> SELECT * FROM users
id | name
1 | Prince
2 | Rahul
(2 rows)
db> DELETE FROM users WHERE id = 1
1 row deleted
db> SELECT * FROM users
id | name
2 | Rahul
(1 row)
db> EXIT
```

And a couple of the error cases, since "doesn't crash on bad input"
was one of the actual goals here:

```
db> SELECT * FROM ghost
error: unknown table 'ghost'
db> INSERT INTO users VALUES (1)
error: table 'users' has 2 column(s) but 1 value(s) were given
db> INSERT INTO users VALUES ("nope", "x")
error: type mismatch for column 'id' (expected INTEGER, got TEXT)
db> SELECT * FROM users WHERE ghost_col = 1
error: unknown column 'ghost_col' on table 'users'
db> DELETE FROM users
error: malformed command: DELETE requires a WHERE clause (deleting a whole table isn't supported)
db> this is not sql
error: malformed command: unrecognized keyword 'this'
```
