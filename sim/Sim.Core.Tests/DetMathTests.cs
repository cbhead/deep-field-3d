using DeepField.Sim;
using Xunit;

namespace DeepField.Sim.Tests;

/// <summary>The sim is machine-portable deterministic, which means no libm in
/// the tick — see <see cref="DetMath"/>. These are the tests that keep the
/// replacements honest: a portable function that is portably *wrong* is worse
/// than the transcendental it replaced, because the error is silent and lands
/// in balance rather than in a crash.</summary>
public class DetMathTests
{
    [Fact]
    public void PowIntAgreesWithRepeatedMultiplicationToFloatPrecision()
    {
        // Deliberately a *relative* tolerance, not bit-equality. Squaring
        // associates differently from a left-to-right loop — (b²)² against
        // ((b·b)·b)·b — so the two disagree in the last ulp or two, and at
        // b=1.35, n=24 that is 13.2641029 against 13.2641068. Neither is more
        // correct; what matters is that PowInt's association order is fixed by
        // its own algorithm, so it gives one answer and keeps giving it on any
        // IEEE-754 machine. Bit-equality with the naive loop is not the
        // guarantee and asserting it would be asserting a coincidence.
        foreach (float b in new[] { 1.22f, 1.15f, 0.94f, 1.35f, 2f, 0.5f })
            for (int n = 0; n <= 24; n++)
            {
                float naive = 1f;
                for (int i = 0; i < n; i++) naive *= b;
                float got = DetMath.PowInt(b, n);
                float tolerance = System.Math.Abs(naive) * 1e-5f + 1e-6f;
                Assert.True(System.Math.Abs(got - naive) <= tolerance,
                    $"{b}^{n}: got {got:R}, naive {naive:R}");
            }
    }

    [Fact]
    public void PowIntIsStableAcrossCalls()
    {
        // The property that actually carries the determinism claim: the same
        // input gives the same bits, every time, with no hidden state.
        foreach (float b in new[] { 1.22f, 0.94f, 1.35f })
            for (int n = 0; n <= 30; n++)
                Assert.Equal(DetMath.PowInt(b, n), DetMath.PowInt(b, n));
    }

    [Fact]
    public void PowIntHandlesZeroAndNegativeExponents()
    {
        Assert.Equal(1f, DetMath.PowInt(1.22f, 0));
        Assert.Equal(1f, DetMath.PowInt(0f, 0));
        Assert.Equal(0f, DetMath.PowInt(0f, 3));
        Assert.Equal(1f / (1.5f * 1.5f), DetMath.PowInt(1.5f, -2), 6);
    }

    [Fact]
    public void PowIntIsExactOnTheGrowthCurvesTheSimActuallyUses()
    {
        // Balance.HpGrowth over a full endless run is the deepest exponent in
        // the game and the one whose drift would be most visible, so it gets
        // named rather than covered by the sweep above.
        float compounded = 1f;
        for (int wave = 0; wave < 60; wave++)
        {
            float got = DetMath.PowInt(1.22f, wave);
            Assert.True(System.Math.Abs(got - compounded) <= System.Math.Abs(compounded) * 1e-4f,
                $"wave {wave}: got {got:R}, compounded {compounded:R}");
            compounded *= 1.22f;
        }
    }

    [Theory]
    [InlineData(0f, 1f)]
    [InlineData(90f, 0f)]
    [InlineData(180f, -1f)]
    [InlineData(270f, 0f)]
    [InlineData(360f, 1f)]
    public void CosDegreesHitsTheCardinals(float degrees, float expected)
        => Assert.Equal(expected, DetMath.CosDegrees(degrees), 6);

    [Fact]
    public void CosDegreesIsAccurateAcrossTheWholeTurn()
    {
        // Every tenth of a degree over a full turn, against double-precision
        // Math.Cos. This is the accuracy claim in DetMath's own summary, and it
        // is the reason the melee arcs and armour arcs can be trusted after
        // being moved off MathF.Cos.
        double worst = 0;
        for (int i = 0; i <= 3600; i++)
        {
            float degrees = i * 0.1f;
            double truth = System.Math.Cos(degrees * System.Math.PI / 180.0);
            worst = System.Math.Max(worst, System.Math.Abs(DetMath.CosDegrees(degrees) - truth));
        }
        Assert.True(worst < 1e-6, $"worst error {worst:E3} over 0-360 degrees");
    }

    [Fact]
    public void CosDegreesIsAccurateOnEveryArcTheContentTablesAuthor()
    {
        // The values that actually matter: melee swing half-angles, the Aegis
        // and Ram front arcs, and the 150-degree rear threshold. A cone test is
        // a comparison against these, so an error here is a hit landing in the
        // wrong armour band.
        foreach (float arc in new[] { 25f, 35f, 45f, 80f, 150f, 30f, 60f, 20f, 40f })
        {
            double truth = System.Math.Cos(arc * System.Math.PI / 180.0);
            Assert.True(System.Math.Abs(DetMath.CosDegrees(arc) - truth) < 1e-6,
                $"cos({arc}) off by {System.Math.Abs(DetMath.CosDegrees(arc) - truth):E3}");
        }
    }

    [Fact]
    public void CosDegreesIsSymmetricAndPeriodic()
    {
        foreach (float a in new[] { 10f, 45f, 75f, 100f, 135f, 170f })
        {
            Assert.Equal(DetMath.CosDegrees(a), DetMath.CosDegrees(-a), 6);
            Assert.Equal(DetMath.CosDegrees(a), DetMath.CosDegrees(a + 360f), 6);
        }
    }
}
