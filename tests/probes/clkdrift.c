/*
 * clkdrift.c -- gettimeofday() vs uclock() rate agreement, mirroring the
 * EXACT frameTime measurement path dossage's own game loop uses.
 *
 * Standalone diagnostic for the DOSSAGE 15fps KPI gap. Two prior probes/
 * instrumented builds already eliminated two hypotheses for the frame
 * limiter's SDL_Delay() overshoot: (1) DJGPP libc delay(1)'s granularity
 * (see this repo's dlygran.c -- measured fine-grained on real hardware),
 * (2) the shared SDL3-DOS backend's DOS_Yield() steady-state per-call
 * cost (measured cheap on real hardware via temporary SDL_SYS_DelayNS
 * instrumentation, ~1ms/frame average -- an order of magnitude below the
 * gap being chased). Real-hardware data from that second round also
 * separately surfaced a one-time title-screen-phase startup transient,
 * NOT covered by this probe (someone else is tracking that).
 *
 * THE NEW HYPOTHESIS: the DOS_Yield() instrumentation measured the
 * SDL_Delay() call's own request (`target`) and its actual duration, both
 * via dos_perf_now()/uclock() -- DJGPP's ~1.19MHz PIT-based clock, used
 * throughout the shared SDL3-DOS backend. But the GAME itself measures
 * `frameTime` (which determines what `target` gets requested each frame)
 * via a completely different code path: minorGems' Time::getCurrentTime()
 * (vendor/minorgems/system/dos/TimeDOS.cpp in this repo), which calls
 * DJGPP's gettimeofday() and truncates the microseconds field to whole
 * milliseconds (currentTime.tv_usec / 1000, integer division).
 *
 * game.cpp's frame limiter reduces to an algebraic identity where
 * frameTime itself cancels out of the steady-state fps prediction:
 *
 *     double frameTime = newTimestamp - lastFrameTimeStamp;   // gettimeofday-based
 *     double extraTime = 1.0 / lockedFrameRate - frameTime;
 *     if( extraTime > 0 ) SDL_Delay( (Uint32)( extraTime * 1000 ) );  // uclock-based
 *
 * so each frame's total wall time reduces to 1/lockedFrameRate plus
 * SDL_Delay's own overshoot, REGARDLESS of frameTime's value -- UNLESS
 * the clock that PRODUCES frameTime (gettimeofday) disagrees with the
 * clock that SDL_Delay measures ITS OWN sleep against (uclock). The prior
 * DOS_Yield() instrumentation had no way to see this: it only checked its
 * own request against its own uclock()-based clock. If gettimeofday()
 * and uclock() aren't just noisy relative to each other but tick at
 * genuinely different real RATES, that mismatch would silently corrupt
 * every `target` the game computes, invisible to any measurement confined
 * to the SDL_Delay() call alone.
 *
 * TWO THINGS THIS PROBE DISTINGUISHES:
 *
 *   1. Pure quantization/truncation noise from the ms truncation --
 *      should be close to bias-free in a start/end difference (two
 *      independently-floored readings), bounded to roughly +/-1ms
 *      regardless of how long the measured interval is.
 *
 *   2. A genuine RATE mismatch between whatever clock gettimeofday() is
 *      ultimately backed by on this DJGPP/DOS target and uclock()'s
 *      PIT-based timebase -- if these two clocks tick at even slightly
 *      different real rates, the gettimeofday-vs-uclock discrepancy
 *      would grow roughly PROPORTIONALLY with the length of the measured
 *      interval. THIS could fully explain a persistent multi-ms/frame
 *      gap that no SDL_Delay()-confined measurement could ever see.
 *
 * (Context, not proven by this probe: DJGPP's <sys/time.h> documents a
 * "__djgpp_clock_tick_interval ... default 54.925 msec clock granularity"
 * for the BIOS/DOS tick, and dlygran.c already established that uclock()
 * REPROGRAMS PIT channel 0 for higher resolution on first use. If
 * whatever underlies gettimeofday() free-runs off that same reprogrammed
 * PIT channel via a chained/rescaled interrupt count rather than an
 * independent oscillator, an imperfect chain-ratio would show up as
 * EXACTLY the proportional rate mismatch this probe tests for. This is a
 * plausible mechanism sketch, not a finding -- only the measured numbers
 * below are the finding.)
 *
 * METHOD: for each of several known, precise uclock()-controlled busy-
 * wait interval lengths (1, 5, 10, 30, ~66.67 [dossage's own 1/15s frame
 * budget], 500 ms), run N back-to-back repetitions of:
 *
 *     gettimeofday(&tv0);  u0 = uclock();
 *     busy-wait until (uclock() - u0) >= target_ticks;
 *     u1 = uclock();  gettimeofday(&tv1);
 *
 * "true" elapsed = (u1 - u0) in ms (uclock/PIT timebase, ground truth for
 * this probe per the brief this was written against).
 *
 * "gettimeofday-measured" elapsed is computed by mirroring
 * Time::getCurrentTime()'s ACTUAL double-conversion exactly (not an
 * idealized one): each endpoint is converted to
 * seconds + (tv_usec/1000 truncated-int)/1000.0, matching TimeDOS.cpp's
 * `*outMilliseconds = currentTime.tv_usec / 1000;` and Time.h's
 * `getCurrentTime() { return currentTimeS + currentTimeMS / 1000.0; }`,
 * then the two endpoint doubles are subtracted exactly as game.cpp's
 * `frameTime = newTimestamp - lastFrameTimeStamp` does. (The Time class's
 * own epoch offset is a constant that cancels out of the subtraction, so
 * this probe uses tv_sec directly rather than reproducing Time::normalize's
 * epoch dance -- mathematically identical for a delta.)
 *
 * diff = gettimeofday-measured elapsed - true (uclock) elapsed, in ms.
 *
 * DECISIVE READING (see VERDICT at the end of the log):
 *   - mean diff roughly constant (within ~+/-1ms) across ALL interval
 *     lengths -> HYPOTHESIS 1 (quantization noise only). This lead is
 *     DEAD; redirect the investigation elsewhere.
 *   - mean diff grows roughly proportionally with interval length (e.g.
 *     materially larger at 500ms than at 1ms, beyond what sqrt(n) noise
 *     growth alone would predict) -> HYPOTHESIS 2 CONFIRMED (real rate
 *     mismatch). The probe also reports the implied rate error as a
 *     fraction (measured/true - 1) from the longest, least-relatively-
 *     noisy interval, and projects it onto dossage's real ~66.667ms/frame
 *     budget for a directly usable per-frame impact estimate.
 *
 * CAVEAT: DOSBox-X's own timing/PIT emulation is NOT representative of
 * real hardware for whatever numbers come out here -- this repo's rule is
 * "DOSBox-X is a correctness/mechanism gate only, never a performance
 * proxy". The real answer comes only from a 486-class real-hardware run.
 *
 * Output: CLKDRIFT.LOG in the working directory (fsync per line). Prints
 * every raw diff sample per interval length plus min/median/mean/stddev/
 * p95/max, then a single decisive verdict.
 *
 * Pure DJGPP libc (time.h uclock()/gettimeofday()). No SDL, no engine,
 * no C++.
 *
 * 8.3 DOS filenames: CLKDRIFT.EXE (8.3), CLKDRIFT.LOG (8.3), CLKDRIFT.BAT.
 *
 * Build: `make -f tests/probes/probes.mk probe-clkdrift` (see that file's
 * registry). DOSBox-X smoke is correctness-only (parses, doesn't crash,
 * writes a log) -- see the CAVEAT above.
 *
 * License: MIT.
 */

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
/* Logging -- fsync per line (cheap, small N here; forensic       */
/* safety in case a real-HW run behaves unexpectedly mid-series). */
/* ============================================================ */

