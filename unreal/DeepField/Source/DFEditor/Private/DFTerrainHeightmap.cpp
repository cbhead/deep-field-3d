#include "DFTerrainHeightmap.h"

#include "Dom/JsonObject.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	// The engine's valid Landscape section sizes (FLandscapeConfig), largest first for the exact-fit search.
	constexpr int32 GSectionSizes[] = { 255, 127, 63, 31, 15, 7 };
	constexpr int32 GSectionCounts[] = { 1, 2 };
	constexpr int32 GMaxComponentsPerAxis = 32;

	bool ReadNumber(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, double& Out, FString& OutError)
	{
		if (!Obj->TryGetNumberField(Field, Out))
		{
			OutError = FString::Printf(TEXT("_height.json is missing the number field '%s'"), Field);
			return false;
		}
		return true;
	}
}

bool FDFTerrainHeightmap::ParseMeta(const FString& JsonText, FDFTerrainHeightmapMeta& OutMeta, FString& OutError)
{
	TSharedPtr<FJsonObject> Obj;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid())
	{
		OutError = TEXT("_height.json does not parse");
		return false;
	}

	FDFTerrainHeightmapMeta Meta;
	Obj->TryGetStringField(TEXT("map"), Meta.MapId);
	Obj->TryGetStringField(TEXT("frame"), Meta.Frame);
	// The images are only meaningful in the sim frame; refuse anything else so a future tool
	// change cannot silently rotate a map.
	if (Meta.Frame != TEXT("sim-metres"))
	{
		OutError = FString::Printf(TEXT("_height.json frame is '%s', expected 'sim-metres'"), *Meta.Frame);
		return false;
	}
	double Width = 0.0, Height = 0.0;
	if (!ReadNumber(Obj, TEXT("minZ"), Meta.MinZ, OutError) || !ReadNumber(Obj, TEXT("maxZ"), Meta.MaxZ, OutError)
		|| !ReadNumber(Obj, TEXT("metresPerSample"), Meta.MetresPerSample, OutError)
		|| !ReadNumber(Obj, TEXT("originX"), Meta.OriginX, OutError) || !ReadNumber(Obj, TEXT("originZ"), Meta.OriginZ, OutError)
		|| !ReadNumber(Obj, TEXT("width"), Width, OutError) || !ReadNumber(Obj, TEXT("height"), Height, OutError))
	{
		return false;
	}
	Meta.Width = static_cast<int32>(Width);
	Meta.Height = static_cast<int32>(Height);
	if (Meta.Width < 2 || Meta.Height < 2 || Meta.MetresPerSample <= 0.0 || Meta.MaxZ <= Meta.MinZ)
	{
		OutError = TEXT("_height.json has a degenerate size, resolution or Z range");
		return false;
	}
	OutMeta = Meta;
	return true;
}

bool FDFTerrainHeightmap::DecodePng(const TArray<uint8>& PngBytes, const FDFTerrainHeightmapMeta& Meta, TArray<uint16>& OutSamples, FString& OutError)
{
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	const TSharedPtr<IImageWrapper> Wrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
	if (!Wrapper.IsValid() || !Wrapper->SetCompressed(PngBytes.GetData(), PngBytes.Num()))
	{
		OutError = TEXT("_height.png is not a PNG");
		return false;
	}
	if (Wrapper->GetWidth() != Meta.Width || Wrapper->GetHeight() != Meta.Height)
	{
		OutError = FString::Printf(TEXT("_height.png is %dx%d but _height.json says %dx%d — regenerate both"), Wrapper->GetWidth(), Wrapper->GetHeight(), Meta.Width, Meta.Height);
		return false;
	}
	if (Wrapper->GetBitDepth() != 16 || Wrapper->GetFormat() != ERGBFormat::Gray)
	{
		OutError = TEXT("_height.png must be 16-bit grayscale");
		return false;
	}
	TArray64<uint8> Raw;
	if (!Wrapper->GetRaw(ERGBFormat::Gray, 16, Raw))
	{
		OutError = TEXT("_height.png failed to decode");
		return false;
	}
	const int64 Count = static_cast<int64>(Meta.Width) * Meta.Height;
	if (Raw.Num() != Count * 2)
	{
		OutError = TEXT("_height.png decoded to an unexpected size");
		return false;
	}
	OutSamples.SetNumUninitialized(Count);
	// The image wrapper hands back the machine-endian 16-bit samples in image order (row-major, top row first).
	FMemory::Memcpy(OutSamples.GetData(), Raw.GetData(), Count * 2);
	return true;
}

