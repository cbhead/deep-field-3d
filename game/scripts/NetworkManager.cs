using Godot;
using System.Collections.Generic;
using System.Linq;
using DeepField.Sim;
using DeepField.Sim.Content;
using SimWorld = DeepField.Sim.World;

namespace DeepField.Game;

/// <summary>Both halves of the wire. As server: owns seat assignment, validates
/// versions, relays commands into the sim, broadcasts snapshots/events/meta.
/// As client: forwards commands, buffers snapshots for interpolation.
/// Trusted-friends doctrine: the only validations are version skew and seat
/// spoofing, both of which are accidents, not attacks.</summary>
public partial class NetworkManager : Node
{
    public const float SnapshotHz = 15f;
    public const float AvatarHz = 20f;
    public const float MetaHz = 8f;

    public bool IsServer { get; private set; }
    public int LocalPlayerId { get; private set; } = 1;

    // Server state.
    private SimWorld? _world;                       // authoritative (server only)
    private readonly Dictionary<long, int> _seats = new();   // peerId → playerId
    private double _snapshotTimer, _metaTimer;

    // Client state.
    public readonly List<(long Tick, List<Protocol.EnemySnap> Enemies)> SnapshotBuffer = new();
    public string? PendingWorldJson;                 // full-state sync at join
    public readonly List<string> PendingEvents = new();
    public Godot.Collections.Dictionary? Meta;       // money/lives/phase/scrap/players
    public string? JoinError;
    public string JoinErrorYours = "";
    public string JoinErrorHost = "";

    public System.Action<Command>? ServerEnqueue;    // host mode: loopback into GameRoot's sim

    // ---------------------------------------------------------------------
    // Setup
    // ---------------------------------------------------------------------

    public Error HostServer(int port, SimWorld world)
    {
        _world = world;
        IsServer = true;
        var peer = new ENetMultiplayerPeer();
        var err = peer.CreateServer(port, maxClients: 8);
        if (err != Error.Ok) return err;
        Multiplayer.MultiplayerPeer = peer;
        Multiplayer.PeerDisconnected += OnPeerDisconnected;
        _seats[1] = 1; // host (or dedicated observer) is seat 1
        return Error.Ok;
    }

    public Error JoinServer(string address, int port)
    {
        IsServer = false;
        var peer = new ENetMultiplayerPeer();
        var err = peer.CreateClient(address, port);
        if (err != Error.Ok) return err;
        Multiplayer.MultiplayerPeer = peer;
        return Error.Ok;
    }

    private void OnPeerDisconnected(long peerId)
    {
        if (!IsServer || !_seats.TryGetValue(peerId, out int playerId)) return;
        _seats.Remove(peerId);
        _world?.Enqueue(new Command.Leave(playerId));
    }

    // ---------------------------------------------------------------------
    // Handshake: client → Hello, server → Welcome (world json) or Rejected.
    // ---------------------------------------------------------------------

    public void SendHello(string name, string factionId, int factionLevel) =>
        RpcId(1, nameof(HelloRpc), Protocol.Version, name, factionId, factionLevel, BuildLabel());

    [Rpc(MultiplayerApi.RpcMode.AnyPeer, TransferMode = MultiplayerPeer.TransferModeEnum.Reliable)]
    private void HelloRpc(int version, string name, string factionId, int factionLevel, string build)
    {
        if (!IsServer || _world is null) return;
        long peerId = Multiplayer.GetRemoteSenderId();

        if (version != Protocol.Version)
        {
            // Name both builds: "your version is wrong" is not actionable,
            // "you are on X, the host is on Y" is.
            RpcId(peerId, nameof(RejectedRpc), "versionMismatch", build, BuildLabel());
            return;
        }

        int playerId = _seats.TryGetValue(peerId, out int existing) ? existing : NextFreeSeat();
        _seats[peerId] = playerId;
        _world.Enqueue(new Command.Join(playerId, name, factionId, factionLevel));

        // Full-state sync: the drop-in join IS the save-game path.
        RpcId(peerId, nameof(WelcomeRpc), playerId, Serialization.Serialize(_world));
    }

    private int NextFreeSeat()
    {
        for (int seat = 2; seat <= 4; seat++)
            if (!_seats.ContainsValue(seat)) return seat;
        return 5; // observer seat; Join will be rejected by faction exhaustion
    }

    [Rpc(MultiplayerApi.RpcMode.Authority, TransferMode = MultiplayerPeer.TransferModeEnum.Reliable)]
    private void WelcomeRpc(int playerId, string worldJson)
    {
        LocalPlayerId = playerId;
        PendingWorldJson = worldJson;
    }

