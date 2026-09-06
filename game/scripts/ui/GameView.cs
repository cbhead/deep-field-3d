using Godot;
using System.Collections.Generic;
using System.Linq;
using DeepField.Sim;
using DeepField.Sim.Content;
using SimWorld = DeepField.Sim.World;

namespace DeepField.Game.Ui;

/// <summary>Mode-agnostic read model. GameRoot rebuilds it each frame from
/// either the authoritative sim (solo/host) or the meta channel (client), and
/// every UI node reads only this — so no UI code branches on RunMode.</summary>
public sealed class PlayerView
{
    public int Id;
    public string Name = "";
    public string FactionId = "";
    public int FactionLevel = 1;
    public Vector3 Pos;
    public float Hp;
    public bool Downed;
    public bool Connected = true;
    public string WeaponId = "sidearm";
    public float AbilityCooldown;
    public int MatchXp;
    public Dictionary<ScrapType, int> Scrap = new();

    // Gunsmith state (drives the armory screen).
    public HashSet<string> OwnedWeapons = new() { "sidearm" };
    public HashSet<string> CraftedAmmo = new() { "standard" };
    public Dictionary<string, Dictionary<AttachmentSlot, string>> Attachments = new();
    public Dictionary<string, string> Ammo = new();

    public Dictionary<AttachmentSlot, string> AttachmentsFor(string weaponId) =>
        Attachments.TryGetValue(weaponId, out var slots) ? slots : new();

    public string AmmoFor(string weaponId) =>
        Ammo.TryGetValue(weaponId, out var ammoId) ? ammoId : "standard";
}

/// <summary>Covers towers, traps, and barricades — anything occupying a socket.</summary>
public sealed class StructureView
{
    public int Id;
    public string DefId = "";
    public string SocketId = "";
    public int[] PathLevels = System.Array.Empty<int>();
    public bool IsTrap;
    public int ChargesLeft;
}

public sealed class GameView
{
    public bool Valid;
    public int LocalPlayerId = 1;

    public int Money, Lives;
    public int Wave = -1, TotalWaves;
    public MatchPhase Phase = MatchPhase.Intermission;
    public float PhaseTimer;
    public int EnemiesRemaining;

    public Dictionary<ScrapType, int> TeamScrap = new();
    public List<PlayerView> Players = new();
    public List<StructureView> Structures = new();

    public PlayerView? Local => Players.FirstOrDefault(p => p.Id == LocalPlayerId);

    public StructureView? AtSocket(string socketId) =>
        Structures.FirstOrDefault(s => s.SocketId == socketId);

    public bool SocketOccupied(string socketId) => AtSocket(socketId) is not null;

    public int TeamScrapOf(ScrapType type) => TeamScrap.GetValueOrDefault(type, 0);

    public int PersonalScrapOf(ScrapType type) =>
        Local?.Scrap.GetValueOrDefault(type, 0) ?? 0;

    // ---- Builders ---------------------------------------------------------

    /// <summary>Authoritative modes read the sim directly.</summary>
    public void FromWorld(SimWorld world, int localPlayerId)
    {
        Valid = true;
        LocalPlayerId = localPlayerId;
        Money = world.Money;
        Lives = world.Lives;
        Wave = world.WaveIndex;
        TotalWaves = world.Map.TotalWaves;
        Phase = world.Phase;
        PhaseTimer = world.PhaseTimer;
        EnemiesRemaining = world.Enemies.Count + world.PendingSpawns.Count;

        TeamScrap = new Dictionary<ScrapType, int>(world.TeamScrap);

        Players.Clear();
        foreach (var p in world.Players.Values)
        {
            Players.Add(new PlayerView
            {
                Id = p.Id, Name = p.Name, FactionId = p.FactionId,
                FactionLevel = p.FactionLevel,
                Pos = new Vector3(p.Pos.X, p.Pos.Y, p.Pos.Z),
                Hp = p.Hp, Downed = p.Downed, Connected = p.Connected,
                WeaponId = p.WeaponId, AbilityCooldown = p.AbilityCooldown,
                MatchXp = p.MatchXp,
                Scrap = new Dictionary<ScrapType, int>(p.Scrap),
                OwnedWeapons = new HashSet<string>(p.OwnedWeapons),
                CraftedAmmo = new HashSet<string>(p.CraftedAmmo),
                Attachments = p.Builds.ToDictionary(
                    kv => kv.Key,
                    kv => new Dictionary<AttachmentSlot, string>(kv.Value.Attachments)),
                Ammo = p.Builds.ToDictionary(kv => kv.Key, kv => kv.Value.AmmoId),
            });
        }

        Structures.Clear();
        foreach (var t in world.Towers)
        {
            Structures.Add(new StructureView
            {
                Id = t.Id, DefId = t.DefId, SocketId = t.SocketId,
                PathLevels = (int[])t.PathLevels.Clone(),
            });
        }
        foreach (var t in world.Traps)
        {
            Structures.Add(new StructureView
            {
                Id = t.Id, DefId = t.DefId, SocketId = t.SocketId,
                IsTrap = true, ChargesLeft = t.ChargesLeft,
            });
        }
    }

