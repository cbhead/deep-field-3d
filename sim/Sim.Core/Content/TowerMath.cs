using System;
using System.Collections.Generic;

namespace DeepField.Sim.Content;

/// <summary>The one place that turns a tower's purchased levels into the
/// numbers it fights with.
///
/// It exists because the client draws a range ring and the sim decides what
/// the tower can shoot, and those two answers have to be the same answer. A
/// ring that is drawn from `def.RangeMeters` is right on the frame the tower
/// is built and wrong from the first Range purchase onward — and a ring that
/// ignores fog is wrong for the whole wave a player most needs it.</summary>
public static class TowerMath
{
    /// <summary>Ids of the paths that grow reach. Every tower has at most one:
    /// Detector calls it "field", Filament "optics", everything else "range".
    /// Multiplying all three is a lookup, not a stack.</summary>
    private static readonly string[] RangePathIds = { "range", "field", "optics" };

    /// <summary>The multiplier a path has earned, from purchases made. Levels
    /// are counted as purchases, so an untouched path is 1.0.</summary>
    public static float PathFactor(TowerDef def, IReadOnlyList<int> pathLevels, string pathId)
    {
        for (int i = 0; i < def.UpgradePaths.Count; i++)
        {
            if (def.UpgradePaths[i].Id != pathId) continue;
            int bought = i < pathLevels.Count ? pathLevels[i] : 0;
            return MathF.Pow(def.UpgradePaths[i].PerLevelFactor, bought);
        }
        return 1f;
    }

    /// <summary>Reach before weather: the def's own range grown by whichever
    /// path this tower uses for range.</summary>
    public static float BaseRange(TowerDef def, IReadOnlyList<int> pathLevels)
    {
        float range = def.RangeMeters;
        foreach (string id in RangePathIds) range *= PathFactor(def, pathLevels, id);
        return range;
    }

    /// <summary>Reach as the tower will actually use it this wave. Weather
    /// rides on top of the swept baseline, never replaces it, so a condition
    /// can shrink a tower's reach but never decide it.</summary>
    public static float Range(TowerDef def, IReadOnlyList<int> pathLevels, MapDef map, int waveIndex)
    {
        float range = BaseRange(def, pathLevels);
        var condition = Conditions.ForWave(map, waveIndex);
        if (condition is not null && !condition.RangeExemptTowerIds.Contains(def.Id))
            range *= condition.TowerRangeFactor;
        return range;
    }

    /// <summary>What <see cref="Range"/> would return after one more purchase
    /// on <paramref name="pathIndex"/> — the number a player is deciding about
    /// while the upgrade panel is open. Returns the current range unchanged
    /// when that path does not move range, or is already at its cap.</summary>
    public static float RangeAfterUpgrade(
        TowerDef def, IReadOnlyList<int> pathLevels, int pathIndex, MapDef map, int waveIndex)
    {
        if (pathIndex < 0 || pathIndex >= def.UpgradePaths.Count)
            return Range(def, pathLevels, map, waveIndex);

        var path = def.UpgradePaths[pathIndex];
        int bought = pathIndex < pathLevels.Count ? pathLevels[pathIndex] : 0;
        if (bought + 1 >= path.MaxLevel) return Range(def, pathLevels, map, waveIndex);

        var next = new int[def.UpgradePaths.Count];
        for (int i = 0; i < next.Length; i++) next[i] = i < pathLevels.Count ? pathLevels[i] : 0;
        next[pathIndex]++;
        return Range(def, next, map, waveIndex);
    }

    /// <summary>The index of the path that grows this tower's range, or -1 if
    /// it has none (a Barricade has no reach to grow).</summary>
    public static int RangePathIndex(TowerDef def)
    {
        for (int i = 0; i < def.UpgradePaths.Count; i++)
        {
            foreach (string id in RangePathIds)
                if (def.UpgradePaths[i].Id == id) return i;
        }
        return -1;
    }
}
