@ECHO OFF
REM CLKDRIFT.BAT - gettimeofday() vs uclock() rate agreement probe.
REM Measures whether minorGems' Time::getCurrentTime() (gettimeofday(),
REM ms-truncated, as dossage's game.cpp frame limiter uses for frameTime)
REM agrees with uclock() (DJGPP's PIT-based clock, used by SDL_Delay's own
REM wait loop) at matching rates across several known interval lengths.
REM Output: CLKDRIFT.LOG.
REM Runtime: a few seconds (6 interval lengths x 40 reps each, dominated
REM by the 500ms x 40 = ~20s series).
REM No env vars. Pure DJGPP libc, no SDL -- CWSDPMI.EXE must still be in
REM the same directory (DJGPP-linked binary).
REM Optional arg: repetitions per interval length (default 40, range
REM 10-500), e.g. CLKDRIFT.EXE 50
ECHO Running CLKDRIFT probe (gettimeofday vs uclock rate, no SDL)...
CLKDRIFT.EXE
ECHO ----
ECHO Probe finished. CLKDRIFT.LOG contents:
TYPE CLKDRIFT.LOG
