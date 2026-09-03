@ECHO OFF
REM PACESIM.BAT - standalone deadline-pacer + gettimeofday() paced-period
REM capture probe. Reproduces game.cpp's absolute-deadline frame pacer
REM (uclock()-timebase) and patch 0035's RUNMANIFEST paced-period capture
REM (gettimeofday()-timebase) in isolation -- no SDL, no rendering, no
REM audio. See docs/PACER-TIMING-INVESTIGATION.md for the question this
REM answers. Output: PACESIM.LOG by default, or <logtag>.LOG -- see below.
REM Runtime: ~niter * 66.67ms at default WORKMS=0 (default niter=3000 is
REM ~200s/~3.3min).
REM No env vars. Pure DJGPP libc, no SDL -- CWSDPMI.EXE must still be in
REM the same directory (DJGPP-linked binary).
REM Optional args: PACESIM.EXE [niter] [work_ms] [logtag]
REM   niter    frame count, default 3000, range 100-50000
REM   work_ms  simulated per-frame work (ms) before each pacer step,
REM            default 0 (pacer/clock mechanism only)
REM   logtag   optional log-file tag: writes <LOGTAG>.LOG instead of
REM            PACESIM.LOG (alnum/underscore, truncated to 8 chars).
REM            IMPORTANT: like DLYGRAN.LOG/CLKDRIFT.LOG/CLKSCALE.LOG, the
REM            log is opened in TRUNCATE mode, not append. Running this
REM            probe twice in the same directory WITHOUT distinct logtags
REM            silently destroys the first run's data (a real-hardware
REM            round hit this for real: a WORKMS=0 pass followed by a
REM            WORKMS=45 pass with no logtag lost the first pass's log).
REM            If running multiple passes back-to-back, either pass a
REM            distinct logtag per pass (see example below) or copy
REM            PACESIM.LOG out before the next invocation.
REM Example: PACESIM.EXE 3000 0        (writes PACESIM.LOG)
REM          PACESIM.EXE 3000 45 W45   (writes W45.LOG -- distinct file,
REM                                     safe to run right after the above
REM                                     without losing either pass's data)
ECHO Running PACESIM probe (deadline pacer + gettimeofday() capture, no SDL)...
PACESIM.EXE %1 %2 %3
ECHO ----
ECHO Probe finished. See PACESIM.LOG (or <logtag>.LOG if one was given).
