# Basika

A small IBM BASICA-compatible interpreter clone written in C.

## Install with Homebrew (macOS)

The easiest way to install basika on macOS is with [Homebrew](https://brew.sh):

```sh
brew tap kdekorte/basika https://github.com/kdekorte/basika.git
brew install basika
```

This will automatically install all required SDL3 dependencies.

To upgrade to a newer version:

```sh
brew update
brew upgrade basika
```

To uninstall:

```sh
brew uninstall basika
brew untap kdekorte/basika
```

## Build from Source

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
./basika demo/hello.bas
```

Enable graphics window mode:

```sh
./basika -w demo/hello.bas
```

Suppress startup output and REPL prompts:

```sh
./basika -q demo/hello.bas
```

Exit immediately after finish in graphics mode (for automation/screenshots):

```sh
./basika -w -x demo/hello.bas
```

Run in headless graphics mode (virtual framebuffer without showing a window, e.g. for CI/CD tests):

```sh
./basika --headless demo/hello.bas
```

Show help:

```sh
./basika -h
```

## Test

Run the repository test suite:

```sh
make test
```

## Supported features

Basika supports a wide range of IBM BASICA-compatible commands, including file I/O, graphics, sound, and robust control flow. 

For a full list of supported commands and their exact syntax, please refer to KEYWORDS.md.

## Future reminders

See `TODO.md` for planned improvements and future work items.
