/*
 * clkscale.c -- uclock() vs time(NULL) RATE SCALE FACTOR, measured across a
 * long second-boundary-aligned window, plus an independent CMOS RTC
 * cross-check.
 *
 * Standalone diagnostic for the DOSSAGE 15fps KPI gap. Context (established
 * elsewhere, NOT re-derived here):
 *
 *   - The absolute-deadline frame pacer targets 1/15s = 66.6667ms/frame. It
 *     paces on SDL_GetTicksNS() -> SDL_GetPerformanceCounter() -> DJGPP
 *     uclock(), with SDL_GetPerformanceFrequency() returning
 *     UCLOCKS_PER_SEC (1193180).
 *   - The game's own exit fps report uses a DIFFERENT clock:
 *     frameCount / (time(NULL) - startTime), i.e. the BIOS/DOS time-of-day.
 *   - Real hardware reports 14.746032 fps. 15.0 / 14.746032 = 1.01722 --
 *     a delivered period of 67.815ms against an intended 66.6667ms, a
 *     CONSTANT 1.72%.
 *   - Removing 1.23ms/frame of genuine per-frame cost moved the measured
 *     period by only 0.086ms. So the pacer IS clamping the period; the
 *     error is a scale factor on its ruler, not accumulating work.
 *
 * Therefore exactly one of these is true, and every measurement so far is
 * structurally blind to both because they all used the same clock family:
 *
 *   (a) uclock() runs SLOW relative to real time -- its true frequency is
 *       not UCLOCKS_PER_SEC -- so a "66.6667ms" deadline is really ~67.8ms
 *       of wall time; or
 *   (b) time(NULL) / the BIOS tick runs FAST relative to real time, so the
 *       fps report's denominator is too small and the game is in fact
 *       already at ~15fps.
 *
 * (This repo's earlier clkdrift.c compared gettimeofday() against uclock()
 * and found them consistent -- but BOTH are PIT-derived, so it could not
 * have caught this. clkscale.c deliberately brings in time(NULL) and the
 * CMOS RTC, which are not on that same leg.)
 *
 * ============================ METHOD =================================
 *
 * time(NULL) has 1-SECOND granularity and the effect is only 1.72%, so
 * naive endpoint sampling over 60s would carry +/-1.7% error and prove
 * nothing. Instead every endpoint is pinned to a true second BOUNDARY:
 *
 *   - Busy-poll time(NULL) in a tight loop. When the returned value TICKS
 *     OVER to a new second, that instant is a true boundary; sample
 *     uclock() immediately before and immediately after the detecting
 *     call and take the midpoint. Boundary error then equals the poll
 *     period (microseconds), not +/-1s.
 *   - The measurement window runs from the FIRST such boundary to the LAST
 *     one at least WINDOW seconds later (default 120s, argv-overridable
 *     30..600), so residual boundary error is far below 0.1%.
 *
 * Four clocks are read in the same poll loop:
 *
 *   1. uclock()       -- DJGPP's ~1.19MHz PIT-derived clock. The pacer's ruler.
 *   2. time(NULL)     -- BIOS/DOS time-of-day, 1s granularity. The fps report's ruler.
 *   3. gettimeofday() -- what minorGems' Time::getCurrentTime() is built on
 *                        (vendor/minorgems/system/dos/TimeDOS.cpp), used by
 *                        the engine elsewhere. Cheap to include, and tells us
 *                        which leg it sits on.
 *   4. CMOS RTC seconds (ports 0x70/0x71, register 0x00) -- see below.
 *
 * ---- Why the CMOS RTC is here (beyond the original brief) ----
 *
 * uclock(), gettimeofday() and the BIOS tick are all ultimately downstream
 * of the SAME 14.31818MHz oscillator via PIT channel 0, so a probe limited
 * to those three can only ever show that they DISAGREE -- never which one is
 * lying. The CMOS RTC (MC146818-compatible) runs off a SEPARATE 32.768kHz
 * crystal that the PIT cannot influence and that uclock()'s PIT-channel-0
 * reprogramming cannot perturb. That makes its seconds register a genuinely
 * independent real-time reference already present on the machine, and it is
 * polled here with the same tick-over boundary technique.
 *
 * If the RTC reads cleanly on the target, it ATTRIBUTES the error:
 *   - uclock agrees with the RTC, time(NULL) does not -> hypothesis (b).
 *   - uclock disagrees with the RTC by the same factor -> hypothesis (a).
 * If the RTC is absent, broken, or reads implausibly, the probe says so and
 * falls back to reporting the disagreement only.
 *
 * ---- What settles the attribution, and what does not ----
 *
 * The CMOS RTC is the arbiter. uclock() and the BIOS tick share the PIT
 * crystal, so a 1.72% divergence between them cannot be a crystal
 * difference -- it must be a counting or arithmetic error in one of them,
 * and only a clock on a different crystal can say which.
 *
 * Host timestamps are NOT a viable substitute at this precision. The rig
 * operator's bracket carries 1-4 s of slop (SSH round-trip latency plus
 * uncertainty about when DOS-side init ends and frame counting begins),
 * while the two hypotheses separate the elapsed figures by only ~2.1 s
 * over a 120 s window. CLKSCALE-BEGIN / CLKSCALE-END are still printed at
 * the exact instants the window opens and closes -- they remain useful
 * context and a sanity check on gross error -- but they are not the
 * arbiter, and the log says so wherever it matters.
 *
 * If the RTC leg fails, the probe reports the disagreement, says plainly
 * that it could not attribute it, and gives the window length at which
 * the host bracket WOULD become usable. It does not guess.
 *
 * ---- Reported quantities ----
 *
 *   elapsed_seconds_per_time_null    -- window length per time(NULL)
 *   elapsed_uclock_ticks             -- raw uclock() ticks over that window
 *   implied_uclock_frequency         -- ticks / time_null_seconds
 *   UCLOCKS_PER_SEC (nominal)        -- 1193180
 *   ratio R = implied / nominal, and its percentage error
 *   actual period of a nominally-66.6667ms deadline, expressed in
 *     time(NULL) seconds (= 66.6667 / R ms) -- directly comparable to the
 *     67.815ms observed on real hardware
 *   corrected tick count / corrected UCLOCKS_PER_SEC that would yield a
 *     true 66.6667ms, and the corrected nominal-ms deadline to program
 *   the same treatment for gettimeofday(), and against the RTC
 *
 * Per-second uclock tick counts are also collected as a DISTRIBUTION
 * (min/median/mean/p95/max), which separates a steady scale factor from
 * jitter or periodic correction -- a single long-window ratio cannot.
 *
 * ---- Deliberate I/O silence ----
 *
 * The probe performs NO log/disk I/O inside the measurement window. A
 * blocking disk write mid-poll could stretch one poll iteration past a
 * second and cause a missed boundary. The two console markers are the only
 * exception, and both are emitted AFTER their endpoint sample is already
 * captured. Transitions whose second-delta is not exactly 1 are excluded
 * from the per-second distribution and flagged.
 *
 * CAVEAT: DOSBox-X's emulated PIT, BIOS tick and RTC are ALL slaved to host
 * time and will therefore agree with each other. DOSBox-X numbers from this
 * probe are NOT representative and must not be quoted as an answer -- this
 * specific question is about real hardware timer behavior. DOSBox-X is a
 * correctness gate only (parses, doesn't crash, writes a log).
 *
 * Output: CLKSCALE.LOG in the working directory.
 *
 * Pure DJGPP libc + direct port I/O. No SDL, no engine, no C++.
 *
 * 8.3 DOS filenames: CLKSCALE.EXE, CLKSCALE.LOG, CLKSCALE.BAT.
 *
 * Build: `make probe-clkscale` from the repo root (see probes.mk).
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

#ifndef CLKSCALE_SELFTEST
#include <dos.h>
#include <pc.h>
#endif

/* ============================================================ */
/* HOST-ONLY SELF-TEST HARNESS (-DCLKSCALE_SELFTEST)             */
/* ============================================================ */
/*
 * Not compiled into the DOS probe. Replaces the four clock sources with a
 * simulated timebase whose scale factors are set by environment variables,
 * so the REAL analysis, projection and verdict code below can be driven
 * against ground truth that is known in advance. This exists because a
 * "it ran and wrote a log" smoke cannot tell a correct scale factor from a
 * plausible-looking wrong one, and DOSBox-X cannot either (its clocks are
 * all slaved to host time and agree by construction).
 *
 * Virtual time advances by a fixed step on every simulated clock read, so
 * the poll loop, detect windows and boundary midpoints all exercise the
 * same code paths they do on hardware.
 *
 *   CLKSCALE_SIM_U / _T / _G / _R -- rate of uclock / time() /
 *   gettimeofday / CMOS RTC relative to virtual real time (1.0 = exact).
 */
