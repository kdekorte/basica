# Supported BASICA Keywords and Syntax

## Control Flow and Program Structure
- `END`: Terminates program execution.
- `FOR var = start TO end [STEP step] ... NEXT [var]`: Standard loop.
- `GOSUB line ... RETURN`: Subroutine call and return.
- `GOTO line`: Unconditional jump.
- `IF condition THEN [line | statement] [ELSE statement]`: Conditional execution.
- `ON expression {GOTO | GOSUB} line1[, line2...]`: Computed jump.
- `ON ERROR GOTO line`: Error trapping.
- `RESUME [0 | NEXT | line]`: Error recovery.
- `STOP`: Halts execution (can be resumed with `CONT`).
- `WHILE condition ... WEND`: Conditional loop.

## Variables and Data
- `DATA constant1[, constant2...]`: Internal data storage.
- `DEF FNname(param) = expression`: User-defined function.
- `DIM var(dim1[, dim2, dim3])`: Array dimensioning (up to 3D).
- `ERASE var`: Reinitializes variables or arrays.
- `LET var = expression`: Assignment (keyword is optional).
- `OPTION BASE {0 | 1}`: Sets minimum array subscript.
- `READ var1[, var2...]`: Reads from `DATA` statements.
- `RESTORE [line]`: Resets `DATA` pointer.
- `SWAP var1, var2`: Exchanges values of two variables or array elements.

## Input and Output
- `BEEP`: Sounds the speaker.
- `CLS`: Clears the screen.
- `COLOR foreground[, background]`: Sets text or graphics colors.
- `INPUT ["prompt"{,|;}] [#n,] var1[, var2...]`: User input.
- `LOCATE row, col`: Positions the cursor.
- `PRINT [#n,] [USING "fmt";] [expressions] [,|;]`: Output to screen or file.
- `SPC(n)`: Used in `PRINT` to output spaces.
- `TAB(n)`: Used in `PRINT` to move to a specific column.

## File and System Operations
- `CHDIR "path"`: Changes current directory.
- `CLOSE [#n]`: Closes files.
- `DELETE "file" | line`: Deletes a file or program line.
- `ENVIRON "VAR=VALUE"`: Sets environment variables.
- `FILES ["pattern"] [, "output_file"]`: Lists files (wildcards supported). Output shows modification date/time, size (or `<DIR>`), and filename — formatted similar to DOS/BASICA. If `"output_file"` is provided, filenames are written to that file (one per line).
- `GET #n, record, length, var$`: Binary/Random file input.
- `KILL "pattern"`: Deletes files using wildcards.
- `MKDIR "path"`: Creates a directory.
- `NAME "old" AS "new"`: Renames a file.
- `OPEN "file" FOR mode AS #n`: Opens a file (INPUT, OUTPUT, RANDOM, RWB).
- `PUT #n, record, length, data$`: Binary/Random file output.
- `RMDIR "path"`: Removes a directory.
- `SEEK #n, pos`: Sets file position.
- `SHELL ["command"]`: Executes a system command.
- `SYSTEM` / `QUIT`: Exits the interpreter.

## Graphics and Sound
- `CIRCLE (x,y), radius[, color]`: Draws a circle.
- `DRAW "mml"`: String-driven graphics command.
- `LINE [(x1,y1)]-(x2,y2)[, [color][, [B|BF]]]`: Draws lines or boxes.
- `PAINT (x,y)[, color[, border]]`: Area fill.
- `PLAY "mml"`: Plays Music Macro Language.
- `PSET (x,y)[, color]`: Sets a pixel.
- `SCREEN mode`: Sets graphics mode.
- `SLEEP ms`: Pauses for a specified number of milliseconds.
- `SOUND freq, duration`: Produces a tone.

## Numeric Functions
- `ABS(n)`, `SQR(n)`, `SIN(n)`, `COS(n)`, `TAN(n)`, `ATN(n)`
- `EXP(n)`, `LOG(n)`, `INT(n)`, `FIX(n)`, `RND[(n)]`, `SGN(n)`
- `LOF(n)`: Length of file #n in bytes.
- `LOC(n)`: Current position in file #n.
- `ASC(s$)`: ASCII value of first character.
- `INSTR([start,] s1$, s2$)`: String search.
- `LEN(s$)`: String length.
- `PEEK(addr)`: Read memory byte.
- `TIMER`: Seconds since midnight/startup.
- `VAL(s$)`: Numeric value of string.
- `VARPTR(var)`: Address of a variable.

## String Functions
- `CHR$(n)`: Character from ASCII code.
- `DATE$`: Current system date.
- `ENVIRON$(name | index)`: Retrieve environment variable.
- `GET$(#n, record, length)`: Read string from file.
- `HEX$(n)` / `OCT$(n)`: Hex/Octal representation.
- `INKEY$`: Read single keypress.
- `LCASE$(s$)` / `UCASE$(s$)`: Case conversion.
- `LEFT$(s$, n)`, `RIGHT$(s$, n)`, `MID$(s$, start[, n])`: Substring.
- `LTRIM$(s$)`, `RTRIM$(s$)`, `TRIM$(s$)`: Whitespace removal.
- `REVERSE$(s$)`: Reverses a string.
- `SPACE$(n)`: Returns string of spaces.
- `STR$(n)`: Converts number to string.
- `TIME$`: Current system time.

## Memory and Special
- `POKE addr, value`: Write byte to memory.

## Misc
- `REM` or `'`: Comments.
- `LIST`: Lists the program.
- `RUN`: Starts program execution.