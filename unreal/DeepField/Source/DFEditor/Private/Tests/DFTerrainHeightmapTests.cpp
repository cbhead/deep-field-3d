#include "DFTerrainHeightmap.h"

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

// The terrain lane's maths, checked without a world: the frame mapping, the component layout and
// the image transpose are where an import silently goes wrong (a map mirrored or a metre-tall
// terrace), so they are pinned here; the commandlet's own verify step checks the saved level.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFTerrainLayoutTest, "DF.Editor.Terrain.Layout", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFTerrainLayoutTest::RunTest(const FString& Parameters)
{
	// Foundry's size: 110x80 playable + 40 belt at 1 m -> 191 columns (x) by 161 rows (z).
	FDFTerrainHeightmapMeta Meta;
	Meta.Width = 191;
	Meta.Height = 161;
	Meta.MetresPerSample = 1.0;
	Meta.OriginX = -95.0;
	Meta.OriginZ = -80.0;
	Meta.MinZ = -2.5;
	Meta.MaxZ = 35.5;
	const FDFTerrainLandscapeLayout Layout = FDFTerrainLandscapeLayout::Compute(Meta);

	// No exact tiling of 160x190 quads exists, so 63-quad components: 3 along X (sim -z), 4 along Y (sim +x).
	TestEqual(TEXT("quads per section"), Layout.QuadsPerSection, 63);
	TestEqual(TEXT("sections per component"), Layout.SectionsPerComponent, 1);
	TestEqual(TEXT("components X"), Layout.ComponentCount.X, 3);
	TestEqual(TEXT("components Y"), Layout.ComponentCount.Y, 4);
	TestEqual(TEXT("verts X"), Layout.SizeX, 190);
	TestEqual(TEXT("verts Y"), Layout.SizeY, 253);
	TestTrue(TEXT("padding covers the image"), Layout.SizeX >= Meta.Height && Layout.SizeY >= Meta.Width);

	// 100 cm quads; the Z scale spans the json range over 16 bits (v -> Z = Loc.Z + (v-32768)*Scale.Z/128).
	TestEqual(TEXT("scale X"), Layout.Scale.X, 100.0);
	TestEqual(TEXT("scale Y"), Layout.Scale.Y, 100.0);
	TestTrue(TEXT("scale Z"), FMath::IsNearlyEqual(Layout.Scale.Z, 38.0 * 100.0 * 128.0 / 65535.0, 1e-9));

	// Vertex (0,0) is sim (originX, maxZ): Unreal X = -80*100, Y = -95*100.
	TestTrue(TEXT("location X"), FMath::IsNearlyEqual(Layout.Location.X, -8000.0, 1e-6));
	TestTrue(TEXT("location Y"), FMath::IsNearlyEqual(Layout.Location.Y, -9500.0, 1e-6));
	// Sim (0,0) is vertex (80, 95) and lands on the Unreal origin.
	const FVector Origin = Layout.VertexToUnreal(80, 95, 32768);
	TestTrue(TEXT("sim origin -> Unreal origin X"), FMath::IsNearlyEqual(Origin.X, 0.0, 1e-6));
	TestTrue(TEXT("sim origin -> Unreal origin Y"), FMath::IsNearlyEqual(Origin.Y, 0.0, 1e-6));
	// Sample 0 is MinZ and 65535 is MaxZ, in cm.
	TestTrue(TEXT("sample 0 -> minZ"), FMath::IsNearlyEqual(Layout.VertexToUnreal(0, 0, 0).Z, -250.0, 1e-6));
	TestTrue(TEXT("sample 65535 -> maxZ"), FMath::IsNearlyEqual(Layout.VertexToUnreal(0, 0, 65535).Z, 3550.0, 1e-6));
	// A point 30 m east and 10 m toward the camera: Unreal (-1000, 3000).
	const FVector P = FDFTerrainLandscapeLayout::SimToUnreal(30.0, 4.0, 10.0);
	TestTrue(TEXT("frame mapping"), P.Equals(FVector(-1000.0, 3000.0, 400.0), 1e-6));
	const FVector PV = Layout.VertexToUnreal(70, 125, 32768);
	TestTrue(TEXT("vertex for sim (30,10)"), FMath::IsNearlyEqual(PV.X, -1000.0, 1e-6) && FMath::IsNearlyEqual(PV.Y, 3000.0, 1e-6));

	// An exact tiling is preferred when one exists: 64x64 verts = one 63-quad component.
	FDFTerrainHeightmapMeta Small = Meta;
	Small.Width = 64;
	Small.Height = 64;
	const FDFTerrainLandscapeLayout SmallLayout = FDFTerrainLandscapeLayout::Compute(Small);
	TestEqual(TEXT("exact tiling components"), SmallLayout.ComponentCount, FIntPoint(1, 1));
	TestEqual(TEXT("exact tiling verts"), SmallLayout.SizeX, 64);
	// 127x127 verts: 126 quads. Both 63x2 (one component) and 63x1 (2x2 components) tile it exactly;
	// FLandscapeImportHelper::ChooseBestComponentSizeForImport (5.8) tries one section before two for
	// each section size, so the engine — and Compute — pick 2x2 single-section components.
	Small.Width = 127;
	Small.Height = 127;
	const FDFTerrainLandscapeLayout Exact126 = FDFTerrainLandscapeLayout::Compute(Small);
	TestEqual(TEXT("126-quad quads per section"), Exact126.QuadsPerSection, 63);
	TestEqual(TEXT("126-quad sections"), Exact126.SectionsPerComponent, 1);
	TestEqual(TEXT("126-quad components"), Exact126.ComponentCount, FIntPoint(2, 2));
	TestEqual(TEXT("126-quad verts"), Exact126.SizeX, 127);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFTerrainTransposeTest, "DF.Editor.Terrain.Transpose", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFTerrainTransposeTest::RunTest(const FString& Parameters)
{
	// A 3-column (x) by 2-row (z) image: row 0 = a b c (z = originZ), row 1 = d e f (z = originZ + 1).
	FDFTerrainHeightmap Image;
	Image.Meta.Width = 3;
	Image.Meta.Height = 2;
	Image.Samples = { 10, 11, 12, 20, 21, 22 };

	FDFTerrainLandscapeLayout Layout;
	Layout.SizeX = 2; // along sim -z
	Layout.SizeY = 3; // along sim +x
	TArray<uint16> Out;
	FDFTerrainLandscapeLayout::ToLandscapeData(Image, Layout, Out);
	// Landscape X index 0 is the LAST image row (largest z), Y follows the columns.
	const TArray<uint16> Expected = { 20, 10, 21, 11, 22, 12 };
	TestEqual(TEXT("transposed size"), Out.Num(), 6);
	TestTrue(TEXT("transpose + row flip"), Out == Expected);

	// Padding beyond the image repeats the edge sample (the ground outside the belt).
	Layout.SizeX = 3;
	Layout.SizeY = 4;
	FDFTerrainLandscapeLayout::ToLandscapeData(Image, Layout, Out);
	const TArray<uint16> Padded = {
		20, 10, 10,   // Y=0: X=0 row1, X=1 row0, X=2 clamps to row0
		21, 11, 11,
		22, 12, 12,
		22, 12, 12,   // Y=3 clamps to the last column
	};
	TestTrue(TEXT("edge-clamped padding"), Out == Padded);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFTerrainParseMetaTest, "DF.Editor.Terrain.ParseMeta", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFTerrainParseMetaTest::RunTest(const FString& Parameters)
{
	const FString Json = TEXT("{\"map\":\"x\",\"frame\":\"sim-metres\",\"minZ\":-1.5,\"maxZ\":20.25,\"metresPerSample\":0.5,\"originX\":-10,\"originZ\":-8,\"width\":41,\"height\":33}");
	FDFTerrainHeightmapMeta Meta;
	FString Error;
	TestTrue(TEXT("parses"), FDFTerrainHeightmap::ParseMeta(Json, Meta, Error));
	TestEqual(TEXT("map"), Meta.MapId, FString(TEXT("x")));
	TestEqual(TEXT("width"), Meta.Width, 41);
	TestEqual(TEXT("height"), Meta.Height, 33);
	TestEqual(TEXT("res"), Meta.MetresPerSample, 0.5);
	TestTrue(TEXT("maxX"), FMath::IsNearlyEqual(Meta.MaxX(), 10.0, 1e-9));
	TestTrue(TEXT("maxZ"), FMath::IsNearlyEqual(Meta.MaxSimZ(), 8.0, 1e-9));

	const FString Wrong = Json.Replace(TEXT("sim-metres"), TEXT("unreal-cm"));
	TestFalse(TEXT("rejects another frame"), FDFTerrainHeightmap::ParseMeta(Wrong, Meta, Error));
	TestTrue(TEXT("says why"), Error.Contains(TEXT("frame")));
	const FString Missing = Json.Replace(TEXT("\"maxZ\":20.25,"), TEXT(""));
	TestFalse(TEXT("rejects a missing field"), FDFTerrainHeightmap::ParseMeta(Missing, Meta, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFTerrainLoadFoundryTest, "DF.Editor.Terrain.LoadFoundry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFTerrainLoadFoundryTest::RunTest(const FString& Parameters)
{
	// The committed build_heightmap.py outputs for the Foundry landform (unreal/content/terrain/out).
	const FString Dir = FDFTerrainHeightmap::DefaultOutDir();
	FDFTerrainHeightmap Map;
	FString Error;
	if (!FDFTerrainHeightmap::Load(FPaths::Combine(Dir, TEXT("foundry_height.json")), FPaths::Combine(Dir, TEXT("foundry_height.png")), Map, Error))
	{
		AddError(FString::Printf(TEXT("load failed: %s"), *Error));
		return false;
	}
	TestEqual(TEXT("width"), Map.Meta.Width, 191);
	TestEqual(TEXT("height"), Map.Meta.Height, 161);
	TestEqual(TEXT("samples"), Map.Samples.Num(), 191 * 161);
	TestTrue(TEXT("range"), Map.Meta.MaxZ > Map.Meta.MinZ + 8.0); // >= 8 m of relief (§3.2 G2)

	uint16 Lo = 65535, Hi = 0;
	for (uint16 S : Map.Samples)
	{
		Lo = FMath::Min(Lo, S);
		Hi = FMath::Max(Hi, S);
	}
	TestEqual(TEXT("zero at the lowest elevation"), static_cast<int32>(Lo), 0);
	TestEqual(TEXT("full scale at the highest"), static_cast<int32>(Hi), 65535);

	auto MetresAt = [&Map](double SimX, double SimZ)
	{
		const int32 Col = FMath::RoundToInt32((SimX - Map.Meta.OriginX) / Map.Meta.MetresPerSample);
		const int32 Row = FMath::RoundToInt32((SimZ - Map.Meta.OriginZ) / Map.Meta.MetresPerSample);
		return Map.SampleToMetres(Map.Samples[Row * Map.Meta.Width + Col]);
	};
	// The landform's contract with the legacy brief: westGate on the high terrace, the core in the basin.
	TestTrue(TEXT("westGate (-40,0) on T1 at 16 m"), FMath::IsNearlyEqual(MetresAt(-40.0, 0.0), 16.0, 0.02));
	TestTrue(TEXT("core (36,6) on the plinth at -2.5 m"), FMath::IsNearlyEqual(MetresAt(36.0, 6.0), -2.5, 0.02));
	TestTrue(TEXT("spawn yard (0,-24) on T2 at 8 m"), FMath::IsNearlyEqual(MetresAt(0.0, -24.0), 8.0, 0.02));
	return true;
}
