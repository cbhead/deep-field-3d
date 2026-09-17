# Editor slots (PROGRAMME.md Section 6.7)

The dev Mac has 8 GB of RAM. At most **two** sessions may have the Unreal editor open for real work at once; a cook or a package takes **both** slots. Take a slot by writing your session handle and workstream on a free line and pushing this one-file commit directly to `unreal/main` (same rule as a claim). Release it at session end. A slot older than 8 h with no matching session-log entry may be cleared by INT.

| Slot | Session handle | Workstream | Taken at (ISO) |
|---|---|---|---|
| 1 | — | — | — |
| 2 | — | — | — |

Code-only sessions (`editor_heavy: false`) run tests with `-nullrhi` and never open the editor UI.