static FILE *g_log = NULL;

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
        fputs(buf, g_log);
        fputc('\n', g_log);
        fflush(g_log);
        fsync(fileno(g_log));
    }
}

static void open_log(void)
{
    g_log = fopen("CLKDRIFT.LOG", "w");
    if (!g_log) {
        fputs("WARNING: could not open CLKDRIFT.LOG for writing\n", stderr);
    }
}

/* ============================================================ */
/* Stats (same shape as dlygran.c's) */
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
/* One interval-length trial series                              */
/* ============================================================ */

typedef struct {
    const char *label;
    double      target_ms;
} interval_t;

/* dossage's own frame budget: 1.0 / lockedFrameRate seconds, lockedFrameRate
 * == 15 (game.cpp). Kept as an exact fraction, not a rounded literal. */
#define DOSSAGE_FRAME_MS (1000.0 / 15.0)

static const interval_t g_intervals[] = {
    { "1ms",                          1.0 },
    { "5ms",                          5.0 },
    { "10ms",                        10.0 },
    { "30ms",                        30.0 },
    { "66.67ms(1/15s frame budget)", DOSSAGE_FRAME_MS },
    { "500ms",                      500.0 },
};
#define NUM_INTERVALS ((int)(sizeof(g_intervals) / sizeof(g_intervals[0])))

