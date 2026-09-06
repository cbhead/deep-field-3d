using Godot;
using System.Collections.Generic;
using System.Linq;
using DeepField.Sim;
using DeepField.Sim.Content;

namespace DeepField.Game.Ui;

/// <summary>Full-screen gunsmith at the armory station. Buy platforms with
/// credits; craft attachments and ammo with personal scrap; save and recraft
/// blueprints. Every action maps to an existing sim command and every refusal
/// is answered inline — the sim stays the authority on what's affordable.</summary>
public partial class ArmoryScreen : Control
{
    public System.Action<Command>? Submit;
    public System.Action<string>? RecraftBlueprint;
    public System.Func<string, bool>? HasBlueprint;

    private GameView _view = new();
    private string _weaponId = "sidearm";
    private AttachmentSlot _slot = AttachmentSlot.Barrel;
    private string _notice = "";

    private VBoxContainer _weaponList = null!;
    private VBoxContainer _slotList = null!;
    private VBoxContainer _optionList = null!;
    private VBoxContainer _ammoList = null!;
    private Label _statLine = null!;
    private Label _noticeLabel = null!;
    private Label _scrapLine = null!;

    public bool IsOpen { get; private set; }

    public override void _Ready()
    {
        SetAnchorsPreset(LayoutPreset.FullRect);
        Visible = false;

        var backdrop = new ColorRect
        {
            Color = new Color(0.02f, 0.03f, 0.04f, 0.92f),
            MouseFilter = MouseFilterEnum.Stop,
        };
        backdrop.SetAnchorsPreset(LayoutPreset.FullRect);
        AddChild(backdrop);

        var frame = new VBoxContainer();
        frame.SetAnchorsPreset(LayoutPreset.FullRect);
        frame.AddThemeConstantOverride("separation", 10);
        frame.OffsetLeft = 60; frame.OffsetTop = 40;
        frame.OffsetRight = -60; frame.OffsetBottom = -40;
        AddChild(frame);

        var header = new HBoxContainer();
        header.AddChild(UiTheme.Text("ARMORY", 24));
        header.AddChild(UiTheme.Text("   [Esc] close", 13, UiTheme.InkDim));
        frame.AddChild(header);

        _scrapLine = UiTheme.Text("", 13, UiTheme.InkDim);
        frame.AddChild(_scrapLine);

        var columns = new HBoxContainer();
        columns.AddThemeConstantOverride("separation", 14);
        columns.SizeFlagsVertical = SizeFlags.ExpandFill;
        frame.AddChild(columns);

        columns.AddChild(Column("WEAPONS", 220, out _weaponList));
        columns.AddChild(Column("SLOTS", 200, out _slotList));
        columns.AddChild(Column("OPTIONS", 320, out _optionList));
        columns.AddChild(Column("AMMO", 220, out _ammoList));

        _statLine = UiTheme.Text("", 13, UiTheme.InkDim);
        frame.AddChild(_statLine);

        _noticeLabel = UiTheme.Text("", 14, UiTheme.Danger);
        frame.AddChild(_noticeLabel);

        var footer = new HBoxContainer();
        footer.AddThemeConstantOverride("separation", 10);
        var recraft = new Button { Text = "RECRAFT SAVED BLUEPRINT" };
        recraft.Pressed += () => { RecraftBlueprint?.Invoke(_weaponId); _notice = "recrafting…"; };
        footer.AddChild(recraft);
        footer.AddChild(UiTheme.Text("crafted attachments and ammo save to your profile automatically",
            11, UiTheme.InkDim));
        frame.AddChild(footer);
    }

    private static Control Column(string title, int width, out VBoxContainer body)
    {
        var card = UiTheme.Card();
        card.CustomMinimumSize = new Vector2(width, 0);
        card.SizeFlagsVertical = SizeFlags.ExpandFill;

        var column = new VBoxContainer();
        column.AddChild(UiTheme.Text(title, 12, UiTheme.InkDim));

        var scroll = new ScrollContainer { SizeFlagsVertical = SizeFlags.ExpandFill };
        body = new VBoxContainer { SizeFlagsHorizontal = SizeFlags.ExpandFill };
        scroll.AddChild(body);
        column.AddChild(scroll);

        card.AddChild(column);
        return card;
    }

    public void Open(GameView view)
    {
        IsOpen = true;
        Visible = true;
        _notice = "";
        _weaponId = view.Local?.WeaponId ?? "sidearm";
        Refresh(view);
    }

    public void Close()
    {
        IsOpen = false;
        Visible = false;
    }

    public void ShowNotice(string text) => _notice = text;

