/*
 * pacesim.c -- standalone reproduction of game.cpp's absolute-deadline
 * frame pacer AND its patch-0035 RUNMANIFEST paced-period capture, with
 * no SDL, no rendering, no audio, no engine.
 *
 * Written for docs/PACER-TIMING-INVESTIGATION.md (read that file first --
 * this header assumes it). THE ANOMALY: every real-hardware run since
 * patch 0035 landed reports EXACTLY fps_p50=16.67 (60ms) and
 * fps_p95=9.09 (110ms), unmoved across 3 video chips and 3 CPUs (two
 * Intel, one AMD, ~33% clock-speed spread) -- while the plain time(NULL)
 * average fps DOES move (13.69-15.02 across those same runs). Something
 * producing exactly two paced-period clusters is indifferent to
 * everything that should make per-frame timing noisy.
 *
 * THE MECHANISM THIS PROBE TARGETS (found by reading game.cpp directly,
 * not guessed -- see lines ~1658-1892 of vendor/passage/gameSource/
 * game.cpp): dossage's DJGPP pacer and its RUNMANIFEST measurement use
 * TWO DIFFERENT CLOCKS:
 *
 *   1. The pacer itself converges on an absolute deadline
 *      (dosNextFrameDeadlineNS) using SDL_GetTicksNS() -- which on this
 *      backend is derived from uclock(), DJGPP's ~1.19MHz PIT-based
 *      clock (see tests/probes/clkdrift.c/clkscale.c's own findings).
 *      It halves the remainder via SDL_DelayNS() down to a 3ms handoff,
 *      then closes the last ~3ms with an SDL_Delay(0)-equivalent
 *      (DOS_Yield()-only, no delay(1)) loop down to a 0.2ms floor. This
 *      converges TIGHTLY on a true ~66.6667ms period, measured on
 *      uclock()'s timebase -- that is the whole point of the deadline
 *      pacer (see the huge comment block at game.cpp:1658 for the full
 *      real-hardware history of why it exists).
 *
 *   2. Patch 0035's RUNMANIFEST capture measures the delta between
 *      consecutive "start timing next frame" timestamps using
 *      Time::getCurrentTime() -- minorGems' gettimeofday() wrapper,
 *      tv_usec truncated to whole ms (TimeDOS.cpp). This is a
 *      COMPLETELY SEPARATE clock read from the one the pacer just
 *      converged against.
 *
 * THE HYPOTHESIS THIS PROBE TESTS: if gettimeofday() free-runs at some
 * granularity coarser than the pacer's own convergence precision (the
 * working hypothesis in the investigation brief is the classic ~54.9255ms
 * BIOS/PIT tick, since 2x that = 109.85ms is a close match for the
 * observed ~110ms cluster), then a REAL, SMOOTH, TIGHTLY-CONVERGED
 * ~66.6667ms period (as delivered on uclock()'s timebase) would be
 * measured by gettimeofday() as ONE of a SMALL FIXED SET of values,
 * determined by the PHASE between the frame boundary and gettimeofday()'s
 * own internal update boundary -- NOT by anything about the real
 * hardware's speed or the true delivered interval. Because the pacer
 * converges to essentially the SAME true period on every machine
 * (that's what a deadline pacer does), the phase distribution -- and
 * therefore the gettimeofday()-measured cluster values -- would ALSO be
 * essentially the same on every machine. That would explain both halves
 * of the anomaly at once: why there are exactly two clusters, and why
 * they don't move with CPU speed, CPU vendor, or video card.
 *
 * This is DELIBERATELY not "does gettimeofday() and uclock() agree on
 * average rate" (already checked, and found to agree, by clkdrift.c/
 * clkscale.c) -- it's "does gettimeofday()'s SAMPLE-TO-SAMPLE behavior
 * across a tightly-converged ~66.67ms interval show discrete clustering
 * rather than a smooth spread". Rate agreement on average is entirely
 * compatible with a coarse per-sample quantization -- the two questions
 * are different, and only this probe (or a re-examination of clkdrift's
 * raw per-sample gtod_ms column at its matching 66.67ms interval, which
 * uses a plain busy-wait rather than this probe's actual deadline-pacer
 * convergence loop -- see the investigation brief's Action 1) tests the
 * second one directly in a shape that matches patch 0035's real capture
 * site.
 *
 * FIDELITY NOTES (read before trusting a null result):
 *
 *   - The deadline-pacer STRUCTURE (resync-if-ahead-more-than-a-frame,
 *     halving-convergence with 3ms handoff, final-approach with 0.2ms
 *     floor, resync-if-behind-more-than-5-frames, skip-sleep-if-
 *     repayable) is reproduced faithfully from game.cpp, using the exact
 *     same constants (dosFrameTickNS = 1e9/15, dosFrameMaxDebtNS =
 *     5*dosFrameTickNS, dosFrameSpinHandoffNS = 3000000,
 *     dosFrameYieldHalfQuantumNS = 200000).
 *   - "SDL_GetTicksNS()" is approximated as uclock() rescaled to ns via
 *     UCLOCKS_PER_SEC (1193180) -- this is what SDL_SYS_DelayNS's
 *     underlying clock reduces to on this backend WHEN the SDL/0122
 *     pump-aware MODE-2 timebase is not active. dossage does not run the
 *     GUS/AdLib OPL pump (SDL/0122's own trigger condition), so this
 *     approximation should hold for dossage specifically -- flagged here
 *     because it would NOT hold verbatim for a port that does use that
 *     pump.
 *   - The halving loop's actual sleep mechanism (SDL_DelayNS, whose
 *     wait-loop calls DJGPP delay(1) whenever >1ms remains -- see
 *     dlygran.c, which measured delay(1) itself as fine-grained,
 *     ~1.236ms median) is reproduced with the same delay(1)-driven
 *     structure. The final-approach loop's SDL_Delay(0) (DOS_Yield()
 *     only, NO delay(1), ~0.25ms measured granularity per game.cpp's own
 *     comment) has no standalone equivalent available outside SDL/DPMI
 *     cooperative scheduling -- this probe substitutes a tight uclock()
 *     spin-poll for that last ~3ms-to-0.2ms stretch instead. This should
 *     not matter for THIS probe's question: what's being tested is
 *     whether a true, tightly-converged ~66.67ms interval gets
 *     misreported by gettimeofday(), and a spin-poll converges at least
 *     as tightly as a yield-poll would (if anything, MORE tightly, since
 *     it cannot be preempted) -- it is a fidelity gap for absolute
 *     interval-length claims, not for the phase-vs-gettimeofday question.
 *   - No SB16-compatible DMA audio IRQs are running (same caveat as
 *     dlygran.c and clkdrift.c). If IRQ load somehow perturbs
 *     gettimeofday()'s own update mechanism, this probe would not see
 *     that. Flagged, not ruled out.
 *   - No rendering, no VESA bank switching, no event polling. Per-frame
 *     "work" is simulated as an optional pure uclock()-timed busy-wait
 *     (argv WORKMS, default 0) so this probe can be re-run with a
 *     representative work amount (e.g. the ~42-58ms/frame range cited in
 *     game.cpp's own pacer comment) without a rebuild, if a work-free run
 *     is inconclusive.
 *
 * METHOD: run N iterations (argv NITER, default 3000 -- ~200s of
 * simulated real time at the ~66.67ms/frame steady-state rate, matching
 * the real benchmark runs' rough duration) of:
 *
 *   1. optional WORKMS busy-wait (uclock()-timed, simulated per-frame
 *      work; default 0)
 *   2. the deadline-pacer convergence block, verbatim structure from
 *      game.cpp
 *   3. the patch-0035 capture: pacedPeriod = gtod_now - gtod_last (both
 *      via the exact TimeDOS.cpp double-conversion, mirroring
 *      clkdrift.c's gtod_as_minorgems_double()), same reject filter
 *      (<=0 or <0.001s) as DOS_PORT_FRAME_TIME_MIN_PLAUSIBLE_S
 *
 * Output: PACESIM.LOG by default, or <argv[3]>.LOG if a third argument is
 * given (fsync throttled to a fixed call cadence, plus always on exit --
 * see FSYNC_EVERY_N_CALLS's own comment for why "every line" turned out
 * to be a real bug, not just a style choice: it measurably perturbed
 * this probe's own raw-sample timing during the first 200 iterations of
 * the first real-hardware run). This probe is far more likely than its
 * siblings to be invoked twice in a row against the same staged
 * directory with different WORKMS -- like dlygran.c/clkdrift.c/
 * clkscale.c, the log is opened in "w" (truncate), not "a", so a second
 * invocation with no logtag silently destroys the first pass's data (hit
 * for real on the first real-hardware round -- see open_log()). Pass a
 * distinct argv[3] logtag per pass instead of relying on append, or copy
 * the log out between passes. Every raw pacedPeriod sample, min/median/
 * mean/p95/max AND a 10ms-bucket histogram (so a bimodal shape is
 * visible without post-processing), reject counts, and fps_p50/fps_p95
 * computed EXACTLY as game.cpp's RUNMANIFEST emission does (sort
 * ascending, index = floor(0.50*(n-1)) / floor(0.95*(n-1)), fps =
 * 1/period) -- directly comparable to the real RUNMANIFEST numbers in
 * docs/benchmarks/.
 *
 * Pure DJGPP libc (time.h uclock()/gettimeofday()/delay()). No SDL, no
 * engine, no C++.
 *
 * 8.3 DOS filenames: PACESIM.EXE (7.3), PACESIM.LOG (7.3), PACESIM.BAT.
 *
 * Build: `make -f tests/probes/probes.mk probe-pacesim` (see that file's
 * registry). DOSBox-X smoke is correctness-only (parses, doesn't crash,
 * writes a plausible log) -- per this repo's standing rule, DOSBox-X's
 * own PIT/gettimeofday()/delay() emulation is NOT a performance proxy;
 * the real answer comes only from a real-hardware run.
 *
 * License: MIT.
 */

