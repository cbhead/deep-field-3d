using Godot;
using System.Collections.Generic;
using System.Linq;
using DeepField.Sim;
using DeepField.Sim.Content;

namespace DeepField.Game.Ui;

/// <summary>The armory, laid out to design's second frame ("Armory / gunsmith
/// + ammo" and "Armory / blueprints" in docs/design-system/ui-screens.js).
///
/// One workbench with two tabs. Gunsmith: the platform rail on the left with
/// the faction's hands under it; the live build in the middle with the seven
/// slot cards around it, each card rendering the module it holds (the same
/// gunsmith models — an icon in a well was the previous frame); the ammo rail
/// beneath with the real rounds, the loaded type brass-framed and a recipe per
/// card; stat deltas, the selected slot's recipe and the blueprint on the
/// right. Blueprints: one card per saved build with a live render of exactly
/// that build and what recrafting it costs against the scrap held now.
///
/// Every action maps to a command the sim already accepts; refusals come back
/// through CraftRejected and land in the notice line. Where the frame shows a
/// number the sim does not model — reserve counts, a Handling stat — the
/// screen shows what is real instead (the magazine is unlimited reserve, and
/// modules change damage, rate and range).</summary>
public partial class ArmoryScreen : Control
{
    public System.Action<Command>? Submit;
    public System.Action<string>? RecraftBlueprint;
    public System.Action<string>? SaveBlueprint;
    public System.Func<string, bool>? HasBlueprint;
    public System.Func<string, IReadOnlyDictionary<string, string>?>? BlueprintSlots;
    public System.Func<string, string?>? BlueprintAmmo;

    private enum Tab { Gunsmith, Blueprints }

    private GameView _view = new();
    private string _weaponId = "sidearm";
    private AttachmentSlot _slot = AttachmentSlot.Barrel;
    private Tab _tab = Tab.Gunsmith;
    private string _notice = "";
    private bool _bought;
    private string _signature = "";

    // header
    private KitButton _gunsmithTab = null!;
    private KitButton _blueprintsTab = null!;
    private Label _moneyLabel = null!;
    private HBoxContainer _scrapStrip = null!;

    // gunsmith tab
    private Control _gunsmithPage = null!;
    private VBoxContainer _platformRail = null!;
    private VBoxContainer _handsBody = null!;
    private Control _bench = null!;
    private Control? _weaponHost;
    private string _weaponFor = "";
    private HBoxContainer _benchCaption = null!;
    private Label _ammoHeader = null!;
    private HBoxContainer _ammoRow = null!;
    private Label _noticeLabel = null!;
    private KitPanel _statPanel = null!;
    private KitPanel _recipePanel = null!;
    private Label _recipeTitle = null!;
    private VBoxContainer _optionColumn = null!;
    private KitPanel _blueprintPanel = null!;

    // blueprints tab
    private GridContainer _blueprintGrid = null!;

    public bool IsOpen { get; private set; }

    /// <summary>Everything this screen renders, flattened. Cheap to build and
    /// compared before touching a single node — the screen used to rebuild
    /// every frame, which destroyed each button between its press and its
    /// release.</summary>
    private string Signature(GameView view, PlayerView local)
    {
        var sb = new System.Text.StringBuilder();
        sb.Append(_tab).Append('|').Append(view.Money).Append('|').Append(local.WeaponId).Append('|')
          .Append(local.FactionId).Append('|').Append(_weaponId).Append('|').Append(_slot).Append('|')
          .Append(_notice).Append('|');
        foreach (string owned in local.OwnedWeapons.OrderBy(x => x, System.StringComparer.Ordinal))
            sb.Append(owned).Append(',');
        sb.Append('|');
        foreach (var weapon in Weapons.All.Keys)
        {
            foreach (var (slot, id) in local.AttachmentsFor(weapon).OrderBy(kv => kv.Key))
                sb.Append(weapon).Append('.').Append(slot).Append('=').Append(id).Append(',');
            sb.Append(local.AmmoFor(weapon)).Append(';');
            if (BlueprintSlots?.Invoke(weapon) is { } saved)
                foreach (var (slot, id) in saved.OrderBy(kv => kv.Key))
                    sb.Append('*').Append(slot).Append('=').Append(id).Append(',');
            sb.Append('*').Append(BlueprintAmmo?.Invoke(weapon) ?? "").Append('|');
        }
        foreach (string crafted in local.CraftedAmmo.OrderBy(x => x, System.StringComparer.Ordinal))
            sb.Append(crafted).Append(',');
        sb.Append('|');
        foreach (ScrapType type in System.Enum.GetValues<ScrapType>())
            sb.Append(view.PersonalScrapOf(type)).Append(',');
        return sb.ToString();
    }

    /// <summary>Design's bench is 1180×756 and the slot cards sit at fixed
    /// coordinates on it: three down the left, three down the right, the
    /// infusion below the gun. Fractions of that frame, scaled at runtime.</summary>
    private const float BenchW = 1180f, BenchH = 756f, CardW = 140f, CardH = 160f, WellW = 124f, WellH = 96f;
    private static readonly (AttachmentSlot Slot, float X, float Y)[] BenchLayout =
    {
        (AttachmentSlot.Barrel, 30f, 30f),
        (AttachmentSlot.Muzzle, 30f, 200f),
        (AttachmentSlot.Optic, 30f, 370f),
        (AttachmentSlot.Magazine, 1010f, 30f),
        (AttachmentSlot.Stock, 1010f, 200f),
        (AttachmentSlot.Underbarrel, 1010f, 370f),
        (AttachmentSlot.Infusion, 520f, 380f),
    };
    private static readonly Rect2 WeaponFrame = new(190f, 110f, 800f, 380f);

    public override void _Ready()
    {
        SetAnchorsAndOffsetsPreset(LayoutPreset.FullRect);
        Visible = false;

        var backdrop = new ColorRect { Color = Tokens.Obsidian900, MouseFilter = MouseFilterEnum.Stop };
        backdrop.SetAnchorsAndOffsetsPreset(LayoutPreset.FullRect);
        AddChild(backdrop);
        AddChild(new KitGrid());

        var frame = new VBoxContainer();
        frame.SetAnchorsAndOffsetsPreset(LayoutPreset.FullRect);
        frame.AddThemeConstantOverride("separation", 0);
        AddChild(frame);

        // --- header: title, the two tabs, the economy strip, close
        var (bar, headerRow) = Kit.ScreenHeader("Armory");
        bar.SizeFlagsHorizontal = SizeFlags.ExpandFill;
        frame.AddChild(bar);

        var tabs = Kit.Row(Tokens.Space2);
        _gunsmithTab = new KitButton("Gunsmith", KitButton.Tone.Primary, Tokens.ControlSm);
        _gunsmithTab.Pressed += () => { _tab = Tab.Gunsmith; Refresh(_view); };
        _blueprintsTab = new KitButton("Blueprints", KitButton.Tone.Secondary, Tokens.ControlSm);
        _blueprintsTab.Pressed += () => { _tab = Tab.Blueprints; Refresh(_view); };
        tabs.AddChild(_gunsmithTab);
        tabs.AddChild(_blueprintsTab);
        headerRow.AddChild(tabs);
        headerRow.AddChild(Kit.Spacer());

        // Design's single-row economy strip for 64 px headers: money, a rule,
        // then the four scrap types you hold — the crafting currency, so it
        // belongs on the screen where it is spent.
        var economy = Kit.Glass(Tokens.ChamferSm);
        economy.CustomMinimumSize = new Vector2(0, 40);
        var economyRow = Kit.Row(Tokens.Space5);
        economy.AddChild(economyRow);
        var money = Kit.Row(Tokens.Space3);
        money.AddChild(new KitDiamond(10f, Tokens.ResGold));
        _moneyLabel = Kit.Numeral("0", Tokens.SizeStat, Tokens.TextAccent);
        money.AddChild(_moneyLabel);
        money.AddChild(Kit.Label("credits"));
        economyRow.AddChild(money);
        var rule = new VSeparator();
        rule.AddThemeStyleboxOverride("separator", new StyleBoxFlat { BgColor = Tokens.BorderPanel, ContentMarginLeft = 1 });
        economyRow.AddChild(rule);
        _scrapStrip = Kit.Row(Tokens.Space5);
        economyRow.AddChild(_scrapStrip);
        headerRow.AddChild(economy);

        var close = new KitButton("Close  ESC", KitButton.Tone.Secondary);
        close.Pressed += Close;
        headerRow.AddChild(close);

        // --- pages
        var margin = new MarginContainer { SizeFlagsVertical = SizeFlags.ExpandFill };
        foreach (string side in new[] { "left", "right", "top", "bottom" })
            margin.AddThemeConstantOverride($"margin_{side}", Tokens.Space8);
        frame.AddChild(margin);

        _gunsmithPage = BuildGunsmithPage();
        margin.AddChild(_gunsmithPage);

        _blueprintGrid = new GridContainer { Columns = 3 };
        _blueprintGrid.AddThemeConstantOverride("h_separation", Tokens.Space6);
        _blueprintGrid.AddThemeConstantOverride("v_separation", Tokens.Space6);
        _blueprintGrid.SizeFlagsHorizontal = SizeFlags.ExpandFill;
        _blueprintGrid.SizeFlagsVertical = SizeFlags.ExpandFill;
        _blueprintGrid.Visible = false;
        margin.AddChild(_blueprintGrid);
    }

