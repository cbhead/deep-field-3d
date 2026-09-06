using Godot;
using System.Collections.Generic;
using System.Linq;
using DeepField.Sim.Content;

namespace DeepField.Game.Ui;

/// <summary>Hold U aiming at a structure: shows each upgrade path with its
/// level pips, next-level price, and the L4 breakpoint scrap recipe with
/// have/need. Number keys pick a path, X sells. Replaces the placeholder that
/// always leveled path 0.</summary>
public partial class UpgradePanel : Control
{
    private VBoxContainer _rows = null!;
    private KitPanel _panel = null!;
    private Label _title = null!;
    private Label _footer = null!;

    private StructureView? _target;
    private string _refusal = "";
    private double _sellHold;

    public bool IsOpen { get; private set; }
    public int TargetId => _target?.Id ?? -1;

    /// <summary>Set when the player completes the sell hold; GameRoot consumes it.</summary>
    public bool SellRequested { get; private set; }

    /// <summary>Path index the player clicked Upgrade on, or -1. GameRoot
    /// drains it — the panel raises intent, it never submits commands.</summary>
    public int UpgradeRequested { get; private set; } = -1;

    public void ConsumeUpgrade() => UpgradeRequested = -1;

    public override void _Ready()
    {
        SetAnchorsAndOffsetsPreset(LayoutPreset.FullRect);
        MouseFilter = MouseFilterEnum.Ignore;
        Visible = false;

        // Design docks this to the right edge under the top bar, wide enough
        // for ten pips and a breakpoint recipe on one line.
        _panel = new KitPanel("Structure", Tokens.Brass500);
        _panel.SetAnchorsPreset(LayoutPreset.TopRight);
        _panel.Position = new Vector2(-536, 90);
        _panel.CustomMinimumSize = new Vector2(520, 0);
        AddChild(_panel);

        _title = Kit.Label("", Tokens.TextSecondary);
        _panel.HeaderTrailing(_title);

        _rows = Kit.Col(Tokens.Space6);
        _panel.Body.AddChild(_rows);

        _footer = Kit.Body("", Tokens.SizeCaption, Tokens.TextMuted);
        _panel.SetFooter(_footer);
    }

    public void Open(StructureView target, GameView view)
    {
        _target = target;
        _refusal = "";
        _sellHold = 0;
        SellRequested = false;
        IsOpen = true;
        Visible = true;
        Rebuild(view);
    }

    public void Close()
    {
        IsOpen = false;
        Visible = false;
        _target = null;
        _sellHold = 0;
        SellRequested = false;
    }

    public void Refresh(GameView view)
    {
        if (!IsOpen || _target is null) return;
        // Re-read the live structure (levels change under us as upgrades land).
        var live = view.Structures.FirstOrDefault(s => s.Id == _target.Id);
        if (live is null) { Close(); return; }
        _target = live;
        Rebuild(view);
    }

    public void ShowRefusal(string reason)
    {
        _refusal = reason;
    }

    /// <summary>Path index for a number key, or -1 if that key has no path.</summary>
    public int PathForKey(int oneBased)
    {
        if (_target is null || _target.IsTrap) return -1;
        var def = Towers.All[_target.DefId];
        int index = oneBased - 1;
        return index >= 0 && index < def.UpgradePaths.Count ? index : -1;
    }

    /// <summary>Sell is a hold, so it can't be fat-fingered mid-fight.</summary>
    public void TickSellHold(double delta, bool held)
    {
        if (!IsOpen) return;
        if (!held) { _sellHold = 0; return; }
        _sellHold += delta;
        if (_sellHold >= 0.7) { SellRequested = true; _sellHold = 0; }
    }

    public void ConsumeSell() => SellRequested = false;

