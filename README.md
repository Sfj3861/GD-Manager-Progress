# GD Manager Progress

Native Geode mod for Geometry Dash 2.2081 (Android 64-bit and Windows).

## Current functionality

- Adds a left-arrow entry point to the Level Info screen.
- Captures gameplay button press and release events, including button ID and player slot.
- Resets the event buffer for a new attempt and excludes replay playback.
- Exports a JSON event file with the `.gdr2` extension when the game reports level completion.
- Records practice/test flags, attempt number, starting percentage, completion timestamp, and an estimated end frame.
- Appends a CSV completion index in the mod's Geode save directory.
- Caps a single attempt at 250,000 captured input events. If the cap is reached, the export records the number of omitted events and marks the capture as truncated.

## Important limitations

- **All exports are marked `unverified`.** The mod does not prove that a run is legitimate.
- It does not comprehensively detect Mega Hack, QOLMod, Eclipse, external tools, memory edits, noclip, speedhack, or startpos manipulation.
- Merely having another mod installed is not evidence that a run was modified.
- From Zero / Startpos / Noclip Death / Noclip Accuracy / Speedhack classification is not yet implemented.
- The `.gdr2` file is this mod's own JSON event-export schema. Compatibility with third-party replay players or an external GDR2 specification has not been established.
- The end-frame value is an estimate derived from the game's progress state; it should not be treated as a verified frame-perfect timeline.
- In-game behavior still needs testing on actual Geometry Dash installations and devices.

## Export location

Open the Geode save directory for this mod. Replay exports are stored in its `replays` subdirectory; the completion index is `completions.csv`.

## Build and verification

GitHub Actions builds Android 64-bit and Windows packages using the Geode SDK. The workflow also checks that the packaged mod metadata targets Geometry Dash 2.2081. A successful CI build confirms compilation and packaging, not that every hook behaves correctly at runtime.
