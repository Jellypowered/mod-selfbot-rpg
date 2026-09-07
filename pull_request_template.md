<!-- First of all, THANK YOU for your contribution. -->

## Changes Proposed:
- Describe the user-visible behavior or implementation change.
- Note compatibility, safety, and backward-compatibility impact.

## Issues Addressed:
<!-- If your fix has a relating issue, link it below -->
- Closes

## SOURCE:
<!-- If you can, include a source that can strengthen your claim -->

## Tests Performed:
<!-- Does it build without errors? Did you test in-game? What did you test? On which OS did you test? Describe any other tests performed -->
- `luac -p addon/SelfBotRPG/SelfBotRPG.lua`
- `git diff --check`
- `/stuff/Source/azerothcore-wotlk/acore.sh compiler build`
- Describe live tests, including mount selection with normal and exotic learned
  mounts, live-node rerouting, dangerous nearby groups, and optional-module
  compatibility when relevant.


## How to Test the Changes:
<!-- Describe in a detailed step-by-step order how to test the changes -->

1. Describe setup and required modules/configuration.
2. Describe exact commands or addon actions.
3. Describe expected status, movement, mount or walking fallback, danger
   skips, loot, and return behavior.

## Architecture refactors

- [ ] Review domain boundaries against `docs/architecture.md`.
- [ ] Run `python3 tests/test_restructure.py`, Lua syntax, whitespace checks, and the final compiler build.
- [ ] Record live beta regression results separately from compiler checks.
- [ ] Keep future skeletons unavailable and unregistered; preserve optional module independence.