    private Control BuildGunsmithPage()
    {
        var columns = Kit.Row(Tokens.Space6);

        // --- left: platforms, hands
        var left = Kit.Col(Tokens.Space4);
        left.CustomMinimumSize = new Vector2(236, 0);
        columns.AddChild(left);
        _platformRail = Kit.Col(Tokens.Space4);
        left.AddChild(_platformRail);
        left.AddChild(new Control { SizeFlagsVertical = SizeFlags.ExpandFill });
        var hands = new KitPanel("Hands");
        _handsBody = hands.Body;
        left.AddChild(hands);

        // --- centre: the bench over the ammo rail
        var centre = Kit.Col(Tokens.Space6);
        centre.SizeFlagsHorizontal = SizeFlags.ExpandFill;
        centre.CustomMinimumSize = new Vector2(520, 0);
        columns.AddChild(centre);

        var benchPanel = Kit.Surface(Tokens.SurfacePanel with { A = 0.5f }, Tokens.BorderPanel, Tokens.ChamferMd, shadow: false);
        benchPanel.SizeFlagsVertical = SizeFlags.ExpandFill;
        if (benchPanel.GetThemeStylebox("panel") is ChamferBox benchBox)
            benchBox.ContentMarginLeft = benchBox.ContentMarginRight = benchBox.ContentMarginTop = benchBox.ContentMarginBottom = 0;
        centre.AddChild(benchPanel);
        _bench = new Control { SizeFlagsVertical = SizeFlags.ExpandFill, ClipContents = true };
        _bench.Draw += DrawBench;
        _bench.Resized += () => { _bench.QueueRedraw(); LayoutBench(); };
        benchPanel.AddChild(_bench);

        var ammoPanel = Kit.Surface(Tokens.SurfacePanel with { A = 0.5f }, Tokens.BorderPanel, Tokens.ChamferMd, shadow: false);
        ammoPanel.CustomMinimumSize = new Vector2(0, 196);
        if (ammoPanel.GetThemeStylebox("panel") is ChamferBox ammoBox)
            ammoBox.ContentMarginLeft = ammoBox.ContentMarginRight = ammoBox.ContentMarginTop = ammoBox.ContentMarginBottom = 0;
        centre.AddChild(ammoPanel);
        var ammoStack = Kit.Col(0);
        ammoPanel.AddChild(ammoStack);
        var ammoHead = new PanelContainer();
        ammoHead.AddThemeStyleboxOverride("panel", new StyleBoxFlat
        {
            BgColor = Tokens.SurfaceInset, BorderColor = Tokens.BorderPanel, BorderWidthBottom = 1,
            ContentMarginLeft = Tokens.Space6, ContentMarginRight = Tokens.Space6, ContentMarginTop = 8, ContentMarginBottom = 8,
        });
        ammoStack.AddChild(ammoHead);
        var ammoHeadRow = Kit.Row(Tokens.Space5);
        ammoHead.AddChild(ammoHeadRow);
        ammoHeadRow.AddChild(Kit.Label("Ammo", Tokens.TextAccent));
        _ammoHeader = Clipped(Kit.Label(""));
        ammoHeadRow.AddChild(_ammoHeader);
        ammoHeadRow.AddChild(Kit.Spacer());
        _noticeLabel = Clipped(Kit.Body("", Tokens.SizeCaption, UiTheme.Danger));
        _noticeLabel.HorizontalAlignment = HorizontalAlignment.Right;
        ammoHeadRow.AddChild(_noticeLabel);
        var ammoBody = new MarginContainer { SizeFlagsVertical = SizeFlags.ExpandFill };
        foreach (string side in new[] { "left", "right" }) ammoBody.AddThemeConstantOverride($"margin_{side}", Tokens.Space6);
        foreach (string side in new[] { "top", "bottom" }) ammoBody.AddThemeConstantOverride($"margin_{side}", Tokens.Space4);
        ammoStack.AddChild(ammoBody);
        // Seven ammo types since M3 against design's five: the rail scrolls
        // sideways when the window is narrower than the cards, rather than
        // the cards dictating how wide the window has to be.
        var scroll = new ScrollContainer
        {
            HorizontalScrollMode = ScrollContainer.ScrollMode.Auto,
            VerticalScrollMode = ScrollContainer.ScrollMode.Disabled,
            SizeFlagsHorizontal = SizeFlags.ExpandFill,
            SizeFlagsVertical = SizeFlags.ExpandFill,
        };
        ammoBody.AddChild(scroll);
        _ammoRow = Kit.Row(Tokens.Space4);
        _ammoRow.SizeFlagsHorizontal = SizeFlags.ExpandFill;
        scroll.AddChild(_ammoRow);

        // --- right: built, recipe, blueprint
        var right = Kit.Col(Tokens.Space5);
        right.CustomMinimumSize = new Vector2(340, 0);
        columns.AddChild(right);

        _statPanel = new KitPanel("Built", Tokens.Arcane500);
        right.AddChild(_statPanel);

        _recipePanel = new KitPanel("Recipe", Tokens.Brass500, hazard: true);
        _recipeTitle = Kit.Label("", Tokens.TextMuted);
        _recipePanel.HeaderTrailing(_recipeTitle);
        right.AddChild(_recipePanel);
        _optionColumn = Kit.Col(Tokens.Space3);
        _recipePanel.Body.AddChild(_optionColumn);

        _blueprintPanel = new KitPanel("Blueprint");
        right.AddChild(_blueprintPanel);

        return columns;
    }

    // =====================================================================

