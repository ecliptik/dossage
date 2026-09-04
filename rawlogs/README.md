# rawlogs -- real-hardware run logs, kept out of git

Raw files pulled off the rig (RUNMANI.LOG, STDOUT.TXT, LOGS\SDLDBG.LOG,
STAGEDBG.LOG) plus the per-round reductions and build manifests behind the
benchmark records in `docs/benchmarks/`. Deliberately untracked (see
`.gitignore`): they are large, per-machine, and the records already carry
every number that matters. This README is the one tracked file here so the
directory's purpose survives a fresh clone.

Layout: `<campaign>/<round>/<cell>/<CELL>_<FILE>` -- one directory per
campaign, one per rig round, one per staged binary (A, B, D, E, D2, F, G,
D3...). `reconstructed/` subdirectories hold chat-transcribed versions that
the raw pulls superseded; never cite those.

- `dx2-50-campaign-2026-09/` -- the 486DX2-50 15 fps campaign
  (docs/benchmarks/mach64-215ct-486dx2-50-round{1,2,3}-2026-09-04.md).
  Reductions were produced from these files, not from relayed summaries.
