using Godot;
using System.Collections.Generic;
using System.Linq;
using DeepField.Sim;
using DeepField.Sim.Content;

namespace DeepField.Game.Ui;

/// <summary>The armory and gunsmith, laid out to design's frame.
///
/// Three columns, because that is what the job actually is: pick a platform on
/// the left, mount modules on it in the middle, read what you did to it on the
/// right. The previous version was four scrolling lists of text, which made
/// building a weapon a reading exercise rather than a spatial one.
///
/// The middle column is the load-bearing idea — seven slots arranged around
/// the gun in the positions the parts physically occupy, so "barrel" is at the
/// front and "stock" is at the back. Every action maps to a command the sim
/// already accepts; refusals come back through CraftRejected and land in the
/// notice line rather than a toast the player has looked away from.</summary>
public partial class ArmoryScreen : Control
{
    public System.Action<Command>? Submit;
    public System.Action<string>? RecraftBlueprint;
    public System.Func<string, bool>? HasBlueprint;

    private GameView _view = new();
    private string _weaponId = "sidearm";
    private AttachmentSlot _slot = AttachmentSlot.Barrel;
    private string _notice = "";

    private VBoxContainer _platformRail = null!;
    private Control _bench = null!;
    private bool _bought;
    private SubViewportContainer? _modelHost;
    private string _modelFor = "";
    private string _signature = "";

    /// <summary>Everything this screen renders, flattened. Cheap to build and
    /// compared before touching a single node.</summary>
    private string Signature(GameView view, PlayerView local)
    {
        var sb = new System.Text.StringBuilder();
        sb.Append(view.Money).Append('|').Append(local.WeaponId).Append('|')
          .Append(_weaponId).Append('|').Append(_slot).Append('|').Append(_notice).Append('|');
        foreach (string owned in local.OwnedWeapons.OrderBy(x => x, System.StringComparer.Ordinal))
            sb.Append(owned).Append(',');
        sb.Append('|');
        foreach (var (slot, id) in local.AttachmentsFor(_weaponId).OrderBy(kv => kv.Key))
            sb.Append(slot).Append('=').Append(id).Append(',');
        sb.Append('|').Append(local.AmmoFor(_weaponId)).Append('|');
        foreach (ScrapType type in System.Enum.GetValues<ScrapType>())
            sb.Append(view.PersonalScrapOf(type)).Append(',');
        return sb.ToString();
    }
    private HFlowContainer _ammoRow = null!;
    private KitPanel _statPanel = null!;
    private KitPanel _recipePanel = null!;
    private KitPanel _blueprintPanel = null!;
    private VBoxContainer _optionColumn = null!;
    private Label _noticeLabel = null!;
    private Label _economyLabel = null!;

    public bool IsOpen { get; private set; }

    /// <summary>Where each slot sits around the gun. These are design's
    /// coordinates on the 900×640 bench, scaled at runtime.</summary>
    private static readonly (AttachmentSlot Slot, float X, float Y)[] BenchLayout =
    {
        (AttachmentSlot.Barrel, 0.18f, 0.10f),
        (AttachmentSlot.Muzzle, 0.02f, 0.34f),
        (AttachmentSlot.Optic, 0.46f, 0.10f),
        (AttachmentSlot.Magazine, 0.60f, 0.70f),
        (AttachmentSlot.Stock, 0.84f, 0.30f),
        (AttachmentSlot.Underbarrel, 0.34f, 0.70f),
        (AttachmentSlot.Infusion, 0.84f, 0.70f),
    };

