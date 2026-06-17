# CHANGELOG

All notable changes to this project are recorded in this file.

## 2026-06-17

- **Headless Graphics Mode**: Add `--headless` command-line option to run graphics and screenshots headlessly (uses standard POSIX dummy video driver and hidden window). (commit ca8d271)
- **Performance Improvements**: Refactored interpreter engine and variable lookups to implement a 2x performance increase. (commit c874659)

## 2026-06-16

- **Performance Tuning**: Additional performance tuning and optimization to double speed of code execution. (commits 44c681d, f0e7fff)

## 2026-06-12

- **Type Suffixes**: Support type suffixes for variables: `%` (integer), `!` (single precision), and `#` (double precision). (commit 6e0fd77)
- **Default Type Declarations**: Implement `DEFINT`, `DEFSTR`, `DEFSNG`, and `DEFDBL` for default type declarations based on starting letters. (commit ef46add)
- **Bitwise Operations**: Implement bitwise operations for `AND`, `OR`, and `XOR` when used with numeric expressions (converts to 16-bit integers). (commit 9672bd5)
- **Exit on Finish**: Add `-x` / `--exit-on-finish` command-line option to exit immediately after program finishes in graphics window mode. (commit 81f898a)
- **Code Quality**: Run `cppcheck` and fix static analysis warnings. (commit d2e424e)

## 2026-06-11

- **Graphics Screenshots**: Implement `SCREENSHOT filename$` command. (commit 3a1886c)
- **Graphics Image Blocks**: Add graphics image block support for `GET` and `PUT` (distinct from file `GET`/`PUT` operations). (commit 625085b)
- **Command Line Arguments**: Pass command-line arguments using QBASIC style `COMMAND$`, and implement `ARGV$(index)` and `ARGC`. (commit eff784f)
- **Shebang & Versioning**: Support executable `.bas` scripts by ignoring shebang headers (`#!`), add support for `make install`/`uninstall`, and add `-v`/`--version` flags. (commit c185678)
- **Formatting Rules**: Update test cases for `PRINT` number formatting rules. (commit eff784f)

## 2026-06-10

- **Standard BASIC Error Handling**: Completed implementation of standard BASIC error codes, tracking variables `ERR` and `ERL`, and updated documentation/tests. (commit 93a22a5)
- **Files Formatting**: Add `FILES` formatting closer to DOS/BASICA output. (commit a8aaf5e)
- **Directory Operations**: Add `CHDIR`, `MKDIR`, and `RMDIR` tests for string variables, existing paths, and non-empty directory removal. (commit ec5ea35)
- **Environment**: Implement `ENVIRON` function. (commit c07b4d1)
- **Arrays**: Implement `OPTION BASE` support. (commit b0d381b)
- **Graphics**: Add `DRAW` command support. (commit 5a6a64e)
- **Documentation**: General documentation updates. (commit 961c096)

## 2026-06-09

- **Font Rendering**: Improve font rendering for graphics mode. (commit b865b66)
- **File Management**: Implement `NAME old AS new` function. (commit 5a4b49c)
- **Graphics**: Implement `PAINT` and improved error messaging. (commit 29adf79)
- **Interpreter**: Initial commit of the core interpreter. (commit 977191b)
