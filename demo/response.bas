10 PRINT "Welcome! This demo will ask you 3 questions and save your answers."
20 PRINT ""
30 PRINT "What is your name?";
40 INPUT NAME$
50 OPEN "response.txt" FOR OUTPUT AS #1
60 PRINT #1, "Name: "; NAME$
70 PRINT ""
80 PRINT "Hi "; NAME$; "! Please answer the following questions."
90 DIM Q$(10)
100 Q$(1) = "What is your favorite color?"
110 Q$(2) = "What is your favorite animal?"
120 Q$(3) = "What city would you like to visit?"
130 Q$(4) = "What is your favorite season?"
140 Q$(5) = "What is a hobby you enjoy?"
150 Q$(6) = "What is your favorite food?"
160 Q$(7) = "What kind of music do you like?"
170 Q$(8) = "What is one word that describes you?"
180 Q$(9) = "What is your favorite movie?"
190 Q$(10) = "What makes you happy?"
200 DIM ANS$(3)
210 DIM SEL(3)
220 FOR N = 1 TO 3
230   GOSUB 400
240   PRINT "Question "; N; ": "; Q$(SEL(N))
250   PRINT #1, "Question "; N; ": "; Q$(SEL(N))
260   INPUT ANS$(N)
270   PRINT #1, "Answer "; N; ": "; ANS$(N)
280 NEXT N
290 PRINT ""
300 PRINT "Thanks "; NAME$; "! Your answers were saved to response.txt."
310 CLOSE #1
320 END
400 REM --- choose a unique question ---
410 Q = INT(RND * 10) + 1
420 FOR J = 1 TO N - 1
430   IF Q = SEL(J) THEN GOTO 410
440 NEXT J
450 SEL(N) = Q
460 RETURN