    public override void _Ready()
    {
        SetAnchorsAndOffsetsPreset(LayoutPreset.FullRect);
        Visible = false;

        // A full-rect stack rather than anchored siblings: anchor presets on
        // sibling Controls left the header sized to its content and the
        // backdrop not covering, so the live world showed between panels.
        var backdrop = new ColorRect
        {
            Color = Tokens.Obsidian900,
            MouseFilter = MouseFilterEnum.Stop,
        };
        backdrop.SetAnchorsAndOffsetsPreset(LayoutPreset.FullRect);
        AddChild(backdrop);
        AddChild(new KitGrid());

        var frame = new VBoxContainer();
        frame.SetAnchorsAndOffsetsPreset(LayoutPreset.FullRect);
        frame.AddThemeConstantOverride("separation", 0);
        AddChild(frame);

        // --- header
        var (bar, headerRow) = Kit.ScreenHeader("Armory");
        bar.SizeFlagsHorizontal = SizeFlags.ExpandFill;
        frame.AddChild(bar);
        headerRow.AddChild(Kit.TagBrass("gunsmith"));
        headerRow.AddChild(Kit.Spacer());
        _economyLabel = Kit.Numeral("", Tokens.SizeStat, Tokens.TextAccent);
        headerRow.AddChild(_economyLabel);
        var close = new KitButton("Close  ESC", KitButton.Tone.Secondary);
        close.Pressed += Close;
        headerRow.AddChild(close);

        // --- three columns
        var margin = new MarginContainer { SizeFlagsVertical = SizeFlags.ExpandFill };
        foreach (string side in new[] { "left", "right", "top", "bottom" })
            margin.AddThemeConstantOverride($"margin_{side}", Tokens.Space8);
        frame.AddChild(margin);

        var columns = Kit.Row(Tokens.Space6);
        margin.AddChild(columns);

        var railPanel = new KitPanel("Platforms");
        railPanel.CustomMinimumSize = new Vector2(268, 0);
        _platformRail = railPanel.Body;
        columns.AddChild(railPanel);

        var benchPanel = new KitPanel("Build", Tokens.Arcane500);
        benchPanel.SizeFlagsHorizontal = SizeFlags.ExpandFill;
        benchPanel.SizeFlagsStretchRatio = 1.6f;
        benchPanel.CustomMinimumSize = new Vector2(360, 0);
        columns.AddChild(benchPanel);

        _bench = new Control { SizeFlagsVertical = SizeFlags.ExpandFill };
        _bench.CustomMinimumSize = new Vector2(0, 420);
        _bench.Draw += DrawBench;
        _bench.Resized += () => { _bench.QueueRedraw(); LayoutBench(); };
        benchPanel.Body.AddChild(_bench);

        benchPanel.Body.AddChild(Kit.Rule());
        benchPanel.Body.AddChild(Kit.Label("ammo"));
        _ammoRow = new HFlowContainer();
        _ammoRow.AddThemeConstantOverride("h_separation", Tokens.Space3);
        _ammoRow.AddThemeConstantOverride("v_separation", Tokens.Space3);
        benchPanel.Body.AddChild(_ammoRow);

        _noticeLabel = Kit.Body("", Tokens.SizeCaption, UiTheme.Danger);
        benchPanel.Body.AddChild(_noticeLabel);

        var right = Kit.Col(Tokens.Space5);
        right.CustomMinimumSize = new Vector2(360, 0);
        right.SizeFlagsHorizontal = SizeFlags.ExpandFill;
        columns.AddChild(right);

        _statPanel = new KitPanel("Built", Tokens.Arcane500);
        right.AddChild(_statPanel);

        _recipePanel = new KitPanel("Recipe", Tokens.Brass500, hazard: true);
        right.AddChild(_recipePanel);

        _optionColumn = Kit.Col(Tokens.Space3);
        _recipePanel.Body.AddChild(_optionColumn);

        _blueprintPanel = new KitPanel("Blueprint");
        right.AddChild(_blueprintPanel);
    }

    // =====================================================================

