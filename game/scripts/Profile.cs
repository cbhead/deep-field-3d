using Godot;
using System.Collections.Generic;

namespace DeepField.Game;

/// <summary>Local persistent player profile (user://profile.json): faction XP,
/// saved weapon blueprints, preferred name. Trusted friends — the server takes
/// the client's word for its faction level (the sim clamps to the cap).</summary>
public sealed class Profile
{
    private const string Path = "user://profile.json";

    public string Name = System.Environment.UserName;
    public string PreferredFaction = "ember";
    public Dictionary<string, int> FactionXp = new();
    // weaponId → { "slots": {slot: attachmentId}, "ammo": ammoId }
    public Dictionary<string, Dictionary<string, string>> BlueprintSlots = new();
    public Dictionary<string, string> BlueprintAmmo = new();

    public int LevelFor(string factionId) =>
        DeepField.Sim.Content.Factions.LevelForXp(FactionXp.GetValueOrDefault(factionId, 0));

    public void BankXp(string factionId, int xp)
    {
        if (xp <= 0) return;
        FactionXp[factionId] = FactionXp.GetValueOrDefault(factionId, 0) + xp;
        Save();
    }

    public void RecordAttachment(string weaponId, string slot, string attachmentId)
    {
        if (!BlueprintSlots.TryGetValue(weaponId, out var slots))
            BlueprintSlots[weaponId] = slots = new Dictionary<string, string>();
        slots[slot] = attachmentId;
        Save();
    }

    public void RecordAmmo(string weaponId, string ammoId)
    {
        BlueprintAmmo[weaponId] = ammoId;
        Save();
    }

    public static Profile Load()
    {
        var profile = new Profile();
        if (!FileAccess.FileExists(Path)) return profile;

        using var file = FileAccess.Open(Path, FileAccess.ModeFlags.Read);
        if (Json.ParseString(file.GetAsText()).AsGodotDictionary() is not { Count: > 0 } data)
            return profile;

        if (data.TryGetValue("name", out var name)) profile.Name = (string)name;
        if (data.TryGetValue("preferredFaction", out var faction)) profile.PreferredFaction = (string)faction;
        if (data.TryGetValue("factionXp", out var xp))
            foreach (var (k, v) in xp.AsGodotDictionary()) profile.FactionXp[(string)k] = (int)v;
        if (data.TryGetValue("blueprintSlots", out var bp))
        {
            foreach (var (weapon, slots) in bp.AsGodotDictionary())
            {
                var dict = new Dictionary<string, string>();
                foreach (var (slot, attachment) in slots.AsGodotDictionary())
                    dict[(string)slot] = (string)attachment;
                profile.BlueprintSlots[(string)weapon] = dict;
            }
        }
        if (data.TryGetValue("blueprintAmmo", out var ammo))
            foreach (var (k, v) in ammo.AsGodotDictionary()) profile.BlueprintAmmo[(string)k] = (string)v;
        return profile;
    }

    public void Save()
    {
        var xp = new Godot.Collections.Dictionary();
        foreach (var (k, v) in FactionXp) xp[k] = v;
        var slots = new Godot.Collections.Dictionary();
        foreach (var (weapon, dict) in BlueprintSlots)
        {
            var inner = new Godot.Collections.Dictionary();
            foreach (var (slot, attachment) in dict) inner[slot] = attachment;
            slots[weapon] = inner;
        }
        var ammo = new Godot.Collections.Dictionary();
        foreach (var (k, v) in BlueprintAmmo) ammo[k] = v;

        var data = new Godot.Collections.Dictionary
        {
            ["name"] = Name,
            ["preferredFaction"] = PreferredFaction,
            ["factionXp"] = xp,
            ["blueprintSlots"] = slots,
            ["blueprintAmmo"] = ammo,
        };

        using var file = FileAccess.Open(Path, FileAccess.ModeFlags.Write);
        file.StoreString(Json.Stringify(data));
    }
}
