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

    private WorldBar _hpBack = null!;
    private WorldBar _hpFill = null!;
    private WorldBar _shieldFill = null!;
    private readonly List<Sprite3D> _statusIcons = new();

    private float _lastHp = 1f;
    private byte _lastBits;

    /// <summary>Vertical offset above the enemy's origin, set from its scale.</summary>
    public float HeadHeight { get; set; } = 2.0f;

    public override void _Ready()
    {
        _hpBack = WorldBar.Make(new Color(0, 0, 0, 0.65f), 0f, BarSize);
        _hpFill = WorldBar.Make(Tokens.BarHp, 0.001f, BarSize);
        _shieldFill = WorldBar.Make(Tokens.BarShield, 0.002f, BarSize);
        // The shield rides its own row above the hp bar. Set here rather than
        // after each Set(), which is where it used to be: Set shifts the bar
        // left as it drains, and re-centring it afterwards made the shield
        // shrink towards its middle instead of emptying the way everything
        // else does. Set only touches X, so this survives.
        _shieldFill.Position = new Vector3(0f, 0.17f, 0.002f);
        AddChild(_hpBack);
        AddChild(_hpFill);
        AddChild(_shieldFill);
        Visible = false;
    }

    private static readonly Vector2 BarSize = new(1.1f, 0.13f);

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
        _hpBack.Set(1f, alpha * 0.65f);
        // Design's single hp threshold: venom green until 30%, threat red under
        // it. Same rule as the player's own bar, so one colour means one thing.
        _hpFill.Set(hpFraction, alpha);
        _hpFill.Tint(Kit.HpColor(hpFraction));
        _hpFill.Visible = hpFraction > 0.001f;

        _shieldFill.Visible = shieldFraction > 0.001f;
        if (_shieldFill.Visible) _shieldFill.Set(shieldFraction, alpha);

        if (statusBits != _lastBits) RebuildStatusIcons(statusBits);
        _lastBits = statusBits;
        _lastHp = hpFraction;
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
