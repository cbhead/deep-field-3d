// Content export: Sim.Core content tables -> unreal/content/json/<table>.json (+ schemas,
// content-ids, legacy level files). See unreal/PLAN/CONTRACTS/content-json.md for the format.
//
//   dotnet run --project tools/content-export -- --bootstrap            # write everything (refuses to overwrite)
//   dotnet run --project tools/content-export -- --bootstrap --force    # overwrite
//   dotnet run --project tools/content-export -- --diff                 # report drift between the sim and the JSON, exit 1 if any
//   dotnet run --project tools/content-export -- --schemas              # regenerate unreal/content/schema/*.schema.json only
//
// Everything is driven by reflection over the record types so a field added to a record
// shows up in the JSON and the schema without editing this file.

using System.Diagnostics;
using System.Reflection;
using System.Text.Encodings.Web;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Text.Json.Serialization;
using System.Text.Json.Serialization.Metadata;
using DeepField.Sim.Content;
using DeepField.Sim;

namespace DeepField.Tools.ContentExport;

internal static class Program
{
    private const string SchemaId = "deepfield-content/1";

    private static int Main(string[] args)
    {
        var mode = args.Contains("--diff") ? "diff" : args.Contains("--schemas") ? "schemas" : args.Contains("--bootstrap") ? "bootstrap" : "";
        if (mode == "")
        {
            Console.Error.WriteLine("usage: content-export --bootstrap [--force] | --diff | --schemas  [--repo <path>]");
            return 2;
        }
        bool force = args.Contains("--force");
        string repo = ArgValue(args, "--repo") ?? FindRepoRoot();
        string outDir = Path.Combine(repo, "unreal", "content", "json");
        string schemaDir = Path.Combine(repo, "unreal", "content", "schema");
        string legacyDir = Path.Combine(repo, "unreal", "content", "levels", "legacy");
        string sha = ArgValue(args, "--source-sha") ?? GitShortSha(repo);

        var tables = BuildTables();
        var levels = BuildLegacyLevels();

        if (mode == "schemas")
        {
            WriteSchemas(schemaDir, tables, force: true);
            return 0;
        }

        if (mode == "diff")
        {
            int drift = 0;
            foreach (var t in tables)
            {
                var path = Path.Combine(outDir, t.Name + ".json");
                if (!File.Exists(path)) { Console.WriteLine($"MISSING  {t.Name}.json"); drift++; continue; }
                var existing = JsonNode.Parse(File.ReadAllText(path))!.AsObject();
                var fresh = Envelope(t, sha, existing["generated"]?.GetValue<string>() ?? "", existing["sourceSha"]?.GetValue<string>() ?? "");
                if (!JsonNode.DeepEquals(existing["rows"], fresh["rows"]))
                {
                    Console.WriteLine($"DRIFT    {t.Name}.json differs from Sim.Core (the JSON is authoritative; this is informational)");
                    drift++;
                }
                else Console.WriteLine($"same     {t.Name}.json");
            }
            return drift == 0 ? 0 : 1;
        }

        Directory.CreateDirectory(outDir);
        Directory.CreateDirectory(legacyDir);
        string generated = DateTime.UtcNow.ToString("yyyy-MM-dd'T'HH:mm:ss'Z'");
        var ids = new JsonObject();
        foreach (var t in tables)
        {
            var path = Path.Combine(outDir, t.Name + ".json");
            if (File.Exists(path) && !force)
            {
                Console.WriteLine($"exists   {t.Name}.json (use --force to overwrite)");
                continue;
            }
            var env = Envelope(t, sha, generated, sha);
            File.WriteAllText(path, env.ToJsonString(Pretty) + "\n");
            Console.WriteLine($"wrote    {t.Name}.json ({env["rows"]!.AsArray().Count} rows)");
        }
        foreach (var t in tables)
            ids[t.Name] = new JsonArray(t.Rows.Select(r => (JsonNode)JsonValue.Create(r.Id)!).ToArray());
        File.WriteAllText(Path.Combine(repo, "unreal", "content", "content-ids.json"), ids.ToJsonString(Pretty) + "\n");
        Console.WriteLine("wrote    content-ids.json");

        foreach (var (mapId, level) in levels)
        {
            var path = Path.Combine(legacyDir, mapId + ".level.json");
            if (File.Exists(path) && !force) { Console.WriteLine($"exists   levels/legacy/{mapId}.level.json"); continue; }
            File.WriteAllText(path, level.ToJsonString(Pretty) + "\n");
            Console.WriteLine($"wrote    levels/legacy/{mapId}.level.json");
        }
        WriteSchemas(schemaDir, tables, force);
        return 0;
    }

