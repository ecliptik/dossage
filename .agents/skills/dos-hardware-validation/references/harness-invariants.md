# Harness invariants: why a check is a check

Distilled from this hub's own `/HARNESS-STANDARD.md` (adopted from
doskutsu, where it originated) -- a formal, platform-agnostic
"agent-driven test harness standard" written with explicit public-release
framing ("nothing in the normative text should assume any particular
project"). Read `/HARNESS-STANDARD.md` itself for the normative spec (the
I1-I3 invariants, the L1-L3 conformance ladder, and the full section-by-
section contract); this file is the *practice* layer -- how those rules
apply concretely on this hub's own vcctrl-driven rig, with this hub's own
worked examples. Read this alongside `cell-protocol.md` and
`abba-methodology.md` -- those cover the DOS-rig-specific mechanics this
file's rules apply to.

## The core problem this whole file addresses

An agent's failure mode tends to resemble its success mode, not announce
itself as failure -- a stale file looks like a fresh one, a sender that
isn't idle yet looks like a completed transfer, a banner printing looks
like a feature engaging. **The point of a harness is to make every claim
a run might make checkable from an artifact that run actually produced**,
so "looks right" and "is right" stop being the same question.

## The three invariants

**I1 -- Attest, do not recall.** Every fact about a run must live in an
artifact the run produced, not in memory, a transcript, or prose written
after the fact. Test: could this run be fully reconstructed from its
artifact bundle alone, with no appeal to what anyone remembers happening?
A manifest field meant to attest something can still fail this if it's
written to a file the collection step never actually ships -- the witness
existed and was never delivered, which is functionally the same as never
existing.

**I2 -- Witness the state, not the artifact.** A side effect proves that
side effect happened, not that the system reached whatever state you're
inferring from it. A file arriving does not mean the sender is idle and
ready for the next step (it may still be inside its own transfer client).
A result file existing does not mean the run that was supposed to write
it actually finished, or even that this run wrote it, rather than a
previous one. A banner printing does not mean the feature it announces
actually engaged on the code path that matters. Each of these gaps has
caused a real incident on this hub's own rig (see `dos-rig-operations`'
file-transfer and log-collection material) -- I2 is the general form all
of those are specific instances of.

**I3 -- Audit for the effect, not the syntax.** When hunting a defect
class, enumerate what the system would actually *do*, not the syntax
pattern you expect causes it. This is the source of the "REM redirect"
lesson already in this hub's docs: searching for unescaped `>` characters
found some real hits, but the actual worst instance was a `REM` comment
whose *quoted example* of the bug still redirected -- documenting the fix
by citing the broken syntax literally recreated the exact file the fix
was meant to stop creating. No syntax-pattern search finds this; auditing
"what file would this line create" does.

## A check that can't distinguish success from failure is not a check

The single rule most of this file's other rules reduce to, already
referenced elsewhere in this hub's skills: **before relying on a check,
state what its failure would look like.** If that's indistinguishable
from what success looks like, it isn't a check -- replace it with one
that can actually fail visibly.

Recurring proxy-for-the-real-thing pairs worth recognizing on sight:
file arrived -> sender ready for next step; result file exists -> the
cell that should have written it finished; a result envelope is complete
-> the cells inside it succeeded (a sweep where every cell died at
startup produces an envelope structurally identical in shape to one where
every cell passed); a command prompt returned -> the command succeeded;
a process is running -> it's making progress; a status's last known value
-> its current value.

**A check that can fabricate an answer is worse than no check at all**,
because it manufactures false confidence rather than leaving an honest
gap. Prefer a check that names the attempt and points at its own witness
(something that cannot be true unless the thing you care about actually
happened) over reading a status whose own provenance you haven't
verified -- does this program even reliably set an exit code on the path
you're checking? A completion message conditioned on an unverified
exit-status source can end up reporting the *previous* command's status
without anyone noticing, because a wrong-but-plausible status looks
exactly like a right one.

### Corollaries worth keeping as their own reminders

- **Having just named a hazard is when it is most likely to recur.**
  Diagnosing a failure shape doesn't confer immunity to it -- the
  dangerous moment is the feeling of being covered right after diagnosis,
  which is exactly when the check that would catch a repeat gets skipped.
  The defense is the procedure, not the understanding of the mechanism.
