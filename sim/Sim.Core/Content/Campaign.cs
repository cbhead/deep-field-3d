namespace DeepField.Sim.Content;

/// <summary>The campaign's shape: which sectors exist, in what order, and what
/// opens the next one. This lives in the sim rather than the client because it
/// is content — the harness can then gate that the chain is well-formed, that
/// every map is reachable, and that no sector is stranded behind a map that
/// does not exist.
///
/// The unlock rule itself is deliberately the weakest one that still means
/// something: clearing a sector opens the next. No star ratings, no gates on
/// lives remaining. The 2D game's front door worked the same way, and the
/// reason to keep it is that a player who barely survived sector 1 is exactly
/// the player who should be allowed into sector 2.</summary>
public static class Campaign
{
    /// <summary>Play order. The first entry is always unlocked; every other is
    /// opened by clearing the one before it.</summary>
    public static readonly IReadOnlyList<string> Sectors = new[]
    {
        "foundry",
        "switchyard",
        "spire",
    };

    /// <summary>Zero-based position in the campaign, or -1 for a map that is not
    /// part of it (the harness fixture).</summary>
    public static int IndexOf(string mapId)
    {
        for (int i = 0; i < Sectors.Count; i++)
            if (Sectors[i] == mapId) return i;
        return -1;
    }

    /// <summary>What clearing this sector opens, if anything.</summary>
    public static string? NextAfter(string mapId)
    {
        int i = IndexOf(mapId);
        return i >= 0 && i + 1 < Sectors.Count ? Sectors[i + 1] : null;
    }

    /// <summary>Whether a sector is playable given the set already cleared.
    /// Takes the cleared set rather than a count so that a profile edited by
    /// hand, or one written by an older build with a shorter chain, degrades to
    /// "locked" rather than to an exception or a silent unlock-everything.</summary>
    public static bool IsUnlocked(string mapId, IReadOnlyCollection<string> cleared)
    {
        int i = IndexOf(mapId);
        if (i < 0) return true;          // not part of the campaign; not gated by it
        if (i == 0) return true;
        return cleared.Contains(Sectors[i - 1]);
    }
}
