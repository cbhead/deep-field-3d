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
    public int Kills;
    public float DamageDealt;
    public int TowersBuilt;
    public int Revives;
    public Dictionary<ScrapType, int> Scrap = new();

    // Gunsmith state (drives the armory screen).
    public HashSet<string> OwnedWeapons = new() { "sidearm" };
    public HashSet<string> CraftedAmmo = new() { "standard" };
    public Dictionary<string, Dictionary<AttachmentSlot, string>> Attachments = new();

    /// <summary>Pack a Punch level per weapon. Uncapped, so this is an int and
    /// not a set of flags.</summary>
    public Dictionary<string, int> PackLevels = new();

    public int PackLevelFor(string weaponId) => PackLevels.GetValueOrDefault(weaponId, 0);
    public Dictionary<string, string> Ammo = new();

    public Dictionary<AttachmentSlot, string> AttachmentsFor(string weaponId) =>
        Attachments.TryGetValue(weaponId, out var slots) ? slots : new();

    public string AmmoFor(string weaponId) =>
        Ammo.TryGetValue(weaponId, out var ammoId) ? ammoId : "standard";
}

/// <summary>Covers towers, traps, and barricades — anything occupying a socket.</summary>
public sealed class PickupView
{
    public int Id;
    public ScrapType Type;
    public int Amount;
    public Vector3 Pos;
}

public sealed class StructureView
{
    public int Id;
    public string DefId = "";
    public string SocketId = "";
    public int[] PathLevels = System.Array.Empty<int>();
    public bool IsTrap;
    public int ChargesLeft;

    /// <summary>Structure health, 0..1. A fraction rather than raw hp so no
    /// reader needs the def to make sense of it, and so anything that cannot be
    /// damaged is simply always 1 instead of a special case.</summary>
    public float HpFraction = 1f;
}

/// <summary>A vehicle as the client needs it: where it is, and who is in it.
/// Seats are the sim's answer, which is why they come through the view rather
/// than being tracked locally — a client that decided for itself who was
/// driving would put two players in one seat the first time two of them
/// pressed E at once.</summary>
public sealed class VehicleView
{
    public string Id = "";
    public string DefId = "";
    public Vector3 Pos;
    public float Yaw;
    public int[] Seats = System.Array.Empty<int>();

    public int SeatOf(int playerId) => System.Array.IndexOf(Seats, playerId);
    public bool SeatFree(int index) => index >= 0 && index < Seats.Length && Seats[index] == 0;
    public int DriverId => Seats.Length > 0 ? Seats[0] : 0;
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
    /// <summary>The party is still assembling (World.Lobby); the match has
    /// not launched.</summary>
    public bool Lobby;
    public bool Endless;
    /// <summary>The hp multiplier the current wave spawned with — the sim's
    /// own number (WavePlan.HpScale), shown on the endless HUD as threat.</summary>
    public float Threat = 1f;

    public Dictionary<ScrapType, int> TeamScrap = new();
    public List<PlayerView> Players = new();
    public List<StructureView> Structures = new();
    public List<PickupView> Pickups = new();
    public List<VehicleView> Vehicles = new();

    public PlayerView? Local => Players.FirstOrDefault(p => p.Id == LocalPlayerId);

    public StructureView? AtSocket(string socketId) =>
        Structures.FirstOrDefault(s => s.SocketId == socketId);

    public bool SocketOccupied(string socketId) => AtSocket(socketId) is not null;

    public int TeamScrapOf(ScrapType type) => TeamScrap.GetValueOrDefault(type, 0);

    public int PersonalScrapOf(ScrapType type) =>
        Local?.Scrap.GetValueOrDefault(type, 0) ?? 0;

    /// <summary>Which vehicle and seat a player is in, if any.</summary>
    public (VehicleView Vehicle, int Seat)? SeatOf(int playerId)
    {
        foreach (var vehicle in Vehicles)
        {
            int seat = vehicle.SeatOf(playerId);
            if (seat >= 0) return (vehicle, seat);
        }
        return null;
    }

    public VehicleView? VehicleById(string id) => Vehicles.FirstOrDefault(v => v.Id == id);

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
        Lobby = world.Lobby;
        Endless = world.Endless;
        Threat = WavePlan.HpScale(world.Map, System.Math.Max(0, world.WaveIndex), System.Math.Max(1, world.ConnectedPlayerCount));