    // ------------------------------------------------------------------ tables

    private sealed record Row(string Id, object Value);
    private sealed record Table(string Name, Type RowType, IReadOnlyList<Row> Rows);

    private static List<Table> BuildTables()
    {
        static Table FromDict<T>(string name, IReadOnlyDictionary<string, T> all) where T : notnull =>
            new(name, typeof(T), all.Select(kv => new Row(kv.Key, kv.Value)).ToList());

        var tables = new List<Table>
        {
            FromDict("towers", Towers.All),
            FromDict("traps", Traps.All),
            FromDict("enemies", Enemies.All),
            FromDict("statuses", Statuses.All),
            new("reactions", typeof(ReactionDef), Reactions.All.Select(r => new Row(r.Id, r)).ToList()),
            FromDict("factions", Factions.All),
            FromDict("weapons", Weapons.All),
            FromDict("melee", Melee.All),
            FromDict("meleeAttachments", Melee.Attachments),
            FromDict("attachments", Attachments.All),
            FromDict("ammo", Ammo.All),
            FromDict("conditions", Conditions.All),
            FromDict("vehicles", Vehicles.All),
        };

        // maps: sector metadata only. Geometry (routes, sockets, stations, gates) goes to
        // levels/legacy/<map>.level.json — the brief for the 3D redesign, not the output.
        var mapRows = Maps.All.Select(kv =>
        {
            var m = kv.Value;
            var o = new JsonObject
            {
                ["id"] = m.Id,
                ["order"] = Campaign.Sectors.ToList().IndexOf(m.Id),
                ["totalWaves"] = m.TotalWaves,
                ["fieldMeters"] = new JsonArray(m.FieldX, m.FieldZ),
                ["conditionSchedule"] = m.ConditionScheduleOrNull is null ? new JsonObject()
                    : new JsonObject(m.ConditionScheduleOrNull.OrderBy(p => p.Key).Select(p => KeyValuePair.Create(p.Key.ToString(), (JsonNode?)p.Value))),
                ["vehicles"] = new JsonArray((m.VehiclesOrNull ?? Array.Empty<VehicleSpawnDef>()).Select(v => (JsonNode)v.DefId).ToArray()),
                ["bossRated"] = false,
                ["lesson"] = "",
                ["tiers"] = new JsonArray("standard", "hardened", "assault"),
                ["legacyLevelFile"] = $"levels/legacy/{m.Id}.level.json",
            };
            return new Row(m.Id, o);
        }).ToList();
        tables.Add(new Table("maps", typeof(JsonObject), mapRows));

        foreach (var (mapId, waves) in Waves.ByMap)
        {
            var rows = new List<Row>();
            for (int w = 0; w < waves.Count; w++)
                for (int g = 0; g < waves[w].Count; g++)
                {
                    var grp = waves[w][g];
                    rows.Add(new Row($"w{w + 1:00}g{g + 1:00}", new JsonObject
                    {
                        ["id"] = $"w{w + 1:00}g{g + 1:00}",
                        ["waveIndex"] = w,
                        ["enemyId"] = grp.EnemyId,
                        ["count"] = grp.Count,
                        ["spacingTicks"] = grp.SpacingTicks,
                        ["startDelayTicks"] = grp.StartDelayTicks,
                        ["routeId"] = grp.RouteId,
                    }));
                }
            tables.Add(new Table("waves_" + mapId, typeof(JsonObject), rows));
        }

        // balance: every public const / static readonly scalar on Balance, as one row "default".
        var balance = new JsonObject { ["id"] = "default" };
        foreach (var f in typeof(Balance).GetFields(BindingFlags.Public | BindingFlags.Static).OrderBy(f => f.MetadataToken))
        {
            var v = f.GetValue(null);
            if (v is null) continue;
            balance[CamelCase(f.Name)] = JsonSerializer.SerializeToNode(v, v.GetType(), Options);
        }
        tables.Add(new Table("balance", typeof(JsonObject), new[] { new Row("default", balance) }));
        return tables;
    }