    [Rpc(MultiplayerApi.RpcMode.Authority, TransferMode = MultiplayerPeer.TransferModeEnum.Reliable)]
    private void RejectedRpc(string reason, string yourBuild, string hostBuild)
    {
        JoinError = reason;
        JoinErrorYours = yourBuild;
        JoinErrorHost = hostBuild;
    }

    /// <summary>What this client is running, for the handshake and for any
    /// refusal that has to explain itself.</summary>
    public static string BuildLabel() =>
        Protocol.BuildLabel((string)ProjectSettings.GetSetting("application/config/version", "dev"));

    // ---------------------------------------------------------------------
    // Client → server commands
    // ---------------------------------------------------------------------

    public void SendCommand(Command command)
    {
        if (IsServer)
        {
            ServerEnqueue?.Invoke(command);
            return;
        }
        RpcId(1, nameof(CommandRpc), Protocol.CommandToWire(command));
    }

    [Rpc(MultiplayerApi.RpcMode.AnyPeer, TransferMode = MultiplayerPeer.TransferModeEnum.Reliable)]
    private void CommandRpc(string wire)
    {
        if (!IsServer || _world is null) return;
        long peerId = Multiplayer.GetRemoteSenderId();
        if (!_seats.TryGetValue(peerId, out int seat)) return;

        var command = Protocol.CommandFromWire(wire);
        if (command is null) return;
        if (!Protocol.CommandClaimsSeat(command, seat)) return;   // typo/race guard
        _world.Enqueue(command);
    }

    // Avatar transforms ride an unreliable channel at their own cadence.
    public void SendAvatar(Vector3 pos, float yaw)
    {
        if (IsServer) return;   // host's avatar syncs via its own PlayerSync loopback
        RpcId(1, nameof(AvatarRpc), pos, yaw);
    }

    [Rpc(MultiplayerApi.RpcMode.AnyPeer, TransferMode = MultiplayerPeer.TransferModeEnum.Unreliable)]
    private void AvatarRpc(Vector3 pos, float yaw)
    {
        if (!IsServer || _world is null) return;
        long peerId = Multiplayer.GetRemoteSenderId();
        if (!_seats.TryGetValue(peerId, out int seat)) return;
        _world.Enqueue(new Command.PlayerSync(seat, new Vec3(pos.X, pos.Y, pos.Z)));
    }

    // ---------------------------------------------------------------------
    // Server broadcast loops (driven by GameRoot after each sim step batch)
    // ---------------------------------------------------------------------

    public void ServerBroadcast(double delta, IReadOnlyList<string> eventLines)
    {
        if (!IsServer || _world is null || Multiplayer.MultiplayerPeer is null) return;

        if (eventLines.Count > 0)
            Rpc(nameof(EventsRpc), eventLines.ToArray());

        _snapshotTimer += delta;
        if (_snapshotTimer >= 1.0 / SnapshotHz)
        {
            _snapshotTimer = 0;
            Rpc(nameof(SnapshotRpc), Protocol.PackEnemies(_world));
        }

        _metaTimer += delta;
        if (_metaTimer >= 1.0 / MetaHz)
        {
            _metaTimer = 0;
            Rpc(nameof(MetaRpc), BuildMeta());
        }
    }