    /// <summary>Clicks a slot the way a player does — a real mouse event
    /// pushed through the viewport, so it goes through hit-testing rather than
    /// around it — and reports whether the selection actually moved.
    ///
    /// This exists because every attachment slot on this bench was inert for
    /// weeks while looking perfectly correct: positioned by hand inside a plain
    /// Control, they had no size, and Godot draws children outside a zero-sized
    /// parent while giving them no hit area. Screenshots could not see it and
    /// neither could I. A rendered surface proves it draws; only a click proves
    /// it works.</summary>
    public bool ProbeClick()
    {
        _bought = false;
        // The infusion slot, bottom-right of the bench.
        Button? target = null;
        void Find(Node n)
        {
            // The Buy button on the first unowned platform: the control a
            // player reaches for first, and the one that was unreachable.
            if (n is Button b && b.Text == "BUY" && !b.Disabled) target ??= b;
            foreach (var c in n.GetChildren()) Find(c);
        }
        Find(this);
        if (target is null) { GD.PrintErr("ERROR: probe found no enabled Buy button"); return false; }

        var centre = target.GetGlobalRect().GetCenter();
        foreach (bool down in new[] { true, false })
        {
            var ev = new InputEventMouseButton
            {
                ButtonIndex = MouseButton.Left,
                Pressed = down,
                Position = centre,
                GlobalPosition = centre,
            };
            GetViewport().PushInput(ev);
        }
        bool bought = _bought;
        if (bought) GD.Print($"[probe] gunsmith Buy click works: reached the handler");
        else GD.PrintErr($"ERROR: gunsmith Buy at {centre} is inert — the click never reached it");
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

        _economyLabel.Text = view.Money.ToString();
        _noticeLabel.Text = _notice;

        // Rebuild only when something it draws has actually changed.
        //
        // This ran every frame, and rebuilding means QueueFree on every button
        // and a fresh one in its place. A Godot Button only emits Pressed if the
        // same instance saw both the press and the release, so any click that
        // spanned a frame boundary — which is nearly all of them — landed on a
        // node that no longer existed. The screen was not missing handlers or
        // hit areas; it was being destroyed underneath the cursor sixty times a
        // second.
        //
        // It also explains why a synthesised click passed while a real one
        // failed: PushInput delivers down and up inside one frame, so the probe
        // was the only clicker fast enough to beat the rebuild.
        _bench.QueueRedraw();   // cheap; the rebuild below is what we gate
        string signature = Signature(view, local);
        if (signature == _signature) return;
        _signature = signature;

        RebuildPlatforms(local);
        LayoutBench();
        RebuildAmmo(local);
        RebuildStats(local);
        RebuildRecipe(local);
        RebuildBlueprint(local);
        _bench.QueueRedraw();
    }

    // =====================================================================
    // Left: platform rail
    // =====================================================================

