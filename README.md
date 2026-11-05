# GD Manager Progress

Native Geode mod for Geometry Dash.

## Current implementation
- Adds a left-arrow entry point to the Level Info screen, not the pause menu.
- Opens a progress panel for the selected level.
- Establishes the Geode CMake project and CI build workflow.

## Planned modules
- Persistent level database and run history.
- Separate categories: From Zero, Startpos, Noclip Death, Noclip Accuracy and Speedhack.
- Replay capture from actual press/release input events.
- GDR2 export after a verified 100% completion.
- Integrity classification: legitimate, modified, or unverified.

## Integrity limitations
A loaded mod being present does not prove that it altered gameplay. Detection must distinguish installed mods from active gameplay modifications. Geode can inspect loaded mods and known settings, but no client-side mod can guarantee detection of every external tool or memory modification. Unknown integrity is never labelled legitimate.

## Build
GitHub Actions builds the mod with the Geode SDK build action. The package is uploaded as a workflow artifact.