    public void Refresh(GameView view)
    {
        _view = view;
        if (!IsOpen || view.Local is not { } local) return;

        _scrapLine.Text = "your scrap:  " + string.Join("   ",
            System.Enum.GetValues<ScrapType>().Select(t => $"{t} {view.PersonalScrapOf(t)}"))
            + $"      credits {view.Money}";
        _noticeLabel.Text = _notice;

        RebuildWeapons(local);
        RebuildSlots(local);
        RebuildOptions(local);
        RebuildAmmo(local);
        RebuildStatLine(local);
    }

    private void RebuildWeapons(PlayerView local)
    {
        foreach (var child in _weaponList.GetChildren()) child.QueueFree();

        foreach (var weapon in Weapons.All.Values)
        {
            bool owned = local.OwnedWeapons.Contains(weapon.Id);
            bool equipped = local.WeaponId == weapon.Id;
            bool affordable = _view.Money >= weapon.Cost;

            var button = new Button
            {
                Text = owned
                    ? $"{(equipped ? "▸ " : "  ")}{weapon.Id.ToUpperInvariant()}"
                    : $"  {weapon.Id.ToUpperInvariant()}   {weapon.Cost}c",
                Alignment = HorizontalAlignment.Left,
                Disabled = !owned && !affordable,
                TooltipText = $"{weapon.Damage} dmg · {weapon.ShotsPerSecond}/s · {weapon.RangeMeters}m"
                    + (weapon.Applies.Count > 0 ? $" · applies {string.Join(",", weapon.Applies)}" : ""),
            };
            if (equipped) button.AddThemeColorOverride("font_color", UiTheme.Accent);

            string captured = weapon.Id;
            button.Pressed += () =>
            {
                _weaponId = captured;
                if (!local.OwnedWeapons.Contains(captured))
                    Submit?.Invoke(new Command.BuyWeapon(_view.LocalPlayerId, captured));
                else
                    Submit?.Invoke(new Command.SelectWeapon(_view.LocalPlayerId, captured));
                _notice = "";
            };
            _weaponList.AddChild(button);
        }
    }

    private void RebuildSlots(PlayerView local)
    {
        foreach (var child in _slotList.GetChildren()) child.QueueFree();

        var equipped = local.AttachmentsFor(_weaponId);
        foreach (AttachmentSlot slot in System.Enum.GetValues<AttachmentSlot>())
        {
            string current = equipped.TryGetValue(slot, out var id) ? id : "—";
            var button = new Button
            {
                Text = $"{(slot == _slot ? "▸ " : "  ")}{slot}\n     {current}",
                Alignment = HorizontalAlignment.Left,
            };
            if (slot == _slot) button.AddThemeColorOverride("font_color", UiTheme.Accent);

            var captured = slot;
            button.Pressed += () => { _slot = captured; _notice = ""; };
            _slotList.AddChild(button);
        }
    }

    private void RebuildOptions(PlayerView local)
    {
        foreach (var child in _optionList.GetChildren()) child.QueueFree();

        var equipped = local.AttachmentsFor(_weaponId);
        var candidates = Attachments.All.Values.Where(a => a.Slot == _slot).ToList();
        if (candidates.Count == 0)
        {
            _optionList.AddChild(UiTheme.Text("nothing for this slot yet", 12, UiTheme.Disabled));
            return;
        }

        foreach (var attachment in candidates)
        {
            bool isEquipped = equipped.TryGetValue(_slot, out var id) && id == attachment.Id;
            bool affordable = attachment.Recipe.All(r => _view.PersonalScrapOf(r.Key) >= r.Value);

            var card = UiTheme.Card(isEquipped ? UiTheme.PanelRaised : UiTheme.Panel,
                isEquipped ? UiTheme.Accent : null);
            var column = new VBoxContainer();

            column.AddChild(UiTheme.Text(attachment.Id, 14,
                isEquipped ? UiTheme.Accent : UiTheme.Ink));

            // Stat deltas read as signed percentages — the tradeoff is the point.
            var deltas = new HBoxContainer();
            deltas.AddThemeConstantOverride("separation", 10);
            AddDelta(deltas, "dmg", attachment.DamageFactor);
            AddDelta(deltas, "rate", attachment.RateFactor);
            AddDelta(deltas, "rng", attachment.RangeFactor);
            if (attachment.Applies is { } status)
                deltas.AddChild(UiTheme.Text($"+{status}", 12, UiTheme.Status(status)));
            column.AddChild(deltas);

            var recipe = new HBoxContainer();
            recipe.AddThemeConstantOverride("separation", 8);
            foreach (var (type, amount) in attachment.Recipe)
                recipe.AddChild(UiTheme.CountChip($"scrap_{type.ToString().ToLowerInvariant()}",
                    _view.PersonalScrapOf(type), UiTheme.Scrap(type), amount));
            column.AddChild(recipe);

            var craft = new Button
            {
                Text = isEquipped ? "EQUIPPED" : (affordable ? "CRAFT" : "NEED SCRAP"),
                Disabled = isEquipped,
            };
            string capturedId = attachment.Id;
            craft.Pressed += () =>
            {
                Submit?.Invoke(new Command.CraftAttachment(_view.LocalPlayerId, _weaponId, capturedId));
                _notice = "";
            };
            column.AddChild(craft);

            card.AddChild(column);
            _optionList.AddChild(card);
        }
    }

