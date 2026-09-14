namespace DeepField.Sim;

/// <summary>Arithmetic the sim is allowed to use, and the reason the rest is not.
///
/// The sim's determinism contract used to be "same machine, same process": both
/// determinism gates run two matches inside one process, CI ran a single
/// ubuntu-latest job, and the netcode is server-authoritative, so nothing ever
/// compared a result across architectures. Under that contract `MathF.Pow`,
/// `Cos`, `Sin` and `Acos` were fine. Under a machine-portable one they are not:
/// they route to the platform's libm, which carries no cross-OS or cross-arch
/// bit-identity guarantee, and their results reach the SHA-256'd event log
/// through hp scaling, bounty rounding and target acquisition order.
///
/// What stays is IEEE-754 binary32 <c>+ - * /</c> and <c>MathF.Sqrt</c>: RyuJIT
/// does not contract to FMA on its own (fusing needs an explicit
/// <c>Math.FusedMultiplyAdd</c>) and does not use x87 excess precision on x64 or
/// ARM64, and hardware square root is correctly rounded. Those are portable.
///
/// So the rule for <c>Sim.Core</c> is: **no transcendental in the tick**. Every
/// one that was there had an integer exponent or a constant argument, so each
/// became either repeated multiplication or a number computed once at authoring
/// time — which is why this file is small and why removing them cost nothing.
/// <c>Protocol.PackYaw</c> keeps its <c>Atan2</c> deliberately: it encodes a
/// wire byte and never enters world state or the log.</summary>
public static class DetMath
{
    /// <summary>x^n for a non-negative integer n, by squaring — the shape every
    /// `MathF.Pow` in the sim actually had. Multiplication only, so it is
    /// bit-identical on any IEEE-754 machine.
    ///
    /// This is not `Pow` with a faster path: the association order is fixed by
    /// the algorithm, so it produces one specific value and keeps producing it.
    /// Results differ from `MathF.Pow` in the last ulp for some inputs, which is
    /// why landing it is a recorded re-baseline rather than a silent change.</summary>
    public static float PowInt(float x, int n)
    {
        if (n < 0) return 1f / PowInt(x, -n);
        float result = 1f;
        float basis = x;
        while (n > 0)
        {
            if ((n & 1) != 0) result *= basis;
            basis *= basis;
            n >>= 1;
        }
        return result;
    }

    /// <summary>Cosine of an angle in degrees, for **authoring-time constants
    /// only** — arc widths in def tables, not anything a tick computes. It is a
    /// polynomial rather than a table because the values wanted are arbitrary
    /// (a 150° rear arc, a 60° swing), and it is accurate to about 1e-7 over the
    /// 0–360 range every caller uses.
    ///
    /// Never call this from inside the tick. A def that needs a cosine should
    /// hold the cosine.</summary>
    public static float CosDegrees(float degrees)
    {
        // Range-reduce to [0, 360) then to [0, 90] with the quadrant sign, so
        // the polynomial only ever sees its accurate interval.
        float d = degrees % 360f;
        if (d < 0f) d += 360f;
        float sign = 1f;
        if (d > 270f) d = 360f - d;
        else if (d > 180f) { d -= 180f; sign = -1f; }
        else if (d > 90f) { d = 180f - d; sign = -1f; }

        // cos(x) = 1 - x²/2! + x⁴/4! - x⁶/6! + x⁸/8! - x¹⁰/10!, x in radians.
        double x = d * (System.Math.PI / 180.0);
        double x2 = x * x;
        double cos = 1.0
            - x2 / 2.0
            + x2 * x2 / 24.0
            - x2 * x2 * x2 / 720.0
            + x2 * x2 * x2 * x2 / 40320.0
            - x2 * x2 * x2 * x2 * x2 / 3628800.0;
        return (float)(sign * cos);
    }
}
