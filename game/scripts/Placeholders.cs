using Godot;

namespace DeepField.Game;

/// <summary>Every code-built graybox in one place.
///
/// These are what the game draws until Claude Design's models land — and the
/// only thing that has to be true of them is that a player can tell two
/// different things apart. Before this file existed, all six towers and all
/// three traps rendered as the same box, which made the M2.5 build wheel a
/// choice you couldn't see the result of.
///
/// As real assets arrive these stop being reached (AssetLibrary prefers the
/// model), so this file shrinks in relevance rather than needing deletion.
/// Colors are placeholders; design owns the palette.</summary>
public static class Placeholders
{
    private static readonly Color Steel = new(0.55f, 0.60f, 0.85f);
    private static readonly Color Olive = new(0.55f, 0.58f, 0.35f);
    private static readonly Color Violet = new(0.60f, 0.42f, 0.90f);
    private static readonly Color Teal = new(0.35f, 0.78f, 0.80f);
    private static readonly Color Amber = new(0.90f, 0.72f, 0.30f);
    private static readonly Color Timber = new(0.60f, 0.45f, 0.30f);
    private static readonly Color Tarry = new(0.16f, 0.20f, 0.16f);
    // Design's enemy base albedo, so a graybox and a shipped model respond to
    // the hp lerp identically (PALETTE.md §Enemies).
    private static readonly Color Bone = new("7D8BA3");

    // =====================================================================
    // Enemies
    // =====================================================================

    /// <summary>Collision and overhead placement read this whether the view is
    /// a graybox or a shipped model, so it stays outside the mesh builders.</summary>
    public static float EnemyScale(string defId) => defId switch
    {
        "mote" => 0.55f,
        "monolith" => 2.4f,
        "aegis" => 1.4f,
        "skiff" => 0.9f,
        "cluster" => 1.15f,
        "mender" => 1.1f,
        _ => 1f,
    };

    public static Node3D Enemy(string defId)
    {
        float scale = EnemyScale(defId);
        var root = new Node3D();

        // Silhouette per family: the readable-silhouette rule applies to the
        // graybox too, or the roster is unreadable until art ships.
        Mesh mesh = defId switch
        {
            "mote" => new SphereMesh { Radius = 0.45f * scale, Height = 0.9f * scale },
            "cluster" => new SphereMesh { Radius = 0.62f * scale, Height = 1.3f * scale },
            "monolith" => new BoxMesh { Size = new Vector3(1.2f * scale, 1.9f * scale, 1.0f * scale) },
            "skiff" => new BoxMesh { Size = new Vector3(1.9f * scale, 0.35f * scale, 0.9f * scale) },
            "aegis" => new BoxMesh { Size = new Vector3(1.3f * scale, 1.7f * scale, 0.8f * scale) },
            // Shade is narrow and tall — a shape you catch out of the corner of
            // your eye, since heroes must spot it before a Detector does.
            "shade" => new PrismMesh { Size = new Vector3(0.55f, 1.9f, 0.55f) },
            // Mender reads as an emitter on a stalk: the heal is the threat, so
            // the thing to shoot is the thing you can see.
            "mender" => new CylinderMesh
            {
                TopRadius = 0.55f * scale, BottomRadius = 0.28f * scale, Height = 1.7f * scale,
            },
            _ => new CapsuleMesh { Radius = 0.45f * scale, Height = 1.6f * scale },
        };

        root.AddChild(new MeshInstance3D
        {
            Name = "Mesh",
            Mesh = mesh,
            MaterialOverride = new StandardMaterial3D { AlbedoColor = Bone },
            Position = new Vector3(0, 0.8f * scale, 0),
        });
        return root;
    }

    // =====================================================================
    // Structures
    // =====================================================================