typedef struct {
    stats_t diff;        /* gettimeofday-measured - true(uclock), ms */
    double  mean_true_ms; /* mean of the uclock-measured "true" elapsed */
    double  rate_frac;    /* mean_diff / mean_true_ms */
} trial_result_t;

/* Mirrors TimeDOS.cpp + Time.h's getCurrentTime() double conversion
 * EXACTLY: milliseconds = tv_usec / 1000 (truncating integer division),
 * then seconds + milliseconds/1000.0. Epoch offset is a constant that
 * cancels out of any two-reading subtraction, so raw tv_sec is used
 * directly (see file header comment). */
static double gtod_as_minorgems_double(const struct timeval *tv)
{
    long ms = tv->tv_usec / 1000; /* truncating int division, same as TimeDOS.cpp */
    return (double)tv->tv_sec + (double)ms / 1000.0;
}

static void run_interval_trial(const interval_t *iv, int n, trial_result_t *out)
{
    dlog("---- Interval %s: target=%.4fms, n=%d reps ----", iv->label, iv->target_ms, n);

    uclock_t target_ticks = (uclock_t)llround(iv->target_ms * (double)UCLOCKS_PER_SEC / 1000.0);

    double *diffs = (double *)malloc(sizeof(double) * (size_t)n);
    double *trues = (double *)malloc(sizeof(double) * (size_t)n);
    if (!diffs || !trues) {
        dlog("FATAL: malloc failed in run_interval_trial(%s)", iv->label);
        free(diffs);
        free(trues);
        memset(out, 0, sizeof(*out));
        return;
    }

    for (int i = 0; i < n; i++) {
        struct timeval tv0, tv1;

        gettimeofday(&tv0, NULL);
        uclock_t u0 = uclock();
        while ((uclock() - u0) < target_ticks) {
            /* pure spin: the "true" ground-truth wait, no delay()/yield
               mixed in -- this probe isolates clock agreement, not the
               SDL_Delay() wait-loop mechanism (that's dlygran.c's job). */
        }
        uclock_t u1 = uclock();
        gettimeofday(&tv1, NULL);

        double true_ms = (double)(u1 - u0) * 1000.0 / (double)UCLOCKS_PER_SEC;
        double gtod_ms = (gtod_as_minorgems_double(&tv1) - gtod_as_minorgems_double(&tv0)) * 1000.0;
        double diff = gtod_ms - true_ms;

        diffs[i] = diff;
        trues[i] = true_ms;
        dlog("  [%3d] true=%9.4fms  gtod=%9.4fms  diff=%+8.4fms", i, true_ms, gtod_ms, diff);
    }

    double *scratch = (double *)malloc(sizeof(double) * (size_t)n);
    if (scratch) {
        compute_stats(scratch, diffs, n, &out->diff);
        free(scratch);
    } else {
        memset(&out->diff, 0, sizeof(out->diff));
    }

    double true_sum = 0.0;
    for (int i = 0; i < n; i++) true_sum += trues[i];
    out->mean_true_ms = true_sum / n;
    out->rate_frac = (out->mean_true_ms != 0.0) ? (out->diff.mean / out->mean_true_ms) : 0.0;

    dlog("%s  diff: n=%d min=%+8.4f median=%+8.4f mean=%+8.4f p95=%+8.4f max=%+8.4f stddev=%7.4f ms",
         iv->label, n, out->diff.min, out->diff.median, out->diff.mean, out->diff.p95, out->diff.max, out->diff.stddev);
    dlog("%s  mean true(uclock) elapsed=%.4fms   rate_frac(mean_diff/mean_true)=%+.6f",
         iv->label, out->mean_true_ms, out->rate_frac);
    dlog("");

    free(diffs);
    free(trues);
}

