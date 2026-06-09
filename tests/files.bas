10 OPEN "tests/f.tmp" FOR OUTPUT AS #1
20 PRINT #1, "test"
30 CLOSE #1
40 FILES "tests/f.tmp"
50 KILL "tests/f.tmp"
60 END