    /// <summary>Clicks the first enabled Buy button the way a player does — a
    /// real mouse event pushed through the viewport, so it goes through
    /// hit-testing rather than around it — and reports whether the handler
    /// ran. Kept because every slot on the previous bench was inert for weeks
    /// while looking perfectly correct.</summary>
    public bool ProbeClick()
    {
        _bought = false;
        Button? target = null;
        void Find(Node n)
        {
            if (n is Button b && b.Text == "BUY" && !b.Disabled) target ??= b;
            foreach (var c in n.GetChildren()) Find(c);
        }
        Find(this);
        if (target is null) { GD.PrintErr("ERROR: probe found no enabled Buy button"); return false; }

        var centre = target.GetGlobalRect().GetCenter();
        // Canvas coordinates in, canvas coordinates through: a window-space
        // event would be run through the viewport's inverse transform first.
        foreach (bool down in new[] { true, false })
        {
            GetViewport().PushInput(new InputEventMouseButton
            {
                ButtonIndex = MouseButton.Left, Pressed = down, Position = centre, GlobalPosition = centre,
            }, inLocalCoords: true);
        }
        bool bought = _bought;
        if (bought) GD.Print("[probe] gunsmith Buy click works: reached the handler");
        else
        {
            GD.PrintErr($"ERROR: gunsmith Buy at {centre} is inert — the click never reached it");
            // What Godot's own hit test finds there, and every control under
            // the point in tree order (the last one listed wins). A headless
            // run with a 64×64 root viewport hovers nothing anywhere: see the
            // root-size fix beside the armory shot preset in GameRoot.
            var hovered = GetViewport().GuiGetHoveredControl();
            GD.Print($"[probe] viewport {GetViewport().GetVisibleRect()} hovers {hovered?.GetType().Name ?? "nothing"} '{hovered?.Name}'");
            void Over(Node n, int depth)
            {
                if (n is Control c && c.IsVisibleInTree() && c.MouseFilter != MouseFilterEnum.Ignore
                    && c.GetGlobalRect().HasPoint(centre))
                    GD.Print($"[probe] under point: {new string(' ', depth)}{c.GetType().Name} '{c.Name}' filter={c.MouseFilter} rect={c.GetGlobalRect()}");
                foreach (var child in n.GetChildren()) Over(child, depth + 1);
            }
            Over(GetTree().Root, 0);
        }
        return bought;
    }

    /// <summary>Every interactive control with its rect, for when a probe
    /// fails and the question is which control lost its hit area.</summary>
    public void DumpHitAreas()
    {
        void Walk(Node n, int depth)
        {
            if (n is BaseButton b)
            {
                var c = (Control)n;
                GD.Print($"[hit] {new string(' ', depth)}{b.GetType().Name} " +
                    $"text='{(b as Button)?.Text ?? ""}' rect={c.GetGlobalRect()} " +
                    $"vis={c.IsVisibleInTree()} filter={c.MouseFilter} disabled={b.Disabled}");
            }
            foreach (var child in n.GetChildren()) Walk(child, depth + 1);
        }
        Walk(this, 0);
    }

    public void Open(GameView view)
    {
        IsOpen = true;
        Visible = true;
        _notice = "";
        _tab = Tab.Gunsmith;
        _weaponId = view.Local?.WeaponId ?? "sidearm";
        Refresh(view);
    }

    public void Close()
    {
        IsOpen = false;
        Visible = false;
    }

    public void ShowNotice(string text) => _notice = text;

    /// <summary>For the review shots: the Blueprints tab is otherwise only
    /// reachable through a click.</summary>
    public void ShowBlueprints() { _tab = Tab.Blueprints; Refresh(_view); }

    public void Refresh(GameView view)
    {
        _view = view;
        if (!IsOpen || view.Local is not { } local) return;

        _moneyLabel.Text = view.Money.ToString();
        _noticeLabel.Text = _notice;

        _bench.QueueRedraw();
        string signature = Signature(view, local);
        if (signature == _signature) return;
        _signature = signature;

        RebuildEconomy();
        _gunsmithPage.Visible = _tab == Tab.Gunsmith;
        _blueprintGrid.Visible = _tab == Tab.Blueprints;
        RestyleTab(_gunsmithTab, _tab == Tab.Gunsmith);
        RestyleTab(_blueprintsTab, _tab == Tab.Blueprints);

        if (_tab == Tab.Gunsmith)
        {
            RebuildPlatforms(local);
            RebuildHands(local);
            LayoutBench();
            RebuildAmmo(local);
            RebuildStats(local);
            RebuildRecipe(local);
            RebuildBlueprint(local);
            _bench.QueueRedraw();
        }
        else
        {
            RebuildBlueprintGrid(local);
        }
    }

    /// <summary>A KitButton bakes its tone into styleboxes, so the active tab
    /// is swapped for a fresh button rather than restyled in place.</summary>
    private void RestyleTab(KitButton button, bool active)
    {
        var replacement = new KitButton(button.Text, active ? KitButton.Tone.Primary : KitButton.Tone.Secondary, Tokens.ControlSm);
        var parent = button.GetParent();
        int index = button.GetIndex();
        parent.RemoveChild(button);
        parent.AddChild(replacement);
        parent.MoveChild(replacement, index);
        if (button == _gunsmithTab) { _gunsmithTab = replacement; replacement.Pressed += () => { _tab = Tab.Gunsmith; Refresh(_view); }; }
        else { _blueprintsTab = replacement; replacement.Pressed += () => { _tab = Tab.Blueprints; Refresh(_view); }; }
        button.QueueFree();
    }

    private void RebuildEconomy()
    {
        foreach (var child in _scrapStrip.GetChildren()) child.QueueFree();
        foreach (ScrapType type in new[] { ScrapType.Alloy, ScrapType.Flux, ScrapType.Plating, ScrapType.Gravium })
        {
            int have = _view.PersonalScrapOf(type);
            _scrapStrip.AddChild(UiTheme.CountChip($"scrap_{type.ToString().ToLowerInvariant()}", have,
                have > 0 ? UiTheme.Scrap(type) : Tokens.TextDisabled));
        }
    }

    // =====================================================================
    // Left: platform rail + hands
    // =====================================================================

    private void RebuildPlatforms(PlayerView local)
    {
        foreach (var child in _platformRail.GetChildren()) child.QueueFree();

        foreach (var def in Weapons.All.Values.OrderBy(ScrapPrice).ThenBy(w => w.Id, System.StringComparer.Ordinal))
        {
            bool owned = local.OwnedWeapons.Contains(def.Id);
            bool equipped = def.Id == local.WeaponId;
            bool selected = def.Id == _weaponId;

            var card = Kit.Card(selected);
            var row = Kit.Row(Tokens.Space5);
            card.AddChild(row);

            row.AddChild(Kit.Icon($"weapon_{def.Id}", selected ? Tokens.TextAccent : Tokens.TextSecondary, 26));

            var text = Kit.Col(2);
            text.SizeFlagsHorizontal = SizeFlags.ExpandFill;
            text.AddChild(Kit.Title(UiTheme.ShortLabel(def.Id).ToUpperInvariant(), Tokens.SizeBody));
            if (equipped || owned)
            {
                text.AddChild(Kit.Label(equipped ? "equipped" : "owned",
                    equipped ? Tokens.TextArcane : Tokens.TextMuted));
            }
            else if (def.Recipe.Count == 0)
            {
                text.AddChild(Kit.Label("free", Tokens.TextMuted));
            }
            else
            {
                // A platform costs personal scrap, so the card says which
                // enemies to go and farm — the same have/need chip the
                // attachment recipes use, for the same reason.
                var price = new HFlowContainer();
                price.AddThemeConstantOverride("h_separation", Tokens.Space3);
                price.AddThemeConstantOverride("v_separation", 2);
                foreach (var (type, need) in def.Recipe)
                    price.AddChild(UiTheme.CountChip($"scrap_{type.ToString().ToLowerInvariant()}",
                        _view.PersonalScrapOf(type), UiTheme.Scrap(type), need));
                text.AddChild(price);
            }
            row.AddChild(text);

            string captured = def.Id;
            bool affordable = def.Recipe.All(kv => _view.PersonalScrapOf(kv.Key) >= kv.Value);
            if (!owned)
            {
                var buy = new KitButton("Buy", affordable ? KitButton.Tone.Primary : KitButton.Tone.Secondary, Tokens.ControlSm);
                buy.Disabled = !affordable;
                buy.Pressed += () =>
                {
                    _bought = true;
                    Submit?.Invoke(new Command.BuyWeapon(_view.LocalPlayerId, captured));
                    _weaponId = captured;
                };
                row.AddChild(buy);
            }
            else if (!equipped)
            {
                var equip = new KitButton("Equip", KitButton.Tone.Secondary, Tokens.ControlSm);
                equip.Pressed += () =>
                {
                    Submit?.Invoke(new Command.SelectWeapon(_view.LocalPlayerId, captured));
                    _weaponId = captured;
                };
                row.AddChild(equip);
            }
            else
            {
                row.AddChild(Kit.TagArcane("1"));
            }

            // Clicking anywhere on the card inspects that platform — underneath
            // the row, not over it, so Buy and Equip keep their own clicks.
            var hit = new Button { Flat = true, MouseFilter = MouseFilterEnum.Pass };
            hit.SetAnchorsAndOffsetsPreset(LayoutPreset.FullRect);
            hit.Pressed += () => { _weaponId = captured; Refresh(_view); };
            card.AddChild(hit);
            card.MoveChild(hit, 0);
            row.MouseFilter = MouseFilterEnum.Ignore;

            _platformRail.AddChild(card);
        }
    }