#include <ctype.h>
#include <dos.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

/* ============================================================ */
/* Logging -- fsync throttled to every FSYNC_EVERY_N_CALLS calls */
/* (forensic safety for a real-HW run that might behave           */
/* unexpectedly mid-series), NOT every call. See dlog()'s own     */
/* comment for why this isn't "every call" -- a real bug, found   */
/* and fixed after real-hardware data exposed it.                 */
/* ============================================================ */

/* Real fsync() call cost is not free, and this probe's own raw-sample
   print cadence is NOT uniform: the first 200 iterations print (and,
   before this fix, fsync'd) EVERY line, while iterations 200+ print only
   every 50th -- so the first 200 iterations previously paid ~200 fsync
   calls' worth of wall-clock cost packed into 200 iterations, versus
   ~56 calls spread across the remaining ~2800. Since each frame's own
   dlog() call happens INSIDE the timed measurement loop (between one
   frame's gettimeofday() capture and the next), that uneven fsync cost
   directly inflates the MEASURED paced period for exactly the
   disproportionately-logged early frames -- not a hypothetical risk:
   the first real-hardware WORKMS=45 run showed a startup-window (i<200)
   with 52% of samples in the ~110ms/2-tick band vs. 23% steady-state
   after i=200 (the EXACT boundary where per-line fsync stopped), and
   both of that run's two ~330ms stall outliers landed at i=80 and
   i=196 -- both inside the densely-fsync'd window, both absent from the
   rest of the 2992-sample run. That skew inflated the reported overall
   mean (68.2ms) well above what a passive measurement should show
   (~66.68ms is what the tick-quantization mechanism alone predicts,
   analytically, for an unperturbed steady-state run -- see
   docs/PACER-TIMING-INVESTIGATION.md). The instrument was perturbing the
   exact thing it measures. Fix: throttle fsync to a fixed CALL-COUNT
   cadence (not fixed per printed line, not fixed per iteration), so its
   wall-clock cost is spread evenly across the whole run regardless of
   how print density varies. fflush() (cheap, no forced physical write)
   still happens every call for live visibility; only the expensive
   fsync() is throttled. */
