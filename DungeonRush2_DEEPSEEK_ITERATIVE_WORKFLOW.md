# DungeonRush2 — DeepSeek Local Agent and Opus Review Workflow

## Purpose

Use DeepSeek in the local/agentic environment for operations that require repository access: inspecting files, building, testing, running the game, making bounded edits, and capturing evidence. Use Opus in chat for high-level architecture/design reasoning, visual critique from screenshots, review of diffs and failure reports, and refinement of high-risk changes. The owner approves major design decisions and runs or observes gameplay when the agent cannot reliably interact with the application.

This is a closed loop. Neither model should be asked to implement the entire wishlist in one pass.

## Non-negotiable rules

1. **Preserve the current working tree.** The supplied inventory showed many modified tracked files, new untracked files/assets, and a deleted tracked file. Do not run `git reset`, `git clean`, destructive checkout commands, or broad formatting/migration scripts. Do not assume the current `main` working tree is clean.
2. Before any edit, create a full sibling-directory backup or obtain the owner's explicit confirmation that a current backup exists. Keep backups and review artifacts outside the repository.
3. Do not implement a large refactor before a baseline report and approved plan exist.
4. Work in one bounded task per iteration. Do not mix ECS migration, fullscreen, AI redesign, major art replacement, and new level mechanics in one batch.
5. Preserve generated-source relationships. In particular, inspect and update `tools/build_level.py` / `tools/build_objects.py` when their generated outputs need changes; do not quietly hand-edit generated files as if they were source-of-truth.
6. Do not add major dependencies, download assets, change licensing, migrate languages, or replace SDL/C++ as an incidental part of a task. Explain the benefit and ask the owner before doing so.
7. Never claim a build, test, screenshot, or gameplay behavior was verified unless it was actually performed. Label evidence as `PASS`, `FAIL`, `BLOCKED`, or `NOT RUN`.
8. Keep the game runnable after every implementation batch. If a task cannot be kept buildable, stop and report the blocker rather than proceeding to the next feature.
9. Keep visual goals concrete. Avoid “make it prettier”; describe palette, silhouette/readability, terrain composition, spacing, scaling, animation, feedback, and measurable before/after acceptance criteria.
10. Do not include secrets, personal desktop content, or unrelated files in review bundles/screenshots.

## Phase 0 — Baseline only (no gameplay or architecture edits)

Start by inspecting the repository and its state. Do not implement new features yet.

### A. Preserve the state

- Print the project root and `git status --short`.
- Record `git diff --stat` and identify untracked/modified/deleted files.
- Make a timestamped backup of the full project to a **sibling path**, excluding nothing that is needed to restore the working state. Do not put the backup inside the project itself. Report its exact path.
- Do not commit the user's current changes automatically. If recommending a baseline commit or branch, explain exactly what it would capture and ask first.

### B. Verify build environment and baseline build

Record macOS version, architecture, compiler, CMake, Python, and SDL package versions where available. Read `README.md`, `build.sh`, and `CMakeLists.txt` before choosing a command. The repository's documented build is roughly:

```bash
./build.sh
# or, from the project root:
cmake -S . -B cmake-build-debug -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-debug -j"$(sysctl -n hw.ncpu)"
```

Use the project's documented workflow first. If configuration or compilation fails, preserve the complete relevant output and diagnose the actual cause. Do not “fix” errors during Phase 0; report them and propose the smallest next step.

### C. Run and capture current behavior if possible

- Launch the existing game from the project root so relative paths behave as expected.
- Capture, at minimum, the title/menu and one representative playable scene if the game can be launched and safely navigated.
- If feasible, capture the HUD/combat with an enemy or projectile, and the fullscreen/resized state. Do not force fullscreen if it risks losing the session; report it as not tested.
- Store evidence outside the repo, e.g. `../DungeonRush2-review/iteration-00/`.
- On macOS, a user-assisted screen capture can be made while the desired game window is visible with `screencapture -x /path/to/review/menu.png`. Close unrelated/private windows first, or crop the screenshot before sharing. If GUI interaction or screenshot capture is unavailable, ask the owner for screenshots instead of inventing them.
- Capture launch/build logs and note exact steps and controls used. Do not assert that gameplay works based on compilation alone.

### D. Produce the baseline packet

Create a concise `BASELINE_REPORT.md` outside the repo containing:

- Environment and exact configure/build/run commands.
- Build result and warnings/errors, with evidence.
- Game launch result and manual smoke-test results.
- Screenshot paths, scene/state, window size/resolution, and anything visually notable.
- Current Git dirty-state summary and backup path.
- Confirmed facts vs hypotheses/unknowns.
- Top 5 risks, prioritized; no broad code changes in this phase.
- Recommended next step and what requires Opus's design decision.

Stop after producing the packet. The owner will send the original repository ZIP, the Opus planning prompt, this baseline report, and screenshots to Opus.