    /// <summary>Design's frame ends the rail with the hands: a faction's
    /// plates are part of the loadout you see in first person, and the model
    /// is real (hands_&lt;faction&gt;.glb), so it is shown rather than named.</summary>
    private void RebuildHands(PlayerView local)
    {
        foreach (var child in _handsBody.GetChildren()) child.QueueFree();
        string faction = local.FactionId.Length > 0 ? local.FactionId : "forge";
        var row = Kit.Row(Tokens.Space4);
        var well = Kit.SlotBox(false, Tokens.SlotLg);
        well.CustomMinimumSize = new Vector2(96, 64);
        string asset = AssetLibrary.Has($"hands_{faction}") ? $"hands_{faction}" : "hands_firstperson";
        if (AssetLibrary.TryInstantiate(asset) is { } hands)
            well.AddChild(new ModelWell(hands, ModelWell.Pose.Hands, new Vector2I(192, 128)));
        else
            well.AddChild(Kit.SlotIcon(UiTheme.Icon($"faction_{faction}", UiTheme.Faction(faction)), UiTheme.Faction(faction), 28));
        row.AddChild(well);
        var text = Kit.Col(2);
        text.SizeFlagsHorizontal = SizeFlags.ExpandFill;
        text.AddChild(Kit.Title($"{UiTheme.ShortLabel(faction).ToUpperInvariant()} PLATES", Tokens.SizeCaption));
        text.AddChild(Kit.TagBrass("equipped"));
        row.AddChild(text);
        _handsBody.AddChild(row);
    }

    // =====================================================================
    // Centre: the bench
    // =====================================================================

    /// <summary>Design's bench at design's size, centred, shrinking only when
    /// the panel cannot hold it — the composition is authored, so it should
    /// not stretch.</summary>
    private (Vector2 Origin, float Scale) BenchFrame()
    {
        var avail = _bench.Size;
        float scale = Mathf.Min(1f, Mathf.Min(avail.X / BenchW, avail.Y / BenchH));
        var size = new Vector2(BenchW, BenchH) * scale;
        return ((avail - size) * 0.5f, scale);
    }

    private void DrawBench()
    {
        var (origin, scale) = BenchFrame();
        if (scale <= 0f) return;
        var frame = new Rect2(origin + WeaponFrame.Position * scale, WeaponFrame.Size * scale);

        // The glow behind the gun: design's radial gradient, as concentric
        // ellipses fading out.
        var centre = frame.Position + new Vector2(frame.Size.X * 0.5f, frame.Size.Y * 0.62f);
        for (int i = 6; i >= 1; i--)
        {
            float t = i / 6f;
            var radius = new Vector2(frame.Size.X * 0.5f, frame.Size.Y * 0.5f) * t;
            _bench.DrawSetTransform(centre, 0f, new Vector2(1f, radius.Y / radius.X));
            _bench.DrawCircle(Vector2.Zero, radius.X, new Color(0.18f, 0.22f, 0.31f, 0.11f));
        }
        _bench.DrawSetTransform(Vector2.Zero);

        if (_weaponHost is null && Tokens.Display is { } font)
        {
            // No model for this platform yet: an outline where the gun would be,
            // so the slots still have something to orbit.
            var body = new Rect2(frame.Position + frame.Size * new Vector2(0.15f, 0.42f), frame.Size * new Vector2(0.70f, 0.18f));
            _bench.DrawRect(body, Tokens.BorderStrong, filled: false, width: 2f);
            _bench.DrawRect(new Rect2(body.Position + new Vector2(body.Size.X * 0.30f, body.Size.Y), new Vector2(body.Size.X * 0.14f, body.Size.Y * 1.1f)),
                Tokens.BorderStrong, filled: false, width: 2f);
            _bench.DrawString(font, body.Position + new Vector2(0f, -8f), $"{_weaponId.ToUpperInvariant()} · model not delivered",
                HorizontalAlignment.Left, -1, Tokens.SizeCaption, Tokens.TextMuted);
        }
    }

    /// <summary>The build you actually have, rendered in the bench. Keyed on
    /// the weapon and what is bolted to it, so fitting a barrel rebuilds it.</summary>
    private void RebuildWeaponModel(PlayerView local, Vector2 origin, float scale)
    {
        var fitted = local.AttachmentsFor(_weaponId);
        string key = _weaponId + "|" + string.Join(",", fitted.OrderBy(kv => kv.Key).Select(kv => $"{kv.Key}={kv.Value}"));
        if (GodotObject.IsInstanceValid(_weaponHost) && _weaponFor == key)
        {
            PlaceWeapon(origin, scale);
            return;
        }
        if (GodotObject.IsInstanceValid(_weaponHost)) _weaponHost!.QueueFree();
        _weaponHost = null;
        _weaponFor = key;

        var gun = WeaponAssembly.Build(_weaponId, fitted);
        if (gun is null) return;
        var host = new Control { MouseFilter = MouseFilterEnum.Ignore };
        host.AddChild(new ModelWell(gun, ModelWell.Pose.Weapon, new Vector2I(1200, 570)));
        _weaponHost = host;
        _bench.AddChild(host);
        _bench.MoveChild(host, 0);
        PlaceWeapon(origin, scale);
    }

    private void PlaceWeapon(Vector2 origin, float scale)
    {
        if (_weaponHost is null) return;
        _weaponHost.Position = origin + WeaponFrame.Position * scale;
        _weaponHost.Size = WeaponFrame.Size * scale;
    }

    private void LayoutBench()
    {
        foreach (var child in _bench.GetChildren().OfType<Control>().ToList())
        {
            if (child == _weaponHost) continue;
            _bench.RemoveChild(child);
            child.QueueFree();
        }
        if (_view.Local is not { } local) return;

        var (origin, scale) = BenchFrame();
        if (scale <= 0f) return;
        RebuildWeaponModel(local, origin, scale);

        var mounted = local.AttachmentsFor(_weaponId);
        foreach (var (slot, x, y) in BenchLayout)
        {
            mounted.TryGetValue(slot, out string? fitted);
            var card = SlotCard(slot, fitted, slot == _slot, scale);
            card.Position = origin + new Vector2(x, y) * scale;
            _bench.AddChild(card);
            // Positioned by hand inside a plain Control, which lays nothing
            // out: without a real rect the card draws and cannot be clicked.
            card.Size = new Vector2(CardW, CardH) * scale;
        }

        // Caption under the gun: which file, and what the build applies.
        _benchCaption = Kit.Row(Tokens.Space4);
        _benchCaption.Position = origin + new Vector2(200f, BenchH - 14f - 24f) * scale;
        string asset = AssetLibrary.Has($"weapon_{_weaponId}_world") ? $"weapon_{_weaponId}_world.glb" : $"weapon_{_weaponId}_vm.glb";
        _benchCaption.AddChild(Kit.Label(_weaponHost is null ? "no model delivered" : $"{asset} · live build", Tokens.TextMuted));
        if (Weapons.All.TryGetValue(_weaponId, out var weapon))
        {
            var applies = weapon.Applies.Concat(mounted.Values
                .Select(id => Attachments.All.TryGetValue(id, out var a) ? a.Applies : null)
                .Where(s => s is not null)!).Distinct().ToList();
            if (mounted.ContainsKey(AttachmentSlot.Barrel)) _benchCaption.AddChild(Kit.TagArcane("fitted barrel"));
            foreach (string status in applies) _benchCaption.AddChild(Kit.Tag(status, UiTheme.Status(status)));
        }
        _bench.AddChild(_benchCaption);
        _benchCaption.ResetSize();
    }

