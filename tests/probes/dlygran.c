/*
 * dlygran.c -- DJGPP libc delay(1) granularity vs. a tight uclock() poll.
 *
 * Standalone diagnostic for the DOSSAGE 15fps KPI gap. Prior bisection
 * (see this repo's PLAN.md) proved real per-frame work (render+flip+
 * audio-pump+loop-tail) totals only ~45.6ms against the 66.7ms/frame
 * budget for 15fps -- ~20ms of real headroom -- and that the entire
 * remaining gap is in the frame limiter's own SDL_Delay() overshooting
 * its requested sleep by a few ms/frame, with meaningful run-to-run
 * variance (measured fps has ranged 12.19-14.5fps across otherwise-
 * identical configs). A flat -3ms compensation on the requested delay
 * was already tried and didn't help (within noise, not repeatable).
 *
 * THE MECHANISM UNDER TEST: the shared SDL3-DOS backend's
 * SDL_SYS_DelayNS() (src/timer/dos/SDL_systimer.c, baseline/pre-existing
 * shared-layer code -- confirmed against this port's
 * .sdl-dos-ports/shared/patches/sdl3-dos/0122-*.patch, which touches
 * this function but preserves its structure) busy-polls against uclock()
 * (DJGPP's ~1.19MHz PIT-based clock) and calls DOS_Yield() every
 * iteration -- but whenever more than 1ms remains in the sleep, it ALSO
 * calls DJGPP libc delay(1):
 *
 *     while ((uclock() - delay_start) < target_ticks) {
 *         DOS_Yield();
 *         uclock_t remaining = target_ticks - (uclock() - delay_start);
 *         if (remaining > (UCLOCKS_PER_SEC / 1000)) {
 *             delay(1);
 *         }
 *     }
 *
 * The code's own comment claims delay(1) "does halt-loop on the PIT,
 * which is lighter than a tight poll" -- implying it is COARSER-grained
 * than millisecond precision (possibly quantized to something like the
 * ~54.9ms/18.2Hz BIOS tick). This has never been measured on real
 * hardware; it is an inherited assumption in a comment, not a finding.
 * This project's hard rule is "measure, don't guess" (see this repo's
 * CLAUDE.md), so this probe measures it directly:
 *
 *   Series A: N back-to-back samples of "uclock() before / delay(1) /
 *             uclock() after", converted to elapsed ms. This is exactly
 *             the delay(1) call site's own behavior, isolated.
 *   Series B: N back-to-back samples of a tight uclock()-only busy-poll
 *             targeting the same ~1ms duration (spin-check uclock()
 *             until 1ms of PIT ticks elapses, no delay(1) at all) --
 *             the direct apples-to-apples alternative actually available
 *             at that call site.
 *
 * DECISIVE READING:
 *   Series A median clusters near 1.0-2.0ms, close to Series B  ->
 *       delay(1) is reasonably fine-grained; NOT the overshoot source.
 *       Eliminate this hypothesis, look elsewhere for the ~1fps gap.
 *   Series A median is many ms above Series B, OR shows a bimodal split
 *   with a cluster near the ~54.9ms BIOS tick period               ->
 *       delay(1) is BIOS-tick-quantized. Confirms the hypothesis: every
 *       delay(1) call inside the frame limiter's wait loop can cost up
 *       to a full ~55ms tick depending on call-to-tick phase, which
 *       explains both the mean overshoot AND the run-to-run variance
 *       (phase drifts differently run to run). Actionable fix: stop
 *       calling delay(1) in SDL_SYS_DelayNS()'s wait loop (shared-layer
 *       change, not this repo's -- see .sdl-dos-ports/shared/).
 *
 * CAVEAT (read before trusting this as the final answer): this measures
 * delay(1) in ISOLATION. The real game has continuous SB16-compatible
 * DMA audio interrupts firing throughout every frame, which this bare
 * probe does not have running. If delay(1) is HLT-based and wakes on
 * ANY interrupt (not just PIT/IRQ0), the real in-game number could
 * differ -- likely *shorter* than what this probe measures, since more
 * IRQs firing would wake the halt more often. Treat this probe's numbers
 * as an upper-bound / first-pass answer to "is this mechanism coarse at
 * all", not necessarily the exact in-game distribution.
 *
 * Output: DLYGRAN.LOG in the working directory (fsync per line). Prints
 * every raw sample for both series plus min/median/mean/stddev/p95/max,
 * a bucket histogram (near-1ms vs near-BIOS-tick vs other), and a single
 * decisive verdict line.
 *
 * Pure DJGPP libc (time.h uclock()/delay()). No SDL, no engine, no C++.
 *
 * 8.3 DOS filenames: DLYGRAN.EXE (7.3), DLYGRAN.LOG (7.3), DLYGRAN.BAT.
 *
 * Build: `make -f tests/probes/probes.mk dlygran` (see that file's header
 * for why probes get their own tiny Makefile instead of hooking the
 * game's). DOSBox-X smoke is correctness-only (parses, doesn't crash,
 * writes a log) -- DOSBox-X's own delay()/PIT emulation is NOT
 * representative of real-hardware BIOS-tick behavior, per this repo's
 * "DOSBox-X is a correctness/mechanism gate only, never a performance
 * proxy" rule. The real answer comes only from a 486DX2-66 real-hardware
 * run.
 *
 * License: MIT.
 */