bool FDFTerrainHeightmap::Load(const FString& JsonPath, const FString& PngPath, FDFTerrainHeightmap& Out, FString& OutError)
{
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *JsonPath))
	{
		OutError = FString::Printf(TEXT("cannot read %s (run tools/ue-bridge/terrain/build_heightmap.py first)"), *JsonPath);
		return false;
	}
	TArray<uint8> PngBytes;
	if (!FFileHelper::LoadFileToArray(PngBytes, *PngPath))
	{
		OutError = FString::Printf(TEXT("cannot read %s"), *PngPath);
		return false;
	}
	FDFTerrainHeightmap Loaded;
	if (!ParseMeta(JsonText, Loaded.Meta, OutError) || !DecodePng(PngBytes, Loaded.Meta, Loaded.Samples, OutError))
	{
		return false;
	}
	Out = MoveTemp(Loaded);
	return true;
}

FString FDFTerrainHeightmap::DefaultOutDir()
{
	// ProjectDir is <repo>/unreal/DeepField/; the tool writes to <repo>/unreal/content/terrain/out/.
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../content/terrain/out")));
}

FDFTerrainLandscapeLayout FDFTerrainLandscapeLayout::Compute(const FDFTerrainHeightmapMeta& Meta)
{
	FDFTerrainLandscapeLayout Layout;
	// The landscape's X axis runs along sim -z (the image's rows), its Y axis along sim +x (the columns).
	const int32 QuadsX = Meta.Height - 1;
	const int32 QuadsY = Meta.Width - 1;

	bool bFound = false;
	for (int32 Size : GSectionSizes)
	{
		for (int32 Count : GSectionCounts)
		{
			const int32 ComponentQuads = Size * Count;
			if (QuadsX % ComponentQuads == 0 && QuadsY % ComponentQuads == 0
				&& QuadsX / ComponentQuads <= GMaxComponentsPerAxis && QuadsY / ComponentQuads <= GMaxComponentsPerAxis)
			{
				Layout.QuadsPerSection = Size;
				Layout.SectionsPerComponent = Count;
				Layout.ComponentCount = FIntPoint(QuadsX / ComponentQuads, QuadsY / ComponentQuads);
				bFound = true;
				break;
			}
		}
		if (bFound)
		{
			break;
		}
	}
	if (!bFound)
	{
		// No exact tiling: expand to whole 63-quad components (the padding is edge-clamped ground
		// outside the belt) — or larger sections if the map would need more than 32 per axis.
		for (int32 Size : { 63, 127, 255 })
		{
			const int32 CX = FMath::DivideAndRoundUp(QuadsX, Size);
			const int32 CY = FMath::DivideAndRoundUp(QuadsY, Size);
			if (CX <= GMaxComponentsPerAxis && CY <= GMaxComponentsPerAxis)
			{
				Layout.QuadsPerSection = Size;
				Layout.SectionsPerComponent = 1;
				Layout.ComponentCount = FIntPoint(CX, CY);
				break;
			}
		}
	}
	const int32 ComponentQuads = Layout.QuadsPerSection * Layout.SectionsPerComponent;
	Layout.SizeX = Layout.ComponentCount.X * ComponentQuads + 1;
	Layout.SizeY = Layout.ComponentCount.Y * ComponentQuads + 1;

	// Scale: one sample = MetresPerSample*100 cm per quad; Z so that 0..65535 spans MinZ..MaxZ.
	// The engine maps a sample v to Z = Location.Z + (v - 32768) * Scale.Z / 128.
	const double RangeCm = (Meta.MaxZ - Meta.MinZ) * 100.0;
	Layout.Scale = FVector(Meta.MetresPerSample * 100.0, Meta.MetresPerSample * 100.0, RangeCm * 128.0 / 65535.0);
	// Location: local vertex (0,0) is sim (OriginX, MaxSimZ) — X index 0 is the LAST image row
	// because Unreal.X = -sim.z; and sample 0 must land on MinZ.
	const FVector Corner = SimToUnreal(Meta.OriginX, Meta.MinZ, Meta.MaxSimZ());
	Layout.Location = FVector(Corner.X, Corner.Y, Corner.Z + 32768.0 * Layout.Scale.Z / 128.0);
	return Layout;
}

void FDFTerrainLandscapeLayout::ToLandscapeData(const FDFTerrainHeightmap& In, const FDFTerrainLandscapeLayout& Layout, TArray<uint16>& Out)
{
	const int32 W = In.Meta.Width;
	const int32 H = In.Meta.Height;
	Out.SetNumUninitialized(Layout.SizeX * Layout.SizeY);
	for (int32 Y = 0; Y < Layout.SizeY; ++Y)
	{
		// Landscape Y runs along sim +x = image columns; clamp to pad beyond the last column.
		const int32 Col = FMath::Min(Y, W - 1);
		for (int32 X = 0; X < Layout.SizeX; ++X)
		{
			// Landscape X runs along sim -z: X index 0 is the last image row, X = H-1 the first.
			const int32 Row = (H - 1) - FMath::Min(X, H - 1);
			Out[Y * Layout.SizeX + X] = In.Samples[Row * W + Col];
		}
	}
}
