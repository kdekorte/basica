10 REM Sieve of Eratosthenes Benchmark
20 CLS
30 PRINT "Calculating primes... please wait."
40 START = TIMER
50 SIZE = 7000
60 DIM FLAGS(SIZE)
70 COUNT = 0
85 REM --- The Main Benchmark Loop ---
90 FOR I = 0 TO SIZE
100   FLAGS(I) = 1
110 NEXT I
120 FOR I = 2 TO SIZE
130   IF FLAGS(I) = 0 THEN GOTO 180
140   PRIME = I
150   K = I + PRIME
160   WHILE K <= SIZE
170     FLAGS(K) = 0: K = K + PRIME
175   WEND
180   COUNT = COUNT + 1
190 NEXT I
200 END_TIME = TIMER
210 PRINT "Primes found: "; COUNT
220 PRINT "Time elapsed: "; END_TIME - START; " seconds"