#ifdef CLKSCALE_SELFTEST
typedef long long uclock_t;
#define UCLOCKS_PER_SEC 1193180

static double g_sim_t = 0.0;        /* virtual real seconds */
static double g_sim_step = 20e-6;   /* virtual cost of one clock read */
static double g_scale_u = 1.0, g_scale_t = 1.0, g_scale_g = 1.0, g_scale_r = 1.0;

static uclock_t sim_uclock(void)
{
    g_sim_t += g_sim_step;
    return (uclock_t)(g_sim_t * g_scale_u * (double)UCLOCKS_PER_SEC);
}

static time_t sim_time(time_t *p)
{
    g_sim_t += g_sim_step;
    time_t v = (time_t)(1000000000LL + (long long)(g_sim_t * g_scale_t));
    if (p) *p = v;
    return v;
}

static int sim_gtod(struct timeval *tv)
{
    g_sim_t += g_sim_step;
    double s = g_sim_t * g_scale_g;
    tv->tv_sec = (time_t)s;
    tv->tv_usec = (long)((s - (double)(time_t)s) * 1000000.0);
    return 0;
}

#define uclock()            sim_uclock()
#define time(p)             sim_time(p)
#define gettimeofday(a, b)  sim_gtod(a)
#endif /* CLKSCALE_SELFTEST */

/* ============================================================ */
/* Logging -- fsync per line, same shape as clkdrift.c/dlygran.c. */
/* NEVER called inside the measurement window (see header).       */
/* ============================================================ */

static FILE *g_log = NULL;

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
        fputs(buf, g_log);
        fputc('\n', g_log);
        fflush(g_log);
        fsync(fileno(g_log));
    }
}

/* Blank separator line. Kept out of dlog() so no call site passes a
   zero-length format string (dlog is printf-attributed and checked). */
static void dnl(void)
{
    fputc('\n', stdout);
    fflush(stdout);
    if (g_log) {
        fputc('\n', g_log);
        fflush(g_log);
        fsync(fileno(g_log));
    }
}

static void open_log(void)
{
    g_log = fopen("CLKSCALE.LOG", "w");
    if (!g_log) {
        fputs("WARNING: could not open CLKSCALE.LOG for writing\n", stderr);
    }
}

/* Console-only marker: used for the two host-timestampable lines emitted
   from INSIDE the measurement window. Deliberately does not touch the log
   file (no fsync, no disk I/O) -- see the header's "I/O silence" note.
   Both are re-logged in full, with their uclock timestamps, afterwards. */
static void mark(const char *s)
{
    fputs(s, stdout);
    fputc('\n', stdout);
    fflush(stdout);
}

/* ============================================================ */
/* Stats (same shape as clkdrift.c's)                            */
/* ============================================================ */

static int dbl_cmp(const void *a, const void *b)
{
    double da = *(const double *)a, db = *(const double *)b;
    return (da > db) - (da < db);
}

typedef struct {
    double min, max, mean, median, stddev, p95;
} stats_t;

static int compute_stats(double *scratch, const double *raw, int n, stats_t *out)
{
    memset(out, 0, sizeof(*out));
    if (n <= 0) return 0;

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
    return 1;
}

/* ============================================================ */
/* CMOS RTC access (MC146818-compatible)                         */
/* ============================================================ */
/*
 * Index port 0x70, data port 0x71. Bit 7 of the index byte is the NMI
 * mask; we write the bare index, leaving NMI in its normal (enabled)
 * state exactly as the BIOS left it -- we never disable and forget to
 * restore it.
 *
 * The index write and data read are bracketed by disable()/enable() so an
 * interrupt handler cannot land between them and repoint the index. That
 * CLI window is a couple of microseconds and cannot perturb uclock(): the
 * PIT counter keeps counting through a CLI regardless, only the IRQ0
 * delivery is briefly deferred.
 *
 * outportb(0x80, 0) is the canonical ISA I/O delay (POST diagnostic port,
 * write-only side effect at worst lights a POST card) giving the CMOS chip
 * its bus recovery time between back-to-back accesses.
 */

#define CMOS_INDEX      0x70
#define CMOS_DATA       0x71
#define CMOS_REG_SEC    0x00
#define CMOS_REG_STAT_A 0x0A
#define CMOS_REG_STAT_B 0x0B
#define CMOS_STAT_A_UIP 0x80  /* update in progress */
#define CMOS_STAT_B_BIN 0x04  /* 1 = binary, 0 = BCD */

