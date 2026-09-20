# waveplan-golden

Dumps golden vectors from the frozen C# sim (`sim/Sim.Core`, ADR-0005) for the Unreal port of
`WavePlan.cs`: the mulberry32 / FNV-1a RNG streams, `DetMath.PowInt`, and whole wave plans.
The output is `unreal/DeepField/Source/DFEnemies/Private/Tests/DFWavePlanGolden.inl`, which
`DF.Unit.WavePlan.*` compares the C++ port against bit for bit.

The `.inl` carries its own copy of the sim's wave tables, so the golden tests keep passing when
`unreal/content/json/waves_*.json` is rebalanced — they test the algorithm, not today's numbers.
`DF.Unit.WavePlanBaseline` is the test that reads live content.

    dotnet run --project tools/waveplan-golden -- unreal/DeepField/Source/DFEnemies/Private/Tests/DFWavePlanGolden.inl

Regenerate only if `sim/Sim.Core` changes (it should not: it is frozen) or the hash recipe does.
Plans are hashed in a canonical order (tick, def id, route id, lateral bits, hp bits) because
`List.Sort` in .NET is unstable: the order of entries that tie on (tick, def id) is an
implementation detail of the runtime, not a property of the plan.
