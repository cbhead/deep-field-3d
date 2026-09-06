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
    private string _tagLabel = "";

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
        SetAnchorsAndOffsetsPreset(LayoutPreset.FullRect);
        MouseFilter = MouseFilterEnum.Ignore;   // never eat gameplay input
        Visible = false;
        ZIndex = 10;
    }

    public void Open(SocketDef socket, GameView view)
    {
        _socketId = socket.Id;
        _tagLabel = socket.Tag.ToString().ToUpperInvariant();
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

        var centre = Size * 0.5f;
        float wedge = Mathf.Tau / _options.Count;
        var display = Tokens.Display;
        var mono = Tokens.Mono;

        // Ring backing keeps the wedges legible over a bright world.
        DrawCircle(centre, Radius + 30f, new Color(0.031f, 0.043f, 0.067f, 0.55f));

        for (int i = 0; i < _options.Count; i++)
        {
            var option = _options[i];
            bool affordable = CanAfford(option);
            bool selected = i == _selected;

            float mid = i * wedge - Mathf.Pi * 0.5f;
            var seat = centre + new Vector2(Mathf.Cos(mid), Mathf.Sin(mid)) * Radius;

            // Each entry is a chamfered card, not a pie slice — design's wheel
            // is a ring of cards so the icon, name and price stay upright.
            var card = new Rect2(seat - new Vector2(58, 46), new Vector2(116, 92));
            DrawChamfered(card, selected ? Tokens.SurfaceRaised : Tokens.SurfaceGlass,
                selected ? Tokens.Brass500 : Tokens.BorderPanel, 8f);

            var ink = !affordable ? Tokens.TextDisabled
                : selected ? Tokens.TextAccent : Tokens.TextPrimary;

            var icon = UiTheme.Icon(option.IconId, ink);
            DrawTextureRect(icon, new Rect2(seat - new Vector2(16, 34), new Vector2(32, 32)),
                false, ink with { A = affordable ? 1f : 0.45f });

            if (display is not null)
                DrawString(display, seat + new Vector2(-52, 16), option.Label,
                    HorizontalAlignment.Center, 104, Tokens.SizeCaption, ink);

            if (mono is not null)
            {
                string price = DisplayCost(option).ToString();
                foreach (var (type, amount) in option.ScrapCost)
                    price += $" +{amount}{type.ToString()[0]}";
                DrawString(mono, seat + new Vector2(-52, 34), price,
                    HorizontalAlignment.Center, 104, Tokens.SizeCaption,
                    affordable ? Tokens.TextAccent : Tokens.StateDanger);
            }
        }

        // Hub: which socket this is and what it accepts.
        DrawCircle(centre, DeadZone + 22f, new Color(0.039f, 0.055f, 0.086f, 0.95f));
        DrawArc(centre, DeadZone + 22f, 0, Mathf.Tau, 48, Tokens.BorderPanel, 1f);

        if (display is not null)
            DrawString(display, centre + new Vector2(-60, -14), "SOCKET",
                HorizontalAlignment.Center, 120, Tokens.SizeMicro, Tokens.TextMuted);
        if (mono is not null)
            DrawString(mono, centre + new Vector2(-60, 10), _socketId.ToUpperInvariant(),
                HorizontalAlignment.Center, 120, Tokens.SizeStat, Tokens.TextPrimary);
        if (display is not null)
            DrawString(display, centre + new Vector2(-60, 30), _tagLabel,
                HorizontalAlignment.Center, 120, Tokens.SizeMicro, Tokens.TextArcane);

        if (_refusal.Length > 0 && display is not null)
            DrawString(display, centre + new Vector2(-160, Radius + 82), _refusal,
                HorizontalAlignment.Center, 320, Tokens.SizeBody, Tokens.StateDanger);
    }

    /// <summary>The kit's two-corner chamfer, drawn directly — the wheel paints
    /// itself rather than nesting Controls so it can follow the mouse at
    /// whatever rate the frame allows.</summary>
    private void DrawChamfered(Rect2 r, Color fill, Color stroke, float c)
    {
        var p = new[]
        {
            r.Position + new Vector2(c, 0),
            r.Position + new Vector2(r.Size.X, 0),
            r.Position + new Vector2(r.Size.X, r.Size.Y - c),
            r.Position + new Vector2(r.Size.X - c, r.Size.Y),
            r.Position + new Vector2(0, r.Size.Y),
            r.Position + new Vector2(0, c),
        };
        DrawColoredPolygon(p, fill);
        var loop = new Vector2[p.Length + 1];
        p.CopyTo(loop, 0);
        loop[^1] = p[0];
        DrawPolyline(loop, stroke, 1f);
    }
}
