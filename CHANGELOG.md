# Changelog

## 2610.0801
- Feature: look-at focus needs line of sight (`bFocusRequiresLineOfSight`, `FocusLineOfSightTraces`): the best-aimed point is traced from the camera through `IsHintPointVisible` and, if blocked, gives way to the next best, up to 3 traces per pass; direct hits are unchanged (T-113).
- Tests: `Interactor.LookAt.LineOfSight.*` (7).

## 2610.0602
- Feature: hint list on `URockInteractorComponent` (`bEnableHints`, `HintRange`, `HintRefreshRate`, `MaxHints`, `HintMaxAimDegrees`, `GetHintPoints()`, `FRockInteractionHintPoint`): Interaction points of candidates in range, the closest to the view centre first and capped, focused point flagged, local pawn only; off by default (T-111).
- Feature: per-point visibility from a round-robin trace (`bTraceHintVisibility`, `HintTracesPerPass`, `HintVisibilityChannel`, `HintVisibilityTolerance`) behind the virtual `IsHintPointVisible` (T-111).
- Tests: `Hints.*` (27).

## 2610.0601
- Fix: `Candidates`, `PersistentCandidates` and the focused context are now GC-visible (`FRockInteractionCandidateEntry` is a USTRUCT); destroyed targets are dropped from both lists (still getting their exit event) and from scoring, focus on a destroyed target is cleared, and `TriggerInteraction` on one does nothing (T-110).
- Behavior: focus is cleared (and broadcast) when the focused target's options become empty after a state change; it used to be kept with no options (T-110).
- Tests: `Interactor.Lifecycle.TargetLifetime.*` (11); `StateChanged_OptionsBecomeEmpty` now expects focus cleared.

## 2610.0302
- Tests: fixed the RockInteractionTests build break

## 2610.0301
- Fix: `bEnableCandidateEnterEvents` and `bEnableCandidateExitEvents` were swapped in `URockInteractorComponent::UpdateCandidates`.
- Fix: a best candidate with no options no longer broadcasts a spurious focus-cleared when nothing was focused.
- Tests: new `RockInteractionTests` Developer module (`BRS.RockInteraction.*`).
