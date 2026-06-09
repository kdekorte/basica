10  REM *********************************************************
20  REM *  UGUESS1                                              *
30  REM *  Guess a random number selected by a BASIC program    *
40  REM *********************************************************
50  RANDOMIZE TIMER
60  NUMBER% = INT(1 + RND * 100)
65  TRIES% = 0
70  CLS
80  PRINT "Guess a number in the range 1 and 100"
90  INPUT GUESS%
95  TRIES% = TRIES% + 1
100 IF GUESS% = NUMBER% THEN GOTO 140
110 IF GUESS% > NUMBER% THEN PRINT "Too High!  Try Again"
120 IF GUESS% < NUMBER% THEN PRINT "Too Low!  Try Again"
130 GOTO 90
140 PRINT "Very Good!  You Guessed the Number."
145 PRINT "It took you "; TRIES%; " tries."
150 END