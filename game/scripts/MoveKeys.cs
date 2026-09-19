using Godot;

namespace DeepField.Game;

/// <summary>The four directional inputs, each answered by its WASD key or its
/// arrow key. Walking, climbing and driving all read direction from here so a
/// player on the arrows gets every one of them, rather than the one surface
/// somebody remembered to wire.
///
/// Physical keys, as the WASD reads always were: on AZERTY the forward key is
/// still the one where W sits, and the arrows are the same on every layout.</summary>
public static class MoveKeys
{
    public static bool Forward => Held(Key.W, Key.Up);
    public static bool Back => Held(Key.S, Key.Down);
    public static bool Left => Held(Key.A, Key.Left);
    public static bool Right => Held(Key.D, Key.Right);

    private static bool Held(Key letter, Key arrow) =>
        Input.IsPhysicalKeyPressed(letter) || Input.IsPhysicalKeyPressed(arrow);
}