#define FSYNC_EVERY_N_CALLS 20

static FILE *g_log = NULL;

/* DOS 8.3: 8-char basename + '.' + 3-char extension + NUL = 13. This
   probe is far more likely than its siblings to be invoked twice in a
   row against the same staged directory with different argv (WORKMS=0
   then WORKMS=45, say) -- caught for real on the first real-hardware
   round: the WORKMS=0 pass's log was silently destroyed by the WORKMS=45
   pass before it could be retrieved (see open_log()'s pre-open check
   below and argv[3] in main()). */
static char g_log_filename[13] = "PACESIM.LOG";

/* format attribute: catches printf-format/argument-count mismatches at
   compile time (bit this probe once already -- see pacesim.c history --
   a dropped argument silently shifted every later %-conversion onto
   whatever garbage happened to be on the stack, printing a plausible-
   looking but wrong "accepted" count with no warning from plain -Wall
   -Wextra, since dlog()'s own varargs signature isn't otherwise checked
   against its format string). */
static void dlog(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

static void dlog(const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);

    fputs(buf, stdout);
    fputc('\n', stdout);
    fflush(stdout);
    if (g_log) {
        static int call_count = 0;
        call_count++;

        fputs(buf, g_log);
        fputc('\n', g_log);
        fflush(g_log);
        /* Throttled, not skipped -- see FSYNC_EVERY_N_CALLS's own comment.
           Uniform per-CALL cadence, deliberately not per-iteration or
           per-printed-line, so it cannot reintroduce the same density
           mismatch it was written to eliminate. */
        if ((call_count % FSYNC_EVERY_N_CALLS) == 0) {
            fsync(fileno(g_log));
        }
    }
}

/* Called once, right before the log is closed, so throttling above never
   costs the LAST few lines their durability guarantee -- a crash between
   the final throttled fsync() and program exit would otherwise lose up
   to FSYNC_EVERY_N_CALLS-1 lines including the SUMMARY/HISTOGRAM/VERDICT
   this probe exists to produce. */
static void dlog_final_sync(void)
{
    if (g_log) {
        fflush(g_log);
        fsync(fileno(g_log));
    }
}