    /// <summary>A slot card: the module rendered in its well (design's
    /// data-attachment canvas), the slot name, the module name, and the file.
    /// Empty slots wear hazard tape and say so.</summary>
    private Control SlotCard(AttachmentSlot slot, string? fitted, bool selected, float scale)
    {
        bool active = !string.IsNullOrEmpty(fitted);
        var card = active
            ? Kit.Card(selected)
            : Kit.Surface(Tokens.SurfaceSlot, selected ? Tokens.Brass500 : Tokens.BorderPanel, Tokens.ChamferSm,
                shadow: false, glow: selected ? Tokens.GlowBrass : null);
        if (card.GetThemeStylebox("panel") is ChamferBox box)
        {
            box.ContentMarginLeft = box.ContentMarginRight = Mathf.RoundToInt(8 * scale);
            box.ContentMarginTop = box.ContentMarginBottom = Mathf.RoundToInt(8 * scale);
            box.Hazard = !active;
        }

        var column = Kit.Col(Mathf.RoundToInt(3 * scale));
        column.Alignment = BoxContainer.AlignmentMode.Begin;
        card.AddChild(column);

        var well = Kit.Surface(Tokens.SurfaceSlot, Tokens.BorderPanel, Tokens.ChamferSm, shadow: false);
        well.CustomMinimumSize = new Vector2(WellW, WellH) * scale;
        well.SizeFlagsHorizontal = SizeFlags.ShrinkCenter;
        if (well.GetThemeStylebox("panel") is ChamferBox wellBox)
            wellBox.ContentMarginLeft = wellBox.ContentMarginRight = wellBox.ContentMarginTop = wellBox.ContentMarginBottom = 0;
        var wellFrame = new Control { CustomMinimumSize = new Vector2(WellW, WellH) * scale, MouseFilter = MouseFilterEnum.Ignore };
        well.AddChild(wellFrame);
        if (active && AssetLibrary.TryInstantiate($"attach_{fitted}") is { } module)
            wellFrame.AddChild(new ModelWell(module, ModelWell.Pose.Attachment, new Vector2I(248, 192)));
        else if (active)
            wellFrame.AddChild(Centered(Kit.SlotIcon(UiTheme.Icon($"attach_{fitted}", Tokens.TextArcane), Tokens.Arcane400, 34)));
        else
            wellFrame.AddChild(Centered(Kit.Label("empty", Tokens.TextDisabled)));
        column.AddChild(well);

        var slotLabel = Kit.Label(slot.ToString().ToLowerInvariant(), Tokens.TextSecondary);
        slotLabel.HorizontalAlignment = HorizontalAlignment.Center;
        column.AddChild(slotLabel);
        var name = Kit.Body(active ? UiTheme.ShortLabel(fitted!) : "empty", Tokens.SizeMicro,
            active ? Tokens.TextArcane : Tokens.TextDisabled);
        name.HorizontalAlignment = HorizontalAlignment.Center;
        column.AddChild(name);
        if (active)
        {
            var file = Kit.Body($"attach_{fitted!.ToLowerInvariant()}.glb", 9, Tokens.TextMuted);
            file.HorizontalAlignment = HorizontalAlignment.Center;
            column.AddChild(file);
        }

        var pick = new Button { Flat = true };
        pick.SetAnchorsAndOffsetsPreset(LayoutPreset.FullRect);
        var captured = slot;
        pick.Pressed += () => { _slot = captured; Refresh(_view); };
        card.AddChild(pick);
        card.MoveChild(pick, 0);
        column.MouseFilter = MouseFilterEnum.Ignore;
        return card;
    }

    /// <summary>A label that yields rather than dictates: five ammo cards
    /// across a 1440-wide window cannot each demand their full text width, or
    /// the frame grows past the screen and the right column leaves with it.</summary>
    /// <summary>Total scrap a platform costs, for ordering the rail cheapest
    /// first — the same order the money price used to give.</summary>
    private static int ScrapPrice(WeaponDef def) => def.Recipe.Values.Sum();

    private static Label Clipped(Label label)
    {
        label.ClipText = true;
        label.TextOverrunBehavior = TextServer.OverrunBehavior.TrimEllipsis;
        label.SizeFlagsHorizontal = SizeFlags.ExpandFill;
        label.CustomMinimumSize = Vector2.Zero;
        return label;
    }

    private static Control Centered(Control inner)
    {
        var box = new CenterContainer { MouseFilter = MouseFilterEnum.Ignore };
        box.SetAnchorsAndOffsetsPreset(LayoutPreset.FullRect);
        box.AddChild(inner);
        return box;
    }

    // =====================================================================
    // Ammo rail
    // =====================================================================

    /// <summary>What a round does, in the words design's cards use.</summary>
    private static string AmmoDelta(AmmoDef def)
    {
        var parts = new List<string> { $"×{def.DamageFactor:0.##}" };
        if (def.IgnoresFlatArmor) parts.Add("no flat armor");
        if (def.UnarmoredBonusFactor > 1.001f) parts.Add($"×{def.UnarmoredBonusFactor:0.##} unarmored");
        if (def.Applies is { } status) parts.Add(status);
        return string.Join(" · ", parts);
    }

    private void RebuildAmmo(PlayerView local)
    {
        foreach (var child in _ammoRow.GetChildren()) child.QueueFree();

        string current = local.AmmoFor(_weaponId);
        _ammoHeader.Text = $"loaded: {UiTheme.ShortLabel(current)} · magazine reserve unlimited";

        foreach (var def in Ammo.All.Values)
        {
            bool crafted = local.CraftedAmmo.Contains(def.Id);
            bool loaded = def.Id == current;

            var card = Kit.Card(loaded);
            card.CustomMinimumSize = new Vector2(196, 0);
            card.ClipContents = true;
            if (card.GetThemeStylebox("panel") is ChamferBox box)
            {
                box.ContentMarginLeft = box.ContentMarginRight = 8;
                box.ContentMarginTop = box.ContentMarginBottom = 8;
            }
            var row = Kit.Row(Tokens.Space4);
            card.AddChild(row);

            // The round itself, standing tip-up, its hue on the base edge.
            var well = Kit.Surface(Tokens.SurfaceSlot, Tokens.BorderPanel, 2f, shadow: false);
            well.CustomMinimumSize = new Vector2(52, 104);
            if (well.GetThemeStylebox("panel") is ChamferBox wellBox)
            {
                wellBox.ContentMarginLeft = wellBox.ContentMarginRight = wellBox.ContentMarginTop = 0;
                wellBox.ContentMarginBottom = 3;
                wellBox.Fill = Tokens.SurfaceSlot;
                wellBox.Stroke = loaded ? Tokens.Brass500 : Tokens.BorderPanel;
            }
            var wellFrame = new Control { CustomMinimumSize = new Vector2(52, 101), MouseFilter = MouseFilterEnum.Ignore };
            well.AddChild(wellFrame);
            if (AssetLibrary.TryInstantiate($"ammo_{def.Id}") is { } round)
                wellFrame.AddChild(new ModelWell(round, ModelWell.Pose.Ammo, new Vector2I(120, 218)));
            else
                wellFrame.AddChild(Centered(Kit.SlotIcon(UiTheme.Icon($"ammo_{def.Id}", Tokens.TextSecondary), Tokens.TextSecondary, 24)));
            row.AddChild(well);

            var col = Kit.Col(Tokens.Space2);
            col.SizeFlagsHorizontal = SizeFlags.ExpandFill;
            row.AddChild(col);

            var head = Kit.Row(Tokens.Space2);
            var name = Clipped(Kit.Title(UiTheme.ShortLabel(def.Id).ToUpperInvariant(), Tokens.SizeCaption));
            head.AddChild(name);
            if (loaded) head.AddChild(Kit.TagBrass("loaded"));
            col.AddChild(head);
            col.AddChild(Clipped(Kit.Numeral(AmmoDelta(def), Tokens.SizeMicro, Tokens.TextPrimary)));

            var recipe = new HFlowContainer();
            recipe.AddThemeConstantOverride("h_separation", Tokens.Space3);
            recipe.AddThemeConstantOverride("v_separation", 2);
            recipe.AddChild(Kit.Label(crafted ? "crafted" : "recipe", Tokens.TextMuted));
            bool affordable = true;
            if (def.Recipe.Count == 0) recipe.AddChild(Kit.Body("—", Tokens.SizeCaption, Tokens.TextDisabled));
            foreach (var (type, need) in def.Recipe)
            {
                int have = _view.PersonalScrapOf(type);
                if (have < need) affordable = false;
                recipe.AddChild(UiTheme.CountChip($"scrap_{type.ToString().ToLowerInvariant()}", have, UiTheme.Scrap(type), crafted ? -1 : need));
            }
            col.AddChild(recipe);

            if (!loaded)
            {
                // One command does both: the sim crafts on first load and
                // charges the recipe, then it is yours for the match.
                var load = new KitButton(crafted ? "Load" : "Craft",
                    crafted ? KitButton.Tone.Secondary : affordable ? KitButton.Tone.Primary : KitButton.Tone.Secondary,
                    Tokens.ControlSm);
                load.Disabled = !crafted && !affordable;
                string captured = def.Id;
                load.Pressed += () =>
                {
                    Submit?.Invoke(new Command.SelectAmmo(_view.LocalPlayerId, _weaponId, captured));
                    _notice = "";
                };
                col.AddChild(load);
            }
            _ammoRow.AddChild(card);
        }
    }

