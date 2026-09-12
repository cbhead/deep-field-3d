using Godot;
using System.Collections.Generic;
using System.Linq;

namespace DeepField.Game.Ui;

/// <summary>Where to. A radial of every other pad on the network, steered and
/// confirmed exactly as the build wheel is.
///
/// A second radial rather than a list panel, because hold-E / steer / release
/// is already the muscle the player has for "choose one of a few things right
/// here", and because the mouse stays captured — a list would have to give the
/// cursor back in the middle of a wave.</summary>
public partial class TeleportPicker : Control
{
    public sealed record Destination(string Id, string Label, float Metres);

    private const float Radius = 150f;
    private const float DeadZone = 34f;

    private readonly List<Destination> _options = new();
    private Vector2 _aim = Vector2.Zero;
    private int _selected = -1;
    private string _fromLabel = "";

    public bool IsOpen { get; private set; }
    public Destination? Selection =>
        _selected >= 0 && _selected < _options.Count ? _options[_selected] : null;

    public override void _Ready()
    {
        SetAnchorsPreset(LayoutPreset.FullRect);
        MouseFilter = MouseFilterEnum.Ignore;
        Visible = false;
    }

    public void Open(string fromLabel, IEnumerable<Destination> destinations)
    {
        _fromLabel = fromLabel;
        _options.Clear();
        _options.AddRange(destinations);
        _aim = Vector2.Zero;
        // Nearest first, and pre-selected: the common case is the pad you were
        // just at, and a picker that opens on nothing costs a wrist flick every
        // time to say what you already meant.
        _selected = _options.Count > 0 ? 0 : -1;
        IsOpen = _options.Count > 0;
        Visible = IsOpen;
        QueueRedraw();
    }

    public void Close()
    {
        IsOpen = false;
        Visible = false;
        _options.Clear();
        _selected = -1;
        QueueRedraw();
    }

    public void Steer(Vector2 relative)
    {
        if (!IsOpen) return;
        _aim += relative;
        if (_aim.Length() > Radius) _aim = _aim.Normalized() * Radius;

        if (_aim.Length() < DeadZone || _options.Count == 0)
        {
            _selected = _options.Count > 0 ? 0 : -1;
        }
        else
        {
            float angle = Mathf.PosMod(Mathf.Atan2(_aim.X, -_aim.Y), Mathf.Tau);
            float wedge = Mathf.Tau / _options.Count;
            _selected = (int)(Mathf.PosMod(angle + wedge * 0.5f, Mathf.Tau) / wedge);
        }
        QueueRedraw();
    }

    public void SelectIndex(int index)
    {
        if (!IsOpen || index < 0 || index >= _options.Count) return;
        _selected = index;
        QueueRedraw();
    }

    public override void _Draw()
    {
        if (!IsOpen || _options.Count == 0) return;

        var centre = Size * 0.5f;
        float wedge = Mathf.Tau / _options.Count;
        var display = Tokens.Display;
        var mono = Tokens.Mono;

        DrawCircle(centre, Radius + 30f, new Color(0.031f, 0.043f, 0.067f, 0.55f));

        for (int i = 0; i < _options.Count; i++)
        {
            bool selected = i == _selected;
            float mid = i * wedge - Mathf.Pi * 0.5f;
            var seat = centre + new Vector2(Mathf.Cos(mid), Mathf.Sin(mid)) * Radius;

            var card = new Rect2(seat - new Vector2(66, 40), new Vector2(132, 80));
            DrawChamfered(card, selected ? Tokens.SurfaceRaised : Tokens.SurfaceGlass,
                selected ? Tokens.Brass500 : Tokens.BorderPanel, 8f);

            var ink = selected ? Tokens.TextAccent : Tokens.TextPrimary;
            if (display is not null)
                DrawString(display, seat + new Vector2(-58, 4), _options[i].Label,
                    HorizontalAlignment.Center, 116, Tokens.SizeCaption, ink);
            if (mono is not null)
                DrawString(mono, seat + new Vector2(-58, 26), $"{_options[i].Metres:0} m",
                    HorizontalAlignment.Center, 116, Tokens.SizeCaption, Tokens.TextMuted);
        }

        // The hub says where you are, because a network of four pads inside
        // four buildings is four identical rooms otherwise.
        if (display is not null)
        {
            DrawString(display, centre + new Vector2(-70, -6), "TELEPORT",
                HorizontalAlignment.Center, 140, Tokens.SizeCaption, Tokens.TextMuted);
            DrawString(display, centre + new Vector2(-70, 14), _fromLabel,
                HorizontalAlignment.Center, 140, Tokens.SizeCaption, Tokens.TextPrimary);
        }
    }

    /// <summary>The same chamfered card the build wheel draws, for the same
    /// reason: a radial that follows the mouse cannot afford a Control per
    /// entry.</summary>
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