## Phase 1 — Opus plans; DeepSeek checks feasibility

Opus should analyze the source and baseline evidence and return a planning packet with a target architecture, design/art direction, phased roadmap, and a first small implementation task. DeepSeek should then check the first task against the actual repository and report any incorrect assumptions before coding. Do not silently expand the task.

## Phase 2 — One implementation slice at a time

For each approved task:

1. **Restate the task.** List goal, out-of-scope work, files likely to change, risks, and acceptance criteria. Flag contradictions before editing.
2. **Checkpoint.** Confirm the current state is backed up and inspect `git status --short`. Do not overwrite unrelated existing changes. Prefer minimal, reviewable edits.
3. **Implement one coherent slice.** Keep changes small. Avoid opportunistic cleanup and unrelated feature additions.
4. **Verify.** Run the requested build/tests and relevant generators. Add focused tests for new pure logic where practical. Run the game for visual/gameplay changes when possible.
5. **Capture evidence.** Record exact commands/results, changed files, known limitations, and before/after screenshots or short clips when useful. Capture the same scene/resolution for fair visual comparison.
6. **Prepare an Opus review bundle.** Include a fresh source ZIP (exclude `.git`, compiled/build outputs and irrelevant caches), the task specification, `git status --short`, a readable diff or a list of every changed/new file, test/build logs, and screenshots. A raw `git diff` alone is insufficient if newly created untracked files exist.
7. **Stop and report.** Do not begin the next task before the owner and/or Opus review the outcome.

## Phase 3 — Opus review and refinement loop

Give Opus a narrow review assignment, not “improve everything.” Ask it to:

- Compare the implementation with the approved task and acceptance criteria.
- Review the diff for correctness, architecture boundary violations, lifecycle/update-order risks, generated-file drift, regressions, and unnecessary complexity.
- Inspect before/after screenshots for visual hierarchy, sprite consistency, environment composition, readability, and UI scaling.
- Separate `must-fix` defects from `should-improve` polish and `future` ideas.
- Return a prioritized, bounded correction list with exact files/behaviors and verification criteria. Prefer review and instructions over a wholesale rewrite of files it cannot run.

DeepSeek then implements only the approved corrections, repeats the verification/capture steps, and prepares the next review packet. Stop when acceptance criteria pass; don't keep polishing indefinitely without a new decision.

## Screenshot guidance for Opus

Screenshots are especially valuable for visual issues because chat Opus may not have a working local build/run environment. Capture only what helps answer a specific question:

- `menu-before.png` / `menu-after.png`: layout, hierarchy, selected-button state, typography, background.
- `gameplay-before.png` / `gameplay-after.png`: terrain repetition, background depth, empty-space balance, camera framing, player/enemy readability.
- `combat.png`: attack telegraphing, projectile visibility, hit feedback, health/HUD clarity.
- `fullscreen.png` and `resized-window.png`: aspect ratio, letterboxing/viewport, UI anchors, clipping and readability.
- `level-overview.png`: level structure, hazards, platforms, goals/portals and checkpoint placement.

Use identical scene, resolution and camera framing for before/after comparisons where possible. Add a short note stating what Opus should judge. A screenshot cannot establish collision correctness, enemy AI quality, frame pacing, or whether an objective is actually completable; pair visuals with a short test report or clip for behavior questions.

## Role split

### DeepSeek local agent

Best for repository-wide search, baseline/build/run, focused implementation, test execution, generator validation, logs, and screenshots. It must be evidence-driven and bounded by the approved task.

### Opus chat

Best for architecture/design trade-offs, feature prioritization, cohesive art direction, reviewing source snapshots/diffs, interpreting screenshots, detecting conflicts in plans, and proposing high-leverage refinements. It should state when it cannot verify runtime behavior from the supplied evidence.

### Owner

Approves scope changes, visual direction, dependencies, licensing, language migration, and any destructive operation; runs or observes tests when the agent cannot access the GUI; brings review packets between agents.

---

# Copy/paste prompt: DeepSeek Phase 0 baseline audit

You are the local coding agent for DungeonRush2. Follow the `DungeonRush2 — DeepSeek Local Agent and Opus Review Workflow` above.

**This is a baseline-only task. Do not change game code, architecture, art assets, dependencies, or gameplay.** First inspect the repository and current Git state, create and report a timestamped full backup outside the repository, and determine the correct documented build/run procedure. Then attempt the baseline build and, if safely possible, launch the game and capture the current menu and one representative gameplay scene. Put reports/screenshots/logs in a sibling review directory, not under the project root. Report exact commands and actual results. Label anything unavailable as `BLOCKED` or `NOT RUN`; do not invent evidence. Preserve all modified, untracked, and deleted files. Do not commit changes or run destructive Git commands. Finish with `BASELINE_REPORT.md` and a short recommendation for the next step, then stop and wait for owner review.