    // =====================================================================
    // Right: what the build did, what the next module costs, the blueprint
    // =====================================================================

    private void RebuildStats(PlayerView local)
    {
        foreach (var child in _statPanel.Body.GetChildren()) child.QueueFree();
        if (!Weapons.All.TryGetValue(_weaponId, out var weapon)) return;

        var mounted = local.AttachmentsFor(_weaponId);
        // Pack a Punch first, because it is the largest multiplier on this
        // screen by a distance and a panel that showed only the attachments
        // would tell a player at level 3 that their gun was 10% better when it
        // is 95% better.
        int packLevel = local.PackLevelFor(_weaponId);
        float damage = Mathf.Pow(Balance.PackDamagePerLevel, packLevel);
        float rate = Mathf.Pow(Balance.PackRatePerLevel, packLevel);
        float range = 1f;
        var applies = new List<string>(weapon.Applies);
        foreach (var id in mounted.Values)
        {
            if (!Attachments.All.TryGetValue(id, out var attachment)) continue;
            damage *= attachment.DamageFactor;
            rate *= attachment.RateFactor;
            range *= attachment.RangeFactor;
            if (attachment.Applies is { } status) applies.Add(status);
        }
        string ammoId = local.AmmoFor(_weaponId);
        if (Ammo.All.TryGetValue(ammoId, out var ammo))
        {
            damage *= ammo.DamageFactor;
            if (ammo.Applies is { } status) applies.Add(status);
        }

        _statPanel.Body.AddChild(Kit.Label($"{UiTheme.ShortLabel(_weaponId)} · built", Tokens.TextSecondary));
        Delta("Damage", weapon.Damage, weapon.Damage * damage, damage);
        Delta("Rate", weapon.ShotsPerSecond, weapon.ShotsPerSecond * rate, rate);
        Delta("Range", weapon.RangeMeters, weapon.RangeMeters * range, range);

        _statPanel.Body.AddChild(Kit.Rule());
        var appliesRow = Kit.Row();
        appliesRow.AddChild(Kit.Label("applies"));
        var chips = Kit.Row(Tokens.Space3);
        chips.SizeFlagsHorizontal = SizeFlags.ExpandFill;
        chips.Alignment = BoxContainer.AlignmentMode.End;
        foreach (string status in applies.Distinct()) chips.AddChild(Kit.Tag(status, UiTheme.Status(status)));
        if (applies.Count == 0) chips.AddChild(Kit.Body("—", Tokens.SizeCaption, Tokens.TextDisabled));
        appliesRow.AddChild(chips);
        _statPanel.Body.AddChild(appliesRow);

        var loadedRow = Kit.Row();
        loadedRow.AddChild(Kit.Label("loaded"));
        var loaded = Kit.Row(Tokens.Space3);
        loaded.SizeFlagsHorizontal = SizeFlags.ExpandFill;
        loaded.Alignment = BoxContainer.AlignmentMode.End;
        loaded.AddChild(Kit.Icon($"ammo_{ammoId}", Tokens.TextAccent, 18));
        var loadedText = Clipped(Kit.Numeral(ammo is null ? ammoId : $"{UiTheme.ShortLabel(ammoId)} · {AmmoDelta(ammo)}",
            Tokens.SizeCaption, Tokens.TextSecondary));
        loadedText.HorizontalAlignment = HorizontalAlignment.Right;
        loaded.AddChild(loadedText);
        loadedRow.AddChild(loaded);
        _statPanel.Body.AddChild(loadedRow);

        void Delta(string name, float baseValue, float built, float factor)
        {
            var column = Kit.Col(Tokens.Space2);
            var header = Kit.Row();
            header.AddChild(Kit.Label(name));
            var readout = Clipped(Kit.Numeral($"{baseValue:0.##} → {built:0.##}  ×{factor:0.00}", Tokens.SizeCaption,
                factor > 1.001f ? Tokens.StateSuccess : factor < 0.999f ? Tokens.StateDanger : Tokens.TextSecondary));
            readout.HorizontalAlignment = HorizontalAlignment.Right;
            header.AddChild(readout);
            column.AddChild(header);
            // Half the bar is the base value (the brass tick), so a build that
            // helps grows right of centre and one that costs you shrinks left.
            column.AddChild(Kit.Bar(Mathf.Clamp(factor * 0.5f, 0f, 1f),
                factor >= 1f ? Tokens.StateSuccess : Tokens.StateDanger, 14f, segments: 2));
            _statPanel.Body.AddChild(column);
        }
    }

    private void RebuildRecipe(PlayerView local)
    {
        foreach (var child in _optionColumn.GetChildren()) child.QueueFree();
        _recipeTitle.Text = $"· {_slot.ToString().ToLowerInvariant()}";

        BuildPackCard(local);

        var mounted = local.AttachmentsFor(_weaponId);
        mounted.TryGetValue(_slot, out string? fitted);

        var candidates = Attachments.All.Values.Where(a => a.Slot == _slot).ToList();
        if (candidates.Count == 0)
        {
            _optionColumn.AddChild(Kit.Body("no modules for this slot yet", Tokens.SizeCaption, Tokens.TextDisabled));
            return;
        }

        foreach (var attachment in candidates)
        {
            bool active = attachment.Id == fitted;
            var card = Kit.Card(active);
            var column = Kit.Col(Tokens.Space3);
            card.AddChild(column);

            var head = Kit.Row(Tokens.Space3);
            head.AddChild(Kit.Icon($"attach_{attachment.Id}", active ? Tokens.TextArcane : Tokens.TextSecondary, 20));
            var name = Kit.Title(UiTheme.ShortLabel(attachment.Id).ToUpperInvariant(), Tokens.SizeCaption);
            name.SizeFlagsHorizontal = SizeFlags.ExpandFill;
            head.AddChild(name);
            var factors = Clipped(Kit.Numeral(
                $"×{attachment.DamageFactor:0.00}d ×{attachment.RateFactor:0.00}r ×{attachment.RangeFactor:0.00}g",
                Tokens.SizeMicro, Tokens.TextMuted));
            factors.HorizontalAlignment = HorizontalAlignment.Right;
            head.AddChild(factors);
            column.AddChild(head);

            // Have/need per scrap type: the player needs to know which enemies
            // to go farm, not just that they are short.
            var recipe = Kit.Row(Tokens.Space4);
            bool affordable = true;
            foreach (var (type, need) in attachment.Recipe)
            {
                int have = _view.PersonalScrapOf(type);
                if (have < need) affordable = false;
                recipe.AddChild(UiTheme.CountChip($"scrap_{type.ToString().ToLowerInvariant()}", have, UiTheme.Scrap(type), need));
            }
            if (attachment.Applies is { } status) recipe.AddChild(Kit.Tag(status, UiTheme.Status(status)));
            recipe.AddChild(Kit.Spacer());

            if (!active)
            {
                var craft = new KitButton("Craft", affordable ? KitButton.Tone.Primary : KitButton.Tone.Secondary, Tokens.ControlSm);
                craft.Disabled = !affordable;
                string capturedId = attachment.Id;
                craft.Pressed += () =>
                {
                    Submit?.Invoke(new Command.CraftAttachment(_view.LocalPlayerId, _weaponId, capturedId));
                    _notice = "";
                };
                recipe.AddChild(craft);
            }
            else
            {
                recipe.AddChild(Kit.TagArcane("fitted"));
            }
            column.AddChild(recipe);
            _optionColumn.AddChild(card);
        }
    }

