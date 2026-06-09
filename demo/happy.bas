10 SCREEN 12
20 CLS
30 REM Happy emoji demo using CIRCLE and PAINT commands
40 CX = 320 : CY = 240 : R = 150
50 REM Draw the face outline and fill it with PAINT
60 CIRCLE (CX, CY), R, 3
70 PAINT (CX, CY), 3, 3
80 REM Draw left eye as a filled circle
90 CIRCLE (CX - 50, CY - 60), 12, 1, BF
100 REM Draw right eye as a filled circle
110 CIRCLE (CX + 50, CY - 60), 12, 1, BF
120 REM Draw mouth - arc from left to right
130 FOR DY = 20 TO 60 STEP 2
140   W = INT(SQR(50 * 50 - (DY - 40) * (DY - 40)))
150   LINE (CX - W, CY + DY)-(CX + W, CY + DY + 2), 1, BF
160 NEXT DY
170 PRINT "HAPPY EMOJI"