static void open_log(void)
{
    /* Same "w"-truncate convention as dlygran.c/clkdrift.c/clkscale.c --
       NOT a pacesim-specific deviation (checked: all three open their
       .LOG in "w" mode too). Switching to "a" here alone would make
       pacesim inconsistent with its siblings AND would silently
       concatenate unrelated runs into one file with no boundary marker,
       which is worse for a log meant to be read by a human or diffed
       against a single RUNMANIFEST-equivalent verdict. The real fix is
       giving back-to-back invocations distinct output files (argv[3],
       see main()) -- this check just makes a same-name clobber loud
       instead of silent when that isn't done. Printed to stdout/stderr,
       NOT the log itself (which is about to be truncated, so it cannot
       carry a warning about its own prior contents). */
    FILE *existing = fopen(g_log_filename, "rb");
    if (existing) {
        fclose(existing);
        fprintf(stderr,
                "WARNING: %s already exists and is about to be OVERWRITTEN "
                "by this run. If this is a second pass (e.g. a different "
                "WORKMS) and you need the prior pass's data, copy it out "
                "BEFORE rerunning, or pass a distinct logtag as argv[3] "
                "(e.g. PACESIM.EXE 3000 45 W45 -> W45.LOG) next time.\n",
                g_log_filename);
        fflush(stderr);
    }

    g_log = fopen(g_log_filename, "w");
    if (!g_log) {
        fprintf(stderr, "WARNING: could not open %s for writing\n", g_log_filename);
    }
}

/* ============================================================ */
/* Stats (same shape as dlygran.c/clkdrift.c's)                  */
/* ============================================================ */

static int dbl_cmp(const void *a, const void *b)
{
    double da = *(const double *)a, db = *(const double *)b;
    return (da > db) - (da < db);
}

typedef struct {
    double min, max, mean, median, stddev, p95;
} stats_t;

static void compute_stats(double *scratch, const double *raw, int n, stats_t *out)
{
    memcpy(scratch, raw, sizeof(double) * (size_t)n);
    qsort(scratch, (size_t)n, sizeof(double), dbl_cmp);

    double sum = 0.0;
    for (int i = 0; i < n; i++) sum += raw[i];
    double mean = sum / n;

    double sqsum = 0.0;
    for (int i = 0; i < n; i++) {
        double d = raw[i] - mean;
        sqsum += d * d;
    }
    double stddev = (n > 1) ? sqrt(sqsum / (n - 1)) : 0.0;

    int p95_idx = (int)(0.95 * (n - 1));
    if (p95_idx < 0) p95_idx = 0;
    if (p95_idx >= n) p95_idx = n - 1;

    out->min = scratch[0];
    out->max = scratch[n - 1];
    out->mean = mean;
    out->median = scratch[n / 2];
    out->stddev = stddev;
    out->p95 = scratch[p95_idx];
}

/* ============================================================ */
/* gettimeofday() as minorGems' Time::getCurrentTime() sees it --   */
/* IDENTICAL to clkdrift.c's gtod_as_minorgems_double(). Epoch      */
/* offset cancels out of any two-reading subtraction (see           */
/* TimeDOS.cpp / Time.h), so raw tv_sec is used directly.           */
/* ============================================================ */

static double gtod_as_minorgems_double(const struct timeval *tv)
{
    long ms = tv->tv_usec / 1000; /* truncating int division, TimeDOS.cpp */
    return (double)tv->tv_sec + (double)ms / 1000.0;
}

static double gtod_now(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return gtod_as_minorgems_double(&tv);
}

/* ============================================================ */
/* uclock()-as-ns clock -- approximates SDL_GetTicksNS() on this    */
/* backend (see file header FIDELITY NOTES: valid when the SDL/0122 */
/* GUS/AdLib pump timebase is not active, which holds for dossage). */
/* uclock_t is `long long` in DJGPP (verified against               */
/* i586-pc-msdosdjgpp/sys-include/time.h) so this multiply cannot    */
/* overflow within any run length this probe uses.                  */
/* ============================================================ */

static uint64_t ticks_ns(void)
{
    uclock_t u = uclock();
    return (uint64_t)((int64_t)u * 1000000000LL / (int64_t)UCLOCKS_PER_SEC);
}

/* ============================================================ */
/* Deadline-pacer constants -- verbatim from game.cpp             */
/* (lockedFrameRate=15, dosFrameTickNS, dosFrameMaxDebtNS,          */
/* dosFrameSpinHandoffNS, dosFrameYieldHalfQuantumNS). See           */
/* vendor/passage/gameSource/game.cpp lines ~1041-1074 and           */
/* ~1757-1813 for the originals this mirrors.                        */
/* ============================================================ */