    private static JsonObject Envelope(Table t, string sha, string generated, string sourceSha)
    {
        var rows = new JsonArray();
        foreach (var r in t.Rows)
        {
            JsonNode node = r.Value is JsonObject jo ? (JsonNode)JsonNode.Parse(jo.ToJsonString())! : JsonSerializer.SerializeToNode(r.Value, t.RowType, Options)!;
            var obj = node.AsObject();
            if (!obj.ContainsKey("id"))
            {
                // id first, for readable diffs. A node has one parent, so detach before re-adding.
                var withId = new JsonObject { ["id"] = r.Id };
                foreach (var kv in obj.ToList()) { obj.Remove(kv.Key); withId[kv.Key] = kv.Value; }
                obj = withId;
            }
            rows.Add(obj);
        }
        return new JsonObject
        {
            ["$schema"] = SchemaId,
            ["table"] = t.Name,
            ["generated"] = generated,
            ["sourceSha"] = sourceSha == "" ? sha : sourceSha,
            ["rows"] = rows,
        };
    }

    // ------------------------------------------------------------ legacy levels

    private static List<(string, JsonObject)> BuildLegacyLevels()
    {
        var list = new List<(string, JsonObject)>();
        foreach (var (id, m) in Maps.All)
        {
            var o = new JsonObject
            {
                ["$schema"] = "deepfield-level/legacy",
                ["id"] = id,
                ["note"] = "Exported from sim/Sim.Core/Content/Maps.cs. The BRIEF for the 3D redesign (routes, lessons, socket counts, route-length ratios) — not the output. See unreal/PLAN/CONTRACTS/map-authoring-3d.md.",
                ["units"] = "metres, Y-up, -Z forward (sim frame)",
                ["field"] = new JsonArray(m.FieldX, m.FieldZ),
                ["totalWaves"] = m.TotalWaves,
                ["heroSpawn"] = Vec(m.HeroSpawn),
                ["armory"] = Vec(m.ArmoryPos),
                ["routes"] = new JsonArray(m.Routes.Select(r => (JsonNode)new JsonObject
                {
                    ["id"] = r.Id,
                    ["layer"] = r.Layer.ToString(),
                    ["waypoints"] = new JsonArray(r.Waypoints.Select(Vec).ToArray()),
                    ["barricadeGate"] = r.BarricadeGate,
                    ["fallbackRouteId"] = r.FallbackRouteId,
                    ["teleportLegs"] = r.TeleportLegs is null ? null : new JsonArray(r.TeleportLegs.Select(i => (JsonNode)i).ToArray()),
                }).ToArray()),
                ["sockets"] = new JsonArray(m.Sockets.Select(s => (JsonNode)new JsonObject { ["id"] = s.Id, ["tag"] = s.Tag.ToString(), ["pos"] = Vec(s.Pos) }).ToArray()),
                ["stations"] = new JsonArray(m.HeroStations.Select(s => (JsonNode)new JsonObject { ["id"] = s.Id, ["pos"] = Vec(s.Pos) }).ToArray()),
                ["conditionSchedule"] = m.ConditionScheduleOrNull is null ? new JsonObject()
                    : new JsonObject(m.ConditionScheduleOrNull.OrderBy(p => p.Key).Select(p => KeyValuePair.Create(p.Key.ToString(), (JsonNode?)p.Value))),
                ["vehicles"] = new JsonArray((m.VehiclesOrNull ?? Array.Empty<VehicleSpawnDef>()).Select(v => (JsonNode)new JsonObject { ["id"] = v.Id, ["defId"] = v.DefId, ["pos"] = Vec(v.Pos), ["yawDegrees"] = v.YawDegrees }).ToArray()),
                ["laneNodeNames"] = m.LaneNodeNamesOrNull is null ? new JsonArray() : JsonSerializer.SerializeToNode(m.LaneNodeNamesOrNull, Options),
                ["laneGates"] = m.LaneGatesOrNull is null ? new JsonArray() : JsonSerializer.SerializeToNode(m.LaneGatesOrNull, Options),
                ["operatedGates"] = m.OperatedGatesOrNull is null ? new JsonArray() : JsonSerializer.SerializeToNode(m.OperatedGatesOrNull, Options),
            };
            list.Add((id, o));
        }
        return list;
    }

    private static JsonNode Vec(Vec3 v) => new JsonArray(v.X, v.Y, v.Z);

    // ---------------------------------------------------------------- schemas

