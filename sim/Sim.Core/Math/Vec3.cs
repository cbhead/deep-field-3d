namespace DeepField.Sim;

/// <summary>World-space position/direction in meters. The sim speaks meters;
/// only the render layer knows about meshes and pixels.</summary>
public readonly record struct Vec3(float X, float Y, float Z)
{
    public static Vec3 Zero => new(0f, 0f, 0f);

    public static Vec3 operator +(Vec3 a, Vec3 b) => new(a.X + b.X, a.Y + b.Y, a.Z + b.Z);
    public static Vec3 operator -(Vec3 a, Vec3 b) => new(a.X - b.X, a.Y - b.Y, a.Z - b.Z);
    public static Vec3 operator *(Vec3 a, float s) => new(a.X * s, a.Y * s, a.Z * s);

    public float Length() => MathF.Sqrt(X * X + Y * Y + Z * Z);
    public float DistanceTo(Vec3 other) => (other - this).Length();

    public Vec3 Normalized()
    {
        float len = Length();
        return len > 1e-6f ? this * (1f / len) : Zero;
    }

    public static Vec3 Lerp(Vec3 a, Vec3 b, float t) => a + (b - a) * t;
}
