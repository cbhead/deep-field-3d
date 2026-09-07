using System.Buffers.Binary;
using System.Text;
using DeepField.Sim.Content;

namespace DeepField.Sim;

/// <summary>The wire contract, shared by client and server — one file both ends
/// import, the protocol.ts rule. Version skew refuses loudly at handshake.
/// Lives in Sim.Core so packing round-trips are fast-lane testable.</summary>
public static class Protocol
{
    /// <summary>Bumped to 2 when melee joined the command vocabulary. An older
    /// client cannot send a swing and a newer one would send four verbs the old
    /// server parses as null — silently dropping every melee action rather than
    /// refusing the connection, which is exactly the failure this number
    /// exists to make loud.</summary>
    public const int Version = 2;

    /// <summary>Human-readable build identity, sent alongside the protocol
    /// number. The number decides compatibility — determinism requires an exact
    /// match — but a refusal that can name both builds is one a player can
    /// actually act on.</summary>
    public static string BuildLabel(string appVersion) => $"{appVersion} · p{Version}";
    public const int DefaultPort = 8787;

    // ------------------------------------------------------------------
    // Enemy snapshot channel: unreliable, 15 Hz, full-world (no deltas —
    // 4 players on a tailnet; tighten when it's actually a problem).
    // Layout: tick i64 | count u16 | per enemy:
    //   id i32 | defIndex u8 | pos 3×f32 | yaw u8 | hpFrac u8 | statusBits u8
    // ------------------------------------------------------------------

    public static readonly IReadOnlyList<string> EnemyDefOrder =
        Enemies.All.Keys.OrderBy(k => k, StringComparer.Ordinal).ToArray();

    private static readonly Dictionary<string, byte> DefIndex =
        EnemyDefOrder.Select((id, i) => (id, i))
            .ToDictionary(x => x.id, x => (byte)x.i);

    public const int BytesPerEnemy = 4 + 1 + 12 + 1 + 1 + 1;

    public sealed record EnemySnap(
        int Id, string DefId, Vec3 Pos, float Yaw, float HpFraction, byte StatusBits);

    public static byte[] PackEnemies(World w)
    {
        var live = w.Enemies.Where(e => !e.Dead).ToList();
        var buffer = new byte[8 + 2 + live.Count * BytesPerEnemy];
        var span = buffer.AsSpan();

        BinaryPrimitives.WriteInt64LittleEndian(span, w.Tick);
        BinaryPrimitives.WriteUInt16LittleEndian(span[8..], (ushort)live.Count);

        int offset = 10;
        foreach (var e in live)
        {
            BinaryPrimitives.WriteInt32LittleEndian(span[offset..], e.Id);
            span[offset + 4] = DefIndex[e.DefId];
            BinaryPrimitives.WriteSingleLittleEndian(span[(offset + 5)..], e.Pos.X);
            BinaryPrimitives.WriteSingleLittleEndian(span[(offset + 9)..], e.Pos.Y);
            BinaryPrimitives.WriteSingleLittleEndian(span[(offset + 13)..], e.Pos.Z);
            span[offset + 17] = PackYaw(e.Facing);
            span[offset + 18] = (byte)System.Math.Clamp((int)(e.Hp / e.MaxHp * 255f), 0, 255);
            span[offset + 19] = PackStatusBits(e);
            offset += BytesPerEnemy;
        }
        return buffer;
    }

    public static (long Tick, List<EnemySnap> Enemies) UnpackEnemies(ReadOnlySpan<byte> data)
    {
        long tick = BinaryPrimitives.ReadInt64LittleEndian(data);
        int count = BinaryPrimitives.ReadUInt16LittleEndian(data[8..]);

        var snaps = new List<EnemySnap>(count);
        int offset = 10;
        for (int i = 0; i < count; i++)
        {
            snaps.Add(new EnemySnap(
                BinaryPrimitives.ReadInt32LittleEndian(data[offset..]),
                EnemyDefOrder[data[offset + 4]],
                new Vec3(
                    BinaryPrimitives.ReadSingleLittleEndian(data[(offset + 5)..]),
                    BinaryPrimitives.ReadSingleLittleEndian(data[(offset + 9)..]),
                    BinaryPrimitives.ReadSingleLittleEndian(data[(offset + 13)..])),
                UnpackYaw(data[offset + 17]),
                data[offset + 18] / 255f,
                data[offset + 19]));
            offset += BytesPerEnemy;
        }
        return (tick, snaps);
    }

