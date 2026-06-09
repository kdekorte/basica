10 CLS
20 REM Setup dimensions for our 2x2 matrices
30 N = 2
40 DIM A(2, 2), B(2, 2), C(2, 2)
50 DIM A_T(2, 2)
60 REM Initialize Matrix A
70 A(1, 1) = 1: A(1, 2) = 2
80 A(2, 1) = 3: A(2, 2) = 4
90 REM Initialize Matrix B
100 B(1, 1) = 5: B(1, 2) = 6
110 B(2, 1) = 7: B(2, 2) = 8
120 REM ---
130 REM 1. Addition (C = A + B)
140 FOR I = 1 TO N
150   FOR J = 1 TO N
160     C(I, J) = A(I, J) + B(I, J)
170   NEXT J
180 NEXT I
190 PRINT "--- Matrix A + B ---"
200 GOSUB 1000
210 REM ---
220 REM 2. Scalar Multiplication (Multiply A by 3)
230 FOR I = 1 TO N
240   FOR J = 1 TO N
250     C(I, J) = A(I, J) * 3
260   NEXT J
270 NEXT I
280 PRINT "--- Matrix A * 3 ---"
290 GOSUB 1000
300 REM ---
310 REM 3. Transpose of A
320 FOR I = 1 TO N
330   FOR J = 1 TO N
340     A_T(J, I) = A(I, J)
350   NEXT J
360 NEXT I
370 PRINT "--- Transpose of A ---"
380 FOR I = 1 TO N
390   FOR J = 1 TO N
400     PRINT A_T(I, J); " ";
410   NEXT J
420   PRINT
430 NEXT I
440 END
1000 REM --- Subroutine to Print the resulting Matrix ---
1010 FOR I = 1 TO N
1020   FOR J = 1 TO N
1030     PRINT C(I, J); " ";
1040   NEXT J
1050   PRINT
1060 NEXT I
1070 RETURN