    /// <summary>Pack a Punch, at the head of every slot's list because it is
    /// not a slot — it is the one thing on this screen you can always buy
    /// again. Attachments are a build you finish; this never closes.</summary>
    private void BuildPackCard(PlayerView local)
    {
        int level = local.PackLevelFor(_weaponId);
        int cost = WeaponBuild.PackCostAt(level + 1);
        int gravium = WeaponBuild.PackGraviumAt(level + 1);
        int have = _view.PersonalScrapOf(ScrapType.Alloy);
        int haveGravium = _view.PersonalScrapOf(ScrapType.Gravium);
        bool affordable = have >= cost && haveGravium >= gravium;

        var card = Kit.Card(level > 0);
        var column = Kit.Col(Tokens.Space3);
        card.AddChild(column);

        var head = Kit.Row(Tokens.Space4);
        head.AddChild(Kit.Icon("scrap_alloy", Tokens.TextArcane, 20));
        var name = Kit.Title("PACK A PUNCH", Tokens.SizeCaption);
        name.SizeFlagsHorizontal = SizeFlags.ExpandFill;
        head.AddChild(name);
        head.AddChild(Kit.Numeral(level > 0 ? $"LV {level}" : "—", Tokens.SizeCaption,
            level > 0 ? Tokens.TextArcane : Tokens.TextMuted));
        column.AddChild(head);

        // What it is worth now and what the next one adds, because an uncapped
        // track with no readout is a button you press hopefully.
        column.AddChild(Kit.Body(
            level > 0
                ? $"×{Mathf.Pow(Balance.PackDamagePerLevel, level):0.00} damage, "
                  + $"×{Mathf.Pow(Balance.PackRatePerLevel, level):0.00} fire rate  →  "
                  + $"×{Mathf.Pow(Balance.PackDamagePerLevel, level + 1):0.00} / "
                  + $"×{Mathf.Pow(Balance.PackRatePerLevel, level + 1):0.00}"
                : $"×{Balance.PackDamagePerLevel:0.00} damage and "
                  + $"×{Balance.PackRatePerLevel:0.00} fire rate, every level, forever",
            Tokens.SizeMicro, Tokens.TextMuted));

        // Where the next milestone is, so a player saving for level 5 knows the
        // Gravium is coming before they are two levels away and short.
        int toMilestone = Balance.PackGraviumEvery - (level % Balance.PackGraviumEvery);
        column.AddChild(Kit.Body(
            gravium > 0
                ? $"milestone level — costs {gravium} gravium as well"
                : $"gravium again in {toMilestone} level{(toMilestone == 1 ? "" : "s")}",
            Tokens.SizeMicro, gravium > 0 ? Tokens.TextArcane : Tokens.TextDisabled));

        var row = Kit.Row(Tokens.Space4);
        row.AddChild(UiTheme.CountChip("scrap_alloy", have, UiTheme.Scrap(ScrapType.Alloy), cost));
        if (gravium > 0)
            row.AddChild(UiTheme.CountChip("scrap_gravium", haveGravium,
                UiTheme.Scrap(ScrapType.Gravium), gravium));
        row.AddChild(Kit.Spacer());
        var buy = new KitButton($"Pack  {cost}",
            affordable ? KitButton.Tone.Primary : KitButton.Tone.Secondary, Tokens.ControlSm);
        buy.Disabled = !affordable;
        buy.Pressed += () =>
        {
            Submit?.Invoke(new Command.PackAPunch(_view.LocalPlayerId, _weaponId));
            _notice = "";
        };
        row.AddChild(buy);
        column.AddChild(row);
        _optionColumn.AddChild(card);
    }

    /// <summary>What recrafting a saved build would cost from here: the recipes
    /// of every saved module not already fitted, plus the saved ammo if it is
    /// not crafted. Nothing for what you already have.</summary>
    private Dictionary<ScrapType, int> RecraftCost(string weaponId, PlayerView local)
    {
        var cost = new Dictionary<ScrapType, int>();
        var fitted = local.AttachmentsFor(weaponId);
        if (BlueprintSlots?.Invoke(weaponId) is { } slots)
            foreach (var attachmentId in slots.Values)
            {
                if (fitted.Values.Contains(attachmentId)) continue;
                if (!Attachments.All.TryGetValue(attachmentId, out var def)) continue;
                foreach (var (type, need) in def.Recipe) cost[type] = cost.GetValueOrDefault(type) + need;
            }
        if (BlueprintAmmo?.Invoke(weaponId) is { } ammoId && !local.CraftedAmmo.Contains(ammoId)
            && Ammo.All.TryGetValue(ammoId, out var ammo))
            foreach (var (type, need) in ammo.Recipe) cost[type] = cost.GetValueOrDefault(type) + need;
        return cost;
    }

    private void RebuildBlueprint(PlayerView local)
    {
        foreach (var child in _blueprintPanel.Body.GetChildren()) child.QueueFree();

        int fitted = local.AttachmentsFor(_weaponId).Count;
        bool saved = HasBlueprint?.Invoke(_weaponId) ?? false;
        var savedSlots = BlueprintSlots?.Invoke(_weaponId);
        var cost = RecraftCost(_weaponId, local);
        bool affordable = cost.All(kv => _view.PersonalScrapOf(kv.Key) >= kv.Value);

        var row = Kit.Row(Tokens.Space5);
        var column = Kit.Col(2);
        column.SizeFlagsHorizontal = SizeFlags.ExpandFill;
        column.AddChild(Clipped(Kit.Title(saved ? $"{UiTheme.ShortLabel(_weaponId).ToUpperInvariant()} BUILD" : "NO BUILD SAVED", Tokens.SizeBody)));
        column.AddChild(Clipped(Kit.Label(saved
            ? $"{UiTheme.ShortLabel(_weaponId)} · {savedSlots?.Count ?? 0} modules saved · {fitted} fitted"
            : $"{UiTheme.ShortLabel(_weaponId)} · {fitted} modules fitted")));
        row.AddChild(column);

        var actions = Kit.Col(Tokens.Space3);
        var save = new KitButton("Save current", KitButton.Tone.Secondary, Tokens.ControlSm);
        save.Disabled = fitted == 0 && local.AmmoFor(_weaponId) == Ammo.Standard.Id;
        save.Pressed += () => { SaveBlueprint?.Invoke(_weaponId); _notice = "blueprint saved"; };
        actions.AddChild(save);
        var recraft = new KitButton(cost.Count == 0 ? "Recraft" : "Recraft · " + string.Join(" ", cost.Select(kv => $"{kv.Value} {kv.Key.ToString().ToLowerInvariant()}")),
            affordable ? KitButton.Tone.Arcane : KitButton.Tone.Secondary, Tokens.ControlSm);
        recraft.Disabled = !saved || !affordable;
        recraft.Pressed += () => { RecraftBlueprint?.Invoke(_weaponId); _notice = "recrafting…"; };
        actions.AddChild(recraft);
        row.AddChild(actions);
        _blueprintPanel.Body.AddChild(row);
    }