    private static void WriteSchemas(string dir, List<Table> tables, bool force)
    {
        Directory.CreateDirectory(dir);
        foreach (var t in tables)
        {
            string name = t.Name.StartsWith("waves_") ? "waves" : t.Name;
            var path = Path.Combine(dir, name + ".schema.json");
            if (File.Exists(path) && !force && !t.Name.StartsWith("waves_")) { continue; }
            JsonObject rowSchema = t.RowType == typeof(JsonObject)
                ? SchemaFromSample(t.Rows.Count > 0 ? (JsonObject)t.Rows[0].Value : new JsonObject())
                : SchemaFromRecord(t.RowType);
            var schema = new JsonObject
            {
                ["$schema"] = "https://json-schema.org/draft/2020-12/schema",
                ["$id"] = $"https://deepfield.dev/content/{name}.schema.json",
                ["title"] = name,
                ["type"] = "object",
                ["additionalProperties"] = false,
                ["required"] = new JsonArray("$schema", "table", "generated", "rows"),
                ["properties"] = new JsonObject
                {
                    ["$schema"] = new JsonObject { ["const"] = SchemaId },
                    ["table"] = new JsonObject { ["type"] = "string" },
                    ["generated"] = new JsonObject { ["type"] = "string" },
                    ["sourceSha"] = new JsonObject { ["type"] = "string" },
                    ["rows"] = new JsonObject { ["type"] = "array", ["items"] = rowSchema },
                },
            };
            File.WriteAllText(path, schema.ToJsonString(Pretty) + "\n");
            Console.WriteLine($"schema   {name}.schema.json");
        }
    }

    private static JsonObject SchemaFromRecord(Type t)
    {
        var ctor = t.GetConstructors().OrderByDescending(c => c.GetParameters().Length).First();
        var props = new JsonObject();
        var required = new JsonArray();
        foreach (var p in ctor.GetParameters())
        {
            string name = CamelCase(StripOrNull(p.Name!));
            props[name] = SchemaForType(p.ParameterType, p);
            bool nullable = p.ParameterType.IsClass && IsNullableRef(p) || Nullable.GetUnderlyingType(p.ParameterType) != null;
            if (!p.HasDefaultValue && !nullable) required.Add(name);
        }
        return new JsonObject { ["type"] = "object", ["additionalProperties"] = false, ["required"] = required, ["properties"] = props };
    }

    private static JsonObject SchemaForType(Type t, ParameterInfo? p = null)
    {
        t = Nullable.GetUnderlyingType(t) ?? t;
        if (t == typeof(string)) return new JsonObject { ["type"] = "string" };
        if (t == typeof(bool)) return new JsonObject { ["type"] = "boolean" };
        if (t == typeof(int) || t == typeof(long)) return new JsonObject { ["type"] = "integer" };
        if (t == typeof(float) || t == typeof(double)) return new JsonObject { ["type"] = "number" };
        if (t.IsEnum) return new JsonObject { ["type"] = "string", ["enum"] = new JsonArray(Enum.GetNames(t).Select(n => (JsonNode)n).ToArray()) };
        if (t == typeof(Vec3)) return new JsonObject { ["type"] = "array", ["items"] = new JsonObject { ["type"] = "number" }, ["minItems"] = 3, ["maxItems"] = 3 };
        if (t.IsGenericType)
        {
            var def = t.GetGenericTypeDefinition();
            var a = t.GetGenericArguments();
            if (def == typeof(IReadOnlyDictionary<,>) || def == typeof(Dictionary<,>))
            {
                var keys = a[0].IsEnum ? new JsonObject { ["enum"] = new JsonArray(Enum.GetNames(a[0]).Select(n => (JsonNode)n).ToArray()) } : null;
                var o = new JsonObject { ["type"] = "object", ["additionalProperties"] = SchemaForType(a[1]) };
                if (keys != null) o["propertyNames"] = keys;
                return o;
            }
            if (def == typeof(IReadOnlyList<>) || def == typeof(IReadOnlyCollection<>) || def == typeof(List<>) || def == typeof(IEnumerable<>))
                return new JsonObject { ["type"] = "array", ["items"] = SchemaForType(a[0]) };
        }
        if (t.IsArray) return new JsonObject { ["type"] = "array", ["items"] = SchemaForType(t.GetElementType()!) };
        if (t.IsClass || t.IsValueType) return SchemaFromRecord(t);
        return new JsonObject();
    }

    private static JsonObject SchemaFromSample(JsonObject sample)
    {
        var props = new JsonObject();
        foreach (var (k, v) in sample)
        {
            props[k] = v switch
            {
                JsonValue val when val.TryGetValue<string>(out _) => new JsonObject { ["type"] = "string" },
                JsonValue val when val.TryGetValue<bool>(out _) => new JsonObject { ["type"] = "boolean" },
                JsonValue val when val.TryGetValue<int>(out _) => new JsonObject { ["type"] = "integer" },
                JsonValue => new JsonObject { ["type"] = "number" },
                JsonArray => new JsonObject { ["type"] = "array" },
                JsonObject => new JsonObject { ["type"] = "object" },
                _ => new JsonObject(),
            };
        }
        return new JsonObject { ["type"] = "object", ["additionalProperties"] = false, ["required"] = new JsonArray("id"), ["properties"] = props };
    }

