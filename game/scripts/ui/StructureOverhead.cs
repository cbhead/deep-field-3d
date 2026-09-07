using Godot;

namespace DeepField.Game.Ui;

/// <summary>Billboarded health bar above a tower, trap or barricade.
///
/// The Ram made this necessary. Until M4 nothing could damage a structure, so a
/// tower was either standing or gone; now it can be halfway through being
/// demolished, and without a bar that state is invisible — the player keeps
/// counting on a lance that is four seconds from rubble, and the first news
/// they get is a toast saying it is already gone.
///
/// Hidden at full health, like the enemy bars, so a defence that is not under
/// attack does not clutter the map. That is also what makes it an alarm: a bar
/// appearing over your own building means something is chewing on it.</summary>
public partial class StructureOverhead : Node3D
{
    private const float MaxVisibleDistance = 60f;
    private static readonly Vector2 BarSize = new(1.4f, 0.16f);

    private WorldBar _back = null!;
    private WorldBar _fill = null!;

    /// <summary>Height above the structure's origin, from its own bounds.</summary>
    public float TopHeight { get; set; } = 2.4f;

    public override void _Ready()
    {
        _back = WorldBar.Make(new Color(0, 0, 0, 0.65f), 0f, BarSize);
        _fill = WorldBar.Make(Tokens.BarHp, 0.001f, BarSize);
        AddChild(_back);
        AddChild(_fill);
        Visible = false;
    }

    public void Set(float hpFraction, float cameraDistance)
    {
        // Structures are further from the player than the enemies walking past
        // them and matter for longer, so they stay legible further out than an
        // enemy bar does — losing a tower across the map is still your problem.
        bool show = hpFraction < 0.999f && cameraDistance <= MaxVisibleDistance;
        Visible = show;
        if (!show) return;

        Position = new Vector3(0, TopHeight, 0);
        float alpha = Mathf.Clamp(1.4f - cameraDistance / MaxVisibleDistance, 0.3f, 1f);

        _back.Set(1f, alpha * 0.65f);
        _fill.Set(hpFraction, alpha);
        _fill.Tint(Kit.HpColor(hpFraction));
    }
}