    private Godot.Collections.Dictionary BuildMeta()
    {
        var players = new Godot.Collections.Array();
        foreach (var p in _world!.Players.Values)
        {
            // Gunsmith state rides along so the armory screen works on clients
            // (7 slots × 4 players is nothing next to the snapshot channel).
            var builds = new Godot.Collections.Dictionary();
            foreach (var (weaponId, build) in p.Builds)
            {
                var slots = new Godot.Collections.Dictionary();
                foreach (var (slot, attachmentId) in build.Attachments)
                    slots[slot.ToString()] = attachmentId;
                builds[weaponId] = new Godot.Collections.Dictionary
                {
                    ["slots"] = slots,
                    ["ammo"] = build.AmmoId,
                };
            }

            var owned = new Godot.Collections.Array();
            foreach (var weaponId in p.OwnedWeapons) owned.Add(weaponId);

            var craftedAmmo = new Godot.Collections.Array();
            foreach (var ammoId in p.CraftedAmmo) craftedAmmo.Add(ammoId);

            players.Add(new Godot.Collections.Dictionary
            {
                ["builds"] = builds,
                ["owned"] = owned,
                ["craftedAmmo"] = craftedAmmo,
                ["id"] = p.Id, ["name"] = p.Name, ["faction"] = p.FactionId,
                ["x"] = p.Pos.X, ["y"] = p.Pos.Y, ["z"] = p.Pos.Z,
                ["hp"] = p.Hp, ["downed"] = p.Downed, ["connected"] = p.Connected,
                ["weapon"] = p.WeaponId,
                ["abilityCd"] = p.AbilityCooldown,
                ["scrap"] = ScrapDict(p.Scrap),
                ["xp"] = p.MatchXp,
                // Contribution, so a client's end-of-match screen is the same
                // screen the host sees rather than a blank column.
                ["kills"] = p.Kills,
                ["dmg"] = p.DamageDealt,
                ["built"] = p.TowersBuilt,
                ["rev"] = p.Revives,
                ["factionLevel"] = p.FactionLevel,
            });
        }

        // Scrap on the floor rides the meta channel with the structures: it is
        // continuous state a client draws, few in number, and a dropped packet
        // costs nothing because the next one carries the whole picture.
        var pickups = new Godot.Collections.Array();
        foreach (var pickup in _world.Pickups)
        {
            pickups.Add(new Godot.Collections.Dictionary
            {
                ["id"] = pickup.Id, ["type"] = pickup.Type.ToString(), ["amount"] = pickup.Amount,
                ["x"] = pickup.Pos.X, ["y"] = pickup.Pos.Y, ["z"] = pickup.Pos.Z,
            });
        }

        // Structures ride the meta channel so clients can drive the build wheel
        // and upgrade panel (they need per-path levels, which events don't carry).
        var structures = new Godot.Collections.Array();
        foreach (var tower in _world.Towers)
        {
            var levels = new Godot.Collections.Array();
            foreach (int level in tower.PathLevels) levels.Add(level);
            float maxHp = Towers.All[tower.DefId].StructureHp;
            structures.Add(new Godot.Collections.Dictionary
            {
                ["id"] = tower.Id, ["def"] = tower.DefId, ["socket"] = tower.SocketId,
                ["levels"] = levels, ["trap"] = false, ["charges"] = 0,
                // Health rides here rather than being reconstructed from damage
                // events: a Ram takes a tower down over seconds, which is
                // continuous state, and the pulled channel is where continuous
                // state goes. A client that missed one unreliable packet still
                // shows the right bar on the next one.
                ["hp"] = maxHp > 0f ? tower.Hp / maxHp : 1f,
            });
        }
        foreach (var trap in _world.Traps)
        {
            structures.Add(new Godot.Collections.Dictionary
            {
                ["id"] = trap.Id, ["def"] = trap.DefId, ["socket"] = trap.SocketId,
                ["levels"] = new Godot.Collections.Array(), ["trap"] = true,
                ["charges"] = trap.ChargesLeft, ["hp"] = 1f,
            });
        }

        return new Godot.Collections.Dictionary
        {
            ["money"] = _world.Money,
            ["lives"] = _world.Lives,
            ["phase"] = (int)_world.Phase,
            ["phaseTimer"] = _world.PhaseTimer,
            ["wave"] = _world.WaveIndex,
            ["totalWaves"] = _world.Map.TotalWaves,
            ["lobby"] = _world.Lobby,
            ["endless"] = _world.Endless,
            ["threat"] = WavePlan.HpScale(_world.Map, System.Math.Max(0, _world.WaveIndex), System.Math.Max(1, _world.ConnectedPlayerCount)),
            ["enemies"] = _world.Enemies.Count + _world.PendingSpawns.Count,
            ["teamScrap"] = ScrapDict(_world.TeamScrap),
            ["players"] = players,
            ["structures"] = structures,
            ["pickups"] = pickups,
        };
    }

    private static Godot.Collections.Dictionary ScrapDict(Dictionary<ScrapType, int> scrap)
    {
        var dict = new Godot.Collections.Dictionary();
        foreach (var (type, amount) in scrap) dict[type.ToString()] = amount;
        return dict;
    }

    [Rpc(MultiplayerApi.RpcMode.Authority, TransferMode = MultiplayerPeer.TransferModeEnum.Unreliable)]
    private void SnapshotRpc(byte[] packed)
    {
        var snapshot = Protocol.UnpackEnemies(packed);
        // Out-of-order unreliable delivery: drop stale.
        if (SnapshotBuffer.Count > 0 && snapshot.Tick <= SnapshotBuffer[^1].Tick) return;
        SnapshotBuffer.Add(snapshot);
        while (SnapshotBuffer.Count > 4) SnapshotBuffer.RemoveAt(0);
    }

    [Rpc(MultiplayerApi.RpcMode.Authority, TransferMode = MultiplayerPeer.TransferModeEnum.Reliable)]
    private void EventsRpc(string[] lines) => PendingEvents.AddRange(lines);

    [Rpc(MultiplayerApi.RpcMode.Authority, TransferMode = MultiplayerPeer.TransferModeEnum.Unreliable)]
    private void MetaRpc(Godot.Collections.Dictionary meta) => Meta = meta;
}