#define DOS_SIM_NS_PER_SECOND     1000000000ULL
#define DOS_SIM_LOCKED_FRAME_RATE 15
#define DOS_SIM_FRAME_TICK_NS     (DOS_SIM_NS_PER_SECOND / DOS_SIM_LOCKED_FRAME_RATE)
#define DOS_SIM_MAX_DEBT_NS       (5ULL * DOS_SIM_FRAME_TICK_NS)
#define DOS_SIM_SPIN_HANDOFF_NS   3000000ULL   /* 3ms, verbatim */
#define DOS_SIM_YIELD_HALF_NS     200000ULL    /* 0.2ms, verbatim */

/* DOS_PORT_FRAME_TIME_MIN_PLAUSIBLE_S -- verbatim reject-filter floor
 * from game.cpp (Time::getCurrentTime() truncates to whole ms; a real
 * paced period should never be anywhere near this). */
#define DOS_SIM_MIN_PLAUSIBLE_S 0.001

/* Approximates the SDL_DelayNS()-routed halving sleep: a busy loop that
 * calls DJGPP delay(1) whenever >1ms of the requested duration remains,
 * else spins on uclock() -- same structure dlygran.c already measured
 * (delay(1) itself found fine-grained: median 1.236ms). See file header
 * FIDELITY NOTES for why the true SDL_SYS_DelayNS()'s DOS_Yield() calls
 * are not reproduced verbatim. */
static void sim_delay_ns(uint64_t request_ns)
{
    if (request_ns == 0) return; /* SDL_Delay(0)-equivalent handled by caller */
    uint64_t start = ticks_ns();
    uint64_t target = start + request_ns;
    for (;;) {
        uint64_t now = ticks_ns();
        if (now >= target) break;
        uint64_t remaining = target - now;
        if (remaining > 1000000ULL) { /* >1ms remaining */
            delay(1);
        } else {
            /* sub-1ms remainder: spin, no delay(1) call site at all --
               matches SDL_SYS_DelayNS's own documented >1ms guard. */
            while (ticks_ns() < target) { /* spin */ }
            break;
        }
    }
}

/* Approximates SDL_Delay(0) -- DOS_Yield()-only, no delay(1). No
 * standalone DOS_Yield() equivalent is available outside SDL/DPMI
 * cooperative scheduling (see file header FIDELITY NOTES) -- substituted
 * with a no-op here; the caller's polling loop provides the re-check. */
static void sim_yield(void)
{
    /* intentionally empty -- see FIDELITY NOTES */
}

typedef struct {
    uint64_t resync_ahead;
    uint64_t converge_normal;
    uint64_t resync_behind;
    uint64_t repay_skip;
} pacer_branch_counts_t;

/* One iteration of the absolute-deadline pacer, verbatim structure from
 * game.cpp lines ~1691-1847. inout_deadline_ns is dosNextFrameDeadlineNS,
 * updated in place exactly as the real loop updates it. */
static void pacer_step(uint64_t *inout_deadline_ns, pacer_branch_counts_t *bc)
{
    const uint64_t nowNS = ticks_ns();
    const int64_t remainingNS = (int64_t)(*inout_deadline_ns) - (int64_t)nowNS;

    if (remainingNS > (int64_t)DOS_SIM_FRAME_TICK_NS) {
        /* Ahead by more than a whole frame: resync. */
        *inout_deadline_ns = nowNS + DOS_SIM_FRAME_TICK_NS;
        sim_yield();
        bc->resync_ahead++;
    } else if (remainingNS > 0) {
        int64_t leftNS = remainingNS;

        while (leftNS > (int64_t)DOS_SIM_SPIN_HANDOFF_NS) {
            sim_delay_ns((uint64_t)(leftNS / 2));

            const uint64_t afterNS = ticks_ns();
            leftNS = (int64_t)(*inout_deadline_ns) - (int64_t)afterNS;

            if (leftNS > (int64_t)DOS_SIM_FRAME_TICK_NS) break;
        }

        while (leftNS > (int64_t)DOS_SIM_YIELD_HALF_NS) {
            sim_yield();

            const uint64_t afterNS = ticks_ns();
            leftNS = (int64_t)(*inout_deadline_ns) - (int64_t)afterNS;

            if (leftNS > (int64_t)DOS_SIM_FRAME_TICK_NS) break;
        }

        *inout_deadline_ns += DOS_SIM_FRAME_TICK_NS;
        bc->converge_normal++;
    } else if (-remainingNS > (int64_t)DOS_SIM_MAX_DEBT_NS) {
        /* Too far behind to repay honestly: resync. */
        *inout_deadline_ns = nowNS + DOS_SIM_FRAME_TICK_NS;
        sim_yield();
        bc->resync_behind++;
    } else {
        /* Behind, but repayable: skip the sleep, let the deadline
           advance normally. */
        *inout_deadline_ns += DOS_SIM_FRAME_TICK_NS;
        sim_yield();
        bc->repay_skip++;
    }
}

