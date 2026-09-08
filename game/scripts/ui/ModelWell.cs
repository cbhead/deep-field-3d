using Godot;
using System.Collections.Generic;
using System.Linq;
using DeepField.Sim.Content;

namespace DeepField.Game.Ui;

/// <summary>A design model rendered inside a UI control — design's
/// <c>ui-weapon-view.js</c>, which gives any card a transparent canvas showing
/// the real thing: the gun you built, the module in its slot, the round on
/// the ammo rail. One of these per card, each with its own world.
///
/// Every gotcha the first bench render paid for is kept here so the next
/// surface does not pay it again: the viewport owns its World3D (or the
/// camera renders whatever level is loaded), it brings its own lighting (an
/// own-world viewport has no environment, so flat-shaded meshes come out
/// black on transparent), it is anchored rather than sized (the host has no
/// size when it is built), and the model is measured only once it is in the
/// tree (GlobalTransform on a detached node is identity).</summary>
public partial class ModelWell : TextureRect
{
    public enum Pose { Weapon, Attachment, Ammo, Hands }

    private readonly Node3D _model;
    private readonly Pose _pose;
    private readonly Vector2I _pixels;
    private Node3D? _pivot;
    private float _time;

    /// <summary>A TextureRect showing the viewport's texture, not a
    /// SubViewportContainer. A container forwards input into its viewport and
    /// marks it handled, and a dozen of them on the armory swallowed every
    /// click before the buttons saw it — the Buy probe caught it. A picture
    /// of the render has no opinion about the mouse.</summary>
    public ModelWell(Node3D model, Pose pose, Vector2I pixels)
    {
        _model = model;
        _pose = pose;
        _pixels = pixels;
        MouseFilter = MouseFilterEnum.Ignore;
        ExpandMode = ExpandModeEnum.IgnoreSize;
        StretchMode = StretchModeEnum.Scale;
        SetAnchorsAndOffsetsPreset(LayoutPreset.FullRect);
    }

    public override void _Ready()
    {
        var viewport = new SubViewport
        {
            TransparentBg = true,
            RenderTargetUpdateMode = SubViewport.UpdateMode.Always,
            Size = _pixels,
            OwnWorld3D = true,
        };
        AddChild(viewport);
        Texture = viewport.GetTexture();

        var stage = new Node3D();
        viewport.AddChild(stage);
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

        // Design's rounds are authored along -Z (the bore); the card stands
        // them tip-up. Everything else is shown as authored, from the left
        // three-quarter, slightly above.
        _model.RotationDegrees = _pose switch
        {
            Pose.Ammo => new Vector3(-90f, 0f, 0f),
            _ => Vector3.Zero,
        };
        _pivot = new Node3D();
        stage.AddChild(_pivot);
        _pivot.AddChild(_model);

        var bounds = MergedBounds(_model);
        var centre = bounds.GetCenter();
        _pivot.Position = centre;
        _model.Position = -centre;
        float reach = Mathf.Max(bounds.Size.Length(), 0.02f);

        var dir = (_pose switch
        {
            Pose.Weapon => new Vector3(-1.4f, 0.5f, -0.55f),
            Pose.Ammo => new Vector3(-0.3f, 0.55f, -1f),
            Pose.Hands => new Vector3(0.35f, 0.18f, -1f),
            _ => new Vector3(-1.0f, 0.55f, -1.0f),
        }).Normalized();
        float fov = _pose == Pose.Weapon ? 28f : 32f;
        float pad = _pose == Pose.Weapon ? 1.05f : 1.15f;
        float back = reach / (2f * Mathf.Tan(Mathf.DegToRad(fov) * 0.5f)) * pad;
        if (_pose == Pose.Weapon)
        {
            // Fit the projected footprint of a long thin object, not its
            // bounding sphere — design's viewer does the same, or a pistol
            // sits small in the middle of a wide well.
            float aspect = (float)_pixels.X / _pixels.Y;
            float vfov = Mathf.DegToRad(fov), hfov = 2f * Mathf.Atan(Mathf.Tan(vfov / 2f) * aspect);
            var size = bounds.Size;
            float a = Mathf.Atan2(Mathf.Abs(dir.X), Mathf.Abs(dir.Z));
            float ext = size.Z * Mathf.Sin(a) + size.X * Mathf.Cos(a) + size.Y * 0.2f;
            back = Mathf.Max((ext * 0.5f * 0.98f) / Mathf.Tan(hfov / 2f), (size.Y * 0.5f * 1.7f) / Mathf.Tan(vfov / 2f)) + size.X * 0.4f;
        }

        var camera = new Camera3D { Position = centre + dir * back, Fov = fov, Current = true };
        stage.AddChild(camera);
        camera.LookAt(centre, Vector3.Up);

        stage.AddChild(new DirectionalLight3D
        {
            RotationDegrees = new Vector3(-28f, 24f, 0f), LightEnergy = 1.7f,
            LightColor = new Color(1f, 0.945f, 0.863f),
        });
        stage.AddChild(new DirectionalLight3D
        {
            RotationDegrees = new Vector3(-6f, -50f, 0f), LightEnergy = 0.55f,
            LightColor = Tokens.Arcane400.Lerp(Colors.White, 0.5f),
        });
    }

