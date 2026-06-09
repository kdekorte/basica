# Basica

A small IBM BASICA-compatible interpreter clone written in C.

## Build

Requirements:
- `gcc`
- `pkg-config`
- `SDL3`, `SDL3_ttf`, and `SDL3_mixer` development headers

Build with:

```sh
make
```

## Run

Run a BASIC program:

```sh
./basica demo/hello.bas
```

Enable graphics window mode:

```sh
./basica -w demo/hello.bas
```

Suppress startup output and REPL prompts:

```sh
./basica -q demo/hello.bas
```

Show help:

```sh
./basica -h
```

## Test

Run the repository test suite:

```sh
make test
```

## Supported features

- Direct mode and line-numbered program entry
- `PRINT`, `LET`, `INPUT`, `IF`, `FOR`, `NEXT`, `GOTO`, `GOSUB`, `RETURN`, `END`
- `DATA`, `READ`, `RESTORE`, `RANDOMIZE`, `RND`
- Numeric and string variables, arrays, `DIM`, `ERASE`
- `ON ERROR GOTO`
- File I/O and System: `OPEN`, `CLOSE`, `SEEK`, `PUT`, `GET`, `DELETE`, `KILL`, `CHDIR`, `MKDIR`, `RMDIR`, `FILES`, `SHELL`
- String input functions: `GET$`, `INKEY$`
- Basic string functions: `ASC`, `CHR$`, `LEFT$`, `RIGHT$`, `MID$`, `UCASE$`, `LCASE$`, `TRIM$`, `TAB`, `SPACE$`, `STR$`, `HEX$`, `OCT$`, `REVERSE`
- Graphics commands: `SCREEN`, `PSET`, `LINE`, `CIRCLE`, `PAINT`, `COLOR`, `LOCATE`, `CLS`, `SLEEP`
- Sound commands: `BEEP`, `SOUND frequency,duration`, `PLAY "MML"`
- Robust error reporting for syntax errors and control flow (e.g., `RETURN without GOSUB`)
- Memory access: `PEEK`, `POKE`, `VARPTR`

## Future reminders

See `TODO.md` for planned improvements and future work items.