    private static byte PackYaw(Vec3 facing)
    {
        float yaw = MathF.Atan2(facing.X, facing.Z);            // [-π, π]
        return (byte)(int)((yaw + MathF.PI) / (2f * MathF.PI) * 255f);
    }

    private static float UnpackYaw(byte packed) =>
        packed / 255f * 2f * MathF.PI - MathF.PI;

    public static byte PackStatusBits(Enemy e)
    {
        byte bits = 0;
        for (int c = 0; c < e.Statuses.Length && c < 8; c++)
            if (e.Statuses[c].Active) bits |= (byte)(1 << c);
        return bits;
    }

    // ------------------------------------------------------------------
    // Command wire: reliable client → server. Fields never contain '|'
    // (ids are lowercase words, names are sanitized at join).
    // ------------------------------------------------------------------

    public static string CommandToWire(Command command) => command switch
    {
        Command.Join c => $"join|{c.PlayerId}|{Sanitize(c.Name)}|{c.FactionId}|{c.FactionLevel}",
        Command.Leave c => $"leave|{c.PlayerId}",
        Command.PlayerSync c => $"sync|{c.PlayerId}|{F(c.Pos.X)}|{F(c.Pos.Y)}|{F(c.Pos.Z)}",
        Command.PlaceTower c => $"place|{c.PlayerId}|{c.TowerId}|{c.SocketId}",
        Command.SellTower c => $"sell|{c.PlayerId}|{c.TowerId}",
        Command.UpgradeTower c => $"upgrade|{c.PlayerId}|{c.TowerId}|{c.PathIndex}",
        Command.StartWave c => $"startWave|{c.PlayerId}",
        Command.PlayerHit c => $"hit|{c.PlayerId}|{c.EnemyId}|{c.WeaponId}",
        Command.BuyWeapon c => $"buy|{c.PlayerId}|{c.WeaponId}",
        Command.SelectWeapon c => $"select|{c.PlayerId}|{c.WeaponId}",
        Command.UseAbility c => $"ability|{c.PlayerId}|{F(c.TargetPos.X)}|{F(c.TargetPos.Y)}|{F(c.TargetPos.Z)}",
        Command.Revive c => $"revive|{c.PlayerId}|{c.TargetPlayerId}",
        Command.CraftAttachment c => $"craft|{c.PlayerId}|{c.WeaponId}|{c.AttachmentId}",
        Command.SelectAmmo c => $"ammo|{c.PlayerId}|{c.WeaponId}|{c.AmmoId}",
        Command.Reload c => $"reload|{c.PlayerId}",
        Command.PlayerMelee c => $"swing|{c.PlayerId}|{F(c.AimPoint.X)}|{F(c.AimPoint.Y)}|{F(c.AimPoint.Z)}",
        Command.BuyMelee c => $"buyMelee|{c.PlayerId}|{c.MeleeId}",
        Command.CraftMeleeAttachment c => $"craftMelee|{c.PlayerId}|{c.MeleeId}|{c.AttachmentId}",
        Command.UpgradeMelee c => $"upMelee|{c.PlayerId}|{c.MeleeId}",
        _ => throw new InvalidOperationException($"unwired command {command.GetType().Name}"),
    };

