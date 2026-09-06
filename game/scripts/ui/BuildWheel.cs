using Godot;
using System.Collections.Generic;
using System.Linq;
using DeepField.Sim.Content;

namespace DeepField.Game.Ui;

/// <summary>Radial build menu. Hold E aiming at a socket to open; the wheel
/// offers exactly what that socket's tag accepts, priced and affordability-
/// checked; mouse direction picks a wedge; releasing E builds it.
///
/// This replaces the placeholder that auto-picked a tower by placement count —
/// the sim always accepted any tower id, only the client couldn't ask.</summary>
public partial class BuildWheel : Control
{
    public sealed record Option(string DefId, string Label, string IconId, int Cost,
        IReadOnlyDictionary<ScrapType, int> ScrapCost, bool IsTrap);

    private const float Radius = 150f;
    private const float DeadZone = 34f;

    private readonly List<Option> _options = new();
    private Vector2 _aim = Vector2.Zero;
    private int _selected = -1;
    private GameView _view = new();
    private string _socketId = "";
    private string _refusal = "";

    public bool IsOpen { get; private set; }
    public string SocketId => _socketId;
    public Option? Selection => _selected >= 0 && _selected < _options.Count ? _options[_selected] : null;

    /// <summary>Options legal on a socket tag. The sim is the authority
    /// (Step.ApplyPlaceTower); this mirrors its rules so the wheel never offers
    /// something that would be refused for tag reasons.</summary>
    public static List<Option> OptionsFor(SocketTag tag)
    {
        var options = new List<Option>();

        if (tag is SocketTag.Ground or SocketTag.Wall)
        {
            foreach (var def in Towers.All.Values)
            {
                if (def.Kind == TowerKind.Barricade) continue;
                options.Add(new Option(def.Id, def.Id.ToUpperInvariant(), $"tower_{def.Id}",
                    def.Cost, new Dictionary<ScrapType, int>(), false));
            }
        }
        else if (tag == SocketTag.Trap)
        {
            foreach (var def in Traps.All.Values)
                options.Add(new Option(def.Id, def.Id.ToUpperInvariant(), $"trap_{def.Id}",
                    def.Cost, def.ScrapCost, true));
        }
        else if (tag == SocketTag.Barricade)
        {
            var def = Towers.Barricade;
            options.Add(new Option(def.Id, "BARRICADE", "tower_barricade",
                def.Cost, new Dictionary<ScrapType, int>(), false));
        }

        return options.OrderBy(o => o.Cost).ToList();
    }

    public override void _Ready()
    {
        SetAnchorsPreset(LayoutPreset.FullRect);
        MouseFilter = MouseFilterEnum.Ignore;   // never eat gameplay input
        Visible = false;
        ZIndex = 10;
    }

    public void Open(SocketDef socket, GameView view)
    {
        _socketId = socket.Id;
        _view = view;
        _options.Clear();
        _options.AddRange(OptionsFor(socket.Tag));
        _aim = Vector2.Zero;
        _selected = -1;
        _refusal = "";
        IsOpen = true;
        Visible = true;
        QueueRedraw();
    }

    public void Refresh(GameView view)
    {
        _view = view;
        if (IsOpen) QueueRedraw();
    }

    public void Close()
    {
        IsOpen = false;
        Visible = false;
        _selected = -1;
    }

    /// <summary>Called with raw mouse motion while the wheel is open (the cursor
    /// stays captured, so direction is accumulated rather than read).</summary>
    public void Steer(Vector2 relative)
    {
        if (!IsOpen) return;
        _aim += relative;
        if (_aim.Length() > Radius) _aim = _aim.Normalized() * Radius;

        if (_aim.Length() < DeadZone || _options.Count == 0)
        {
            _selected = -1;
        }
        else
        {
            // Wedge 0 starts at 12 o'clock and runs clockwise.
            float angle = Mathf.PosMod(Mathf.Atan2(_aim.X, -_aim.Y), Mathf.Tau);
            float wedge = Mathf.Tau / _options.Count;
            _selected = (int)(Mathf.PosMod(angle + wedge * 0.5f, Mathf.Tau) / wedge);
        }
        QueueRedraw();
    }

    /// <summary>Direct pick by number key (1-6), for players who prefer keys.</summary>
    public void SelectIndex(int index)
    {
        if (!IsOpen || index < 0 || index >= _options.Count) return;
        _selected = index;
        QueueRedraw();
    }

