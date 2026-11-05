# GD Manager Progress

A native Geode mod for Geometry Dash.

## Implemented

- Adds an entry arrow to the Level Info screen. The progress panel is not placed in the pause menu.
- Hooks gameplay input events and records both button presses and releases, including the player slot and game frame.
- Starts a fresh input buffer when an attempt is reset, so the exported replay contains the attempt that reached completion.
- Exports a replay file when a level completion occurs. Practice/editor state is recorded in the export metadata and is never promoted to a legitimate record.
- Appends a compact completion index to the mod's save directory.

## Replay export format

Files use the `.gdr2` extension and contain JSON with the `GDR2` format marker, schema version, level metadata, mode flags, integrity status, and an ordered input-event array.

**Compatibility note:** this is currently GD Manager Progress' own documented event-export schema. Compatibility with third-party GDR2 readers or a separate established GDR2 specification has not been verified. Do not treat these files as interchangeable with other replay formats until that compatibility is tested.

Each event stores:
- `frame`: game frame at which the event was observed
- `button`: Geometry Dash input button identifier
- `player2`: whether the event belongs to player two
- `down`: `true` for press and `false` for release

## Integrity and categories

- Every export is labelled `unverified` by default.
- Practice and editor/test flags are stored separately.
- The mod does not currently claim to detect active Mega Hack, QOLMod, Eclipse, external speedhacks, noclip, or memory edits comprehensively.
- Merely having another mod installed is not proof that a run was modified.
- Until reliable signals are implemented and tested, the mod must not label a run `legitimate`.

The panel lists the intended run categories: From Zero, Startpos, Noclip Death, Noclip Accuracy, and Speedhack. Automatic classification for all of those categories is not yet implemented.

## Build

GitHub Actions builds Windows and Android packages with the Geode SDK and uploads each `.geode` package as a workflow artifact.