    public static Command? CommandFromWire(string wire)
    {
        var p = wire.Split('|');
        try
        {
            return p[0] switch
            {
                "join" => new Command.Join(int.Parse(p[1]), p[2], p[3],
                    p.Length > 4 ? int.Parse(p[4]) : 1),
                "leave" => new Command.Leave(int.Parse(p[1])),
                "sync" => new Command.PlayerSync(int.Parse(p[1]), new Vec3(Pf(p[2]), Pf(p[3]), Pf(p[4]))),
                "place" => new Command.PlaceTower(int.Parse(p[1]), p[2], p[3]),
                "sell" => new Command.SellTower(int.Parse(p[1]), int.Parse(p[2])),
                "upgrade" => new Command.UpgradeTower(int.Parse(p[1]), int.Parse(p[2]), int.Parse(p[3])),
                "startWave" => new Command.StartWave(int.Parse(p[1])),
                "hit" => new Command.PlayerHit(int.Parse(p[1]), int.Parse(p[2]), p[3]),
                "buy" => new Command.BuyWeapon(int.Parse(p[1]), p[2]),
                "select" => new Command.SelectWeapon(int.Parse(p[1]), p[2]),
                "ability" => new Command.UseAbility(int.Parse(p[1]), new Vec3(Pf(p[2]), Pf(p[3]), Pf(p[4]))),
                "revive" => new Command.Revive(int.Parse(p[1]), int.Parse(p[2])),
                "craft" => new Command.CraftAttachment(int.Parse(p[1]), p[2], p[3]),
                "ammo" => new Command.SelectAmmo(int.Parse(p[1]), p[2], p[3]),
                "reload" => new Command.Reload(int.Parse(p[1])),
                "swing" => new Command.PlayerMelee(int.Parse(p[1]), new Vec3(Pf(p[2]), Pf(p[3]), Pf(p[4]))),
                "buyMelee" => new Command.BuyMelee(int.Parse(p[1]), p[2]),
                "craftMelee" => new Command.CraftMeleeAttachment(int.Parse(p[1]), p[2], p[3]),
                "upMelee" => new Command.UpgradeMelee(int.Parse(p[1]), p[2]),
                _ => null,
            };
        }
        catch (FormatException) { return null; }
        catch (IndexOutOfRangeException) { return null; }
    }

    /// <summary>Player ids are seat numbers assigned by the server; a client
    /// claiming another seat's id is ignored (trusted friends, but typos and
    /// reconnect races are real).</summary>
    public static bool CommandClaimsSeat(Command command, int seatPlayerId) => command switch
    {
        Command.Join c => c.PlayerId == seatPlayerId,
        Command.Leave c => c.PlayerId == seatPlayerId,
        Command.PlayerSync c => c.PlayerId == seatPlayerId,
        Command.PlaceTower c => c.PlayerId == seatPlayerId,
        Command.SellTower c => c.PlayerId == seatPlayerId,
        Command.UpgradeTower c => c.PlayerId == seatPlayerId,
        Command.StartWave c => c.PlayerId == seatPlayerId,
        Command.PlayerHit c => c.PlayerId == seatPlayerId,
        Command.BuyWeapon c => c.PlayerId == seatPlayerId,
        Command.SelectWeapon c => c.PlayerId == seatPlayerId,
        Command.UseAbility c => c.PlayerId == seatPlayerId,
        Command.Revive c => c.PlayerId == seatPlayerId,
        Command.CraftAttachment c => c.PlayerId == seatPlayerId,
        Command.SelectAmmo c => c.PlayerId == seatPlayerId,
        Command.Reload c => c.PlayerId == seatPlayerId,
        Command.PlayerMelee c => c.PlayerId == seatPlayerId,
        Command.BuyMelee c => c.PlayerId == seatPlayerId,
        Command.CraftMeleeAttachment c => c.PlayerId == seatPlayerId,
        Command.UpgradeMelee c => c.PlayerId == seatPlayerId,
        // Deliberately closed rather than defaulting to true: an unknown
        // command from a client should be refused, not trusted. The cost is
        // that a new command forgotten here is silently rejected for network
        // players while working in solo, so the gate below enumerates types
        // rather than trusting this list to stay complete.
        _ => false,
    };

    private static string Sanitize(string name)
    {
        var sb = new StringBuilder(name.Length);
        foreach (char c in name)
            if (c != '|' && !char.IsControl(c)) sb.Append(c);
        return sb.Length > 0 ? sb.ToString(0, System.Math.Min(sb.Length, 24)) : "player";
    }

    private static string F(float v) => v.ToString("R", System.Globalization.CultureInfo.InvariantCulture);
    private static float Pf(string s) => float.Parse(s, System.Globalization.CultureInfo.InvariantCulture);
}
