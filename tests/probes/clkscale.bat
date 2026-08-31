@ECHO OFF
REM CLKSCALE.BAT - uclock() vs time(NULL) rate scale factor probe.
REM Measures the CONSTANT scale factor between DJGPP uclock() (the frame
REM pacer's ruler, nominal 1193180 Hz) and time(NULL) (the BIOS/DOS
REM time-of-day the game's exit fps report divides by), with both window
REM endpoints pinned to true time(NULL) second boundaries. Also cross-
REM checks against the CMOS RTC's independent 32.768kHz crystal, which is
REM what lets the probe say WHICH clock is lying rather than only that
REM they disagree. Output: CLKSCALE.LOG.
REM
REM Runtime: about 2 minutes by default (a 120-second measurement window
REM plus ~2s of boundary alignment). This is EXPECTED - the long window is
REM what drives the residual error below 0.1% against a 1.72% effect.
REM The probe deliberately prints nothing and writes no file during the
REM window; a blank screen for two minutes is normal.
REM
REM OPERATOR: host-timestamp the CLKSCALE-BEGIN and CLKSCALE-END lines
REM that appear on the console. Those two timestamps are the external
REM real-time reference and are the ultimate arbiter of which clock is
REM lying if the CMOS RTC cross-check comes back unavailable.
REM
REM No env vars. Pure DJGPP libc + port I/O, no SDL -- CWSDPMI.EXE must
REM still be in the same directory (DJGPP-linked binary).
REM Optional arg: measurement window in seconds (default 120, range
REM 30-600), e.g. CLKSCALE.EXE 300
ECHO Running CLKSCALE probe (uclock vs time(NULL) scale factor, no SDL)...
ECHO This takes about 2 minutes and is silent while measuring.
CLKSCALE.EXE
ECHO ----
ECHO Probe finished. CLKSCALE.LOG contents:
TYPE CLKSCALE.LOG
