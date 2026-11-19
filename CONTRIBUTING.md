# Contributing to RLOOP

> Convention v1.0, confirmed 2026-11-19. Canonical gameplay specification: [docs/spec.md](docs/spec.md).

## Workflow

- `main` is the **only long-lived integration branch**; do not create `develop`.
- Create short-lived branches from up-to-date `main`, e.g. `feat/12-world-simulation`, `fix/21-laser-damage`, `docs/15-spec`, `chore/16-ci`, `spike/24-ue5-core`. A bootstrap branch may have a descriptive slug without an issue number.
- Make a focused PR targeting `main`. One primary issue per PR; delete branches after merging. Default merge policy: **Squash Merge**.
- Request one review from **another team member**, pass applicable CI, resolve blocking conversations, then merge. These are team requirements; repository protection must be configured separately.
- Use `Closes #N` only when the PR fully completes Issue N, or `Refs #N` when it only contributes. Keep docs-only issues open until the relevant PR merges.
- Prefer small, testable changes; no direct pushes to `main` by agreement.

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

## Scopes / Issue Area Labels

Commit scopes and `area:*` label suffixes share names:

| Scope | Area label | Responsibility |
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

- **Type labels**: `type:feature`, `type:bug`, `type:docs`, `type:test`, `type:chore`, `type:spike`. `type:spike` means a time-bounded technical investigation with concrete findings.
- **Priority labels**: `priority:P0` (current delivery blocker), `priority:P1` (important but later), `priority:P2` (optional/optimization).
- Optional labels: `epic` and `blocked`. One Type + one primary Area + one Priority per issue. Create labels in the repository before applying them; do not assume uncreated labels exist.

## Issue Template

```md
## Goal
What measurable outcome is needed, and why?

## Scope
- [ ] Concrete work

## Acceptance criteria
- [ ] Observable result / reproducible test

## Dependencies / Related
Blocked by #N / Refs #N (if applicable)

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
