' window_view_demo.bas
' Demonstrates WINDOW (logical coordinate mapping) and VIEW (viewport clipping).
'
' The screen is divided into two side-by-side panels:
'   Left:  A sine wave plotted in math coordinates (Y-up).
'   Right: A cosine wave in screen coordinates (Y-down) using WINDOW SCREEN.

SCREEN 12

PI = 3.14159265

' ─── LEFT VIEWPORT: sine wave, math coordinates ───────────────────────────────
' Reserve left half: pixels (10,30)-(309,449)
VIEW (10, 30)-(309, 449), 0, 7

' Map logical coords: X from -PI to PI, Y from -1.2 to 1.2 (Y increases upward)
WINDOW (-PI, -1.2)-(PI, 1.2)

' Draw axes
LINE (-PI, 0)-(PI, 0), 8
LINE (0, -1.2)-(0, 1.2), 8

' Tick marks on X axis
LINE (-PI, -0.08)-(-PI, 0.08), 7
LINE (-PI/2, -0.08)-(-PI/2, 0.08), 7
LINE (PI/2, -0.08)-(PI/2, 0.08), 7
LINE (PI, -0.08)-(PI, 0.08), 7

' Plot sine wave (step 0.02 = ~315 points, fast enough)
X = -PI
sine_loop:
  Y = SIN(X)
  PSET (X, Y), 10
  X = X + 0.02
  IF X <= PI THEN GOTO sine_loop

' ─── RIGHT VIEWPORT: cosine wave, Y-down screen coordinates ──────────────────
VIEW SCREEN (330, 30)-(629, 449), 0, 7

' WINDOW SCREEN: X 0..360, Y 0..200 with Y increasing downward
WINDOW SCREEN (0, 0)-(360, 200)

' Draw axes
LINE (0, 100)-(360, 100), 8
LINE (0, 0)-(0, 200), 8

' Tick marks on Y axis at 25/75/125/175
LINE (-3, 25)-(3, 25), 7
LINE (-3, 75)-(3, 75), 7
LINE (-3, 125)-(3, 125), 7
LINE (-3, 175)-(3, 175), 7

' Plot cosine (step 1 = 360 points)
DEG = 0
cos_loop:
  RAD = DEG * PI / 180
  Y = 100 - COS(RAD) * 80
  PSET (DEG, Y), 14
  DEG = DEG + 1
  IF DEG <= 360 THEN GOTO cos_loop

' ─── Reset and draw labels ───────────────────────────────────────────────────
' Reset so LOCATE/PRINT work in full-screen text mode
VIEW
WINDOW

' Panel titles above each viewport
LOCATE 1, 3:  PRINT "  Sine Wave (Y-up, math coords)  "
LOCATE 1, 44: PRINT "Cosine Wave (Y-down, screen coords)"

' Divider line between panels
LINE (320, 0)-(320, 479), 8

END
