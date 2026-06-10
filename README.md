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
- Control flow: `PRINT`, `LET`, `INPUT`, `IF-THEN-ELSE`, `FOR-NEXT`, `WHILE-WEND`, `GOTO`, `GOSUB`, `RETURN`, `ON...GOTO`, `ON...GOSUB`, `END`
- Data/Variables: `DATA`, `READ`, `RESTORE`, `RANDOMIZE`, `RND`, `DIM`, `ERASE`, `SWAP`, `DEF FN`
- Error handling: `ON ERROR GOTO`
- File I/O and System: `OPEN`, `CLOSE`, `SEEK`, `PUT`, `GET`, `DELETE`, `KILL`, `NAME`, `CHDIR`, `MKDIR`, `RMDIR`, `FILES`, `SHELL`, `SYSTEM`
- Keyboard/String input: `GET$`, `INKEY$`
- String functions: `ASC`, `CHR$`, `LEFT$`, `RIGHT$`, `MID$`, `UCASE$`, `LCASE$`, `TRIM$`, `LTRIM$`, `RTRIM$`, `TAB`, `SPC`, `SPACE$`, `STR$`, `HEX$`, `OCT$`, `STRING$`, `REVERSE`
- Mathematical functions: `ABS`, `ATN`, `COS`, `EXP`, `FIX`, `INT`, `LOG`, `SGN`, `SIN`, `SQR`, `TAN`
- Graphics commands: `SCREEN`, `PSET`, `LINE`, `CIRCLE`, `PAINT`, `COLOR`, `LOCATE`, `CLS`, `SLEEP`, `DRAW`
- Sound commands: `BEEP`, `SOUND frequency,duration`, `PLAY "MML"`
- System variables/helpers: `DATE$`, `TIME$`, `TIMER`
- Memory access: `PEEK`, `POKE`, `VARPTR`
- Robust error reporting for syntax errors and control flow (e.g., `RETURN without GOSUB`)

## Future reminders

See `TODO.md` for planned improvements and future work items.
