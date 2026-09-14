using Godot;
using System.Collections.Generic;

namespace DeepField.Game;

/// <summary>Design's receipt for a drop — <c>game/assets/structures/manifest.json</c>,
/// one row per exported file — read for the two facts a map builder cannot
/// get from the mesh: whether a piece was drawn to be instanced, and how wide
/// a linear module is. TowerRig reads the same file for rig limits; this is
/// the same idea for the map kit, kept separate so neither has to know the
/// other's rows.</summary>
public static class AssetManifest
{
    private const string Path = "res://assets/structures/manifest.json";
    private static Dictionary<string, Godot.Collections.Dictionary>? _rows;

    private static Dictionary<string, Godot.Collections.Dictionary> Rows => _rows ??= Load();

    private static Dictionary<string, Godot.Collections.Dictionary> Load()
    {
        var rows = new Dictionary<string, Godot.Collections.Dictionary>();
        if (!FileAccess.FileExists(Path)) return rows;
        using var file = FileAccess.Open(Path, FileAccess.ModeFlags.Read);
        var json = new Json();
        if (file is null || json.Parse(file.GetAsText()) != Error.Ok) return rows;
        var root = json.Data.AsGodotDictionary();
        if (!root.TryGetValue("files", out var files)) return rows;
        foreach (var entry in files.AsGodotArray())
        {
            var row = entry.AsGodotDictionary();
            if (!row.TryGetValue("file", out var name)) continue;
            string key = name.AsString();
            int slash = key.LastIndexOf('/');
            if (slash >= 0) key = key[(slash + 1)..];
            if (key.EndsWith(".glb")) key = key[..^4];
            rows[key.ToLowerInvariant()] = row;
        }
        return rows;
    }

    /// <summary>True when design marked the file for the multimesh path — a
    /// module drawn once per part and multiplied by every placement, with
    /// nothing in it to thin per copy.</summary>
    public static bool Instanced(string asset)
        => Rows.TryGetValue(asset.ToLowerInvariant(), out var row)
           && row.TryGetValue("instanced", out var flag) && flag.AsBool();

    /// <summary>A linear module's stated width in metres, or the fallback when
    /// the row carries none.</summary>
    public static float Width(string asset, float fallback)
        => Rows.TryGetValue(asset.ToLowerInvariant(), out var row)
           && row.TryGetValue("width", out var width) ? (float)width.AsDouble() : fallback;

    /// <summary>See UiTheme.ReleaseCaches: static Godot resources are let go
    /// before the SceneTree tears down, and a re-entered match re-reads a fresh drop.</summary>
    public static void ReleaseCaches() => _rows = null;
}