    /// <summary>One graybox per tower/trap def, distinct at build-wheel
    /// distance. Pivot at ground centre, matching the model contract.</summary>
    public static Node3D Structure(string defId)
    {
        var root = new Node3D();
        switch (defId)
        {
            case "lance":
                Add(root, new BoxMesh { Size = new Vector3(0.9f, 1.8f, 0.9f) }, Steel, new Vector3(0, 0.9f, 0));
                Add(root, new CylinderMesh { TopRadius = 0.12f, BottomRadius = 0.16f, Height = 2.2f },
                    Steel, new Vector3(0, 1.9f, 0.5f), new Vector3(90, 0, 0));
                break;

            case "nova":
                Add(root, new CylinderMesh { TopRadius = 0.95f, BottomRadius = 1.05f, Height = 0.9f },
                    Olive, new Vector3(0, 0.45f, 0));
                Add(root, new CylinderMesh { TopRadius = 0.42f, BottomRadius = 0.5f, Height = 1.5f },
                    Olive, new Vector3(0, 1.2f, -0.15f), new Vector3(-35, 0, 0));
                break;

            case "singularity":
                Add(root, new TorusMesh { InnerRadius = 0.75f, OuterRadius = 1.0f },
                    Violet, new Vector3(0, 1.2f, 0), new Vector3(90, 0, 0));
                Add(root, new SphereMesh { Radius = 0.42f, Height = 0.84f }, Violet,
                    new Vector3(0, 1.2f, 0), emissive: true);
                break;

            case "skywatch":
                Add(root, new BoxMesh { Size = new Vector3(0.5f, 3.0f, 0.5f) }, Teal, new Vector3(0, 1.5f, 0));
                Add(root, new CylinderMesh { TopRadius = 0.85f, BottomRadius = 0.15f, Height = 0.5f },
                    Teal, new Vector3(0, 3.1f, 0), new Vector3(-25, 0, 0));
                break;

            case "arc":
                Add(root, new CylinderMesh { TopRadius = 0.7f, BottomRadius = 0.8f, Height = 0.7f },
                    Amber, new Vector3(0, 0.35f, 0));
                Add(root, new CylinderMesh { TopRadius = 0.5f, BottomRadius = 0.6f, Height = 0.6f },
                    Amber, new Vector3(0, 1.0f, 0));
                Add(root, new SphereMesh { Radius = 0.4f, Height = 0.8f },
                    Amber, new Vector3(0, 1.6f, 0), emissive: true);
                break;

            case "barricade":
                Add(root, new BoxMesh { Size = new Vector3(4.5f, 2.2f, 0.8f) }, Timber, new Vector3(0, 1.1f, 0));
                break;

            case "spike":
                Add(root, new CylinderMesh { TopRadius = 1.5f, BottomRadius = 1.5f, Height = 0.15f },
                    new Color(0.8f, 0.55f, 0.25f), new Vector3(0, 0.08f, 0));
                for (int i = 0; i < 4; i++)
                {
                    float a = Mathf.Tau * i / 4f;
                    Add(root, new CylinderMesh { TopRadius = 0f, BottomRadius = 0.16f, Height = 0.7f },
                        new Color(0.85f, 0.85f, 0.88f),
                        new Vector3(Mathf.Cos(a) * 0.7f, 0.5f, Mathf.Sin(a) * 0.7f));
                }
                break;

            case "tar":
                Add(root, new CylinderMesh { TopRadius = 1.7f, BottomRadius = 1.7f, Height = 0.08f },
                    Tarry, new Vector3(0, 0.05f, 0));
                break;

            case "launcher":
                Add(root, new CylinderMesh { TopRadius = 1.5f, BottomRadius = 1.5f, Height = 0.15f },
                    new Color(0.75f, 0.60f, 0.30f), new Vector3(0, 0.08f, 0));
                Add(root, new CylinderMesh { TopRadius = 0.55f, BottomRadius = 0.6f, Height = 0.5f },
                    Amber, new Vector3(0, 0.35f, 0), emissive: true);
                break;

            default:
                Add(root, new BoxMesh { Size = new Vector3(1.2f, 2.6f, 1.2f) }, Steel, new Vector3(0, 1.3f, 0));
                break;
        }
        return root;
    }

    /// <summary>The graybox stand-in for one upgrade path's stage module. Grows
    /// with level and sits on its own side of the chassis, so a tower's build
    /// state is legible from across the map before art lands.</summary>
    public static Node3D TowerModule(string pathId, int pathIndex, int level)
    {
        Color color = pathId switch
        {
            "damage" => new Color(0.90f, 0.42f, 0.35f),
            "range" => new Color(0.42f, 0.70f, 0.95f),
            "rate" => new Color(0.95f, 0.82f, 0.35f),
            _ => new Color(0.70f, 0.72f, 0.76f),
        };

        float grow = 0.18f + 0.06f * level;
        float angle = Mathf.Tau * pathIndex / 3f + Mathf.Pi / 6f;
        var root = new Node3D();
        Add(root, new BoxMesh { Size = new Vector3(grow, grow + 0.10f * level, grow) }, color,
            new Vector3(Mathf.Cos(angle) * 0.72f, 0.6f + 0.05f * level, Mathf.Sin(angle) * 0.72f),
            emissive: level >= 4);
        return root;
    }

    public static Node3D Projectile(string towerDefId)
    {
        var color = towerDefId switch
        {
            "nova" => Olive,
            "skywatch" => Teal,
            "arc" => Amber,
            _ => new Color(1f, 0.9f, 0.3f),
        };
        float radius = towerDefId == "nova" ? 0.24f : 0.15f;

        var root = new Node3D();
        Add(root, new SphereMesh { Radius = radius, Height = radius * 2f }, color, Vector3.Zero, emissive: true);
        return root;
    }

    public static Node3D Hero(string factionId)
    {
        var color = factionId switch
        {
            "forge" => new Color(0.95f, 0.60f, 0.25f),
            "ember" => new Color(0.93f, 0.35f, 0.22f),
            "tempest" => new Color(0.60f, 0.50f, 0.95f),
            _ => new Color(0.3f, 0.8f, 0.9f),
        };

        var root = new Node3D();
        Add(root, new CapsuleMesh { Radius = 0.4f, Height = 1.8f }, color, new Vector3(0, 0.9f, 0));
        return root;
    }

    // =====================================================================

    private static void Add(Node3D parent, Mesh mesh, Color color, Vector3 position,
        Vector3 rotationDegrees = default, bool emissive = false)
    {
        parent.AddChild(new MeshInstance3D
        {
            Mesh = mesh,
            MaterialOverride = new StandardMaterial3D
            {
                AlbedoColor = color,
                EmissionEnabled = emissive,
                Emission = color * 0.6f,
            },
            Position = position,
            RotationDegrees = rotationDegrees,
        });
    }
}
