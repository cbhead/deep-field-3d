#pragma once

#include "CoreMinimal.h"

// The terrain lane (ADR-0018, PROGRAMME.md §3.2): tools/ue-bridge/terrain/build_heightmap.py renders
// unreal/content/terrain/<map>.terrain.json to unreal/content/terrain/out/<map>_height.png (16-bit)
// plus <map>_height.json. These types read that pair and work out how it lands in Unreal; the
// commandlet (DFTerrainImportCommandlet) does the actor work on top of them so the maths stays
// testable without a world.
//
// Frames. The images are in the SIM frame: sample (col, row) sits at sim metres
// (OriginX + col*MetresPerSample, OriginZ + row*MetresPerSample), columns along +x, rows along +z,
// y up. Unreal is cm, Z up, +X forward: Unreal.X = -Sim.Z*100, Unreal.Y = Sim.X*100,
// Unreal.Z = Sim.Y*100. A Landscape's heightmap array runs X-major along its local X, so the sim
// image is transposed and its rows reversed on the way in (see ToLandscapeData).

/** The metadata block build_heightmap.py writes beside the PNG. Metres, sim frame. */
struct DFEDITOR_API FDFTerrainHeightmapMeta
{
	FString MapId;
	FString Frame;
	double MinZ = 0.0;
	double MaxZ = 1.0;
	double MetresPerSample = 1.0;
	double OriginX = 0.0;
	double OriginZ = 0.0;
	int32 Width = 0;
	int32 Height = 0;

	/** Sim x of the last column / sim z of the last row. */
	double MaxX() const { return OriginX + (Width - 1) * MetresPerSample; }
	double MaxSimZ() const { return OriginZ + (Height - 1) * MetresPerSample; }
};

/** A loaded heightmap in the sim layout: Samples[row * Width + col]. */
struct DFEDITOR_API FDFTerrainHeightmap
{
	FDFTerrainHeightmapMeta Meta;
	TArray<uint16> Samples;

	/** Parse <map>_height.json. Fails (with a reason) on a missing field or a frame we do not understand. */
	static bool ParseMeta(const FString& JsonText, FDFTerrainHeightmapMeta& OutMeta, FString& OutError);

	/** Decode a 16-bit grayscale PNG into Samples; the size must match the meta. */
	static bool DecodePng(const TArray<uint8>& PngBytes, const FDFTerrainHeightmapMeta& Meta, TArray<uint16>& OutSamples, FString& OutError);

	/** Load the json + png pair from disk. */
	static bool Load(const FString& JsonPath, const FString& PngPath, FDFTerrainHeightmap& Out, FString& OutError);

	/** Where the tool writes for a map, relative to the repo root (the project dir is unreal/DeepField). */
	static FString DefaultOutDir();

	/** Height in sim metres of a raw sample, per the json's encoding (0 = MinZ, 65535 = MaxZ). */
	double SampleToMetres(uint16 Sample) const
	{
		return Meta.MinZ + (Meta.MaxZ - Meta.MinZ) * (static_cast<double>(Sample) / 65535.0);
	}
};

/** Component layout and actor transform that place the sim image in Unreal. */
struct DFEDITOR_API FDFTerrainLandscapeLayout
{
	int32 QuadsPerSection = 63;
	int32 SectionsPerComponent = 1;
	FIntPoint ComponentCount = FIntPoint(1, 1);
	/** Landscape vertices along its local X (= sim -z) and Y (= sim +x); >= the image size, padded by edge clamp. */
	int32 SizeX = 0;
	int32 SizeY = 0;
	/** Actor location: local vertex (0, 0) sits at sim (OriginX, MaxSimZ), and the Z that maps sample 32768. */
	FVector Location = FVector::ZeroVector;
	/** Actor scale: XY = MetresPerSample*100 (100 cm quads at 1 m/sample); Z from the MinZ..MaxZ range. */
	FVector Scale = FVector(100.0, 100.0, 100.0);

	/** Same policy as FLandscapeImportHelper::ChooseBestComponentSizeForImport, preferring 63-quad
	 *  single-section components: an exact tiling if one exists, else the smallest expansion. */
	static FDFTerrainLandscapeLayout Compute(const FDFTerrainHeightmapMeta& Meta);

	/** Sim (row = z, col = x) -> landscape (X index along -z, Y index along +x), edge-clamped into SizeX*SizeY. */
	static void ToLandscapeData(const FDFTerrainHeightmap& In, const FDFTerrainLandscapeLayout& Layout, TArray<uint16>& Out);

	/** The Unreal-cm position of a landscape vertex under this layout (what the engine will compute). */
	FVector VertexToUnreal(int32 X, int32 Y, uint16 Sample) const
	{
		// LANDSCAPE_ZSCALE is 1/128: a sample of 32768 sits on the actor's Z.
		return FVector(Location.X + X * Scale.X, Location.Y + Y * Scale.Y, Location.Z + (static_cast<double>(Sample) - 32768.0) * Scale.Z / 128.0);
	}

	/** The contract's frame mapping, in one place. */
	static FVector SimToUnreal(double SimX, double SimY, double SimZ)
	{
		return FVector(-SimZ * 100.0, SimX * 100.0, SimY * 100.0);
	}
};