    /// <summary>Clients read the meta channel (NetworkManager.BuildMeta).</summary>
    public void FromMeta(Godot.Collections.Dictionary meta, int localPlayerId)
    {
        Valid = true;
        LocalPlayerId = localPlayerId;
        Money = (int)meta["money"];
        Lives = (int)meta["lives"];
        Wave = (int)meta["wave"];
        TotalWaves = (int)meta["totalWaves"];
        Phase = (MatchPhase)(int)meta["phase"];
        PhaseTimer = (float)meta["phaseTimer"];
        EnemiesRemaining = meta.TryGetValue("enemies", out var remaining) ? (int)remaining : 0;

        TeamScrap.Clear();
        foreach (var (key, value) in meta["teamScrap"].AsGodotDictionary())
            TeamScrap[System.Enum.Parse<ScrapType>((string)key)] = (int)value;

        Players.Clear();
        foreach (Godot.Collections.Dictionary entry in meta["players"].AsGodotArray())
        {
            var player = new PlayerView
            {
                Id = (int)entry["id"], Name = (string)entry["name"],
                FactionId = (string)entry["faction"],
                FactionLevel = entry.TryGetValue("factionLevel", out var lvl) ? (int)lvl : 1,
                Pos = new Vector3((float)entry["x"], (float)entry["y"], (float)entry["z"]),
                Hp = (float)entry["hp"], Downed = (bool)entry["downed"],
                Connected = (bool)entry["connected"], WeaponId = (string)entry["weapon"],
                AbilityCooldown = (float)entry["abilityCd"],
                MatchXp = entry.TryGetValue("xp", out var xp) ? (int)xp : 0,
            };
            foreach (var (key, value) in entry["scrap"].AsGodotDictionary())
                player.Scrap[System.Enum.Parse<ScrapType>((string)key)] = (int)value;

            if (entry.TryGetValue("owned", out var owned))
            {
                player.OwnedWeapons.Clear();
                foreach (var weaponId in owned.AsGodotArray()) player.OwnedWeapons.Add((string)weaponId);
            }
            if (entry.TryGetValue("craftedAmmo", out var craftedAmmo))
            {
                player.CraftedAmmo.Clear();
                foreach (var ammoId in craftedAmmo.AsGodotArray()) player.CraftedAmmo.Add((string)ammoId);
            }
            if (entry.TryGetValue("builds", out var builds))
            {
                foreach (var (weaponId, build) in builds.AsGodotDictionary())
                {
                    var buildDict = build.AsGodotDictionary();
                    var slots = new Dictionary<AttachmentSlot, string>();
                    foreach (var (slot, attachmentId) in buildDict["slots"].AsGodotDictionary())
                        slots[System.Enum.Parse<AttachmentSlot>((string)slot)] = (string)attachmentId;
                    player.Attachments[(string)weaponId] = slots;
                    player.Ammo[(string)weaponId] = (string)buildDict["ammo"];
                }
            }

            Players.Add(player);
        }

        Structures.Clear();
        if (!meta.TryGetValue("structures", out var structures)) return;
        foreach (Godot.Collections.Dictionary entry in structures.AsGodotArray())
        {
            var levels = new List<int>();
            foreach (var level in entry["levels"].AsGodotArray()) levels.Add((int)level);
            Structures.Add(new StructureView
            {
                Id = (int)entry["id"], DefId = (string)entry["def"],
                SocketId = (string)entry["socket"], PathLevels = levels.ToArray(),
                IsTrap = (bool)entry["trap"], ChargesLeft = (int)entry["charges"],
            });
        }
    }
}
