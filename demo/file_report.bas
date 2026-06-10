10 REM File I/O Demo: List, Sort, and Report File Sizes
20 T1 = TIMER
30 PRINT "Gathering file list..."
40 FILES "*", "list.tmp"
50 DIM N$(500), S(500)
60 C = 0
70 OPEN "list.tmp" FOR INPUT AS #1
80 IF EOF(1) THEN GOTO 130
90 C = C + 1
100 INPUT #1, N$(C)
110 IF C < 500 THEN GOTO 80
120 PRINT "Warning: directory has too many files, truncated to 500"
130 CLOSE #1
140 KILL "list.tmp"
150 PRINT "Processing "; C; " files..."
160 FOR I = 1 TO C
170   F$ = N$(I)
180   OPEN F$ FOR INPUT AS #1
190   S(I) = LOF(1)
200   CLOSE #1
210   REM Remove extension
220   DOT = 0
230   FOR J = 1 TO LEN(F$)
240     IF MID$(F$, J, 1) = "." THEN DOT = J
250   NEXT J
260   IF DOT > 0 THEN N$(I) = LEFT$(F$, DOT - 1)
270 NEXT I
280 PRINT "Sorting by size descending..."
290 REM Bubble sort by size descending
300 FOR I = 1 TO C - 1
310   FOR J = 1 TO C - I
320     IF S(J) < S(J+1) THEN SWAP S(J), S(J+1): SWAP N$(J), N$(J+1)
330   NEXT J
340 NEXT I
350 PRINT "Writing report..."
360 OPEN "report.txt" FOR OUTPUT AS #1
370 FOR I = 1 TO C
380   LINE$ = N$(I) + ", " + STR$(S(I))
390   PRINT LINE$
400   PRINT #1, LINE$
410 NEXT I
420 CLOSE #1
430 KILL "report.txt"
440 T2 = TIMER
450 PRINT "----------------------------------------"
460 PRINT "Operation completed in "; T2 - T1; " seconds."
470 END
