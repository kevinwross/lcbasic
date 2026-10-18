10 REM FamousBirthdays.bas - a small database of famous people.
11 REM
12 REM Records: first name, last name, a description of 50 characters or fewer, date of
13 REM birth and an optional date of death. Stored in samples\FamousBirthdays.dat, one
14 REM record per line, five pipe-separated fields:
15 REM
16 REM     first|last|description|YYYY-MM-DD|YYYY-MM-DD
17 REM
18 REM The file is plain text, so records can be added or edited in any editor; the fifth
19 REM field is empty for someone still living. Dates are ISO order, so sorting them as
20 REM strings also sorts them in time.
21 REM
22 REM Keys: UP and DOWN move between fields, ENTER accepts a form, ESC cancels it. Our
23 REM INKEY$ returns the arrow keys as single CP437 characters, 130 for UP and 136 for
24 REM DOWN, rather than GW-BASIC's two-character CHR$(0) sequence.
30 SCREEN 720
40 CLS
50 MX = 200
60 DIM AF$(200)
61 DIM AL$(200)
62 DIM AD$(200)
63 DIM AB$(200)
64 DIM AX$(200)
70 DIM F$(5)
71 DIM ML(5)
72 DIM FR(5)
73 DIM IX(200)
80 FILE$ = "samples\FamousBirthdays.dat"
90 ML(1) = 20: FR(1) = 7: ML(2) = 25: FR(2) = 9: ML(3) = 50: FR(3) = 11: ML(4) = 10: FR(4) = 13: ML(5) = 10: FR(5) = 15
96 N = 0: GOSUB 3000: GOTO 500
100 REM
200 REM ---------------- menu ----------------
500 CLS
505 LOCATE 2, 4: PRINT "FAMOUS BIRTHDAYS  -  "; N; " entries"
510 LOCATE 5, 4: PRINT "1   Add an entry"
515 LOCATE 6, 4: PRINT "2   Edit an entry"
520 LOCATE 7, 4: PRINT "3   Delete an entry"
525 LOCATE 9, 4: PRINT "4   Report by date of birth"
530 LOCATE 10, 4: PRINT "5   Report by date of death"
535 LOCATE 11, 4: PRINT "6   Report by last name"
540 LOCATE 13, 4: PRINT "7   Quit"
545 LOCATE 15, 4: PRINT "Choice: ";
550 K$ = INKEY$: IF K$ = "" THEN 550
555 IF K$ = "1" THEN GOSUB 1000: GOTO 500
560 IF K$ = "2" THEN GOSUB 1200: GOTO 500
565 IF K$ = "3" THEN GOSUB 1400: GOTO 500
570 IF K$ = "4" THEN GOSUB 1600: GOTO 500
575 IF K$ = "5" THEN GOSUB 1700: GOTO 500
580 IF K$ = "6" THEN GOSUB 1800: GOTO 500
585 IF K$ = "7" THEN CLS: END
590 IF K$ = CHR$(27) THEN CLS: END
595 GOTO 550
600 REM
1000 REM ---------------- add ----------------
1005 IF N >= MX THEN LOCATE 19, 4: PRINT "The database is full.";: GOSUB 2000: RETURN
1010 FOR I = 1 TO 5: F$(I) = "": NEXT I
1015 GOSUB 2050
1020 IF ACC = 0 THEN RETURN
1025 N = N + 1
1030 AF$(N) = F$(1): AL$(N) = F$(2): AD$(N) = F$(3): AB$(N) = F$(4): AX$(N) = F$(5)
1035 GOSUB 3300
1040 RETURN
1100 REM
1200 REM ---------------- edit ----------------
1205 GOSUB 1900
1210 IF R = 0 THEN RETURN
1215 F$(1) = AF$(R): F$(2) = AL$(R): F$(3) = AD$(R): F$(4) = AB$(R): F$(5) = AX$(R)
1220 GOSUB 2050
1225 IF ACC = 0 THEN RETURN
1230 AF$(R) = F$(1): AL$(R) = F$(2): AD$(R) = F$(3): AB$(R) = F$(4): AX$(R) = F$(5)
1235 GOSUB 3300
1240 RETURN
1300 REM
1400 REM ---------------- delete ----------------
1405 GOSUB 1900
1410 IF R = 0 THEN RETURN
1415 FOR I = R TO N - 1
1420 AF$(I) = AF$(I + 1): AL$(I) = AL$(I + 1): AD$(I) = AD$(I + 1)
1425 AB$(I) = AB$(I + 1): AX$(I) = AX$(I + 1)
1430 NEXT I
1435 N = N - 1
1440 GOSUB 3300
1445 RETURN
1500 REM
1600 REM ---------------- report by date of birth ----------------
1605 H$ = "BY DATE OF BIRTH": GOSUB 2700
1610 FOR I = 1 TO N: IX(I) = I: NEXT I
1615 FOR I = 1 TO N - 1: FOR J = I + 1 TO N
1620 IF AB$(IX(J)) < AB$(IX(I)) THEN S = IX(I): IX(I) = IX(J): IX(J) = S
1625 NEXT J: NEXT I
1630 FOR I = 1 TO N: GOSUB 2800: NEXT I
1635 GOSUB 2000
1640 RETURN
1650 REM
1700 REM ---------------- report by date of death, living people skipped ----------
1705 H$ = "BY DATE OF DEATH": GOSUB 2700
1710 M = 0
1715 FOR I = 1 TO N: IF AX$(I) <> "" THEN M = M + 1: IX(M) = I
1720 NEXT I
1725 FOR I = 1 TO M - 1: FOR J = I + 1 TO M
1730 IF AX$(IX(J)) < AX$(IX(I)) THEN S = IX(I): IX(I) = IX(J): IX(J) = S
1735 NEXT J: NEXT I
1740 FOR I = 1 TO M: GOSUB 2800: NEXT I
1745 IF M = 0 THEN LOCATE 5, 4: PRINT "Nobody here has a date of death."
1750 GOSUB 2000
1755 RETURN
1760 REM
1800 REM ---------------- report by last name ----------------
1805 H$ = "BY LAST NAME": GOSUB 2700
1810 FOR I = 1 TO N: IX(I) = I: NEXT I
1815 FOR I = 1 TO N - 1: FOR J = I + 1 TO N
1820 IF AL$(IX(J)) < AL$(IX(I)) THEN S = IX(I): IX(I) = IX(J): IX(J) = S
1825 NEXT J: NEXT I
1830 FOR I = 1 TO N: GOSUB 2800: NEXT I
1835 GOSUB 2000
1840 RETURN
1850 REM
1900 REM ---------------- pick a record, R = 0 when cancelled ----------------
1905 IF N = 0 THEN LOCATE 19, 4: PRINT "The database is empty.";: GOSUB 2000: R = 0: RETURN
1910 CLS
1915 LOCATE 2, 4: PRINT "RECORDS"
1920 FOR I = 1 TO N
1925 IF I > 34 THEN 1945
1930 LOCATE I + 3, 4: PRINT I; ". "; AL$(I); ", "; AF$(I)
1935 NEXT I
1945 LOCATE 40, 4: PRINT "Number, or 0 to cancel: ";
1950 L$ = "": GOSUB 2200
1955 R = VAL(L$)
1960 IF R < 0 OR R > N THEN R = 0
1965 RETURN
1970 REM
2000 REM wait for one key
2005 K$ = INKEY$: IF K$ = "" THEN 2005
2010 RETURN
2020 REM
2050 REM ---------------- form editor, ACC = 1 when accepted ----------------
2055 ACC = 0: CF = 1
2060 GOSUB 2500
2065 K$ = INKEY$: IF K$ = "" THEN 2065
2070 C = ASC(K$)
2075 IF C = 13 THEN ACC = 1: RETURN
2080 IF C = 27 THEN RETURN
2085 IF C = 130 THEN IF CF > 1 THEN CF = CF - 1: GOTO 2060
2090 IF C = 136 THEN IF CF < 5 THEN CF = CF + 1: GOTO 2060
2095 IF C = 8 THEN GOSUB 2600: GOTO 2060
2100 IF C >= 32 AND C <> 127 THEN GOSUB 2620: GOTO 2060
2105 GOTO 2065
2110 REM
2200 REM ---------------- read a line into L$, echoing as it goes ----------------
2205 L$ = ""
2210 K$ = INKEY$: IF K$ = "" THEN 2210
2215 C = ASC(K$)
2220 IF C = 13 THEN RETURN
2225 IF C = 27 THEN L$ = "": RETURN
2230 IF C = 8 THEN IF LEN(L$) > 0 THEN L$ = LEFT$(L$, LEN(L$) - 1): PRINT CHR$(8); " "; CHR$(8);: GOTO 2210
2235 IF C >= 32 THEN L$ = L$ + K$: PRINT K$;: GOTO 2210
2240 GOTO 2210
2245 REM
2500 REM ---------------- draw the form ----------------
2505 CLS
2510 LOCATE 2, 4: PRINT "FAMOUS BIRTHDAYS  -  ENTRY"
2515 LOCATE 4, 4: PRINT "UP and DOWN move between fields.  ENTER accepts.  ESC cancels."
2520 LOCATE 7, 4: PRINT "First name     :"
2525 LOCATE 9, 4: PRINT "Last name      :"
2530 LOCATE 11, 4: PRINT "Description    :"
2535 LOCATE 13, 4: PRINT "Date of birth  :"
2540 LOCATE 15, 4: PRINT "Date of death  :"
2545 LOCATE 17, 4: PRINT "Dates as YYYY-MM-DD; leave the death date blank if living."
2550 FOR I = 1 TO 5
2555 LOCATE FR(I), 21: PRINT "["; F$(I); SPACE$(ML(I) - LEN(F$(I))); "]";
2560 NEXT I
2565 LOCATE FR(CF), 19: PRINT ">"
2566 REM put the block cursor at the typing point, inside the field
2567 LOCATE FR(CF), 22 + LEN(F$(CF))
2570 RETURN
2575 REM
2600 REM backspace and append for the current field
2605 IF LEN(F$(CF)) > 0 THEN F$(CF) = LEFT$(F$(CF), LEN(F$(CF)) - 1)
2610 RETURN
2620 IF LEN(F$(CF)) < ML(CF) THEN F$(CF) = F$(CF) + K$
2625 RETURN
2630 REM
2700 REM ---------------- report header and first line row ----------------
2705 CLS
2710 LOCATE 2, 3: PRINT H$
2715 LOCATE 4, 1: PRINT TAB(5); "#"; TAB(10); "LAST NAME"; TAB(29); "FIRST NAME"; TAB(45); "BORN"; TAB(56); "DIED"; TAB(67); "DESCRIPTION"
2721 LOCATE 5, 3: PRINT STRING$(112, "-");
2725 RW = 6
2730 RETURN
2735 REM
2800 REM ---------------- one tabular line for record IX(I) ----------------
2805 LOCATE RW, 4: PRINT USING "###"; I;
2810 LOCATE RW, 9: PRINT USING "\                \"; AL$(IX(I));
2815 LOCATE RW, 28: PRINT USING "\            \"; AF$(IX(I));
2820 LOCATE RW, 44: PRINT AB$(IX(I));
2825 IF AX$(IX(I)) = "" THEN LOCATE RW, 55: PRINT "living"; ELSE LOCATE RW, 55: PRINT AX$(IX(I));
2830 LOCATE RW, 66: PRINT AD$(IX(I))
2835 RW = RW + 1
2840 IF RW <= 44 THEN RETURN
2845 LOCATE 45, 3: PRINT "Press a key for more.";
2850 GOSUB 2000
2855 GOSUB 2700
2860 RETURN
3000 REM ---------------- load ----------------
3005 ON ERROR GOTO 3035
3010 OPEN FILE$ FOR INPUT AS #1
3015 IF EOF(1) THEN 3025
3020 LINE INPUT #1, L$
3021 IF L$ = "" THEN 3015
3022 GOSUB 3200: IF N < MX THEN N = N + 1: AF$(N) = P1$: AL$(N) = P2$: AD$(N) = P3$: AB$(N) = P4$: AX$(N) = P5$
3023 GOTO 3015
3025 CLOSE #1
3030 ON ERROR GOTO 0: RETURN
3035 REM no file yet: start with nothing
3040 RESUME 3045
3045 ON ERROR GOTO 0: RETURN
3050 REM
3200 REM ---------------- split L$ on "|" into P1$..P5$ ----------------
3205 T$ = L$
3210 I = INSTR(T$, "|"): IF I = 0 THEN P1$ = T$: P2$ = "": P3$ = "": P4$ = "": P5$ = "": RETURN
3215 P1$ = LEFT$(T$, I - 1): T$ = MID$(T$, I + 1)
3220 I = INSTR(T$, "|"): IF I = 0 THEN P2$ = T$: P3$ = "": P4$ = "": P5$ = "": RETURN
3225 P2$ = LEFT$(T$, I - 1): T$ = MID$(T$, I + 1)
3230 I = INSTR(T$, "|"): IF I = 0 THEN P3$ = T$: P4$ = "": P5$ = "": RETURN
3235 P3$ = LEFT$(T$, I - 1): T$ = MID$(T$, I + 1)
3240 I = INSTR(T$, "|"): IF I = 0 THEN P4$ = T$: P5$ = "": RETURN
3245 P4$ = LEFT$(T$, I - 1): T$ = MID$(T$, I + 1)
3250 P5$ = T$
3255 RETURN
3260 REM
3300 REM ---------------- save ----------------
3305 OPEN FILE$ FOR OUTPUT AS #1
3310 FOR I = 1 TO N
3315 PRINT #1, AF$(I); "|"; AL$(I); "|"; AD$(I); "|"; AB$(I); "|"; AX$(I)
3320 NEXT I
3325 CLOSE #1
3330 RETURN