static int g_rtc_binary = 0; /* 0 = BCD encoding, 1 = binary */

#ifndef CLKSCALE_SELFTEST
static void io_delay(void)
{
    outportb(0x80, 0);
}

static unsigned char cmos_read(unsigned char idx)
{
    unsigned char v;
    disable();
    outportb(CMOS_INDEX, idx);
    io_delay();
    v = inportb(CMOS_DATA);
    enable();
    io_delay();
    return v;
}
#else
/* Host self-test: a well-behaved BCD RTC, never mid-update, ticking at
   g_scale_r relative to virtual real time. */
static int g_sim_no_rtc = 0;  /* CLKSCALE_SIM_NORTC=1 -> simulate a dead RTC */

static unsigned char cmos_read(unsigned char idx)
{
    g_sim_t += g_sim_step * 2;
    if (g_sim_no_rtc) return 0xFF;            /* UIP stuck + out-of-range seconds */
    if (idx == CMOS_REG_STAT_A) return 0x26;  /* UIP clear, divider = 2 */
    if (idx == CMOS_REG_STAT_B) return 0x02;  /* BCD, 24-hour */
    if (idx == CMOS_REG_SEC) {
        long long sec = (long long)(g_sim_t * g_scale_r) % 60;
        return (unsigned char)(((sec / 10) << 4) | (sec % 10));
    }
    return 0;
}
#endif

static int bcd_to_bin(unsigned char v)
{
    return (v & 0x0F) + 10 * ((v >> 4) & 0x0F);
}

/*
 * Returns 0..59, or -1 if the read must be discarded.
 *
 * Two independent guards against catching a mid-update register:
 *
 *   - UIP (update-in-progress) is checked both BEFORE and AFTER reading
 *     the seconds register. Checking only before leaves a race where the
 *     update cycle starts in between and hands back a torn value.
 *   - The seconds register is read TWICE and the two must agree.
 *
 * The UIP guard is skipped when g_rtc_ignore_uip is set. Some targets
 * (DOSBox-X among them -- it reports status A = 0xA6 with UIP apparently
 * asserted permanently) never present a UIP-clear window, which would
 * otherwise disable the RTC cross-check outright. Since the RTC leg is
 * the only thing that can ATTRIBUTE the error rather than merely show a
 * disagreement, losing it to a fussy UIP implementation is much worse
 * than falling back to the double-read agreement check alone. main()
 * sets this flag only after UIP-gated reads have demonstrably never
 * succeeded, and the log records which mode was used.
 */
static int g_rtc_ignore_uip = 0;

static int cmos_seconds(void)
{
    if (!g_rtc_ignore_uip && (cmos_read(CMOS_REG_STAT_A) & CMOS_STAT_A_UIP)) return -1;

    unsigned char r1 = cmos_read(CMOS_REG_SEC);
    unsigned char r2 = cmos_read(CMOS_REG_SEC);
    if (r1 != r2) return -1; /* straddled an update -- discard */

    if (!g_rtc_ignore_uip && (cmos_read(CMOS_REG_STAT_A) & CMOS_STAT_A_UIP)) return -1;

    int s = g_rtc_binary ? (int)r1 : bcd_to_bin(r1);
    if (s < 0 || s > 59) return -1;
    return s;
}

/* ============================================================ */
/* Transition records                                            */
/* ============================================================ */

typedef struct {
    long long sec;       /* absolute second index (time_t, or synthesized for RTC) */
    uclock_t  u_mid;     /* uclock() midpoint of the detecting call */
    uclock_t  detect_w;  /* width of the detect window, in uclock ticks */
    double    gtod;      /* gettimeofday() at the transition, as double seconds */
} xition_t;

typedef struct {
    xition_t *v;
    int       n;
    int       cap;
    int       overflow;
} xlist_t;

static int xlist_init(xlist_t *L, int cap)
{
    L->v = (xition_t *)malloc(sizeof(xition_t) * (size_t)cap);
    L->n = 0;
    L->cap = cap;
    L->overflow = 0;
    return L->v != NULL;
}

/*
 * lo = uclock() immediately after the PREVIOUS successful read of this
 * clock (which still showed the old second); hi = uclock() immediately
 * after the read that first showed the new second. The true boundary lies
 * somewhere in [lo, hi], so the midpoint is the best estimate and (hi-lo)
 * is the honest uncertainty. Bracketing only the detecting call would
 * understate that interval and bias the estimate late -- the bias largely
 * cancels between the two endpoints, but the reported uncertainty would
 * be wrong, and that figure is what justifies trusting the result.
 */
static void xlist_push(xlist_t *L, long long sec, uclock_t lo, uclock_t hi, double g)
{
    if (L->n >= L->cap) { L->overflow = 1; return; }
    L->v[L->n].sec = sec;
    L->v[L->n].u_mid = lo + (hi - lo) / 2;
    L->v[L->n].detect_w = hi - lo;
    L->v[L->n].gtod = g;
    L->n++;
}

static double gtod_seconds(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}

/* ============================================================ */
/* Analysis of one reference clock's transition list             */
/* ============================================================ */

/* dossage's own frame budget: 1.0 / lockedFrameRate seconds,
   lockedFrameRate == 15 (game.cpp). Kept as an exact fraction. */
#define DOSSAGE_FRAME_MS (1000.0 / 15.0)

/* The real-hardware delivered period this investigation is explaining,
   quoted for direct comparison only -- NOT measured by this probe. */
#define OBSERVED_PERIOD_MS 67.815

/* The scale factor the real-hardware fps gap predicts, if a clock
   disagreement is what is behind it: 15.0 / 14.746032 = 1.01722 period
   inflation, i.e. R = 1/1.01722. */
#define R_PREDICTED_DECL (1.0 / (15.0 / 14.746032))

/* True PIT crystal, and the BIOS tick it drives (crystal / 65536). Note
   UCLOCKS_PER_SEC is the rounded 1193180; the crystal is 1193181.8. The
   difference is 1.5e-6 and cannot itself explain a 1.72% error. */
#define PIT_CRYSTAL_HZ   1193181.8
#define BIOS_TICK_DIVISOR 65536.0
#define BIOS_TICK_TRUE_MS (1000.0 / (PIT_CRYSTAL_HZ / BIOS_TICK_DIVISOR))  /* 54.9255 ms */
#define BIOS_TICK_NAIVE_MS 54.0
/* Signature of conflating the true BIOS tick period with a naive "54 ms":
   54.9255 / 54 = 1.017137. Reported as a PATTERN MATCH against whatever is
   measured -- it never steers the verdict. */
#define CONFLATION_RATIO (BIOS_TICK_TRUE_MS / BIOS_TICK_NAIVE_MS)