    public void ShowRefusal(string reason)
    {
        _refusal = reason;
        QueueRedraw();
    }

    public bool CanAfford(Option option)
    {
        if (_view.Money < DisplayCost(option)) return false;
        foreach (var (type, amount) in option.ScrapCost)
            if (_view.TeamScrapOf(type) < amount) return false;
        return true;
    }

    /// <summary>Forge's build discount is a sim rule; showing the discounted
    /// price keeps the wheel honest about what will actually be charged.</summary>
    public int DisplayCost(Option option)
    {
        if (option.IsTrap) return option.Cost;
        return _view.Local?.FactionId == Factions.Forge.Id
            ? (int)(option.Cost * Balance.ForgeBuildDiscount)
            : option.Cost;
    }

    public override void _Draw()
    {
        if (!IsOpen || _options.Count == 0) return;

        var center = Size * 0.5f;
        float wedge = Mathf.Tau / _options.Count;

        DrawCircle(center, Radius + 14f, new Color(0, 0, 0, 0.45f));

        for (int i = 0; i < _options.Count; i++)
        {
            var option = _options[i];
            bool affordable = CanAfford(option);
            bool selected = i == _selected;

            float mid = i * wedge - Mathf.Pi * 0.5f;
            var dir = new Vector2(Mathf.Cos(mid), Mathf.Sin(mid));
            var seat = center + dir * (Radius * 0.62f);

            // Wedge backing.
            var fill = selected
                ? (affordable ? UiTheme.Accent with { A = 0.55f } : UiTheme.Danger with { A = 0.45f })
                : new Color(0.10f, 0.12f, 0.15f, 0.75f);
            DrawArcFilled(center, Radius, i * wedge - wedge * 0.5f - Mathf.Pi * 0.5f, wedge, fill);

            var ink = affordable ? UiTheme.Ink : UiTheme.Disabled;

            // Icon chip (design's art when present, placeholder chip until then).
            var icon = UiTheme.Icon(option.IconId, affordable ? UiTheme.Accent : UiTheme.Disabled);
            var iconSize = new Vector2(34, 34);
            DrawTextureRect(icon, new Rect2(seat - iconSize * 0.5f - new Vector2(0, 14), iconSize), false,
                new Color(1, 1, 1, affordable ? 1f : 0.5f));

            DrawString(ThemeDB.FallbackFont, seat + new Vector2(-38, 16), option.Label,
                HorizontalAlignment.Center, 76, 13, ink);

            string price = $"{DisplayCost(option)}c";
            foreach (var (type, amount) in option.ScrapCost)
                price += $" +{amount}{type.ToString()[0]}";
            DrawString(ThemeDB.FallbackFont, seat + new Vector2(-38, 32), price,
                HorizontalAlignment.Center, 76, 11,
                affordable ? UiTheme.InkDim : UiTheme.Danger);
        }

        // Hub: current selection detail, or the prompt.
        DrawCircle(center, DeadZone, new Color(0.05f, 0.06f, 0.08f, 0.92f));
        string hub = Selection is { } sel ? sel.Label : "AIM";
        DrawString(ThemeDB.FallbackFont, center + new Vector2(-DeadZone, 4), hub,
            HorizontalAlignment.Center, DeadZone * 2, 12, UiTheme.Ink);

        // Footer: credits on hand, and any refusal the sim answered with.
        DrawString(ThemeDB.FallbackFont, center + new Vector2(-100, Radius + 42),
            $"{_view.Money} credits   ·   release E to build", HorizontalAlignment.Center, 200, 12,
            UiTheme.InkDim);

        if (_refusal.Length > 0)
            DrawString(ThemeDB.FallbackFont, center + new Vector2(-100, Radius + 62),
                _refusal, HorizontalAlignment.Center, 200, 13, UiTheme.Danger);
    }

    private void DrawArcFilled(Vector2 center, float radius, float startAngle, float sweep, Color color)
    {
        const int steps = 16;
        var points = new Vector2[steps + 2];
        points[0] = center;
        for (int i = 0; i <= steps; i++)
        {
            float a = startAngle + sweep * i / steps;
            points[i + 1] = center + new Vector2(Mathf.Cos(a), Mathf.Sin(a)) * radius;
        }
        DrawColoredPolygon(points, color);
    }
}