#include <dos.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* ============================================================ */
/* Logging -- fsync per line (cheap, small N here; forensic      */
/* safety in case a real-HW run stalls mid-series on a coarse    */
/* delay(1)).                                                     */
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
    g_log = fopen("DLYGRAN.LOG", "w");
    if (!g_log) {
        fputs("WARNING: could not open DLYGRAN.LOG for writing\n", stderr);
    }
}

/* ============================================================ */
/* Stats                                                          */
/* ============================================================ */

static int dbl_cmp(const void *a, const void *b)
{
    double da = *(const double *)a, db = *(const double *)b;
    return (da > db) - (da < db);
}

typedef struct {
    double min, max, mean, median, stddev, p95;
} stats_t;

static void compute_stats(double *samples_sorted_scratch, const double *raw, int n, stats_t *out)
{
    memcpy(samples_sorted_scratch, raw, sizeof(double) * (size_t)n);
    qsort(samples_sorted_scratch, (size_t)n, sizeof(double), dbl_cmp);

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

    out->min = samples_sorted_scratch[0];
    out->max = samples_sorted_scratch[n - 1];
    out->mean = mean;
    out->median = samples_sorted_scratch[n / 2];
    out->stddev = stddev;
    out->p95 = samples_sorted_scratch[p95_idx];
}

/* Bucket a sample into: near-1ms (< 3ms), mid (3ms..40ms), or
 * near-BIOS-tick (>= 40ms -- the ~54.9ms/18.2Hz tick minus slop). */
#define BUCKET_NEAR1MS 0
#define BUCKET_MID     1
#define BUCKET_BIOSTICK 2

static int bucket_of(double ms)
{
    if (ms < 3.0) return BUCKET_NEAR1MS;
    if (ms < 40.0) return BUCKET_MID;
    return BUCKET_BIOSTICK;
}

/* ============================================================ */
/* Series A: delay(1) call-site isolation                        */
/* ============================================================ */

static void run_series_a(double *samples, int n)
{
    dlog("---- Series A: raw delay(1) samples (ms) ----");
    for (int i = 0; i < n; i++) {
        uclock_t t0 = uclock();
        delay(1);
        uclock_t t1 = uclock();
        double ms = (double)(t1 - t0) * 1000.0 / (double)UCLOCKS_PER_SEC;
        samples[i] = ms;
        dlog("A[%3d] = %9.4f ms", i, ms);
    }
}

/* ============================================================ */
/* Series B: tight uclock()-only busy-poll targeting ~1ms         */
/* ============================================================ */

static void run_series_b(double *samples, int n)
{
    dlog("---- Series B: raw tight-poll samples (ms), target=1.000ms ----");
    uclock_t target_ticks = (uclock_t)(UCLOCKS_PER_SEC / 1000);
    for (int i = 0; i < n; i++) {
        uclock_t t0 = uclock();
        while ((uclock() - t0) < target_ticks) {
            /* pure spin -- no yield, no delay: this is the direct
               alternative to delay(1) at the SDL_SYS_DelayNS() call
               site being contrasted here. */
        }
        uclock_t t1 = uclock();
        double ms = (double)(t1 - t0) * 1000.0 / (double)UCLOCKS_PER_SEC;
        samples[i] = ms;
        dlog("B[%3d] = %9.4f ms", i, ms);
    }
}

static void summarize(const char *label, const double *raw, int n, stats_t *st)
{
    double *scratch = (double *)malloc(sizeof(double) * (size_t)n);
    if (!scratch) { dlog("FATAL: malloc failed in summarize(%s)", label); return; }
    compute_stats(scratch, raw, n, st);
    free(scratch);

    int buckets[3] = {0, 0, 0};
    for (int i = 0; i < n; i++) buckets[bucket_of(raw[i])]++;

    dlog("%s  n=%d  min=%8.4f  median=%8.4f  mean=%8.4f  p95=%8.4f  max=%8.4f  stddev=%7.4f  ms",
         label, n, st->min, st->median, st->mean, st->p95, st->max, st->stddev);
    dlog("%s  buckets: near-1ms(<3ms)=%d  mid(3-40ms)=%d  near-BIOS-tick(>=40ms)=%d",
         label, buckets[BUCKET_NEAR1MS], buckets[BUCKET_MID], buckets[BUCKET_BIOSTICK]);
}

