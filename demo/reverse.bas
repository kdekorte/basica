10 DIM L$(100)
20 OPEN "input.txt" FOR INPUT AS #1
30 INPUT #1, L$
40 PRINT "Read: "; L$
50 CLOSE #1
60 REVERSE L$
70 OPEN "output.txt" FOR OUTPUT AS #2
80 PRINT #2, L$
90 CLOSE #2
100 PRINT "Reversed content saved to output.txt"