        TeamScrap = new Dictionary<ScrapType, int>(world.TeamScrap);

        // Rebuilt only when there is something to rebuild: this runs every
        // frame, and a match spends most of it with a clean floor.
        if (Pickups.Count > 0 || world.Pickups.Count > 0)
        {
            Pickups.Clear();
            foreach (var p in world.Pickups)
                Pickups.Add(new PickupView { Id = p.Id, Type = p.Type, Amount = p.Amount, Pos = new Vector3(p.Pos.X, p.Pos.Y, p.Pos.Z) });
        }

        Vehicles.Clear();
        foreach (var v in world.Vehicles)
            Vehicles.Add(new VehicleView
            {
                Id = v.Id, DefId = v.DefId,
                Pos = new Vector3(v.Pos.X, v.Pos.Y, v.Pos.Z),
                Yaw = v.YawDegrees, Seats = (int[])v.Seats.Clone(),
            });

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
                Kills = p.Kills, DamageDealt = p.DamageDealt,
                TowersBuilt = p.TowersBuilt, Revives = p.Revives,
                Scrap = new Dictionary<ScrapType, int>(p.Scrap),
                OwnedWeapons = new HashSet<string>(p.OwnedWeapons),
                CraftedAmmo = new HashSet<string>(p.CraftedAmmo),
                Attachments = p.Builds.ToDictionary(
                    kv => kv.Key,
                    kv => new Dictionary<AttachmentSlot, string>(kv.Value.Attachments)),
                PackLevels = p.Builds.ToDictionary(kv => kv.Key, kv => kv.Value.PackLevel),
                Ammo = p.Builds.ToDictionary(kv => kv.Key, kv => kv.Value.AmmoId),
            });
        }

        Structures.Clear();
        foreach (var t in world.Towers)
        {
            float maxHp = Towers.All[t.DefId].StructureHp;
            Structures.Add(new StructureView
            {
                Id = t.Id, DefId = t.DefId, SocketId = t.SocketId,
                PathLevels = (int[])t.PathLevels.Clone(),
                HpFraction = maxHp > 0f ? Mathf.Clamp(t.Hp / maxHp, 0f, 1f) : 1f,
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
        Lobby = meta.TryGetValue("lobby", out var lobby) && (bool)lobby;
        Endless = meta.TryGetValue("endless", out var endless) && (bool)endless;
        Threat = meta.TryGetValue("threat", out var threat) ? (float)threat : 1f;

        TeamScrap.Clear();
        foreach (var (key, value) in meta["teamScrap"].AsGodotDictionary())
            TeamScrap[System.Enum.Parse<ScrapType>((string)key)] = (int)value;

        Pickups.Clear();
        if (meta.TryGetValue("pickups", out var dropped))
            foreach (Godot.Collections.Dictionary entry in dropped.AsGodotArray())
                Pickups.Add(new PickupView
                {
                    Id = (int)entry["id"],
                    Type = System.Enum.Parse<ScrapType>((string)entry["type"]),
                    Amount = (int)entry["amount"],
                    Pos = new Vector3((float)entry["x"], (float)entry["y"], (float)entry["z"]),
                });

        Vehicles.Clear();
        if (meta.TryGetValue("vehicles", out var parked))
            foreach (Godot.Collections.Dictionary entry in parked.AsGodotArray())
            {
                var seats = new List<int>();
                foreach (var occupant in entry["seats"].AsGodotArray()) seats.Add((int)occupant);
                Vehicles.Add(new VehicleView
                {
                    Id = (string)entry["id"], DefId = (string)entry["def"],
                    Pos = new Vector3((float)entry["x"], (float)entry["y"], (float)entry["z"]),
                    Yaw = (float)entry["yaw"], Seats = seats.ToArray(),
                });
            }

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
                Kills = entry.TryGetValue("kills", out var k) ? (int)k : 0,
                DamageDealt = entry.TryGetValue("dmg", out var dmg) ? (float)dmg : 0f,
                TowersBuilt = entry.TryGetValue("built", out var built) ? (int)built : 0,
                Revives = entry.TryGetValue("rev", out var rev) ? (int)rev : 0,
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
                HpFraction = (float)entry["hp"],
            });
        }
    }
}
