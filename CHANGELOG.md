# CHANGELOG

All notable changes to this project are recorded in this file.

Generated: 2026-06-10

## 2026-06-10

- Implement bitwise operations for `AND`, `OR`, and `XOR` when used with numeric expressions (converts to 16-bit integers).
- Implement standard BASIC error codes and tracking variables `ERR` and `ERL`. Updated error reporting to include line numbers.
- Implement `LOF(n)` and `LOC(n)` functions for file length and position.
- Extend `FILES` command to support redirection to a file.
- Update `OPEN` statement to support string expressions for file paths.
- Add `demo/file_report.bas` to demonstrate file operations, sorting, and timing.
- Add `CHDIR`, `MKDIR`, and `RMDIR` tests for string variables, existing paths, and non-empty directory removal. (commit ec5ea35)
- Documentation updates. (commit 961c096)
- Implement `ENVIRON`. (commit c07b4d1)
- Implement `OPTION BASE`. (commit b0d381b)
- Add `DRAW` command support. (commit 5a6a64e)
 - Format `FILES` output to match DOS/BASICA (date/time, size/<DIR>, filename); updated tests and docs. (implemented 2026-06-10)

## 2026-06-09

- Improve font rendering. (commit b865b66)
- Implement `NAME old AS new` function. (commit 5a4b49c)
- Update TODO. (commit 83192ae)
- Implement `PAINT` and error messaging. (commit 29adf79)
- Ignore built files. (commit f8e4b87)
- Commit the interpreter. (commit 977191b)

## Notes

- This changelog was generated from recent Git commits and the repository LLM edit notes. See the repository note for details: [memories/repo/llm_edits.md](memories/repo/llm_edits.md).
- If you want full diffs or commit bodies for any entry, tell me which commit(s) and I'll append them.
