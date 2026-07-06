' A simple demo that doesn't use line numbers
' It loops to guess a secret number (simplified)

PRINT "Welcome to the Modern BASICA Demo!"
PRINT "We will count from 1 to 5 using GOTO labels."

COUNT = 1

loop_start:
  PRINT "Count is: "; COUNT
  COUNT = COUNT + 1
  IF COUNT <= 5 THEN GOTO loop_start

PRINT "Done counting!"
PRINT "Now calling a subroutine."
GOSUB print_message
PRINT "Demo Finished."
END

print_message:
  PRINT " --> Inside the subroutine!"
  RETURN
