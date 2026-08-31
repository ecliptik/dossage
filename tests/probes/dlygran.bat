@ECHO OFF
REM DLYGRAN.BAT - libc delay(1) granularity vs. tight uclock() poll probe.
REM Measures whether DJGPP delay(1), as called by the shared SDL3-DOS
REM backend's SDL_SYS_DelayNS() frame-limiter wait loop, is millisecond-
REM fine or BIOS-tick-coarse (~54.9ms). Output: DLYGRAN.LOG.
REM Runtime: well under 1 sec (100 x ~1ms samples per series, unless
REM delay(1) turns out coarse, in which case Series A can take several
REM seconds -- that itself is diagnostic).
REM No env vars. Pure DJGPP libc, no SDL -- CWSDPMI.EXE must still be in
REM the same directory (DJGPP-linked binary).
REM Optional arg: sample count per series (default 100, range 10-5000),
REM e.g. DLYGRAN.EXE 200
ECHO Running DLYGRAN probe (delay(1) granularity, no SDL)...
DLYGRAN.EXE
ECHO ----
ECHO Probe finished. DLYGRAN.LOG contents:
TYPE DLYGRAN.LOG