/* Operator host-timestamp bracket slop on the rig: SSH round-trip latency
   plus uncertainty about when DOS-side init ends. Documented by the rig
   operator as 1-4 s; the pessimistic end is used for margin arithmetic. */
#define HOST_BRACKET_SLOP_S 4.0

typedef struct {
    int       valid;
    double    ref_seconds;     /* window length per the reference clock */
    double    uclock_ticks;    /* uclock ticks across the same window */
    double    gtod_seconds;    /* gettimeofday seconds across the same window */
    double    implied_freq;    /* uclock_ticks / ref_seconds */
    double    ratio;           /* implied_freq / UCLOCKS_PER_SEC */
    double    gtod_ratio;      /* gtod_seconds / ref_seconds */
    stats_t   per_sec;         /* per-1s uclock tick counts */
    int       per_sec_n;
    int       skipped;         /* transitions whose delta != 1 second */
    stats_t   detect;          /* detect-window widths, in ms */
} analysis_t;

static void analyze(const xlist_t *L, const char *name, analysis_t *out)
{
    memset(out, 0, sizeof(*out));
    if (L->n < 2) {
        dlog("%s: only %d transition(s) captured -- cannot analyze.", name, L->n);
        return;
    }

    const xition_t *first = &L->v[0];
    const xition_t *last = &L->v[L->n - 1];

    out->ref_seconds = (double)(last->sec - first->sec);
    out->uclock_ticks = (double)(last->u_mid - first->u_mid);
    out->gtod_seconds = last->gtod - first->gtod;

    if (out->ref_seconds <= 0.0) {
        dlog("%s: non-positive reference span (%.1f s) -- cannot analyze.", name, out->ref_seconds);
        return;
    }

    out->implied_freq = out->uclock_ticks / out->ref_seconds;
    out->ratio = out->implied_freq / (double)UCLOCKS_PER_SEC;
    out->gtod_ratio = out->gtod_seconds / out->ref_seconds;

    /* Per-second distribution: only consecutive pairs exactly 1s apart. */
    double *per = (double *)malloc(sizeof(double) * (size_t)L->n);
    double *dw = (double *)malloc(sizeof(double) * (size_t)L->n);
    double *scratch = (double *)malloc(sizeof(double) * (size_t)L->n);
    if (!per || !dw || !scratch) {
        dlog("%s: malloc failed in analyze() -- per-second distribution skipped.", name);
        free(per); free(dw); free(scratch);
        out->valid = 1;
        return;
    }

    int np = 0;
    for (int i = 1; i < L->n; i++) {
        if (L->v[i].sec - L->v[i - 1].sec == 1) {
            per[np++] = (double)(L->v[i].u_mid - L->v[i - 1].u_mid);
        } else {
            out->skipped++;
        }
    }
    for (int i = 0; i < L->n; i++) {
        dw[i] = (double)L->v[i].detect_w * 1000.0 / (double)UCLOCKS_PER_SEC;
    }

    compute_stats(scratch, per, np, &out->per_sec);
    out->per_sec_n = np;
    compute_stats(scratch, dw, L->n, &out->detect);

    free(per); free(dw); free(scratch);
    out->valid = 1;
}

static void report_analysis(const analysis_t *a, const char *refname)
{
    if (!a->valid) {
        dlog("---- %s reference: UNAVAILABLE ----", refname);
        dnl();
        return;
    }

    dlog("---- %s as reference clock ----", refname);
    dlog("  elapsed_seconds_per_%-14s = %.1f s", refname, a->ref_seconds);
    dlog("  elapsed_uclock_ticks           = %.0f", a->uclock_ticks);
    dlog("  implied_uclock_frequency       = %.2f Hz   (ticks / %s seconds)",
         a->implied_freq, refname);
    dlog("  nominal UCLOCKS_PER_SEC        = %ld Hz", (long)UCLOCKS_PER_SEC);
    dlog("  ratio R = implied / nominal    = %.6f  (%+.4f%% error)",
         a->ratio, (a->ratio - 1.0) * 100.0);
    dlog("  1/R (period inflation factor)  = %.6f", (a->ratio != 0.0) ? 1.0 / a->ratio : 0.0);
    dnl();
    dlog("  gettimeofday() elapsed         = %.6f s over the same window",
         a->gtod_seconds);
    dlog("  gtod / %-14s        = %.6f  (%+.4f%% error)",
         refname, a->gtod_ratio, (a->gtod_ratio - 1.0) * 100.0);
    dnl();
    dlog("  per-1s uclock tick counts (n=%d, %d transition(s) excluded for delta!=1s):",
         a->per_sec_n, a->skipped);
    if (a->per_sec_n > 0) {
        dlog("    min=%.0f median=%.0f mean=%.1f p95=%.0f max=%.0f stddev=%.1f",
             a->per_sec.min, a->per_sec.median, a->per_sec.mean,
             a->per_sec.p95, a->per_sec.max, a->per_sec.stddev);
        dlog("    (nominal would be %ld; spread here separates a STEADY scale factor",
             (long)UCLOCKS_PER_SEC);
        dlog("     from jitter or periodic correction -- a single ratio cannot)");
    }
    dlog("  boundary detect-window width (poll granularity = boundary uncertainty):");
    dlog("    min=%.4f median=%.4f mean=%.4f p95=%.4f max=%.4f ms",
         a->detect.min, a->detect.median, a->detect.mean,
         a->detect.p95, a->detect.max);
    if (a->ref_seconds > 0.0) {
        double worst = a->detect.max * 2.0 / (a->ref_seconds * 1000.0);
        dlog("    worst-case contribution to R from both boundaries: %.6f%% -- compare",
             worst * 100.0);
        dlog("    against the %+.4f%% effect being measured.", (a->ratio - 1.0) * 100.0);
    }
    dnl();

    /* Frame-budget projection. */
    if (a->ratio > 0.0) {
        double actual_ms = DOSSAGE_FRAME_MS / a->ratio;
        dlog("  FRAME-BUDGET PROJECTION (%s taken as real time):", refname);
        dlog("    a nominally-%.4fms uclock deadline actually lasts %.4f ms",
             DOSSAGE_FRAME_MS, actual_ms);
        dlog("    -> implied fps %.6f  (vs 15.000000 intended)", 1000.0 / actual_ms);
        dlog("    real-hardware observed period, for comparison: %.3f ms", OBSERVED_PERIOD_MS);
        dlog("    difference vs observed: %+.4f ms", actual_ms - OBSERVED_PERIOD_MS);
        dlog("    CORRECTION, if this reference is real time:");
        dlog("      corrected tick count for a true %.4fms frame = %.1f ticks",
             DOSSAGE_FRAME_MS, DOSSAGE_FRAME_MS / 1000.0 * a->implied_freq);
        dlog("      (vs %.1f ticks at nominal UCLOCKS_PER_SEC)",
             DOSSAGE_FRAME_MS / 1000.0 * (double)UCLOCKS_PER_SEC);
        dlog("      corrected UCLOCKS_PER_SEC to report as the perf frequency = %.0f",
             a->implied_freq);
        dlog("      equivalently, keep nominal and program the deadline as %.4f ms",
             DOSSAGE_FRAME_MS * a->ratio);
    }
    dnl();
}

