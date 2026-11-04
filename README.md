# RE5-Recompiled

Unofficial, experimental. Goal: run the Xbox 360 version of Resident Evil 5 natively on
Snapdragon/Adreno Android phones through static recompilation with the
[ReXGlue SDK](https://github.com/rexglue/rexglue-sdk).

Started from a GPL-3.0 Android launcher by [Gabrick75](https://github.com/gabrick75). See `NOTICE`.

## What this repo does NOT contain

No game files and no code generated from the game. You must supply your own legally obtained
Xbox 360 copy. `assets/`, `generated/`, `default.xex` and `codegen.log` are git-ignored on purpose.

## Status

Early setup. The Android launcher has been cloned and renamed for RE5, but the RE5
recompile is not finished and the game does not run yet. Settings in the launcher do nothing until
the runtime reads them.

## Plan

1. **Codegen:** turn your `default.xex` into C++ with `re5_manifest.toml` (ReXGlue v0.10.x format).
2. **Desktop boot:** build the generated code and implement missing kernel imports.
3. **Android:** the launcher and native glue for arm64 Adreno phones.
4. **Performance and stability.**

## Honest expectations

Weeks to months of work. No guarantee RE5 runs fully.