    private static bool IsNullableRef(ParameterInfo p)
    {
        var ctx = new NullabilityInfoContext();
        return ctx.Create(p).WriteState == NullabilityState.Nullable;
    }

    // ---------------------------------------------------------------- json options

    private static readonly JsonSerializerOptions Pretty = new()
    {
        WriteIndented = true,
        Encoder = JavaScriptEncoder.UnsafeRelaxedJsonEscaping,
        TypeInfoResolver = new DefaultJsonTypeInfoResolver(),   // JsonNode.ToJsonString needs one for primitives
    };

    private static readonly JsonSerializerOptions Options = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        DictionaryKeyPolicy = null,
        DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull,
        Converters = { new JsonStringEnumConverter(), new Vec3Converter() },
        TypeInfoResolver = new DefaultJsonTypeInfoResolver { Modifiers = { OnlyConstructorProperties } },
        Encoder = JavaScriptEncoder.UnsafeRelaxedJsonEscaping,
    };

    /// <summary>Records expose computed properties (MaxLevel, Magnitude, CosFrontArmorHalfArc…);
    /// the JSON carries only what the record was constructed from. Also strips the `OrNull`
    /// suffix the sim uses for optional map fields.</summary>
    private static void OnlyConstructorProperties(JsonTypeInfo info)
    {
        if (info.Kind != JsonTypeInfoKind.Object) return;
        var ctor = info.Type.GetConstructors().OrderByDescending(c => c.GetParameters().Length).FirstOrDefault();
        if (ctor is null || ctor.GetParameters().Length == 0) return;
        var names = ctor.GetParameters().Select(p => p.Name!).ToHashSet(StringComparer.Ordinal);
        for (int i = info.Properties.Count - 1; i >= 0; i--)
        {
            var prop = info.Properties[i];
            var clr = prop.AttributeProvider is PropertyInfo pi ? pi.Name : prop.Name;
            if (!names.Contains(clr)) { info.Properties.RemoveAt(i); continue; }
            if (clr.EndsWith("OrNull", StringComparison.Ordinal)) prop.Name = CamelCase(StripOrNull(clr));
        }
    }

    private sealed class Vec3Converter : JsonConverter<Vec3>
    {
        public override Vec3 Read(ref Utf8JsonReader reader, Type typeToConvert, JsonSerializerOptions options)
        {
            var a = JsonSerializer.Deserialize<float[]>(ref reader, options)!;
            return new Vec3(a[0], a[1], a[2]);
        }
        public override void Write(Utf8JsonWriter writer, Vec3 v, JsonSerializerOptions options)
        {
            writer.WriteStartArray(); writer.WriteNumberValue(v.X); writer.WriteNumberValue(v.Y); writer.WriteNumberValue(v.Z); writer.WriteEndArray();
        }
    }

    // ---------------------------------------------------------------- helpers

    private static string CamelCase(string s) => s.Length == 0 ? s : char.ToLowerInvariant(s[0]) + s[1..];
    private static string StripOrNull(string s) => s.EndsWith("OrNull", StringComparison.Ordinal) ? s[..^6] : s;

    private static string? ArgValue(string[] args, string key)
    {
        int i = Array.IndexOf(args, key);
        return i >= 0 && i + 1 < args.Length ? args[i + 1] : null;
    }

    private static string FindRepoRoot()
    {
        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        while (dir != null && !Directory.Exists(Path.Combine(dir.FullName, "unreal", "PLAN"))) dir = dir.Parent;
        return dir?.FullName ?? throw new InvalidOperationException("run from inside the repository, or pass --repo <path>");
    }

    private static string GitShortSha(string repo)
    {
        try
        {
            var psi = new ProcessStartInfo("git", "rev-parse --short HEAD") { WorkingDirectory = repo, RedirectStandardOutput = true, UseShellExecute = false };
            using var p = Process.Start(psi)!;
            var s = p.StandardOutput.ReadToEnd().Trim();
            p.WaitForExit();
            return p.ExitCode == 0 && s.Length > 0 ? s : "unknown";
        }
        catch { return "unknown"; }
    }
}