/* ============================================================ */
/* main                                                            */
/* ============================================================ */

#define N_SAMPLES 100

int main(int argc, char **argv)
{
    int n = N_SAMPLES;
    if (argc > 1) {
        int req = atoi(argv[1]);
        if (req >= 10 && req <= 5000) n = req;
    }

    open_log();
    dlog("=== DLYGRAN: delay(1) granularity vs. tight uclock() poll ===");
    dlog("UCLOCKS_PER_SEC = %ld (expected ~1193180 / ~1.19MHz PIT timebase)", (long)UCLOCKS_PER_SEC);
    dlog("n = %d samples per series", n);
    dlog("");
    dlog("CAVEAT: this probe measures delay(1) in isolation, with no SB16-");
    dlog("compatible DMA audio IRQs running (unlike the real game, which has");
    dlog("them firing continuously). If delay(1) is HLT-based and wakes on ANY");
    dlog("interrupt (not just PIT/IRQ0), real in-game numbers could be SHORTER");
    dlog("than measured here. Treat this as an upper-bound / first-pass answer,");
    dlog("not necessarily the exact in-game distribution.");
    dlog("");

    /* Warm-up: DJGPP's uclock() reprograms PIT ch0 for higher resolution
       on its first call -- burn that one-time cost before timing starts
       so sample [0] of Series A isn't contaminated by it. */
    uclock_t warm0 = uclock();
    delay(1);
    uclock_t warm1 = uclock();
    dlog("(warm-up delay(1) sample, not counted: %.4f ms)",
         (double)(warm1 - warm0) * 1000.0 / (double)UCLOCKS_PER_SEC);
    dlog("");

    double *a = (double *)malloc(sizeof(double) * (size_t)n);
    double *b = (double *)malloc(sizeof(double) * (size_t)n);
    if (!a || !b) {
        dlog("FATAL: malloc failed for sample arrays");
        if (g_log) fclose(g_log);
        return 2;
    }

    run_series_a(a, n);
    dlog("");
    run_series_b(b, n);
    dlog("");

    dlog("==== SUMMARY ====");
    stats_t st_a, st_b;
    summarize("A(delay1)", a, n, &st_a);
    summarize("B(poll)  ", b, n, &st_b);
    dlog("");

    double gap = st_a.median - st_b.median;
    dlog("Series A vs B median gap: %+.4f ms", gap);
    dlog("");

    dlog("---- VERDICT ----");
    if (st_a.median < 3.0 && st_a.p95 < 5.0 && gap < 2.0) {
        dlog("VERDICT: delay(1) is FINE-GRAINED (median %.3fms, p95 %.3fms, close to",
             st_a.median, st_a.p95);
        dlog("the tight-poll baseline of %.3fms). This does NOT explain the frame", st_b.median);
        dlog("limiter's overshoot. ELIMINATE this hypothesis -- look elsewhere for");
        dlog("the ~1fps gap (candidates: DOS_Yield() cost itself, scheduler");
        dlog("interaction, or something in the render/flip/audio-pump path not yet");
        dlog("isolated).");
    } else if (st_a.median >= 40.0) {
        dlog("VERDICT: delay(1) is COARSE and BIOS-TICK-QUANTIZED (median %.3fms,",
             st_a.median);
        dlog("vs. tight-poll baseline of %.3fms). CONFIRMS the hypothesis: every", st_b.median);
        dlog("delay(1) call inside SDL_SYS_DelayNS()'s wait loop can cost up to a");
        dlog("full ~55ms BIOS tick depending on call-to-tick phase, which explains");
        dlog("both the mean overshoot and the run-to-run fps variance (phase drifts");
        dlog("differently run to run). ACTIONABLE: stop calling delay(1) in that");
        dlog("wait loop (shared-layer SDL3-DOS backend change, coordinate with the");
        dlog("sdldos/hub-repo session -- this file lives outside this port's own");
        dlog("patches/).");
    } else {
        dlog("VERDICT: MIXED/INCONCLUSIVE by the >=40ms / <3ms thresholds above --");
        dlog("median=%.3fms p95=%.3fms gap-vs-poll=%+.3fms. Read the bucket counts", st_a.median, st_a.p95, gap);
        dlog("and raw A[] samples directly: a nontrivial near-BIOS-tick(>=40ms)");
        dlog("bucket count alongside a mostly-near-1ms median would indicate");
        dlog("call-to-tick-phase-dependent occasional coarse stalls (a partial");
        dlog("confirmation -- rare but large overshoots, consistent with the");
        dlog("observed run-to-run variance even if the median looks fine).");
    }

    free(a);
    free(b);

    dlog("");
    dlog("=== DLYGRAN done ===");
    if (g_log) fclose(g_log);
    return 0;
}