    private void RebuildPlatforms(PlayerView local)
    {
        foreach (var child in _platformRail.GetChildren()) child.QueueFree();

        foreach (var def in Weapons.All.Values)
        {
            bool owned = local.OwnedWeapons.Contains(def.Id);
            bool equipped = def.Id == local.WeaponId;
            bool selected = def.Id == _weaponId;

            var card = Kit.Card(selected);
            var row = Kit.Row(Tokens.Space5);
            card.AddChild(row);

            row.AddChild(Kit.Icon($"weapon_{def.Id}",
                selected ? Tokens.TextAccent : Tokens.TextSecondary, 26));

            var text = Kit.Col(2);
            text.SizeFlagsHorizontal = SizeFlags.ExpandFill;
            text.AddChild(Kit.Title(def.Id.ToUpperInvariant(), Tokens.SizeBody));
            text.AddChild(Kit.Label(equipped ? "equipped" : owned ? "owned" : $"{def.Cost} credits",
                equipped ? Tokens.TextArcane : Tokens.TextMuted));
            row.AddChild(text);

            string captured = def.Id;
            if (!owned)
            {
                var buy = new KitButton("Buy", KitButton.Tone.Primary, Tokens.ControlSm);
                buy.Disabled = _view.Money < def.Cost;
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

            // Clicking anywhere on the card inspects that platform — but
            // underneath the row, not over it. Added last it covered the whole
            // card including Buy and Equip, and since input goes to the topmost
            // control and MouseFilter.Pass propagates to the parent rather than
            // to siblings beneath, those buttons never saw a click: pressing Buy
            // silently re-selected the platform instead of buying it.
            //
            // The row is set to Ignore so clicks on its labels and icon fall
            // through to this, while the Buttons inside it still take their own
            // — a control's filter governs itself, not its children.
            var hit = new Button { Flat = true, MouseFilter = MouseFilterEnum.Pass };
            hit.SetAnchorsAndOffsetsPreset(LayoutPreset.FullRect);
            hit.Pressed += () => { _weaponId = captured; Refresh(_view); };
            card.AddChild(hit);
            card.MoveChild(hit, 0);
            row.MouseFilter = MouseFilterEnum.Ignore;

            _platformRail.AddChild(card);
        }
    }

    // =====================================================================
    // Centre: the bench
    // =====================================================================

    /// <summary>The gun silhouette, drawn rather than modelled — design's frame
    /// shows the weapon's outline with the slots arranged around it, and a
    /// placeholder polygon reads the same at this size.</summary>
    /// <summary>Design's bench is 900x640 and the slot coordinates are
    /// fractions of it. Scaling that to whatever the window happens to be made
    /// the weapon a grey slab most of a metre wide at 1080p and pushed the
    /// slots to the far corners. The bench is now centred at design's size,
    /// shrinking only if the panel is too small to hold it — the composition is
    /// authored, so it should not stretch.</summary>
    private Rect2 BenchRect()
    {
        var avail = _bench.Size;
        float scale = Mathf.Min(1f, Mathf.Min(avail.X / 900f, avail.Y / 640f));
        var size = new Vector2(900f * scale, 640f * scale);
        return new Rect2((avail - size) * 0.5f, size);
    }

    /// <summary>World-space bounds of every mesh under a node, merged.</summary>
    private static Aabb MergedBounds(Node3D root)
    {
        Aabb? merged = null;
        foreach (var m in root.FindChildren("*", "MeshInstance3D", true, false).OfType<MeshInstance3D>())
        {
            var box = m.GlobalTransform * m.GetAabb();
            merged = merged is { } acc ? acc.Merge(box) : box;
        }
        return merged ?? new Aabb(Vector3.Zero, new Vector3(0.2f, 0.1f, 0.2f));
    }

    /// <summary>The weapon design actually shipped, rendered in the bench.
    ///
    /// The first attempts at this failed for a reason that had nothing to do
    /// with framing, which is where I kept looking: a SubViewport inherits its
    /// parent's World3D unless told otherwise, so the camera was rendering the
    /// Foundry. That is why it looked like the lens was buried inside geometry
    /// — it was, just not the gun's. The same code in the lobby looked correct
    /// only because no level is loaded there.
    ///
    /// Rebuilt on weapon change, not per refresh.</summary>
    private void RebuildWeaponModel()
    {
        if (GodotObject.IsInstanceValid(_modelHost) && _modelFor == _weaponId) return;
        if (GodotObject.IsInstanceValid(_modelHost)) _modelHost!.QueueFree();
        _modelHost = null;
        _modelFor = _weaponId;

        string asset = AssetLibrary.Has($"weapon_{_weaponId}_world")
            ? $"weapon_{_weaponId}_world" : $"weapon_{_weaponId}_vm";
        if (!AssetLibrary.Has(asset)) return;

        var viewport = new SubViewport
        {
            TransparentBg = true,
            RenderTargetUpdateMode = SubViewport.UpdateMode.Always,
            Size = new Vector2I(760, 320),
            OwnWorld3D = true,
        };
        var stage = new Node3D();
        viewport.AddChild(stage);

        // An own-world viewport has no environment, so ambient light is zero and
        // flat-shaded meshes come out black on a transparent background — which
        // reads as "nothing rendered" and sent me looking at the camera three
        // times. Preview viewports have to bring their own lighting.
        stage.AddChild(new WorldEnvironment
        {
            Environment = new Godot.Environment
            {
                BackgroundMode = Godot.Environment.BGMode.Canvas,
                AmbientLightSource = Godot.Environment.AmbientSource.Color,
                AmbientLightColor = new Color(0.62f, 0.68f, 0.82f),
                AmbientLightEnergy = 1.6f,
            },
        });

        var gun = AssetLibrary.Instantiate(asset, () => new Node3D());
        gun.RotationDegrees = new Vector3(10f, 208f, 0f);
        stage.AddChild(gun);

        _modelHost = new SubViewportContainer { Stretch = true, MouseFilter = MouseFilterEnum.Ignore };
        _modelHost.AddChild(viewport);
        _bench.AddChild(_modelHost);
        _bench.MoveChild(_modelHost, 0);

        // Anchored, not sized. The bench has no size yet when this runs — the
        // container was coming out 0x0 and the viewport 2x2, which is why the
        // model never appeared however the camera was placed. Absolute sizing
        // always lost that race and, now that Refresh only runs on change, there
        // was no later pass to correct it. Anchors just follow the parent.
        _modelHost.AnchorLeft = 0.10f;
        _modelHost.AnchorRight = 0.90f;
        _modelHost.AnchorTop = 0.30f;
        _modelHost.AnchorBottom = 0.72f;
        _modelHost.OffsetLeft = _modelHost.OffsetRight = 0f;
        _modelHost.OffsetTop = _modelHost.OffsetBottom = 0f;

        // Read after attaching: GlobalTransform on a detached node warns and
        // returns identity.
        var bounds = MergedBounds(gun);
        float reach = Mathf.Max(bounds.Size.Length(), 0.05f);
        const float fov = 34f;
        float back = reach / (2f * Mathf.Tan(Mathf.DegToRad(fov) * 0.5f)) * 1.25f;

        stage.AddChild(new Camera3D
        {
            Position = bounds.GetCenter() + new Vector3(0f, reach * 0.05f, back),
            RotationDegrees = new Vector3(-4f, 0f, 0f),
            Fov = fov,
            Current = true,
        });
        stage.AddChild(new DirectionalLight3D
        {
            RotationDegrees = new Vector3(-28f, 24f, 0f), LightEnergy = 1.7f,
        });
        stage.AddChild(new DirectionalLight3D
        {
            RotationDegrees = new Vector3(-6f, -50f, 0f), LightEnergy = 0.55f,
            LightColor = Tokens.Arcane400.Lerp(Colors.White, 0.5f),
        });
    }

    private void DrawBench()
    {

        var rect = BenchRect();
        var origin = rect.Position;
        var size = rect.Size;
        if (size.X < 10 || size.Y < 10) return;

        var body = new Rect2(origin + new Vector2(size.X * 0.16f, size.Y * 0.40f),
            new Vector2(size.X * 0.66f, size.Y * 0.20f));
        // Outline, not a fill. Design's frame puts the weapon's silhouette here
        // and this stands in for it until the viewmodels land — filled, at
        // design's own 900x640, it read as a grey slab across the middle of the
        // screen rather than as a guide for where the slots attach.

        // Barrel forward, grip below — enough shape to orient the slots.
        _bench.DrawRect(new Rect2(origin + new Vector2(size.X * 0.06f, size.Y * 0.45f),
            new Vector2(size.X * 0.12f, size.Y * 0.07f)), Tokens.BorderPanel, filled: false, width: 2f);
        _bench.DrawRect(new Rect2(origin + new Vector2(size.X * 0.34f, size.Y * 0.58f),
            new Vector2(size.X * 0.09f, size.Y * 0.16f)), Tokens.BorderPanel, filled: false, width: 2f);

        if (Tokens.Display is { } font)
            _bench.DrawString(font, origin + new Vector2(size.X * 0.16f, size.Y * 0.36f),
                _weaponId.ToUpperInvariant(), HorizontalAlignment.Left, -1,
                Tokens.SizeCaption, Tokens.TextMuted);
    }

    private void LayoutBench()
    {
        foreach (var child in _bench.GetChildren().OfType<Control>().ToList())
        {
            // The weapon view outlives a slot rebuild. Purging it took it out of
            // the tree while _modelFor still matched, so RebuildWeaponModel
            // skipped itself and the model was never in the scene to render —
            // which is the whole reason the bench looked empty no matter what
            // the camera, the lighting or the anchors were doing.
            if (child == _modelHost) continue;
            _bench.RemoveChild(child);
            child.QueueFree();
        }
        if (_view.Local is not { } local) return;

        RebuildWeaponModel();

        var mounted = local.AttachmentsFor(_weaponId);
        var rect = BenchRect();
        var origin = rect.Position;
        var size = rect.Size;
        if (size.X < 10) return;

        foreach (var (slot, fx, fy) in BenchLayout)
        {
            mounted.TryGetValue(slot, out string? fitted);
            bool active = !string.IsNullOrEmpty(fitted);
            bool selected = slot == _slot;

            var column = Kit.Col(Tokens.Space2);
            column.Position = origin + new Vector2(size.X * fx, size.Y * fy);
            _bench.AddChild(column);

            var well = Kit.SlotBox(selected, Tokens.SlotLg);
            if (active)
                well.AddChild(Kit.SlotIcon(UiTheme.Icon($"attach_{fitted}", Tokens.TextArcane),
                    Tokens.Arcane400, 34));
            column.AddChild(well);

            column.AddChild(Kit.Label(slot.ToString(), Tokens.TextSecondary));
            column.AddChild(Kit.Body(active ? fitted! : "empty", Tokens.SizeMicro,
                active ? Tokens.TextArcane : Tokens.TextDisabled));

            var pick = new Button { Flat = true };
            pick.SetAnchorsAndOffsetsPreset(LayoutPreset.FullRect);
            var captured = slot;
            pick.Pressed += () => { _slot = captured; Refresh(_view); };
            well.AddChild(pick);

            // Give the column a real rect. It is positioned by hand inside a
            // plain Control, which lays nothing out, so without this it keeps
            // its default zero size — and Godot draws children outside a
            // zero-sized parent quite happily while giving them no hit area.
            // Every slot on this bench looked correct and could not be clicked,
            // which is the worst shape a bug can take: the screen is not
            // broken-looking, it is inert.
            column.ResetSize();
        }
    }

    private void RebuildAmmo(PlayerView local)
    {
        foreach (var child in _ammoRow.GetChildren()) child.QueueFree();

        string current = local.AmmoFor(_weaponId);
        foreach (var def in Ammo.All.Values)
        {
            bool crafted = local.CraftedAmmo.Contains(def.Id);
            bool active = def.Id == current;

            var chip = Kit.Surface(active ? Tokens.Obsidian600 : Tokens.SurfaceSlot,
                active ? Tokens.Brass500 : Tokens.BorderPanel, 4f, shadow: false);
            var row = Kit.Row(Tokens.Space3);
            chip.AddChild(row);
            row.AddChild(Kit.Icon($"ammo_{def.Id}",
                active ? Tokens.TextAccent : crafted ? Tokens.TextSecondary : Tokens.TextDisabled, 18));
            row.AddChild(Kit.Label(def.Id,
                active ? Tokens.TextAccent : crafted ? Tokens.TextSecondary : Tokens.TextDisabled));

            string captured = def.Id;
            var pick = new Button { Flat = true };
            pick.SetAnchorsAndOffsetsPreset(LayoutPreset.FullRect);
            pick.Pressed += () =>
            {
                Submit?.Invoke(new Command.SelectAmmo(_view.LocalPlayerId, _weaponId, captured));
                _notice = "";
            };
            chip.AddChild(pick);
            _ammoRow.AddChild(chip);
        }
    }

    // =====================================================================
    // Right: what the build did, what the next module costs
    // =====================================================================

    private void RebuildStats(PlayerView local)
    {
        foreach (var child in _statPanel.Body.GetChildren()) child.QueueFree();
        if (!Weapons.All.TryGetValue(_weaponId, out var weapon)) return;

        var mounted = local.AttachmentsFor(_weaponId);
        float damage = 1f, rate = 1f, range = 1f;
        var applies = new List<string>(weapon.Applies);
        foreach (var id in mounted.Values)
        {
            if (!Attachments.All.TryGetValue(id, out var attachment)) continue;
            damage *= attachment.DamageFactor;
            rate *= attachment.RateFactor;
            range *= attachment.RangeFactor;
            if (attachment.Applies is { } status) applies.Add(status);
        }

        Delta("Damage", weapon.Damage, weapon.Damage * damage, damage);
        Delta("Rate", weapon.ShotsPerSecond, weapon.ShotsPerSecond * rate, rate);
        Delta("Range", weapon.RangeMeters, weapon.RangeMeters * range, range);

        _statPanel.Body.AddChild(Kit.Rule());
        var appliesRow = Kit.Row();
        appliesRow.AddChild(Kit.Label("applies"));
        var chips = Kit.Row(Tokens.Space3);
        chips.SizeFlagsHorizontal = SizeFlags.ExpandFill;
        foreach (string status in applies.Distinct())
            chips.AddChild(Kit.Tag(status, UiTheme.Status(status)));
        if (applies.Count == 0) chips.AddChild(Kit.Body("—", Tokens.SizeCaption, Tokens.TextDisabled));
        appliesRow.AddChild(chips);
        _statPanel.Body.AddChild(appliesRow);

        void Delta(string name, float baseValue, float built, float factor)
        {
            var column = Kit.Col(Tokens.Space2);
            var header = Kit.Row();
            header.AddChild(Kit.Label(name));
            var readout = Kit.Numeral($"{baseValue:0.##} → {built:0.##}  ×{factor:0.00}",
                Tokens.SizeCaption,
                factor > 1.001f ? Tokens.StateSuccess : factor < 0.999f ? Tokens.StateDanger : Tokens.TextSecondary);
            readout.SizeFlagsHorizontal = SizeFlags.ExpandFill;
            readout.HorizontalAlignment = HorizontalAlignment.Right;
            header.AddChild(readout);
            column.AddChild(header);
            // Half the bar is the base value, so a build that helps grows right
            // of centre and one that costs you shrinks left of it.
            column.AddChild(Kit.Bar(Mathf.Clamp(factor * 0.5f, 0f, 1f),
                factor >= 1f ? Tokens.StateSuccess : Tokens.StateDanger, 14f));
            _statPanel.Body.AddChild(column);
        }
    }

    private void RebuildRecipe(PlayerView local)
    {
        foreach (var child in _optionColumn.GetChildren()) child.QueueFree();

        var mounted = local.AttachmentsFor(_weaponId);
        mounted.TryGetValue(_slot, out string? fitted);

        _optionColumn.AddChild(Kit.Label($"{_slot} options", Tokens.TextSecondary));

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
            head.AddChild(Kit.Icon($"attach_{attachment.Id}",
                active ? Tokens.TextArcane : Tokens.TextSecondary, 20));
            var name = Kit.Title(attachment.Id.ToUpperInvariant(), Tokens.SizeCaption);
            name.SizeFlagsHorizontal = SizeFlags.ExpandFill;
            head.AddChild(name);
            head.AddChild(Kit.Numeral(
                $"×{attachment.DamageFactor:0.00}d ×{attachment.RateFactor:0.00}r ×{attachment.RangeFactor:0.00}g",
                Tokens.SizeMicro, Tokens.TextMuted));
            column.AddChild(head);

            // Have/need per scrap type: the player needs to know which enemies
            // to go farm, not just that they are short.
            var recipe = Kit.Row(Tokens.Space4);
            bool affordable = true;
            foreach (var (type, need) in attachment.Recipe)
            {
                int have = _view.PersonalScrapOf(type);
                if (have < need) affordable = false;
                recipe.AddChild(UiTheme.CountChip($"scrap_{type.ToString().ToLowerInvariant()}",
                    have, UiTheme.Scrap(type), need));
            }
            if (attachment.Applies is { } status) recipe.AddChild(Kit.Tag(status, UiTheme.Status(status)));
            recipe.AddChild(Kit.Spacer());

            if (!active)
            {
                var craft = new KitButton("Fit", affordable ? KitButton.Tone.Primary : KitButton.Tone.Secondary,
                    Tokens.ControlSm);
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

    private void RebuildBlueprint(PlayerView local)
    {
        foreach (var child in _blueprintPanel.Body.GetChildren()) child.QueueFree();

        int fitted = local.AttachmentsFor(_weaponId).Count;
        bool saved = HasBlueprint?.Invoke(_weaponId) ?? false;

        var column = Kit.Col(Tokens.Space3);
        column.SizeFlagsHorizontal = SizeFlags.ExpandFill;
        column.AddChild(Kit.Title(saved ? "SAVED BUILD" : "NO BUILD SAVED", Tokens.SizeBody));
        column.AddChild(Kit.Label($"{_weaponId} · {fitted} modules"));

        var actions = Kit.Row(Tokens.Space4);
        var recraft = new KitButton("Recraft", KitButton.Tone.Arcane, Tokens.ControlMd);
        recraft.Disabled = !saved;
        recraft.Pressed += () => { RecraftBlueprint?.Invoke(_weaponId); _notice = "recrafting…"; };
        actions.AddChild(recraft);

        var row = Kit.Row(Tokens.Space5);
        row.AddChild(column);
        row.AddChild(actions);
        _blueprintPanel.Body.AddChild(row);
    }
}