/* ============================================================ */
/* main                                                          */
/* ============================================================ */

#define WINDOW_DEFAULT 120
#define WINDOW_MIN      30
/* Raised from 600 to 900 s: with host-bracket slop at 1-4 s, the
   RTC-unavailable fallback needs roughly 700 s for the two hypotheses to
   separate by 3x the slop. 600 s could not reach that. The default stays
   120 s -- long windows are only needed if the RTC leg fails. */
#define WINDOW_MAX     900
#define RTC_GRACE_SECS   5

int main(int argc, char **argv)
{
#ifdef CLKSCALE_SELFTEST
    {
        const char *e;
        if ((e = getenv("CLKSCALE_SIM_U")) != NULL) g_scale_u = atof(e);
        if ((e = getenv("CLKSCALE_SIM_T")) != NULL) g_scale_t = atof(e);
        if ((e = getenv("CLKSCALE_SIM_G")) != NULL) g_scale_g = atof(e);
        if ((e = getenv("CLKSCALE_SIM_R")) != NULL) g_scale_r = atof(e);
        /* Virtual cost of one clock read, in seconds. Raising it models a
           slow poll loop (expensive DOS calls / emulation) and is how the
           resolution gate itself gets tested. */
        if ((e = getenv("CLKSCALE_SIM_STEP")) != NULL) g_sim_step = atof(e);
        if ((e = getenv("CLKSCALE_SIM_NORTC")) != NULL) g_sim_no_rtc = atoi(e);
        fprintf(stderr, "[SELFTEST] sim scales: u=%.6f t=%.6f g=%.6f r=%.6f\n",
                g_scale_u, g_scale_t, g_scale_g, g_scale_r);
    }
#endif

    int window = WINDOW_DEFAULT;
    if (argc > 1) {
        int req = atoi(argv[1]);
        if (req >= WINDOW_MIN && req <= WINDOW_MAX) {
            window = req;
        } else {
            fprintf(stderr, "ignoring out-of-range window '%s' (valid %d..%d), using %d\n",
                    argv[1], WINDOW_MIN, WINDOW_MAX, WINDOW_DEFAULT);
        }
    }

    open_log();
    dlog("=== CLKSCALE: uclock() vs time(NULL) rate scale factor ===");
    dlog("nominal UCLOCKS_PER_SEC = %ld", (long)UCLOCKS_PER_SEC);
    dlog("dossage frame budget (1/15s) = %.6f ms", DOSSAGE_FRAME_MS);
    dlog("measurement window = %d s of time(NULL) (argv override, range %d..%d)",
         window, WINDOW_MIN, WINDOW_MAX);
    dnl();
    dlog("Method: both endpoints are pinned to true time(NULL) SECOND BOUNDARIES");
    dlog("by busy-polling for the tick-over, so boundary error is the poll period");
    dlog("(microseconds) rather than +/-1s. See the file header for the full");
    dlog("hypothesis writeup and the honest limits on attribution.");
    dnl();

    /* ---- Warm-up. DJGPP's uclock() reprograms PIT channel 0 for higher
       resolution on its first call (established in this repo's dlygran.c);
       burn that one-time cost, and touch every other clock once, before
       any timing starts. ---- */
    uclock_t warm_u0 = uclock();
    (void)time(NULL);
    (void)gtod_seconds();
    uclock_t warm_u1 = uclock();
    dlog("(warm-up: uclock() delta across one time()+gettimeofday() pair = %.4f ms, not counted)",
         (double)(warm_u1 - warm_u0) * 1000.0 / (double)UCLOCKS_PER_SEC);

    /* ---- CMOS RTC probe/configuration ---- */
    unsigned char stat_a = cmos_read(CMOS_REG_STAT_A);
    unsigned char stat_b = cmos_read(CMOS_REG_STAT_B);
    g_rtc_binary = (stat_b & CMOS_STAT_B_BIN) ? 1 : 0;
    dlog("CMOS RTC: status A = 0x%02X (divider bits 6-4 = %u, expected 2 for a",
         stat_a, (unsigned)((stat_a >> 4) & 0x07));
    dlog("          32.768kHz crystal), status B = 0x%02X -> %s encoding",
         stat_b, g_rtc_binary ? "BINARY" : "BCD");

    int rtc_avail = 1;
    int rtc_probe_ok = 0;
    for (int i = 0; i < 2000; i++) {
        if (cmos_seconds() >= 0) { rtc_probe_ok = 1; break; }
    }
    if (!rtc_probe_ok) {
        /* UIP never cleared in 2000 tries. Rather than lose the only leg
           that can attribute the error, retry without the UIP gate and
           lean on the double-read agreement check alone. */
        g_rtc_ignore_uip = 1;
        for (int i = 0; i < 2000; i++) {
            if (cmos_seconds() >= 0) { rtc_probe_ok = 1; break; }
        }
        if (rtc_probe_ok) {
            dlog("          UIP never cleared in 2000 tries -- falling back to the");
            dlog("          double-read agreement check with the UIP gate DISABLED.");
            dlog("          (Expected on DOSBox-X; on real hardware UIP is asserted for");
            dlog("          only ~244us per second, so the gate should normally work.)");
        } else {
            g_rtc_ignore_uip = 0;
        }
    }
    if (!rtc_probe_ok) {
        rtc_avail = 0;
        dlog("          seconds register never read cleanly -- RTC cross-check DISABLED.");
    } else {
        dlog("          seconds register reads cleanly (UIP gate %s) -- RTC",
             g_rtc_ignore_uip ? "DISABLED, see above" : "active");
        dlog("          cross-check ENABLED.");
        dlog("          The RTC runs off its own 32.768kHz crystal, independent of the");
        dlog("          PIT that uclock() and the BIOS tick both derive from. This is");
        dlog("          the probe's only on-board chance to ATTRIBUTE the error rather");
        dlog("          than merely show a disagreement.");
    }
    dnl();
    dlog("Starting measurement. Expected runtime: about %d seconds, plus up to", window);
    dlog("~2s of boundary alignment. The probe writes NOTHING to disk during the");
    dlog("window (a blocking write could stretch a poll past a second boundary).");
    dnl();
    dlog("OPERATOR: host-timestamp the CLKSCALE-BEGIN and CLKSCALE-END console");
    dlog("lines below. These are a sanity check on gross error, NOT the arbiter --");
    dlog("a 1-4s host bracket cannot resolve a ~2.1s separation over %ds. The CMOS", window);
    dlog("RTC's independent crystal is what attributes the error.");
    dnl();

    xlist_t tl, rl;
    if (!xlist_init(&tl, window + 16) || !xlist_init(&rl, window + 16)) {
        dlog("FATAL: malloc failed allocating transition lists.");
        if (g_log) fclose(g_log);
        return 1;
    }

    /* ---- The measurement loop. No I/O beyond the two console markers. ---- */
    time_t t_prev = time(NULL);
    int    rs_prev = -1;
    if (rtc_avail) {
        for (int i = 0; i < 2000 && rs_prev < 0; i++) rs_prev = cmos_seconds();
        if (rs_prev < 0) rtc_avail = 0;
    }
    long long rtc_count = 0;

    uclock_t poll_min = 0, poll_max = 0, poll_sum = 0;
    long long poll_n = 0;
    uclock_t u_last_top = uclock();
    /* End of the last successful read of each clock -- the lower bound of
       the next transition's uncertainty interval (see xlist_push). */
    uclock_t t_prev_end = uclock();
    uclock_t r_prev_end = t_prev_end;

    int begin_marked = 0;
    int done = 0;

    while (!done) {
        uclock_t ub = uclock();
        time_t   tn = time(NULL);
        uclock_t ua = uclock();

        /* Poll-period accounting (top-of-loop to top-of-loop). */
        uclock_t period = ub - u_last_top;
        u_last_top = ub;
        if (poll_n == 0 || period < poll_min) poll_min = period;
        if (poll_n == 0 || period > poll_max) poll_max = period;
        poll_sum += period;
        poll_n++;

        if (tn != t_prev) {
            t_prev = tn;
            xlist_push(&tl, (long long)tn, t_prev_end, ua, gtod_seconds());
            if (!begin_marked) {
                /* Emitted AFTER the endpoint sample is already captured, so
                   the print cost cannot contaminate it. */
                mark("CLKSCALE-BEGIN");
                begin_marked = 1;
            }
        }
        t_prev_end = ua;

        if (rtc_avail) {
            int      rs = cmos_seconds();
            uclock_t ra = uclock();
            if (rs >= 0) {
                if (rs != rs_prev) {
                    int d = (rs - rs_prev + 60) % 60;
                    rtc_count += d;
                    rs_prev = rs;
                    xlist_push(&rl, rtc_count, r_prev_end, ra, gtod_seconds());
                }
                /* Advanced ONLY on a successful read, so a discarded
                   mid-update read correctly widens the next interval
                   instead of silently shrinking it. */
                r_prev_end = ra;
            }
        }

        /* Termination. */
        int time_done = (tl.n >= 2) && ((tl.v[tl.n - 1].sec - tl.v[0].sec) >= window);
        int rtc_done  = !rtc_avail || ((rl.n >= 2) && ((rl.v[rl.n - 1].sec - rl.v[0].sec) >= window));
        int time_over = (tl.n >= 2) && ((tl.v[tl.n - 1].sec - tl.v[0].sec) >= window + RTC_GRACE_SECS);

        if (time_done && (rtc_done || time_over)) {
            if (!rtc_done) rtc_avail = 0; /* RTC never completed its own window */
            done = 1;
        }
        if (tl.overflow && rl.overflow) done = 1;
    }
    mark("CLKSCALE-END");

    /* ---- Everything below is post-window; disk I/O is safe again. ---- */

    dlog("CLKSCALE-BEGIN was printed at uclock tick %.0f (first time(NULL) boundary)",
         (tl.n > 0) ? (double)tl.v[0].u_mid : 0.0);
    dlog("CLKSCALE-END   was printed at uclock tick %.0f (last time(NULL) boundary)",
         (tl.n > 0) ? (double)tl.v[tl.n - 1].u_mid : 0.0);
    dnl();
    dlog("Poll loop: %lld iterations, period min=%.4f mean=%.4f max=%.4f ms",
         poll_n,
         (double)poll_min * 1000.0 / (double)UCLOCKS_PER_SEC,
         (poll_n > 0) ? (double)poll_sum / (double)poll_n * 1000.0 / (double)UCLOCKS_PER_SEC : 0.0,
         (double)poll_max * 1000.0 / (double)UCLOCKS_PER_SEC);
    dlog("time(NULL) transitions captured: %d%s", tl.n, tl.overflow ? " (LIST OVERFLOWED)" : "");
    dlog("CMOS RTC   transitions captured: %d%s", rl.n, rl.overflow ? " (LIST OVERFLOWED)" : "");
    dnl();

    analysis_t at, ar;
    analyze(&tl, "time(NULL)", &at);
    if (rtc_avail && rl.n >= 2) {
        analyze(&rl, "CMOS RTC", &ar);
    } else {
        memset(&ar, 0, sizeof(ar));
    }

    report_analysis(&at, "time(NULL)");
    report_analysis(&ar, "CMOS RTC");

    /* ============================================================ */
    /* VERDICT                                                       */
    /* ============================================================ */

    dlog("================ VERDICT ================");

    if (!at.valid) {
        dlog("INCONCLUSIVE: the time(NULL) reference window did not complete. No");
        dlog("scale factor could be computed. Re-run; if it fails again, the");
        dlog("time(NULL) tick-over polling itself is suspect on this target.");
        dnl();
        dlog("=== CLKSCALE done ===");
        free(tl.v); free(rl.v);
        if (g_log) fclose(g_log);
        return 1;
    }

    double R = at.ratio;
    double err_pct = (R - 1.0) * 100.0;

    /* ---- Resolution gate ----
     * The whole method rests on the boundary uncertainty being far smaller
     * than the effect. If the poll loop is slow (expensive time() calls, a
     * heavily loaded machine, or an emulator where each DOS call costs
     * milliseconds of emulated time), the worst-case boundary contribution
     * can rival or exceed the 1.72% being measured -- and the most
     * dangerous outcome is then a confident but meaningless "the clocks
     * AGREE" reading. Gate the verdict on it explicitly rather than
     * leaving the operator to notice. */
    double boundary_pct = (at.ref_seconds > 0.0)
        ? (at.detect.max * 2.0 / (at.ref_seconds * 1000.0)) * 100.0
        : 1.0e9;
    int resolution_ok = (boundary_pct < 0.2);

    if (!resolution_ok) {
        dlog("*** RESOLUTION WARNING -- THIS RUN'S VERDICT IS NOT TRUSTWORTHY ***");
        dlog("Worst-case boundary uncertainty is %.4f%% of the window, which is NOT", boundary_pct);
        dlog("small against the %+.4f%% effect being measured. The poll loop was too",
             (R_PREDICTED_DECL - 1.0) * 100.0);
        dlog("slow (max detect window %.2f ms over a %.0f s window) for this window", at.detect.max, at.ref_seconds);
        dlog("length to resolve the question.");
        dlog("RE-RUN with a longer window: CLKSCALE.EXE %d", (int)ceil(at.detect.max) + 1 > WINDOW_MAX ? WINDOW_MAX : ((int)ceil(at.detect.max) + 1 < WINDOW_MIN ? WINDOW_MIN : (int)ceil(at.detect.max) + 1));
        dlog("In particular, a \"the clocks AGREE\" reading below CANNOT be trusted at");
        dlog("this resolution -- an agreement is exactly what insufficient resolution");
        dlog("counterfeits.");
        dnl();
    }
    const double R_PREDICTED = R_PREDICTED_DECL;

    dlog("uclock() vs time(NULL): R = %.6f (%+.4f%%). The real-hardware fps gap",
         R, err_pct);
    dlog("predicts R = %.6f (%+.4f%%) if a clock scale factor is the whole story.",
         R_PREDICTED, (R_PREDICTED - 1.0) * 100.0);
    dnl();

    int agree = (fabs(R - 1.0) < 0.002);                       /* within 0.2% -> no disagreement */
    int matches_gap = (fabs(R - R_PREDICTED) < 0.004);         /* within 0.4% of the predicted factor */

    if (agree && !resolution_ok) {
        dlog("FINDING: INCONCLUSIVE (resolution-limited). The measured difference is");
        dlog("%+.4f%%, but this run's own boundary uncertainty is %.4f%% -- so the", err_pct, boundary_pct);
        dlog("apparent agreement is indistinguishable from the measurement floor and");
        dlog("proves NOTHING about either hypothesis. Re-run with a longer window as");
        dlog("directed in the RESOLUTION WARNING above before drawing any conclusion.");
    } else if (agree) {
        dlog("FINDING: uclock() and time(NULL) AGREE to within %+.4f%% -- well below the",
             err_pct);
        dlog("1.72%% effect being chased. NEITHER hypothesis (a) nor (b) is supported.");
        dnl();
        dlog("This FALSIFIES the clock-scale-factor theory outright. If the pacer");
        dlog("still delivers ~%.3f ms against a %.4f ms deadline, then it is",
             OBSERVED_PERIOD_MS, DOSSAGE_FRAME_MS);
        dlog("overshooting its deadline in REAL time on a ruler that is not lying,");
        dlog("and the next place to look is the pacer's own wait mechanism (what it");
        dlog("does between deadline checks) -- not its frequency constant.");
        dlog("NOTE: this is exactly the result DOSBox-X will produce regardless of");
        dlog("real-hardware truth, because its PIT, BIOS tick and RTC are all slaved");
        dlog("to host time. Only trust this verdict from a real-hardware run.");
    } else {
        dlog("FINDING: uclock() and time(NULL) DISAGREE by %+.4f%%%s.",
             err_pct, matches_gap ? ", which MATCHES the observed fps gap" : "");
        if (matches_gap) {
            dlog("A nominally-%.4fms uclock deadline lasts %.4f ms of time(NULL) time,",
                 DOSSAGE_FRAME_MS, DOSSAGE_FRAME_MS / R);
            dlog("against %.3f ms observed on real hardware. The scale factor fully",
                 OBSERVED_PERIOD_MS);
            dlog("accounts for the gap.");
        } else {
            dlog("This does NOT match the predicted %+.4f%%, so a clock scale factor is",
                 (R_PREDICTED - 1.0) * 100.0);
            dlog("at most a PARTIAL explanation for the observed gap. Treat the");
            dlog("remainder as still unexplained.");
        }
        dnl();
        /* Named-suspect pattern match. Reported as an observation only --
           it takes no part in the attribution logic below. */
        dlog("BIOS-TICK CONFLATION CHECK (pattern match only, steers nothing):");
        dlog("  measured period inflation 1/R      = %.6f", 1.0 / R);
        dlog("  54.9255ms true tick / 54ms naive   = %.6f", CONFLATION_RATIO);
        dlog("  difference                         = %+.4f%%",
             (1.0 / R / CONFLATION_RATIO - 1.0) * 100.0);
        if (fabs(1.0 / R / CONFLATION_RATIO - 1.0) < 0.001) {
            dlog("  -> MATCHES to within 0.1%%. A 54ms-vs-54.9255ms conflation somewhere");
            dlog("     in the timebase chain is a live suspect worth grepping for. This");
            dlog("     is a numeric coincidence of the right size, NOT evidence of the");
            dlog("     mechanism -- the attribution below is what identifies the clock.");
        } else {
            dlog("  -> does NOT match the 54-vs-54.9255ms conflation signature.");
        }
        dnl();

        dlog("*** WHICH CLOCK IS LYING -- ATTRIBUTION ***");

        if (ar.valid) {
            double Rr = ar.ratio;
            /* RTC vs time(NULL) directly: both are 1-second-granularity
               references pinned the same way, so their ratio is the
               cleanest statement of the disagreement, independent of
               uclock() and of UCLOCKS_PER_SEC entirely. */
            double rtc_vs_time = (ar.ref_seconds != 0.0 && at.ref_seconds != 0.0 && Rr != 0.0)
                ? (R / Rr) : 0.0;
            dlog("The CMOS RTC read cleanly. Its 32.768kHz crystal is independent of the");
            dlog("%.4fMHz PIT crystal that BOTH uclock() and the BIOS tick descend from",
                 PIT_CRYSTAL_HZ / 1000000.0);
            dlog("(BIOS tick = PIT crystal / %.0f = %.4f Hz), so it is the tiebreaker:",
                 BIOS_TICK_DIVISOR, PIT_CRYSTAL_HZ / BIOS_TICK_DIVISOR);
            dlog("a divergence between uclock() and the BIOS tick CANNOT come from the");
            dlog("crystal they share -- it has to be a counting or arithmetic error in");
            dlog("one of them, and the RTC says which.");
            dnl();
            dlog("  uclock vs RTC:        R_rtc  = %.6f (%+.4f%%)", Rr, (Rr - 1.0) * 100.0);
            dlog("  uclock vs time(NULL): R_time = %.6f (%+.4f%%)", R, err_pct);
            dlog("  RTC vs time(NULL)             = %.6f (%+.4f%%)  [uclock-independent]",
                 rtc_vs_time, (rtc_vs_time - 1.0) * 100.0);
            dnl();
            int ucl_ok = (fabs(Rr - 1.0) < 0.002);
            int rtc_tracks_time = (fabs(Rr - R) < 0.004);

            if (ucl_ok && !rtc_tracks_time) {
                dlog("VERDICT: HYPOTHESIS (b) -- time(NULL) / the BIOS tick runs FAST.");
                dlog("uclock() tracks the independent RTC crystal to within %+.4f%%, so",
                     (Rr - 1.0) * 100.0);
                dlog("uclock() IS real time and the pacer's ruler is correct. The fps");
                dlog("report's denominator (time(NULL) - startTime) is therefore too");
                dlog("SMALL, and the game is already delivering ~15fps in real time.");
                dlog("ACTION: fix the MEASUREMENT, not the pacer -- have the exit fps");
                dlog("report use the same uclock-based clock the pacer uses, then");
                dlog("re-measure before changing any pacing code.");
            } else if (rtc_tracks_time && !ucl_ok) {
                dlog("VERDICT: HYPOTHESIS (a) -- uclock() runs SLOW relative to real time.");
                dlog("uclock() disagrees with the independent RTC crystal by %+.4f%%, in", (Rr - 1.0) * 100.0);
                dlog("the same direction and by nearly the same magnitude as it disagrees");
                dlog("with time(NULL). Two independent references agreeing against uclock()");
                dlog("means uclock()'s true frequency is NOT UCLOCKS_PER_SEC on this");
                dlog("machine -- so a \"%.4fms\" deadline really is ~%.4fms of wall time.",
                     DOSSAGE_FRAME_MS, DOSSAGE_FRAME_MS / R);
                dlog("ACTION: the pacer's frequency constant is wrong. Use the corrected");
                dlog("figures in the time(NULL) section above (implied frequency %.0f, or",
                     at.implied_freq);
                dlog("equivalently program the deadline as %.4f ms nominal).",
                     DOSSAGE_FRAME_MS * R);
            } else {
                dlog("VERDICT: AMBIGUOUS. The RTC cross-check did not cleanly separate the");
                dlog("two hypotheses (R_rtc=%.6f, R_time=%.6f -- neither 'uclock matches", Rr, R);
                dlog("RTC' nor 'RTC matches time(NULL)' holds within tolerance). All three");
                dlog("clocks disagree with each other. Fall back to the host-timestamped");
                dlog("CLKSCALE-BEGIN / CLKSCALE-END markers: whichever of the reported");
                dlog("elapsed figures matches the host's wall-clock delta is real time.");
            }
        } else {
            dlog("The CMOS RTC cross-check was UNAVAILABLE on this run, so THIS PROBE");
            dlog("CANNOT ATTRIBUTE THE ERROR BY ITSELF. It has proved only that uclock()");
            dlog("and time(NULL) DISAGREE, and by exactly how much -- not which one");
            dlog("tracks real time. Both are ultimately PIT-derived, so no combination");
            dlog("of them can settle it.");
            dnl();
            double sep = fabs(at.ref_seconds - at.uclock_ticks / (double)UCLOCKS_PER_SEC);
            double margin = sep / HOST_BRACKET_SLOP_S;
            dlog("The host timestamps CANNOT rescue this at the default window length.");
            dlog("The two hypotheses separate the elapsed figures by only %.2f s over this",
                 sep);
            dlog("window (time(NULL) says %.1f s, uclock-at-nominal says %.1f s), while the",
                 at.ref_seconds, at.uclock_ticks / (double)UCLOCKS_PER_SEC);
            dlog("rig operator's host bracket carries 1-4 s of slop (SSH round-trip plus");
            dlog("uncertainty about when DOS-side init ends). Margin over the pessimistic");
            dlog("4 s slop is only %.1fx -- %s.", margin,
                 (margin >= 3.0) ? "adequate, but confirm the bracket method"
                                 : "NOT sufficient to attribute anything");
            dnl();
            dlog("WHAT TO DO INSTEAD, in order of preference:");
            dlog("  1. Find out why the RTC read failed and re-run with it working. It is");
            dlog("     the only clean arbiter: an independent crystal, no operator in the");
            dlog("     loop. Check the CMOS lines near the top of this log.");
            dlog("  2. Failing that, re-run with a window of at least %d s, which is what",
                 (int)ceil(3.0 * HOST_BRACKET_SLOP_S / (fabs(1.0 - R) > 1e-9 ? fabs(1.0 - R) : 1.0)));
            dlog("     it takes for the separation to reach 3x the 4 s slop. Then the host");
            dlog("     bracket becomes usable:");
            dlog("       host delta near the time(NULL) figure -> HYPOTHESIS (a), uclock()");
            dlog("         runs slow; the pacer's frequency constant is wrong.");
            dlog("       host delta near the uclock figure     -> HYPOTHESIS (b), time(NULL)");
            dlog("         runs fast; the fps REPORT is wrong and the game is at ~15fps.");
            dlog("  3. Report the raw numbers and do NOT force a verdict. An unattributed");
            dlog("     measurement is a usable result; a guessed attribution is not.");
        }
    }

    dnl();
    dlog("gettimeofday() leg: gtod/time(NULL) = %.6f (%+.4f%%), gtod/uclock-nominal",
         at.gtod_ratio, (at.gtod_ratio - 1.0) * 100.0);
    {
        double ucl_secs = at.uclock_ticks / (double)UCLOCKS_PER_SEC;
        double g_over_u = (ucl_secs != 0.0) ? at.gtod_seconds / ucl_secs : 0.0;
        dlog("= %.6f (%+.4f%%). minorGems' Time::getCurrentTime() sits on whichever of",
             g_over_u, (g_over_u - 1.0) * 100.0);
        dlog("those two is ~1.000000, so the engine's own frameTime shares that leg's");
        dlog("error. (This repo's clkdrift.c already found gtod consistent with");
        dlog("uclock; a %+.4f%% figure here either confirms or overturns that.)",
             (g_over_u - 1.0) * 100.0);
    }

    dnl();
    dlog("REMINDER: DOSBox-X slaves its PIT, BIOS tick and RTC all to host time, so");
    dlog("it will report near-perfect agreement no matter what real hardware does.");
    dlog("A DOSBox-X run of this probe is a correctness gate only, never the answer.");
    dnl();
    dlog("=== CLKSCALE done ===");

    free(tl.v);
    free(rl.v);
    if (g_log) fclose(g_log);
    return 0;
}