/* ============================================================ */
/* main                                                            */
/* ============================================================ */

#define N_REPS_DEFAULT 40

int main(int argc, char **argv)
{
    int n = N_REPS_DEFAULT;
    if (argc > 1) {
        int req = atoi(argv[1]);
        if (req >= 10 && req <= 500) n = req;
    }

    open_log();
    dlog("=== CLKDRIFT: gettimeofday() vs uclock() rate agreement ===");
    dlog("UCLOCKS_PER_SEC = %ld (expected ~1193180 / ~1.19MHz PIT timebase)", (long)UCLOCKS_PER_SEC);
    dlog("dossage frame budget (1/15s) = %.6fms", DOSSAGE_FRAME_MS);
    dlog("n = %d repetitions per interval length", n);
    dlog("");
    dlog("Method: mirrors the EXACT frameTime measurement path dossage's game");
    dlog("loop uses (minorGems Time::getCurrentTime() -> gettimeofday() with");
    dlog("tv_usec truncated to whole ms), compared against uclock()-measured");
    dlog("ground truth for the same busy-wait interval. See file header for");
    dlog("full hypothesis writeup.");
    dlog("");

    /* Warm-up: DJGPP's uclock() reprograms PIT ch0 for higher resolution
       on its first call (established in dlygran.c) -- burn that one-time
       cost, and take one gettimeofday() call to rule out any analogous
       first-call setup cost there, before timing starts. */
    uclock_t warm_u0 = uclock();
    struct timeval warm_tv;
    gettimeofday(&warm_tv, NULL);
    uclock_t warm_u1 = uclock();
    dlog("(warm-up: uclock() delta around one gettimeofday() call = %.4fms, not counted)",
         (double)(warm_u1 - warm_u0) * 1000.0 / (double)UCLOCKS_PER_SEC);
    dlog("");

    trial_result_t results[NUM_INTERVALS];
    for (int i = 0; i < NUM_INTERVALS; i++) {
        run_interval_trial(&g_intervals[i], n, &results[i]);
    }

    dlog("==== SUMMARY (mean diff = gettimeofday-measured - true(uclock), ms) ====");
    for (int i = 0; i < NUM_INTERVALS; i++) {
        dlog("  %-32s target=%9.4fms  mean_diff=%+8.4fms  stddev=%7.4fms  rate_frac=%+.6f",
             g_intervals[i].label, g_intervals[i].target_ms, results[i].diff.mean,
             results[i].diff.stddev, results[i].rate_frac);
    }
    dlog("");

    /* ---- Decisive verdict ---- */
    double diff_min = results[0].diff.mean, diff_max = results[0].diff.mean;
    for (int i = 1; i < NUM_INTERVALS; i++) {
        if (results[i].diff.mean < diff_min) diff_min = results[i].diff.mean;
        if (results[i].diff.mean > diff_max) diff_max = results[i].diff.mean;
    }
    double diff_range = diff_max - diff_min;

    /* Proportionality check: compare rate_frac across the two longest,
       least relatively-noisy intervals (66.67ms and 500ms) against the
       shortest (1ms), where quantization noise dominates any fraction
       computation and is expected to be unstable/uninformative. */
    double frac_66 = results[4].rate_frac;  /* index 4 == 66.67ms entry */
    double frac_500 = results[5].rate_frac; /* index 5 == 500ms entry */
    double frac_avg_long = (frac_66 + frac_500) / 2.0;
    int long_fracs_agree = (fabs(frac_66) > 1e-9 && fabs(frac_500) > 1e-9)
        ? (fabs(frac_66 - frac_500) < 0.5 * fabs(frac_avg_long) + 1e-6)
        : 0;

    dlog("---- VERDICT ----");
    dlog("mean-diff range across all interval lengths: %.4fms (max=%+.4f min=%+.4f)",
         diff_range, diff_max, diff_min);
    dlog("rate_frac at 66.67ms frame budget = %+.6f, at 500ms = %+.6f (agree-in-sign-and-", frac_66, frac_500);
    dlog("magnitude: %s)", long_fracs_agree ? "YES" : "NO");
    dlog("");

    if (diff_range < 2.0) {
        dlog("VERDICT: HYPOTHESIS 1 (quantization/truncation noise only). Mean diff");
        dlog("stays within ~%.2fms across ALL interval lengths from 1ms to 500ms --", diff_range);
        dlog("no growth with interval length. This is consistent with bias-free");
        dlog("+/-1ms floor-truncation noise, NOT a clock rate mismatch. This lead");
        dlog("is DEAD -- gettimeofday()/uclock() disagreement does not explain the");
        dlog("unreconciled ~4.4ms/frame gap. Redirect the investigation elsewhere.");
    } else if (long_fracs_agree && fabs(frac_avg_long) > 0.0005) {
        double per_frame_ms = frac_avg_long * DOSSAGE_FRAME_MS;
        dlog("VERDICT: HYPOTHESIS 2 CONFIRMED (real rate mismatch). Mean diff grows");
        dlog("with interval length (range %.4fms across 1-500ms) and the implied", diff_range);
        dlog("rate-error fraction is consistent between the 66.67ms and 500ms");
        dlog("intervals (%+.6f vs %+.6f) -- NOT explained by sqrt(n) sampling noise,", frac_66, frac_500);
        dlog("which would show fraction estimates diverging wildly at longer");
        dlog("intervals, not agreeing. gettimeofday()'s underlying clock ticks at a");
        dlog("genuinely different real rate than uclock()'s PIT timebase on this");
        dlog("target.");
        dlog("");
        dlog("Implied rate error (measured/true - 1), best estimate from the two");
        dlog("longest/least-noisy intervals: %+.6f (%.4f%%)", frac_avg_long, frac_avg_long * 100.0);
        dlog("Projected per-frame impact at dossage's real ~%.4fms/frame budget:", DOSSAGE_FRAME_MS);
        dlog("  %+.4fms/frame -- directly comparable to the unreconciled ~4.4ms/frame gap.", per_frame_ms);
        dlog("ACTIONABLE: every frameTime the game computes via gettimeofday() is");
        dlog("systematically wrong by this fraction, which corrupts the SDL_Delay()");
        dlog("`target` request BEFORE SDL_Delay/uclock ever sees it -- invisible to");
        dlog("any instrumentation confined to SDL_SYS_DelayNS() itself. Candidate");
        dlog("fixes: have game.cpp's frame limiter use uclock() (or an equivalent");
        dlog("PIT-timebase call) instead of Time::getCurrentTime()/gettimeofday()");
        dlog("for frameTime, OR apply a %+.6f correction factor if a clock swap is", frac_avg_long);
        dlog("too invasive -- either is a passage-engine (game.cpp/TimeDOS.cpp)");
        dlog("change, not this repo's probe.");
    } else {
        dlog("VERDICT: MIXED/INCONCLUSIVE by the <2ms-flat / proportional-and-stable");
        dlog("thresholds above -- diff_range=%.4fms, rate_frac(66.67ms)=%+.6f,", diff_range, frac_66);
        dlog("rate_frac(500ms)=%+.6f (agree: %s). Read the per-interval SUMMARY", frac_500, long_fracs_agree ? "YES" : "NO");
        dlog("lines and raw per-rep samples above directly: a diff that grows with");
        dlog("interval length but with an unstable rate_frac could indicate a");
        dlog("nonlinear effect (e.g. periodic BIOS-tick-driven correction inside");
        dlog("gettimeofday()'s own implementation) rather than a simple constant-");
        dlog("rate mismatch -- worth a second probe run with a larger n before");
        dlog("concluding either way.");
    }

    dlog("");
    dlog("=== CLKDRIFT done ===");
    if (g_log) fclose(g_log);
    return 0;
}
