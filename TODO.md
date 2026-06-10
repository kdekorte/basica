# Basica TODO

## Short-term improvements

- Expand `PLAY` MML coverage beyond the initial `T`, `O`, `L`, notes, rests, dotted notes, and octave changes.

## Audio

- Add more complete `PLAY` MML support: `MB`, `MF`, `MN`, `ML`, `MS`, `Nn`, and foreground/background behavior.
- Add tests for `PLAY` octave changes, sharps/flats, dotted notes, rests, and tempo changes.
- Add `SOUND` range validation matching BASICA more closely.
- Consider a quiet/test audio mode flag so audio commands can be tested without delays.

## Interpreter correctness

- Add support for `CONT`, `STOP`, and better direct-mode behavior.
- Add broader tests for `PEEK`, `POKE`, and `VARPTR` edge cases.

## Graphics

- Add graphics image block support for `GET`/`PUT`, if distinct from file `GET`/`PUT`.
- Add screenshot or pixel-buffer tests for graphics primitives.

## Testing and project hygiene

- Add `make check-clean` or test cleanup verification.
- Add sanitizer builds: `make asan`, `make ubsan`.
- Add a developer note documenting how to add a new BASIC keyword.

## UX and documentation

- Add a command-line option to dump supported commands.
- Add a command support matrix: supported, partial, planned.
- Add examples for file I/O, graphics, sound, arrays, and error handling.
