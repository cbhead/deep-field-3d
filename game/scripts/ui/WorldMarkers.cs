using Godot;
using System.Collections.Generic;

namespace DeepField.Game.Ui;

/// <summary>Transient world-space feedback that isn't attached to a persistent
/// view: floating damage numbers, revive beacons that read through walls, and
/// named reaction callouts (the co-op combo has to be legible to both players
/// who caused it).</summary>
public partial class WorldMarkers : Node3D
{
    private readonly List<(Node3D Node, double Life, Vector3 Drift)> _transients = new();
    private readonly Dictionary<int, Label3D> _reviveBeacons = new();

    public bool ShowDamageNumbers { get; set; } = true;

    public override void _Process(double delta)
    {
        for (int i = _transients.Count - 1; i >= 0; i--)
        {
            var (node, life, drift) = _transients[i];
            double remaining = life - delta;
            if (remaining <= 0 || !IsInstanceValid(node))
            {
                if (IsInstanceValid(node)) node.QueueFree();
                _transients.RemoveAt(i);
                continue;
            }

            node.Position += drift * (float)delta;
            if (node is Label3D label)
                label.Modulate = label.Modulate with { A = Mathf.Clamp((float)remaining * 1.6f, 0f, 1f) };

            _transients[i] = (node, remaining, drift);
        }
    }

    public void DamageNumber(Vector3 worldPos, float amount, Color color)
    {
        if (!ShowDamageNumbers || amount < 0.5f) return;

        var label = new Label3D
        {
            Text = amount >= 10 ? $"{amount:0}" : $"{amount:0.#}",
            FontSize = 48,
            PixelSize = 0.006f,
            Billboard = BaseMaterial3D.BillboardModeEnum.Enabled,
            Shaded = false,
            Modulate = color,
            Position = worldPos + new Vector3(
                (float)GD.RandRange(-0.3, 0.3), 1.2f, (float)GD.RandRange(-0.3, 0.3)),
        };
        AddChild(label);
        _transients.Add((label, 0.9, new Vector3(0, 1.6f, 0)));
    }

    /// <summary>Named callout for a reaction — deliberately louder than a damage
    /// number, because it's the payoff of two players' kits combining.</summary>
    public void Callout(Vector3 worldPos, string text, Color color)
    {
        var label = new Label3D
        {
            Text = text,
            FontSize = 64,
            PixelSize = 0.008f,
            Billboard = BaseMaterial3D.BillboardModeEnum.Enabled,
            Shaded = false,
            NoDepthTest = true,
            Modulate = color,
            OutlineSize = 8,
            OutlineModulate = new Color(0, 0, 0, 0.8f),
            Position = worldPos + new Vector3(0, 2.0f, 0),
        };
        AddChild(label);
        _transients.Add((label, 1.4, new Vector3(0, 0.9f, 0)));
    }

    /// <summary>Downed teammates get a through-wall beacon with a live revive
    /// progress readout, so reaching them is a navigation problem, not a search.</summary>
    public void SetReviveBeacon(int playerId, Vector3 worldPos, string name, float distance, bool active)
    {
        if (!active)
        {
            if (_reviveBeacons.Remove(playerId, out var stale) && IsInstanceValid(stale))
                stale.QueueFree();
            return;
        }

        if (!_reviveBeacons.TryGetValue(playerId, out var beacon) || !IsInstanceValid(beacon))
        {
            beacon = new Label3D
            {
                FontSize = 40,
                PixelSize = 0.007f,
                Billboard = BaseMaterial3D.BillboardModeEnum.Enabled,
                Shaded = false,
                NoDepthTest = true,          // reads through geometry
                Modulate = UiTheme.Danger,
                OutlineSize = 6,
                OutlineModulate = new Color(0, 0, 0, 0.85f),
            };
            AddChild(beacon);
            _reviveBeacons[playerId] = beacon;
        }

        beacon.Text = $"⬇ {name}  {distance:0}m\nhold R";
        beacon.Position = worldPos + new Vector3(0, 2.2f, 0);
    }

    public void ClearBeacons()
    {
        foreach (var beacon in _reviveBeacons.Values)
            if (IsInstanceValid(beacon)) beacon.QueueFree();
        _reviveBeacons.Clear();
    }
}
