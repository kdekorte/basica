10 SCREEN 12
20 CLS
30 REM Happy emoji demo using CIRCLE command
40 CX = 320 : CY = 240 : R = 150
50 REM Draw the face outline as a filled circle
60 CIRCLE (CX, CY), R, 3, BF
70 REM Draw left eye as a filled circle
80 CIRCLE (CX - 50, CY - 60), 12, 1, BF
90 REM Draw right eye as a filled circle
100 CIRCLE (CX + 50, CY - 60), 12, 1, BF
110 REM Draw mouth - arc from left to right
120 FOR DY = 20 TO 60 STEP 2
130   W = INT(SQR(50 * 50 - (DY - 40) * (DY - 40)))
140   LINE (CX - W, CY + DY)-(CX + W, CY + DY + 2), 1, BF
150 NEXT DY
160 PRINT "HAPPY EMOJI"