    // =====================================================================
    // Blueprints tab
    // =====================================================================

    /// <summary>One card per saved build — the profile keeps one per
    /// platform — with a live render of exactly that build, its module icons,
    /// ammo, and what recrafting costs against the scrap held now. The rest of
    /// the six cells are empty slots that save the current build.</summary>
    private void RebuildBlueprintGrid(PlayerView local)
    {
        foreach (var child in _blueprintGrid.GetChildren()) child.QueueFree();

        int cells = 0;
        foreach (var weapon in Weapons.All.Values.OrderBy(ScrapPrice).ThenBy(w => w.Id, System.StringComparer.Ordinal))
        {
            if (BlueprintSlots?.Invoke(weapon.Id) is not { } saved || saved.Count == 0) continue;
            _blueprintGrid.AddChild(BlueprintCard(weapon, saved, BlueprintAmmo?.Invoke(weapon.Id) ?? Ammo.Standard.Id, local));
            cells++;
        }
        for (; cells < 6; cells++)
        {
            var empty = Kit.Surface(Tokens.SurfaceSlot, Tokens.BorderPanel, Tokens.ChamferSm, shadow: false);
            if (empty.GetThemeStylebox("panel") is ChamferBox box) box.Hazard = true;
            empty.SizeFlagsHorizontal = SizeFlags.ExpandFill;
            empty.SizeFlagsVertical = SizeFlags.ExpandFill;
            var col = Kit.Col(Tokens.Space4);
            col.Alignment = BoxContainer.AlignmentMode.Center;
            col.AddChild(Centered(Kit.Label("empty slot", Tokens.TextDisabled)));
            var save = new KitButton("Save current build", KitButton.Tone.Secondary, Tokens.ControlSm);
            save.SizeFlagsHorizontal = SizeFlags.ShrinkCenter;
            save.Pressed += () => { SaveBlueprint?.Invoke(local.WeaponId); _notice = "blueprint saved"; };
            col.AddChild(save);
            var centred = new CenterContainer();
            centred.SetAnchorsAndOffsetsPreset(LayoutPreset.FullRect);
            centred.AddChild(col);
            empty.AddChild(centred);
            _blueprintGrid.AddChild(empty);
        }
    }

    private Control BlueprintCard(WeaponDef weapon, IReadOnlyDictionary<string, string> saved, string ammoId, PlayerView local)
    {
        bool equipped = weapon.Id == local.WeaponId;
        var fitted = local.AttachmentsFor(weapon.Id);
        bool isCurrent = equipped && saved.All(kv => fitted.Values.Contains(kv.Value)) && local.AmmoFor(weapon.Id) == ammoId;
        var cost = RecraftCost(weapon.Id, local);
        bool affordable = cost.All(kv => _view.PersonalScrapOf(kv.Key) >= kv.Value);

        var panel = new KitPanel($"{UiTheme.ShortLabel(weapon.Id)} build", equipped ? Tokens.Brass500 : null);
        panel.SizeFlagsHorizontal = SizeFlags.ExpandFill;
        panel.SizeFlagsVertical = SizeFlags.ExpandFill;
        panel.HeaderTrailing(equipped ? Kit.TagBrass("equipped") : Kit.Tag("saved"));

        var body = Kit.Row(Tokens.Space5);
        body.SizeFlagsVertical = SizeFlags.ExpandFill;
        panel.Body.AddChild(body);

        // The build, live.
        var slots = new Dictionary<AttachmentSlot, string>();
        foreach (var (slotName, attachmentId) in saved)
            if (System.Enum.TryParse<AttachmentSlot>(slotName, true, out var slot)) slots[slot] = attachmentId;
        var stage = new Control { SizeFlagsHorizontal = SizeFlags.ExpandFill, SizeFlagsVertical = SizeFlags.ExpandFill, CustomMinimumSize = new Vector2(220, 150), MouseFilter = MouseFilterEnum.Ignore };
        if (WeaponAssembly.Build(weapon.Id, slots) is { } gun)
            stage.AddChild(new ModelWell(gun, ModelWell.Pose.Weapon, new Vector2I(640, 360)));
        else
            stage.AddChild(Centered(Kit.Icon($"weapon_{weapon.Id}", Tokens.TextSecondary, 48)));
        var file = Kit.Body($"weapon_{weapon.Id.ToLowerInvariant()}_world", 9, Tokens.TextMuted);
        file.Position = new Vector2(8, 6);
        stage.AddChild(file);
        body.AddChild(stage);

        var side = Kit.Col(Tokens.Space4);
        side.CustomMinimumSize = new Vector2(180, 0);
        body.AddChild(side);
        var icons = new HFlowContainer();
        icons.AddThemeConstantOverride("h_separation", Tokens.Space2);
        foreach (var attachmentId in saved.Values)
            icons.AddChild(Kit.Icon($"attach_{attachmentId}", Tokens.TextSecondary, 20));
        side.AddChild(icons);
        var ammoRow = Kit.Row();
        ammoRow.AddChild(Kit.Label("ammo"));
        var ammo = Kit.Row(Tokens.Space3);
        ammo.SizeFlagsHorizontal = SizeFlags.ExpandFill;
        ammo.Alignment = BoxContainer.AlignmentMode.End;
        ammo.AddChild(Kit.Icon($"ammo_{ammoId}", Tokens.TextSecondary, 16));
        ammo.AddChild(Kit.Numeral(UiTheme.ShortLabel(ammoId), Tokens.SizeMicro, Tokens.TextSecondary));
        ammoRow.AddChild(ammo);
        side.AddChild(ammoRow);
        side.AddChild(Kit.Rule());
        side.AddChild(Kit.Label("recraft"));
        if (cost.Count == 0) side.AddChild(Kit.Body("nothing to craft", Tokens.SizeCaption, Tokens.TextDisabled));
        foreach (var (type, need) in cost)
        {
            int have = _view.PersonalScrapOf(type);
            var line = Kit.Row(Tokens.Space3);
            line.AddChild(Kit.Icon($"scrap_{type.ToString().ToLowerInvariant()}", UiTheme.Scrap(type), 16));
            var needLabel = Kit.Numeral(need.ToString(), Tokens.SizeCaption, have >= need ? Tokens.TextPrimary : Tokens.StateDanger);
            needLabel.SizeFlagsHorizontal = SizeFlags.ExpandFill;
            line.AddChild(needLabel);
            line.AddChild(Kit.Numeral($"/ {have}", Tokens.SizeMicro, Tokens.TextMuted));
            side.AddChild(line);
        }

        var footer = Kit.Row(Tokens.Space4);
        var missing = cost.Where(kv => _view.PersonalScrapOf(kv.Key) < kv.Value)
            .Select(kv => $"{kv.Value - _view.PersonalScrapOf(kv.Key)} {kv.Key.ToString().ToLowerInvariant()}").ToList();
        var status = Kit.Label(isCurrent ? "current build" : affordable ? "affordable" : "missing " + string.Join(", ", missing),
            isCurrent ? Tokens.TextAccent : affordable ? Tokens.StateSuccess : Tokens.StateDanger);
        status.SizeFlagsHorizontal = SizeFlags.ExpandFill;
        footer.AddChild(status);
        if (isCurrent)
        {
            var save = new KitButton("Save current", KitButton.Tone.Secondary, Tokens.ControlSm);
            save.Pressed += () => { SaveBlueprint?.Invoke(weapon.Id); _notice = "blueprint saved"; };
            footer.AddChild(save);
        }
        else
        {
            var recraft = new KitButton("Recraft", affordable ? KitButton.Tone.Arcane : KitButton.Tone.Secondary, Tokens.ControlSm);
            recraft.Disabled = !affordable;
            string captured = weapon.Id;
            recraft.Pressed += () =>
            {
                if (!local.OwnedWeapons.Contains(captured)) { _notice = "buy the platform first"; return; }
                RecraftBlueprint?.Invoke(captured);
                _notice = "recrafting…";
            };
            footer.AddChild(recraft);
        }
        panel.SetFooter(footer);
        return panel;
    }
}
