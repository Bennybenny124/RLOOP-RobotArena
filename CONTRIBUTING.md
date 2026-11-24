# Contributing to RLOOP

> Convention v1.1, updated 2026-11-22 (v1.0 confirmed 2026-11-19). Canonical gameplay specification: [docs/spec.md](docs/spec.md).

## Workflow

- `main` is the **only long-lived integration branch**; do not create `develop`.
- Create short-lived branches from up-to-date `main`, e.g. `feat/12-world-simulation`, `fix/21-laser-damage`, `docs/15-spec`, `chore/16-ci`, `spike/24-ue5-core`. A bootstrap branch may have a descriptive slug without an issue number.
- Make a focused PR targeting `main`. One primary issue per PR; delete branches after merging.
- Default merge policy: **Create a merge commit**, so the full commit history of the branch is preserved in `main`. Do not use Squash Merge or Rebase and merge by default; a different method needs an explicit reason recorded in the PR.
- Because every branch commit lands in `main`, keep branch history clean: each commit is a focused, buildable-or-readable Conventional Commit, with no `wip`, `fix typo` or review-fixup noise. Tidy the branch before requesting review.
- Request one review from **another team member**, pass applicable CI, resolve blocking conversations, then merge. These are team requirements; repository protection must be configured separately (including allowing only merge commits in the repository's merge settings).
- Use `Closes #N` only when the PR fully completes Issue N, or `Refs #N` when it only contributes. Keep docs-only issues open until the relevant PR merges.
- Prefer small, testable changes; no direct pushes to `main` by agreement.

## Documentation freshness

- If a PR changes behavior, workflow, versions, commands, schemas or file locations, it must also update every affected document (README, `docs/spec.md`, ADRs, schema docs, etc.) so that the documents are correct **as of the moment the PR merges**. Do this in the same PR; do not defer it to a follow-up issue.
- Do not write documentation that depends on a future event, such as "will be replaced once this PR merges". Describe the state after merge.
- If no document needs to change, say why in the PR description.
- Reviewers check this item together with the code or spec change.

## Conventional Commits

Format: `<type>(<scope>): <imperative description in English>` (scope optional for cross-cutting changes). Issue number is not a mandatory commit suffix; reference the Issue via PR.

| Type | Purpose |
| --- | --- |
| `feat` | New functionality |
| `fix` | Bug fix |
| `refactor` | Restructure without behavior change |
| `test` | Tests only |
| `docs` | Documentation |
| `chore` | Maintenance |
| `build` | Build system or dependencies |
| `ci` | Continuous integration |
| `perf` | Performance improvement |

Examples: `feat(arena): implement chained box pushing`, `fix(combat): consume cooldown on missed shots`, `test(core): verify deterministic replay`.

## Scopes / Issue Classification

Commit scopes and Area names share the same vocabulary:

| Scope | Area | Responsibility |
| --- | --- | --- |
| `infra` | `area:infra` | CMake, CI, development environment |
| `core` | `area:core` | World, robot state, ticking, actions, replay |
| `arena` | `area:arena` | Arena, walls, cover, collision, box pushing |
| `combat` | `area:combat` | Attack, damage, cooldown, healing, win/loss |
| `pvp` | `area:pvp` | Input, debug viewer, local PvP |
| `bindings` | `area:bindings` | pybind11, environment interface |
| `rl` | `area:rl` | DQN, teacher, opponent pool, curriculum |
| `eval` | `area:eval` | Match runner, benchmark, model comparison |
| `ue5` | `area:ue5` | Unreal integration, visualization, inference |

- **Type**: `type:feature`, `type:bug`, `type:docs`, `type:test`, `type:chore`, `type:spike`. `type:spike` means a time-bounded technical investigation with concrete findings.
- **Priority**: `priority:P0` (current delivery blocker), `priority:P1` (important but later), `priority:P2` (optional/optimization).
- Optional markers: `epic` and `blocked`. Each issue has one Type + one primary Area + one Priority.
- **Labels are optional.** Writing the classification in a `## Classification` block in the issue body is sufficient and is the default. Repository labels may be created and applied if the team wants filtering, but do not assume they exist and do not require them.

## Issue Template

```md
## Goal
What measurable outcome is needed, and why?

## Scope
- [ ] Concrete work

## Acceptance criteria
- [ ] Observable result / reproducible test
- [ ] Affected documents (README, spec, ADR, etc.) updated to their post-merge state

## Dependencies / Related
Blocked by #N / Refs #N (if applicable)

## Classification
- Type: `type:...`
- Area: `area:...`
- Priority: `priority:P...`

## Notes / Open decisions
Unapproved assumptions, risks, or API choices
```

A feature includes its normal unit tests in acceptance criteria. Separate test issues are reserved for cross-cutting integration or regression verification.

## Pull Request Template

```md
## Summary
What changed, and why?

## Linked issue
Closes #N (or Refs #N)

## Validation
- [ ] C++ build / automated tests (attach results or explain why not)
- [ ] Manual or replay verification when applicable
- [ ] Spec / schema versions updated when necessary
- [ ] README / spec / ADR / other docs updated to the post-merge state (or explain why none needed)

## Risks and open decisions
What reviewers must decide or verify
```

## Engineering and Testing

- The C++ Core is headless, framework-agnostic and the authoritative game simulation. Python training and UE5 presentation do not duplicate game rules.
- Use a fixed simulation step, reproducible test seeds and serialized action sequences. Test replay determinism under documented compiler/platform/numerical assumptions; do not assume cross-platform floating-point bitwise equivalence.
- Cover kinematic collision, chained box pushes, simultaneous attack resolution, cooldown on firing, out-of-combat healing and exact HP-percentage timeouts.
- When changing Action, Observation, Replay or model manifest formats, increment the appropriate schema version and document compatibility.
- Keep model weights, binary replays and large UE assets out of ordinary Git history; evaluate LFS or external artifact storage with only metadata tracked.
- The character balance values and certain collision boundary choices are **not finalized**. Clearly label suggestions and record new design decisions in a PR discussion or future `docs/adr/` entry.
- Tag deliverables with versions such as `v0.1.0`. Create GitHub milestones only when corresponding delivery scope is established.
