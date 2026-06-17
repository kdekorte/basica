# Basica

A small IBM BASICA-compatible interpreter clone written in C.

## Build

Requirements:
- `gcc`
- `pkg-config`
- `SDL3`, `SDL3_ttf`, `SDL3_mixer`, and `SDL3_image` development headers

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

Exit immediately after finish in graphics mode (for automation/screenshots):

```sh
./basica -w -x demo/hello.bas
```

Run in headless graphics mode (virtual framebuffer without showing a window, e.g. for CI/CD tests):

```sh
./basica --headless demo/hello.bas
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

Basica supports a wide range of IBM BASICA-compatible commands, including file I/O, graphics, sound, and robust control flow. 

For a full list of supported commands and their exact syntax, please refer to KEYWORDS.md.

## Future reminders

See `TODO.md` for planned improvements and future work items.