/* ============================================================ */
/* Histogram -- 10ms buckets from 0 to 200ms, plus overflow, so a  */
/* bimodal shape (e.g. clusters near 60ms and 110ms) is visible     */
/* without any post-processing.                                     */
/* ============================================================ */

#define HIST_BUCKET_MS   10.0
#define HIST_NUM_BUCKETS 21 /* 0-10,10-20,...,190-200,200+ */

static void print_histogram(const double *periods_ms, int n)
{
    int buckets[HIST_NUM_BUCKETS];
    memset(buckets, 0, sizeof(buckets));

    for (int i = 0; i < n; i++) {
        int b = (int)(periods_ms[i] / HIST_BUCKET_MS);
        if (b < 0) b = 0;
        if (b >= HIST_NUM_BUCKETS) b = HIST_NUM_BUCKETS - 1;
        buckets[b]++;
    }

    dlog("---- HISTOGRAM (paced period, ms, %d-wide buckets) ----", (int)HIST_BUCKET_MS);
    for (int b = 0; b < HIST_NUM_BUCKETS; b++) {
        if (buckets[b] == 0) continue;
        double lo = b * HIST_BUCKET_MS;
        char bar[65];
        int barlen = buckets[b];
        if (barlen > 60) barlen = 60;
        memset(bar, '#', (size_t)barlen);
        bar[barlen] = '\0';
        if (b == HIST_NUM_BUCKETS - 1) {
            dlog("  [%6.1f+     ] n=%5d  %s", lo, buckets[b], bar);
        } else {
            dlog("  [%6.1f-%6.1f] n=%5d  %s", lo, lo + HIST_BUCKET_MS, buckets[b], bar);
        }
    }
    dlog("%s", "");
}

/* ============================================================ */
/* main                                                            */
/* ============================================================ */

#define NITER_DEFAULT 3000
#define NITER_MIN     100
#define NITER_MAX     50000