    private void Rebuild(GameView view)
    {
        if (_target is null) return;

        foreach (var child in _rows.GetChildren()) child.QueueFree();

        if (_target.IsTrap)
        {
            var trap = Traps.All[_target.DefId];
            _title.Text = $"{_target.DefId} · {_target.SocketId}";
            var card = Kit.Card();
            var column = Kit.Col(Tokens.Space3);
            card.AddChild(column);
            column.AddChild(Kit.Between(Kit.Label("charges"),
                Kit.Numeral($"{_target.ChargesLeft}/{trap.Charges}", Tokens.SizeStatSm,
                    _target.ChargesLeft > 0 ? Tokens.TextPrimary : Tokens.StateDanger)));
            column.AddChild(Kit.Bar(trap.Charges == 0 ? 0f : (float)_target.ChargesLeft / trap.Charges,
                Tokens.Brass500, 8f));
            column.AddChild(Kit.Body("traps expend and expire — they are not upgraded",
                Tokens.SizeCaption, Tokens.TextMuted));
            _rows.AddChild(card);
            _footer.Text = _refusal.Length > 0 ? _refusal : "hold X to sell";
            return;
        }

        var def = Towers.All[_target.DefId];
        _title.Text = $"{_target.DefId} · {_target.SocketId}";

        for (int i = 0; i < def.UpgradePaths.Count; i++)
        {
            var path = def.UpgradePaths[i];
            int level = i < _target.PathLevels.Length ? _target.PathLevels[i] : 0;
            bool maxed = level >= path.LevelCosts.Count;

            var card = Kit.Card(selected: i == 0);
            var row = Kit.Row(Tokens.Space5);
            card.AddChild(row);

            row.AddChild(Kit.Icon($"path_{path.Id}",
                i == 0 ? Tokens.TextAccent : Tokens.TextSecondary, 24));

            var column = Kit.Col(Tokens.Space3);
            column.SizeFlagsHorizontal = SizeFlags.ExpandFill;
            row.AddChild(column);

            // Path name left, the level step and what it buys on the right.
            var head = Kit.Row();
            var name = Kit.Title(path.Id.ToUpperInvariant(), Tokens.SizeBody);
            name.SizeFlagsHorizontal = SizeFlags.ExpandFill;
            head.AddChild(name);
            head.AddChild(Kit.Numeral(
                maxed ? $"L{level} · MAX" : $"L{level} → L{level + 1}   ×{path.PerLevelFactor:0.00}",
                Tokens.SizeCaption, maxed ? Tokens.StateSuccess : Tokens.TextSecondary));
            column.AddChild(head);

            var pips = new KitPips();
            pips.Set(level, path.LevelCosts.Count);
            column.AddChild(pips);

            // Breakpoint chips: brass once reached, so a player can see which
            // level is the one that changes the tower rather than its numbers.
            var marks = Kit.Row(Tokens.Space3);
            foreach (int bp in new[] { 4, 7, 10 })
            {
                if (bp > path.LevelCosts.Count) continue;
                marks.AddChild(level >= bp
                    ? Kit.TagBrass($"L{bp}")
                    : Kit.Tag($"L{bp}"));
            }
            if (path.BreakpointRecipe.Count > 0 && level < 4)
            {
                foreach (var (type, amount) in path.BreakpointRecipe)
                    marks.AddChild(UiTheme.CountChip($"scrap_{type.ToString().ToLowerInvariant()}",
                        view.TeamScrapOf(type), UiTheme.Scrap(type), amount));
            }
            column.AddChild(marks);

            var actions = Kit.Col(Tokens.Space3);
            if (maxed)
            {
                actions.AddChild(Kit.TagOk("max"));
            }
            else
            {
                int cost = path.LevelCosts[level];
                bool affordable = view.Money >= cost;
                var upgrade = new KitButton($"Upgrade  {i + 1}",
                    affordable ? KitButton.Tone.Primary : KitButton.Tone.Secondary, Tokens.ControlSm);
                upgrade.Disabled = !affordable;
                int captured = i;
                upgrade.Pressed += () => UpgradeRequested = captured;
                actions.AddChild(upgrade);
                var price = Kit.Numeral(cost.ToString(), Tokens.SizeStatSm,
                    affordable ? Tokens.TextAccent : Tokens.StateDanger);
                price.HorizontalAlignment = HorizontalAlignment.Right;
                actions.AddChild(price);
            }
            row.AddChild(actions);

            _rows.AddChild(card);
        }

        int refund = 0;
        var structureDef = Towers.All[_target.DefId];
        refund = structureDef.Cost * Balance.SellRefundPercent / 100;   // placement floor
        _footer.Text = _refusal.Length > 0
            ? _refusal
            : $"1-{def.UpgradePaths.Count} upgrade   ·   hold X to sell (≈{refund}c back)";
    }

    private static string Pips(int level, int max)
    {
        var text = new System.Text.StringBuilder();
        for (int i = 0; i < max; i++) text.Append(i < level ? '●' : '○');
        return text.ToString();
    }
}
