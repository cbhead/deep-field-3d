using Godot;
using System;
using System.Collections.Generic;
using System.Linq;

namespace DeepField.Game;

/// <summary>The drop-in seam for Claude Design's 3D art.
///
/// Every model in the game is requested by the exact name it carries in
/// docs/DESIGN-BRIEF.md — "enemy_drifter", "tower_lance_chassis",
/// "proj_nova_shell". If the file exists under game/assets/ it renders; if it
/// doesn't, <see cref="Placeholders"/> builds the graybox stand-in that has
/// been carrying the game so far. Nothing in GameRoot knows the difference.
///
/// The consequence that matters: integrating art is copying .glb files into
/// the right folder. No code change, no scene surgery, no rebuild ordering.
/// Same contract UiTheme.Icon already uses for 2D icons.
///
/// A .tscn of the same name wins over the .glb, so a model that needs an
/// import tweak (collision shape, animation player, material override) can be
/// wrapped in a scene without touching this file.</summary>
public static class AssetLibrary
{
    /// <summary>Name prefix → folder under res://assets/. The prefixes are the
    /// ones the design brief assigns; a new prefix needs a row here.</summary>
    private static readonly (string Prefix, string Folder)[] Routes =
    {
        ("enemy_", "enemies"),
        ("boss_", "enemies"),
        ("tower_", "structures"),
        ("trap_", "structures"),
        ("socket_", "structures"),
        ("weapon_", "weapons"),
        ("attach_", "weapons"),
        ("ammo_", "weapons"),
        ("hands_", "weapons"),
        ("hero_", "heroes"),
        ("proj_", "vfx"),
        ("vfx_", "vfx"),
        ("pickup_", "economy"),
        ("foundry_", "maps"),
        ("switchyard_", "maps"),
        ("shared_", "maps"),
        ("prop_", "maps"),
        ("ui_", "ui"),
    };

    private static readonly Dictionary<string, PackedScene?> Cache = new();

    /// <summary>See UiTheme.ReleaseCaches: static Godot resources must be let
    /// go before the SceneTree tears down, or mono aborts at shutdown.</summary>
    public static void ReleaseCaches() => Cache.Clear();

    private static readonly SortedSet<string> RequestedNames = new();
    private static readonly SortedSet<string> ResolvedNames = new();

    /// <summary>Every asset name the running game has actually asked for. The
    /// other half of the coverage question: tools/asset-report.sh says what
    /// design has delivered, this says what code is wired to consume.</summary>
    public static IReadOnlyCollection<string> Requested => RequestedNames;
    public static IReadOnlyCollection<string> Resolved => ResolvedNames;
    public static IEnumerable<string> Missing => RequestedNames.Except(ResolvedNames);

    /// <summary>Design names every file in lower case; sim content ids are
    /// camelCase ("emberPistol", "longBarrel", "ignitionWave"). Without this
    /// those three silently resolved to placeholder chips — and because the
    /// placeholder is designed to look deliberate, nobody noticed.</summary>
    private static string Normalise(string asset) => asset.ToLowerInvariant();

    public static string FolderFor(string asset)
    {
        foreach (var (prefix, folder) in Routes)
            if (asset.StartsWith(prefix, StringComparison.Ordinal)) return folder;
        return "misc";
    }

    /// <summary>Where design drops the file. Printed in the report so nobody
    /// has to guess the folder from the prefix table.</summary>
    public static string PathFor(string asset)
    {
        asset = Normalise(asset);
        return $"res://assets/{FolderFor(asset)}/{asset}.glb";
    }

    /// <summary>The model if design has shipped it, else null. Callers that
    /// want a graybox fallback use <see cref="Instantiate"/>; callers where
    /// absence simply means "skip this flourish" use this directly.</summary>
    public static Node3D? TryInstantiate(string asset)
    {
        asset = Normalise(asset);
        RequestedNames.Add(asset);

        if (!Cache.TryGetValue(asset, out var scene))
        {
            string folder = FolderFor(asset);
            string tscn = $"res://assets/{folder}/{asset}.tscn";
            string glb = $"res://assets/{folder}/{asset}.glb";
            string? found = ResourceLoader.Exists(tscn) ? tscn
                : ResourceLoader.Exists(glb) ? glb
                : null;
            scene = found is null ? null : GD.Load<PackedScene>(found);
            Cache[asset] = scene;
        }

        if (scene is null) return null;
        ResolvedNames.Add(asset);
        return scene.Instantiate() as Node3D;
    }

    /// <summary>Design's model, or the graybox that stands in for it.</summary>
    public static Node3D Instantiate(string asset, Func<Node3D> placeholder)
        => TryInstantiate(asset) ?? placeholder();

    public static bool Has(string asset)
    {
        asset = Normalise(asset);
        string folder = FolderFor(asset);
        return ResourceLoader.Exists($"res://assets/{folder}/{asset}.tscn")
            || ResourceLoader.Exists($"res://assets/{folder}/{asset}.glb");
    }

    /// <summary>The model name for a placeable, shared by the world view and
    /// the build ghost so the thing you preview is the thing you get.</summary>
    public static string StructureAsset(string defId) => defId switch
    {
        "spike" => "trap_spike_armed",
        "tar" => "trap_tar_full",
        "launcher" => "trap_launcher_charged",
        "barricade" => "tower_barricade",
        _ => $"tower_{defId}_chassis",
    };

    /// <summary>One line for the log at shutdown — the running answer to "how
    /// much of the art is actually in the build?"</summary>
    public static string Summary()
    {
        int asked = RequestedNames.Count, got = ResolvedNames.Count;
        return asked == 0
            ? "[assets] nothing requested"
            : $"[assets] {got}/{asked} resolved, {asked - got} on placeholders";
    }
}