int main(int argc, char **argv)
{
    int niter = NITER_DEFAULT;
    double work_ms = 0.0;

    if (argc > 1) {
        int req = atoi(argv[1]);
        if (req >= NITER_MIN && req <= NITER_MAX) niter = req;
    }
    if (argc > 2) {
        double req = atof(argv[2]);
        if (req >= 0.0 && req <= 200.0) work_ms = req;
    }
    if (argc > 3) {
        /* Optional log-file tag (see g_log_filename's own comment) --
           alnum/underscore only, truncated to 8 chars, always ".LOG".
           Invalid/empty input silently keeps the "PACESIM.LOG" default
           rather than erroring, matching this file's other argv
           validation style (out-of-range niter/work_ms fall back the
           same way). */
        const char *tag = argv[3];
        char sanitized[9];
        int len = 0;
        for (const char *p = tag; *p != '\0' && len < 8; p++) {
            if (isalnum((unsigned char)*p) || *p == '_') {
                sanitized[len++] = (char)toupper((unsigned char)*p);
            }
        }
        sanitized[len] = '\0';
        if (len > 0) {
            snprintf(g_log_filename, sizeof g_log_filename, "%s.LOG", sanitized);
        }
    }

    open_log();
    dlog("=== PACESIM: standalone deadline-pacer + gettimeofday() paced-period capture ===");
    dlog("(this run's log file: %s -- opened in truncate mode, same convention as", g_log_filename);
    dlog("dlygran.c/clkdrift.c/clkscale.c; pass a distinct argv[3] logtag for a second");
    dlog("back-to-back pass instead of relying on append)");
    dlog("Reproduces game.cpp's absolute-deadline pacer (uclock()-timebase) and patch");
    dlog("0035's RUNMANIFEST paced-period capture (gettimeofday()-timebase) in isolation.");
    dlog("See docs/PACER-TIMING-INVESTIGATION.md for the full question this answers.");
    dlog("%s", "");
    dlog("UCLOCKS_PER_SEC = %ld (expected ~1193180 / ~1.19MHz PIT timebase)", (long)UCLOCKS_PER_SEC);
    dlog("dossage frame tick (1/15s) = %.6fms (%llu ns)", 1000.0 / DOS_SIM_LOCKED_FRAME_RATE,
         (unsigned long long)DOS_SIM_FRAME_TICK_NS);
    dlog("niter = %d simulated frames  (~%.1fs of simulated real time at steady state)",
         niter, niter * (1000.0 / DOS_SIM_LOCKED_FRAME_RATE) / 1000.0);
    dlog("simulated per-frame work = %.3fms (argv WORKMS, 0 = pacer/clock mechanism only)", work_ms);
    dlog("%s", "");

    /* Warm-up: burn uclock()'s one-time PIT-reprogramming cost (established
       in dlygran.c) before timing starts. */
    uclock_t warm0 = uclock();
    delay(1);
    uclock_t warm1 = uclock();
    dlog("(warm-up delay(1) sample, not counted: %.4f ms)",
         (double)(warm1 - warm0) * 1000.0 / (double)UCLOCKS_PER_SEC);
    dlog("%s", "");

    double *samples_s = (double *)malloc(sizeof(double) * (size_t)niter);
    if (!samples_s) {
        dlog("FATAL: malloc failed for sample array (n=%d)", niter);
        dlog_final_sync();
        if (g_log) fclose(g_log);
        return 2;
    }

    int sample_count = 0;
    int rejected_nonpositive = 0;
    int rejected_toosmall = 0;

    uint64_t deadline_ns = ticks_ns() + DOS_SIM_FRAME_TICK_NS;
    double last_gtod = gtod_now();
    pacer_branch_counts_t bc;
    memset(&bc, 0, sizeof(bc));

    uclock_t work_ticks = (uclock_t)llround(work_ms * (double)UCLOCKS_PER_SEC / 1000.0);

    dlog("---- raw samples (paced period, ms; gettimeofday()-measured) ----");
    for (int i = 0; i < niter; i++) {
        if (work_ticks > 0) {
            /* signed uclock_t comparison, matching clkdrift.c/dlygran.c's
               own busy-wait style -- uclock() is documented (game.cpp) as
               not strictly monotonic, so this must stay signed rather than
               being cast to an unsigned type that could turn a small
               negative glitch into a near-infinite wait. */
            uclock_t w0 = uclock();
            while ((uclock() - w0) < work_ticks) { /* simulated work */ }
        }

        pacer_step(&deadline_ns, &bc);

        double now_gtod = gtod_now();
        double paced_period_s = now_gtod - last_gtod;

        if (paced_period_s <= 0.0) {
            rejected_nonpositive++;
        } else if (paced_period_s < DOS_SIM_MIN_PLAUSIBLE_S) {
            rejected_toosmall++;
        } else if (sample_count < niter) {
            samples_s[sample_count] = paced_period_s;
            sample_count++;
        }

        if (i < 200 || (i % 50) == 0) {
            /* Full-resolution logging for the first 200 frames (enough to
               see any startup transient) and every 50th frame thereafter,
               to keep the log a manageable size at niter=3000+ while still
               retaining a large, honestly-labeled raw sample for
               histogramming -- the SUMMARY/HISTOGRAM below are computed
               from ALL accepted samples, not just the logged subset. */
            dlog("  [%5d] paced_period=%9.4fms", i, paced_period_s * 1000.0);
        }

        last_gtod = now_gtod;
    }
    dlog("(raw log thinned above frame 200: only every 50th sample printed --");
    dlog(" SUMMARY/HISTOGRAM below cover all %d accepted samples, not just those printed)", sample_count);
    dlog("%s", "");

    dlog("---- Pacer branch counts (out of %d iterations) ----", niter);
    dlog("  resync_ahead(>1 frame early)=%llu  converge_normal=%llu  resync_behind(>5 frame debt)=%llu  repay_skip=%llu",
         (unsigned long long)bc.resync_ahead, (unsigned long long)bc.converge_normal,
         (unsigned long long)bc.resync_behind, (unsigned long long)bc.repay_skip);
    dlog("%s", "");

    dlog("---- Reject counts (same filter as DOS_PORT_FRAME_TIME_MIN_PLAUSIBLE_S) ----");
    dlog("  rejected_nonpositive=%d  rejected_toosmall(<%.3fs)=%d  accepted=%d  reject_rate=%.4f%%",
         rejected_nonpositive, DOS_SIM_MIN_PLAUSIBLE_S, rejected_toosmall, sample_count,
         100.0 * (rejected_nonpositive + rejected_toosmall) / niter);
    dlog("%s", "");

    if (sample_count < 2) {
        dlog("FATAL: fewer than 2 accepted samples (%d) -- cannot compute stats.", sample_count);
        free(samples_s);
        dlog_final_sync();
        if (g_log) fclose(g_log);
        return 3;
    }

    /* Convert to ms for stats/histogram (samples_s is in seconds, matching
       game.cpp's own units for frameTimeSamples). */
    double *periods_ms = (double *)malloc(sizeof(double) * (size_t)sample_count);
    if (!periods_ms) {
        dlog("FATAL: malloc failed for periods_ms (n=%d)", sample_count);
        free(samples_s);
        dlog_final_sync();
        if (g_log) fclose(g_log);
        return 2;
    }
    for (int i = 0; i < sample_count; i++) periods_ms[i] = samples_s[i] * 1000.0;

    double *scratch = (double *)malloc(sizeof(double) * (size_t)sample_count);
    stats_t st;
    if (scratch) {
        compute_stats(scratch, periods_ms, sample_count, &st);
        free(scratch);
    } else {
        memset(&st, 0, sizeof(st));
    }

    dlog("==== SUMMARY (paced period, ms) ====");
    dlog("n=%d  min=%9.4f  median=%9.4f  mean=%9.4f  p95=%9.4f  max=%9.4f  stddev=%8.4f",
         sample_count, st.min, st.median, st.mean, st.p95, st.max, st.stddev);
    dlog("%s", "");

    print_histogram(periods_ms, sample_count);

    /* ---- fps_p50/fps_p95, computed EXACTLY as game.cpp's RUNMANIFEST
       emission does: sort frameTimeSamples ascending (seconds), index =
       (int)(0.50*(n-1)) / (int)(0.95*(n-1)), fps = 1.0/sample[index]. See
       vendor/passage/gameSource/game.cpp lines ~2272-2289. */
    double *sorted_s = (double *)malloc(sizeof(double) * (size_t)sample_count);
    if (!sorted_s) {
        dlog("FATAL: malloc failed for sorted_s (n=%d)", sample_count);
        free(periods_ms);
        free(samples_s);
        dlog_final_sync();
        if (g_log) fclose(g_log);
        return 2;
    }
    memcpy(sorted_s, samples_s, sizeof(double) * (size_t)sample_count);
    qsort(sorted_s, (size_t)sample_count, sizeof(double), dbl_cmp);

    int p50_idx = (int)(0.50 * (sample_count - 1));
    int p95_idx = (int)(0.95 * (sample_count - 1));
    double fps_p50 = (sorted_s[p50_idx] > 0.0) ? (1.0 / sorted_s[p50_idx]) : 0.0;
    double fps_p95 = (sorted_s[p95_idx] > 0.0) ? (1.0 / sorted_s[p95_idx]) : 0.0;

    dlog("==== RUNMANIFEST-EQUIVALENT ====");
    dlog("fps_p50=%.2f (period=%.4fms)   fps_p95=%.2f (period=%.4fms)",
         fps_p50, sorted_s[p50_idx] * 1000.0, fps_p95, sorted_s[p95_idx] * 1000.0);
    dlog("%s", "");
    dlog("Compare directly against docs/benchmarks/ real-hardware RUNMANIFEST lines --");
    dlog("every run since patch 0035 landed reports fps_p50=16.67 (60ms) and");
    dlog("fps_p95=9.09 (110ms), unmoved across 3 video chips and 3 CPUs.");
    dlog("%s", "");

    dlog("---- VERDICT (pattern match only -- read the histogram above yourself) ----");
    int near60 = 0, near110 = 0;
    for (int i = 0; i < sample_count; i++) {
        if (periods_ms[i] > 55.0 && periods_ms[i] < 65.0) near60++;
        if (periods_ms[i] > 105.0 && periods_ms[i] < 115.0) near110++;
    }
    double frac_near60 = 100.0 * near60 / sample_count;
    double frac_near110 = 100.0 * near110 / sample_count;
    dlog("samples in [55,65)ms band: %d (%.1f%%)   samples in [105,115)ms band: %d (%.1f%%)",
         near60, frac_near60, near110, frac_near110);

    if (frac_near60 > 20.0 || frac_near110 > 20.0) {
        dlog("VERDICT: the isolated pacer+gettimeofday() capture ALREADY reproduces a");
        dlog("cluster in the same band(s) the real-hardware anomaly reports, with NO");
        dlog("rendering/audio/event-poll involved. This is strong evidence the effect");
        dlog("is a gettimeofday()-vs-pacer clock-domain artifact (the two-clock");
        dlog("mismatch described in this file's header), NOT anything full-game-");
        dlog("specific. Next step: confirm the exact quantization mechanism inside");
        dlog("gettimeofday()/DJGPP's BIOS-tick handling, and consider whether patch");
        dlog("0035's capture should be switched to a single clock family (uclock()-");
        dlog("based, matching what the pacer itself converges against) instead of");
        dlog("gettimeofday().");
    } else {
        dlog("VERDICT: the isolated pacer+gettimeofday() capture did NOT reproduce a");
        dlog("cluster in the real-hardware anomaly's bands. Either the two-clock-");
        dlog("domain hypothesis is wrong, or something else present in the full game");
        dlog("loop (rendering, audio pump, event polling, DPMI paging) is required to");
        dlog("trigger it -- redirect toward a full-game-loop-based instrumentation");
        dlog("approach instead of a further isolated probe. Re-run with a nonzero");
        dlog("WORKMS argv (simulated per-frame work) before concluding this, in case");
        dlog("the effect requires the pacer to actually spend real work time in the");
        dlog("frameTime/extraTime math this probe does not otherwise exercise.");
    }

    free(sorted_s);
    free(periods_ms);
    free(samples_s);

    dlog("%s", "");
    dlog("=== PACESIM done ===");
    dlog_final_sync();
    if (g_log) fclose(g_log);
    return 0;
}
