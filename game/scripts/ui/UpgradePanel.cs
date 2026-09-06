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
    private PanelContainer _card = null!;
    private Label _title = null!;
    private Label _footer = null!;

    private StructureView? _target;
    private string _refusal = "";
    private double _sellHold;

    public bool IsOpen { get; private set; }
    public int TargetId => _target?.Id ?? -1;

    /// <summary>Set when the player completes the sell hold; GameRoot consumes it.</summary>
    public bool SellRequested { get; private set; }

    public override void _Ready()
    {
        SetAnchorsPreset(LayoutPreset.FullRect);
        MouseFilter = MouseFilterEnum.Ignore;
        Visible = false;

        _card = UiTheme.Card(UiTheme.Panel, UiTheme.Accent with { A = 0.4f });
        _card.SetAnchorsPreset(LayoutPreset.CenterRight);
        _card.Position = new Vector2(-380, -120);
        _card.CustomMinimumSize = new Vector2(340, 0);
        AddChild(_card);

        var column = new VBoxContainer();
        _card.AddChild(column);

        _title = UiTheme.Text("", 16);
        column.AddChild(_title);

        _rows = new VBoxContainer();
        column.AddChild(_rows);

        _footer = UiTheme.Text("", 12, UiTheme.InkDim);
        column.AddChild(_footer);
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
            _title.Text = $"{_target.DefId.ToUpperInvariant()}   ({_target.SocketId})";
            var row = new HBoxContainer();
            row.AddChild(UiTheme.Text($"charges {_target.ChargesLeft}/{trap.Charges}", 13, UiTheme.Ink));
            _rows.AddChild(row);
            _rows.AddChild(UiTheme.Text("traps aren't upgradeable — they expend and expire", 11, UiTheme.InkDim));
            _footer.Text = _refusal.Length > 0 ? _refusal : "hold X to sell";
            return;
        }

        var def = Towers.All[_target.DefId];
        _title.Text = $"{_target.DefId.ToUpperInvariant()}   ({_target.SocketId})";

        for (int i = 0; i < def.UpgradePaths.Count; i++)
        {
            var path = def.UpgradePaths[i];
            int level = i < _target.PathLevels.Length ? _target.PathLevels[i] : 0;
            bool maxed = level >= path.LevelCosts.Count;

            var row = new HBoxContainer();
            row.AddThemeConstantOverride("separation", 8);

            row.AddChild(Kit.Numeral($"{i + 1}", 13, UiTheme.Accent));
            var name = Kit.Label(path.Id, Tokens.TextSecondary);
            name.CustomMinimumSize = new Vector2(70, 0);
            row.AddChild(name);

            // Design's pips: arcane for a bought level, brass at the L4/L7/L10
            // breakpoints, empty wells for the endless band that opens at M4.
            var pips = new KitPips();
            pips.Set(level, path.LevelCosts.Count);
            row.AddChild(pips);

            if (maxed)
            {
                row.AddChild(UiTheme.Text("MAX", 12, UiTheme.Good));
            }
            else
            {
                int cost = path.LevelCosts[level];
                bool affordable = view.Money >= cost;
                row.AddChild(UiTheme.Text($"{cost}c", 12, affordable ? UiTheme.Ink : UiTheme.Danger));

                // The L4 breakpoint also charges scrap — surface have/need so a
                // player knows which enemies to farm, not just that they're short.
                if (level + 1 == 4)
                {
                    foreach (var (type, amount) in path.BreakpointRecipe)
                        row.AddChild(UiTheme.CountChip($"scrap_{type.ToString().ToLowerInvariant()}",
                            view.TeamScrapOf(type), UiTheme.Scrap(type), amount));
                }
            }

            _rows.AddChild(row);
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
