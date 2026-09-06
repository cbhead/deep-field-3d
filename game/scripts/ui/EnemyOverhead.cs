using Godot;
using System.Collections.Generic;
using DeepField.Sim;
using DeepField.Sim.Content;

namespace DeepField.Game.Ui;

/// <summary>Billboarded hp/shield bar and status icons above an enemy. Hidden
/// at full health (so a clean field stays clean), hidden entirely while the
/// enemy is burrowed, and faded by distance. Status icons resolve through
/// UiTheme.Icon, so they upgrade to design's art with no code change.</summary>
public partial class EnemyOverhead : Node3D
{
    private const float MaxVisibleDistance = 45f;

    private MeshInstance3D _hpBack = null!;
    private MeshInstance3D _hpFill = null!;
    private MeshInstance3D _shieldFill = null!;
    private readonly List<Sprite3D> _statusIcons = new();

    private float _lastHp = 1f;
    private byte _lastBits;

    /// <summary>Vertical offset above the enemy's origin, set from its scale.</summary>
    public float HeadHeight { get; set; } = 2.0f;

    public override void _Ready()
    {
        _hpBack = MakeBar(new Color(0, 0, 0, 0.65f), 0f);
        _hpFill = MakeBar(new Color(0.85f, 0.25f, 0.22f), 0.001f);
        _shieldFill = MakeBar(new Color(0.45f, 0.80f, 1.0f), 0.002f);
        AddChild(_hpBack);
        AddChild(_hpFill);
        AddChild(_shieldFill);
        Visible = false;
    }

    private static MeshInstance3D MakeBar(Color color, float depthNudge)
    {
        return new MeshInstance3D
        {
            Mesh = new QuadMesh { Size = new Vector2(1.1f, 0.13f) },
            MaterialOverride = new StandardMaterial3D
            {
                AlbedoColor = color,
                ShadingMode = BaseMaterial3D.ShadingModeEnum.Unshaded,
                BillboardMode = BaseMaterial3D.BillboardModeEnum.Enabled,
                Transparency = BaseMaterial3D.TransparencyEnum.Alpha,
                NoDepthTest = false,
                RenderPriority = 1,
            },
            Position = new Vector3(0, 0, depthNudge),
        };
    }

    /// <summary>hpFraction/shieldFraction in 0..1; statusBits packed per
    /// Protocol.PackStatusBits (bit index = Channel).</summary>
    public void Set(float hpFraction, float shieldFraction, byte statusBits,
        bool burrowed, float cameraDistance)
    {
        if (burrowed || cameraDistance > MaxVisibleDistance)
        {
            Visible = false;
            return;
        }

        bool interesting = hpFraction < 0.999f || shieldFraction > 0f || statusBits != 0;
        Visible = interesting;
        if (!interesting) return;

        Position = new Vector3(0, HeadHeight, 0);

        // Fade with distance so a big wave doesn't become a wall of bars.
        float alpha = Mathf.Clamp(1.4f - cameraDistance / MaxVisibleDistance, 0.25f, 1f);

        _hpBack.Visible = true;
        SetBar(_hpBack, 1f, alpha * 0.65f);
        SetBar(_hpFill, hpFraction, alpha);
        _hpFill.Visible = hpFraction > 0.001f;

        _shieldFill.Visible = shieldFraction > 0.001f;
        if (_shieldFill.Visible)
        {
            SetBar(_shieldFill, shieldFraction, alpha);
            _shieldFill.Position = new Vector3(0, 0.17f, 0.002f);
        }

        if (statusBits != _lastBits) RebuildStatusIcons(statusBits);
        _lastBits = statusBits;
        _lastHp = hpFraction;
    }

    private static void SetBar(MeshInstance3D bar, float fraction, float alpha)
    {
        fraction = Mathf.Clamp(fraction, 0f, 1f);
        bar.Scale = new Vector3(fraction, 1f, 1f);
        // Quads scale about their center; shift left so the bar drains rightward.
        bar.Position = new Vector3(-(1f - fraction) * 0.55f, bar.Position.Y, bar.Position.Z);
        if (bar.MaterialOverride is StandardMaterial3D material)
            material.AlbedoColor = material.AlbedoColor with { A = alpha };
    }

    private void RebuildStatusIcons(byte bits)
    {
        foreach (var icon in _statusIcons) icon.QueueFree();
        _statusIcons.Clear();

        var active = new List<string>();
        if ((bits & (1 << (int)Channel.Movement)) != 0) active.Add("chill");
        if ((bits & (1 << (int)Channel.Thermal)) != 0) active.Add("burn");
        if ((bits & (1 << (int)Channel.Vulnerability)) != 0) active.Add("mark");
        if ((bits & (1 << (int)Channel.Defense)) != 0) active.Add("shred");
        if ((bits & (1 << (int)Channel.Control)) != 0) active.Add("freeze");
        if ((bits & (1 << (int)Channel.Detection)) != 0) active.Add("reveal");

        for (int i = 0; i < active.Count; i++)
        {
            var sprite = new Sprite3D
            {
                Texture = UiTheme.Icon($"status_{active[i]}", UiTheme.Status(active[i])),
                PixelSize = 0.006f,
                Billboard = BaseMaterial3D.BillboardModeEnum.Enabled,
                Shaded = false,
                NoDepthTest = false,
                Modulate = UiTheme.Status(active[i]),
                Position = new Vector3(-0.18f * (active.Count - 1) / 2f + i * 0.18f, 0.34f, 0),
            };
            AddChild(sprite);
            _statusIcons.Add(sprite);
        }
    }
}
