# Contracts C1–C16 (+ map-authoring-3d)

Each document here is the human-readable twin of a header, asset or file format that more than one workstream depends on. The code is canonical; the document explains it and records the change rule. Change rules (PROGRAMME.md Section 3.1): **A** append-only via a `contract-append` PR, **R** RFC (`../rfcs/`), **I** integration-owned.

| # | Contract | Doc | Owner | Rule |
|---|---|---|---|---|
| C1 | Tag registry | [tags.md](tags.md) | WS-00 (native) / each WS (own ini) | native R; ini A |
| C2 | Content row schema | [content-rows.md](content-rows.md) | WS-01 | R (optional field with default = A) |
| C3 | Content JSON format | [content-json.md](content-json.md) | WS-01 | R; `$schema` bump on break |
| C4 | GAS attribute sets | [gas.md](gas.md) | WS-02 | R (new attribute = A) |
| C5 | Status / reaction semantics | [status.md](status.md) | WS-02 | R |
| C6 | Lane graph + socket asset | [lanegraph.md](lanegraph.md) | WS-09 | R; socket ids never renamed |
| C7 | Naming | [naming.md](naming.md) | WS-00 | A (new prefix) |
| C8 | Palette / tint material contract | [palette.md](palette.md) | WS-00 defines, WS-31 implements | R |
| C9 | Tower rig | [rig.md](rig.md) | WS-04 | R |
| C10 | Niagara parameter contract | [vfx.md](vfx.md) | WS-14 | R |
| C11 | Audio event contract | [audio.md](audio.md) | WS-13 | A (mappings), R (inputs) |
| C12 | UI view models | [viewmodel.md](viewmodel.md) | WS-12 | A (fields), R (rename) |
| C13 | Save / profile | [profile.md](profile.md) | WS-11 | R; migrations by Version |
| C14 | Online API | [online.md](online.md) | WS-11 | R |
| C15 | Message inventory | [messages.md](messages.md) | WS-00 | A (new), R (fields) |
| C16 | Collision & input | [collision-input.md](collision-input.md) | WS-00 | I |
| — | Map authoring in 3D (successor of `docs/MAP-AUTHORING.md` §4) | [map-authoring-3d.md](map-authoring-3d.md) | WS-09 | R |