    private static void AddDelta(Control row, string label, float factor)
    {
        if (Mathf.IsEqualApprox(factor, 1f)) return;
        int percent = Mathf.RoundToInt((factor - 1f) * 100f);
        row.AddChild(UiTheme.Text($"{label} {(percent > 0 ? "+" : "")}{percent}%", 12,
            percent > 0 ? UiTheme.Good : UiTheme.Danger));
    }

    private void RebuildAmmo(PlayerView local)
    {
        foreach (var child in _ammoList.GetChildren()) child.QueueFree();

        string current = local.AmmoFor(_weaponId);
        foreach (var ammo in Ammo.All.Values)
        {
            bool selected = ammo.Id == current;
            bool crafted = local.CraftedAmmo.Contains(ammo.Id);
            bool affordable = crafted || ammo.Recipe.All(r => _view.PersonalScrapOf(r.Key) >= r.Value);

            var card = UiTheme.Card(selected ? UiTheme.PanelRaised : UiTheme.Panel,
                selected ? UiTheme.Accent : null);
            var column = new VBoxContainer();
            column.AddChild(UiTheme.Text(ammo.Id, 13, selected ? UiTheme.Accent : UiTheme.Ink));

            var notes = new HBoxContainer();
            notes.AddThemeConstantOverride("separation", 8);
            AddDelta(notes, "dmg", ammo.DamageFactor);
            if (ammo.IgnoresFlatArmor) notes.AddChild(UiTheme.Text("ignores armor", 11, UiTheme.Good));
            if (ammo.UnarmoredBonusFactor > 1f)
                notes.AddChild(UiTheme.Text($"+{Mathf.RoundToInt((ammo.UnarmoredBonusFactor - 1) * 100)}% vs soft",
                    11, UiTheme.Good));
            if (ammo.Applies is { } status)
                notes.AddChild(UiTheme.Text($"+{status}", 11, UiTheme.Status(status)));
            column.AddChild(notes);

            if (!crafted && ammo.Recipe.Count > 0)
            {
                var recipe = new HBoxContainer();
                foreach (var (type, amount) in ammo.Recipe)
                    recipe.AddChild(UiTheme.CountChip($"scrap_{type.ToString().ToLowerInvariant()}",
                        _view.PersonalScrapOf(type), UiTheme.Scrap(type), amount));
                column.AddChild(recipe);
            }

            var button = new Button
            {
                Text = selected ? "LOADED" : (crafted ? "LOAD" : (affordable ? "CRAFT + LOAD" : "NEED SCRAP")),
                Disabled = selected,
            };
            string capturedId = ammo.Id;
            button.Pressed += () =>
            {
                Submit?.Invoke(new Command.SelectAmmo(_view.LocalPlayerId, _weaponId, capturedId));
                _notice = "";
            };
            column.AddChild(button);

            card.AddChild(column);
            _ammoList.AddChild(card);
        }
    }

    private void RebuildStatLine(PlayerView local)
    {
        if (!Weapons.All.TryGetValue(_weaponId, out var weapon)) { _statLine.Text = ""; return; }

        // Mirror the sim's WeaponBuild math so the preview can't drift from play.
        var build = new WeaponBuild { AmmoId = local.AmmoFor(_weaponId) };
        foreach (var (slot, id) in local.AttachmentsFor(_weaponId)) build.Attachments[slot] = id;

        float damage = weapon.Damage * build.DamageFactor(targetArmored: false);
        float rate = weapon.ShotsPerSecond * build.RateFactor();
        float range = weapon.RangeMeters * build.RangeFactor();
        var applies = weapon.Applies.Concat(build.ExtraApplies()).Distinct().ToList();

        _statLine.Text = $"{_weaponId.ToUpperInvariant()} as built:  "
            + $"{damage:0.#} dmg   ·   {rate:0.##}/s   ·   {damage * rate:0.#} dps   ·   {range:0}m"
            + (build.IgnoresFlatArmor ? "   ·   ignores flat armor" : "")
            + (applies.Count > 0 ? $"   ·   applies {string.Join(", ", applies)}" : "")
            + (HasBlueprint?.Invoke(_weaponId) == true ? "   ·   blueprint saved" : "");
    }
}