- **Verification must be routine, not reserved for doubtful-looking
  claims.** A "check suspicious things" policy catches nothing, because
  a claim that looks suspicious would have been questioned anyway. This
  matters most for a report that *explains away your own error* -- it
  arrives with a built-in reason to accept it, which makes it the least
  likely claim to get independently checked, not the most.
- **A watch/monitor/alarm is a check too, and is unusually good at being
  blind to its own event**, because "nothing to report" is both its
  normal state and its broken state. Before arming a watch, state what it
  would actually emit if the exact failure it exists for happened right
  now -- if the honest answer is "nothing," it isn't a watch yet. A stall
  detector that compares the current frame signature against the
  previous one is a concrete instance of this: a dead/black capture
  yields a null signature every time, null never equals the prior null,
  so a wedged, unchanging screen scores as "activity" indefinitely (this
  is the exact mechanism behind the frame-diff stall-detector incident
  referenced in this hub's own rig-hazard material). **A null/absent/error
  reading must never flow into a comparison that treats "different" as
  "healthy"** -- absence is a third state, not a degenerate case of the
  other two. A watch should be tested by deliberately inducing its event
  before it's trusted; one that has never fired has not been shown to
  work.
- **Silence may be the failure signal, not the absence of one.** Where
  success announces itself with a banner and failure just says nothing, a
  broken run looks exactly like a clean one from the outside. Never infer
  success from an absence of complaint -- assert the positive consequence
  directly (the module is resident, the file is present, the capability
  is actually offered) instead of trusting that a problem would have said
  something.
- **An unstated scope gets read as covering whatever the reader is
  currently worried about.** A verdict is only sound within some
  boundary; if a check doesn't state that boundary in its own output,
  different readers will supply different boundaries -- typically
  whichever one answers their current question. A harness-readiness
  check that answers "can the apparatus drive the target" easily gets
  read as "is the target correctly configured to be measured," and a
  fully green harness can drive a misconfigured target for an entire
  session without anyone noticing the difference. Any check emitting a
  verdict should name its subject, and should say what it does *not*
  cover wherever a reader could plausibly over-extend it.
- **A reading must be shown to belong to the current epoch, not just to
  be currently readable.** A status surface (an LED, a "ready" flag, a
  cached value) retains its last published value through a discontinuity
  it didn't cause -- a power cycle, a reboot, a re-enumeration -- so a
  target that's mid-reset and publishing nothing just keeps showing
  whatever was true before the reset. That's not flagged stale; it reads
  as exactly the same value a genuinely-current reading would show. A
  level-check on this kind of state is invalid across any discontinuity;
  what's actually needed is a transition observed *after* the
  discontinuity (wait for the indicator to go false first, proving the
  next true you see belongs to this boot, not the previous one) or a
  value that carries its own epoch marker. The moment right after
  something changed is exactly the moment a retained reading is most
  likely to get consulted -- and exactly the moment it's least trustworthy.

## A platform profile is a lint gate, and its false-positive rate is a real requirement

Every project should maintain its own list of what its target platform
does silently -- as checkable rules, enforced by a lint gate that runs
*before* packaging, not discovered after deployment. This hub's own
DOS/DJGPP/real-MS-DOS instance of that list -- no escape character in
`COMMAND.COM`, redirection parsed inside `REM` comments, a 256-byte
environment block silently dropping a `SET`, and similar -- lives in
`docs/dos-scripting.md`, not duplicated here.

The methodology point worth keeping regardless of platform: **a gate's
false-positive rate is a functional requirement, not a nuisance to tune
out eventually.** A noisy gate stops getting run, and its mere existence
then creates false confidence that the class of bug it was meant to
catch is covered -- which is strictly worse than having no gate at all,
because the belief persists after the practice has actually lapsed. A
gate that reported 534 failures (22 real, 512 from two bugs in the gate's
own parser) went unused for exactly this reason until the parser bugs
were fixed and the count dropped to a trustworthy 25.

## Declared vs. detected are different kinds of fact -- keep both

**Declared**: human-asserted, unverifiable by a machine, goes stale the
moment the hardware changes underneath it. Must be parameterized, never
hardcoded -- an empty/unset declared field honestly reads as "nobody
stated this yet"; a hardcoded string keeps asserting its original value
forever, silently wrong the moment reality moves on.

**Detected**: software-observed, recorded verbatim, must name what
actually performed the detection. Detection can itself be masked and
report the wrong layer -- a compatibility shim that reports its own
identity rather than the real underlying hardware means a detection
field attests the shim, not the thing you actually wanted to know about.
The real discriminators are often already sitting in other log lines
(a mode list, an aperture address, a chip-specific identification
string) rather than needing new instrumentation.

Neither replaces the other -- they answer different questions, and a
result envelope being structurally complete does not imply the cells
inside it succeeded (a sweep that died at init on every cell produces an
envelope indistinguishable in shape from one where everything passed; the
separating fact is a captured completion witness, not envelope presence).

## Hardware swap discipline

A hardware change (a card swap, in this hub's own case) is the single
riskiest moment in a campaign, because it's exactly when a stale
declaration is both most likely and least visible:

- A swap ends the session -- any comparison across the swap is a
  cross-session comparison by construction (see the noise-floor section
  below), never treat it as same-session.
- Re-anchor on the new hardware (run a control + repeat pair) before
  measuring anything else -- a baseline does not survive a swap.
- Update every declared field as part of the swap procedure itself, not
  as an afterthought once someone notices something looks off.
- Re-read every detected field after the swap -- it's the only
  independent check on whether the declared fields still match reality.
  A swap where declared and detected disagree is a stopped campaign, not
  a footnote to work around.
- A replacement part lacking a mode/resolution/feature the campaign
  depends on threatens the *validity* of results from it, not just their
  comparability to other parts -- a config that can't run the reference
  workload identically must not contribute rows to a shared comparison
  table at all.
- Carry a known defect on a specific configuration forward explicitly,
  or it gets rediscovered later and mistaken for a property of whatever's
  actually under test.

## Preflight and the physical layer

Logs, screen captures, and status flags all report *software* state --
a physical-layer fault (a marginal connector, a partially-seated card, a
failing cable) sits upstream of every one of them, and no amount of
better logging reaches it. A harness needs to be able to say "I cannot
determine this" and escalate to a physical check, rather than continuing
to generate plausible-sounding software hypotheses. A confident sequence
of plausible causes, none of them right, is itself a symptom of not
having real evidence yet -- plausibility is cheap and isn't the same
thing as progress.

Two sharper rules worth applying directly to this hub's own vcctrl-driven
rig work:

- **Prove the control path at the far end, not just at the near end.**
  Every near-end status (driver loaded, device opened, write reported
  success) can be green while nothing actually arrives at the target --
  get a response *from the target itself* proving the path end-to-end
  before trusting a run. This needs four states, not two:
  answered (proven working), did not answer (a real fault), could not
  look (this configuration has no return channel to check -- must never
  collapse into "did not answer"), and tool failed (the check itself
  didn't run). Collapsing "could not look" into "did not answer" turns an
  honest gap into a false failure.
- **Preflight is one command with one exit code.** A list of several
  individually-correct readiness checks is not a gate -- lists get
  skipped exactly when a run is finally ready to start, which is the
  moment they matter most. Combine into one verdict: any fault anywhere
  -> FAULT; else any unknown -> UNKNOWN (never silently reads as pass);
  else PASS -- but still report which specific check decided the
  verdict, and don't let a later fault mask an earlier unknown or vice
  versa. Run non-invasive checks before invasive ones, so a target not
  fit to be written to is discovered without having been written to. A
  check that can't run for an incidental reason (another session holds
  the rig lock) is UNKNOWN, never FAULT -- name what's blocking it.

## Preconditions checked after the run are receipts, not gates

A precondition (no other TSR resident, the right driver loaded, the right
provider active) must be checked *before* the gated work runs, as an
executable check, not written down as prose in a run sheet read
beforehand. This is an ordering requirement, not a diligence one -- a
correctly-worded precondition assertion, applied to the returned logs
after a 14-minute run whose graphics provider had silently failed to
load, still "worked": it accurately reported the run was worthless,
fourteen minutes too late to matter. What makes this trap persistent:
a post-hoc check still passes review (it's present, correctly specified,
and does fire -- its defect is invisible in the document, visible only on
a calendar); it's cheapest to write on the wrong side (a grep over a
returned log is easy to write; checking the *live* machine needs a
different instrument, so the grep gets written first and never replaced);
and a run sheet read in order means "preparation" formally ends where the
round begins, so a post-run conditions list gets read after the round is
already over. A post-hoc version of the same check is still worth keeping
as a *second* gate (it catches drift during a run that a pure preflight
can't see) -- it should just never be *the* gate.

The software stack under test (drivers, firmware, resident providers,
compatibility shims) needs this same per-cell attestation, not just the
hardware -- a silently-declined driver falling back to a card's bare ROM
VBE is exactly this shape of failure (see `docs/video.md`'s UniVBE
material), and the discriminating field is usually already sitting in a
log line nobody had checked yet.

## Noise floor: two bands, not one

Within-session (a control + repeat pair run in one sitting) and
across-session (the identical nominal config, a different sitting -- a
different day, a card reseated) are **not interchangeable, and any
comparison must state which band it's being read against.** On this
hub's own real-hardware history: within-session repeatability measured as
tight as 0.0-0.1 fps; the identical nominal config measured a day apart
differed by up to 0.85 fps -- over 4x wider. An effect smaller than the
relevant band cannot be attributed to anything that changed, no matter
how tight the within-session numbers looked in isolation. **Quoting a
within-session figure and then comparing it against a different
session's number is one of the most common ways a benchmark manufactures
a result that isn't real** -- always requantify the band for whichever
comparison is actually being made, and treat any delta smaller than that
band as unresolved, not as a win or a loss.

The same discipline applies to validating a change to the measurement
apparatus itself (a new rig host, a different capture path, an updated
driver) -- across-session confounds exist even when nobody can name their
cause, so comparing an apparatus change against an absolute banked figure
has no power to fail; a number landing near the old one reads as a clean
pass whether or not the comparison could ever have failed. Validate an
apparatus change on *differences* instead: does the control-pair spread
widen on the new apparatus (a regression regardless of absolute numbers)?
Does a previously-measured arm delta reproduce within the known band on
the new apparatus? Pick a workload that's already gated end-to-end for
this, so an apparatus effect doesn't get confounded with an unrelated
precondition failure.

## Decompose a single metric before trusting it

A single throughput number conflates distinct costs and can actively
invert a ranking. Concrete worked example from this exact hazard: a
render-loop metric extrapolated as `flips / (route_duration -
overhead)` read as 114.9 fps on a cell that was, in wall-clock terms, the
slowest of the batch -- because 82% of that cell's wall time was booked
as one-time load-stall overhead (a scene-transition data upload), and the
extrapolation treats overhead as if it were free once it's subtracted
out, which stops meaning anything once overhead dominates the total. Fix:
decompose into an in-loop rate and an out-of-loop overhead as two
separate numbers (`per_loop_fps` and `overhead_s`), and only trust the
extrapolated per-loop figure when overhead is a small fraction of total
duration (roughly under 25%, as a starting heuristic) -- above that,
quote the plain wall-clock mean and a per-stage breakdown instead. Sanity
check: `per_loop_fps / fps_mean` should roughly equal
`duration / (duration - overhead)`; if those disagree by more than a few
percent, something else is wrong, not just the overhead ratio.

## One mechanism per measured build

Shipping more than one new mechanism in the same measured binary makes
per-mechanism attribution impossible the moment a regression shows up --
correlated mechanisms can return the identical number for every arm of a
killswitch matrix, and untangling which one actually caused an effect
after the fact costs far more than a single-lever ship would have. One
new mechanism per build; multiple ablation *cells* around that one
mechanism (default-off, killswitch-on, an alternate value) are
encouraged, not a violation -- that's a proper ablation matrix, not a
second mechanism. Pure instrumentation, banners, and probes riding along
don't count as a mechanism either. This rule exists because a real
improvement was banked, shipped default-on based on a result that looked
solid, and did not reproduce -- the discipline is the direct answer to
that incident, not a hypothetical precaution.

## An emulator is a correctness instrument, not a performance proxy

Where an emulator and real hardware disagree, real hardware wins, full
stop -- and the disagreement is not small or predictable. Measured
scaling between emulated and real-hardware timings of the *same* code
path, in the *same* binary and scene, ranged from 0.81x to 286x depending
on which code path was being measured. This is not "the emulator is
uniformly slower" or "uniformly faster" -- some code paths it barely
represents at all (a whole path it never models realistically runs
unrealistically fast), others it represents faithfully enough to be
dramatically slower per instruction than real silicon. Use an emulator
for *qualitative* ranking (which bracket of the code dominates cost) and
correctness/logic verification, never for absolute numbers or
cross-environment comparison -- `dos-emulator-workflow`'s
`local-benchmarking.md` covers the practical version of this rule.

A subtler failure mode from the same gap: **a green smoke-test pass in an
emulator only proves the code paths the emulator actually reached
executed cleanly** -- a device whose emulated behavior diverges from real
hardware (an emulated sound chip that "succeeds" a reset sequence real
hardware would fail, for instance) can silently route the smoke test
through a safe fallback branch instead of the risky real-hardware-I/O
path it was meant to exercise. "9/9 smoke cells passed" can quietly mean
"9/9 of the safe branches the emulator's own fallback took," while the
actual risky path freezes on first contact with real hardware. Before
trusting an emulator smoke pass for anything touching real port writes,
IRQ handling, or DMA, confirm the log shows the *risky* path actually
fired -- not just that nothing crashed.

## Fault handling

A retry is fine; a retry silently presented as if it were the first
attempt is not -- record retries as retries. A hung cell should be
reported as a hole in the data, never silently retried into a substitute
result: a round with a known, disclosed gap is worth more than one with
an undisclosed substitution. Classify a timeout from a rolling window of
observations, not a single frame -- a genuine wedge, a slow loading
stage, and a stage that's rendering nothing at all can be indistinguishable
in one still image, and become obvious over tens of seconds of history.
If a capture mechanism repeats frames to preserve timing across a gap, it
holds more frames than were actually captured -- report both the
captured count and the held/repeated count; the difference between them
*is* the stall structure, which is usually exactly what a stall
investigation is trying to measure.

## Archive, never delete, when clearing a collection directory

Clearing a collection/log directory before a run (to avoid the stale-
artifact-from-a-prior-run hazard this file already covers under
"declared vs. detected") must move the old contents aside, not remove
them -- the clearing step is itself part of a run's provenance chain, and
applying it destructively to the very artifacts you're trying to keep
provenance for is self-defeating. Concrete incident: correctly-archived
DOS-side artifacts survived a cleanup pass, but the harness runner's own
stdout log was deleted in the same pass under the same "clean up before
next run" reasoning -- and that log was the only recording of the actual
failure mode under investigation (a stall detector reporting false
"activity" for several minutes against a dead capture -- the same
mechanism as this file's "a watch can be blind to its own event"
section). This applies to the collection directory, the runner's own
stdout, and any capture file named after a fixed tag rather than a
run-unique one (which silently overwrites every prior poll under that
tag).

## What a project needs to emit to be drivable at all

Five requirements, in the order they matter:

1. **A bounded, observable run.** A fixed-length run with a detectable
   end. Fully deterministic (recorded input, fixed seed, fixed
   termination) is the strongest form; a scripted demo plus a hard
   termination cap is an acceptable, cheaper substitute that buys the
   bound without buying full determinism. Expect this to require real
   engine changes -- it gates everything else on this list.
2. **One schema-versioned, machine-readable manifest record at exit**,
   carrying build identity and headline metrics. This is RUNMANIFEST's
   actual justification -- see `runmanifest-log.md` and
   `shared/include/runmanifest.h`.
3. **Start and end banners.** Often a harness's only reliable progress
   signal when nothing else is instrumented yet.
4. **Decomposed metrics**, per the section above -- a single number that
   conflates distinct costs is worse than two numbers that don't.
5. **File-based configuration** that can be shipped and diffed, with
   documented precedence against any env-var overrides.

Plus the two-witness rule already referenced elsewhere in this hub's
skills: a string embedded in a binary (`strings | grep`) proves
compilation, never execution -- dead code keeps its string literals too.
Only a runtime log line, captured from an actual run, proves the code
path fired.