    /// <summary>A slow sway keeps the highlights moving — design's cards do
    /// the same. Attachments turn steadily; guns and rounds rock.</summary>
    public override void _Process(double delta)
    {
        if (_pivot is null) return;
        _time += (float)delta;
        _pivot.Rotation = new Vector3(0f, _pose switch
        {
            Pose.Weapon => Mathf.Sin(_time * 0.35f) * 0.22f,
            Pose.Ammo => Mathf.Sin(_time * 0.5f) * 0.35f,
            Pose.Hands => Mathf.Sin(_time * 0.3f) * 0.3f,
            _ => _time * 0.4f,
        }, 0f);
    }

    /// <summary>World-space bounds of every mesh under a node, merged.</summary>
    public static Aabb MergedBounds(Node3D root)
    {
        Aabb? merged = null;
        foreach (var m in root.FindChildren("*", "MeshInstance3D", true, false).OfType<MeshInstance3D>())
        {
            var box = m.GlobalTransform * m.GetAabb();
            merged = merged is { } acc ? acc.Merge(box) : box;
        }
        if (root is MeshInstance3D self)
        {
            var box = self.GlobalTransform * self.GetAabb();
            merged = merged is { } acc ? acc.Merge(box) : box;
        }
        return merged ?? new Aabb(Vector3.Zero, new Vector3(0.2f, 0.1f, 0.2f));
    }
}

/// <summary>The gun you built, assembled the way design's gunsmith assembles
/// it: a stock part is hidden when its slot is filled, and each module hangs
/// off the weapon's <c>&lt;weapon&gt;_mount_&lt;slot&gt;</c> node — a muzzle
/// device off the fitted barrel's own <c>attach_mount_muzzle</c>, so a barrel
/// swap takes it along. Shared by the armory bench, the blueprint cards and
/// anything else that needs to show a build rather than a platform.</summary>
public static class WeaponAssembly
{
    /// <summary>Null when design has not shipped the platform yet.</summary>
    public static Node3D? Build(string weaponId, IReadOnlyDictionary<AttachmentSlot, string> fitted, bool world = true)
    {
        string id = weaponId.ToLowerInvariant();
        string asset = world && AssetLibrary.Has($"weapon_{id}_world") ? $"weapon_{id}_world" : $"weapon_{id}_vm";
        if (!AssetLibrary.Has(asset)) return null;
        var gun = AssetLibrary.Instantiate(asset, () => new Node3D());

        // Barrel first: the muzzle mount may live on it.
        Node3D? barrel = null;
        foreach (var slot in fitted.Keys.OrderBy(s => s == AttachmentSlot.Barrel ? 0 : 1))
        {
            string attachmentId = fitted[slot];
            var module = AssetLibrary.TryInstantiate($"attach_{attachmentId}");
            if (module is null) continue;          // model not delivered yet
            module.Name = $"attach_{slot.ToString().ToLowerInvariant()}";

            string stock = slot switch
            {
                AttachmentSlot.Barrel => $"{id}_barrel",
                AttachmentSlot.Magazine => $"{id}_magazine",
                AttachmentSlot.Stock => $"{id}_stock",
                _ => "",
            };
            if (stock.Length > 0 && gun.FindChild(stock, true, false) is Node3D replaced)
                replaced.Visible = false;

            Node3D? mount = gun.FindChild($"{id}_mount_{slot.ToString().ToLowerInvariant()}", true, false) as Node3D;
            if (slot == AttachmentSlot.Muzzle && barrel?.FindChild("attach_mount_muzzle", true, false) is Node3D onBarrel)
                mount = onBarrel;
            (mount ?? gun).AddChild(module);
            if (slot == AttachmentSlot.Barrel) barrel = module;
        }
        return gun;
    }
}
