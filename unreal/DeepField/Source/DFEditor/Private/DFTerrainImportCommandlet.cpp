#include "DFTerrainImportCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Editor.h"
#include "EditorLevelUtils.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/LevelStreamingAlwaysLoaded.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "GameFramework/PlayerStart.h"
#include "Landscape.h"
#include "LandscapeEdit.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "LandscapeInfo.h"
#include "LandscapeProxy.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogDFTerrainImport, Log, All);

namespace
{
	// Every landscape gets this physical material until WS-09's physmat set (§3.2 vehicles) lands.
	// SurfaceType3 is "Grass" in DefaultEngine.ini [PhysicsSettings] PhysicalSurfaces.
	const TCHAR* GDefaultPhysMatPackage = TEXT("/Game/DF/Core/PhysicalMaterials/PM_Grass");
	const TCHAR* GDefaultPhysMatName = TEXT("PM_Grass");

	bool SaveAssetPackage(UPackage* Package, UObject* Asset)
	{
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		return UPackage::SavePackage(Package, Asset, *Filename, Args);
	}

	/** The image sample under a sim (x, z) that lies exactly on the lattice, or INDEX_NONE. */
	int32 SampleIndexAt(const FDFTerrainHeightmapMeta& Meta, double SimX, double SimZ)
	{
		const double Col = (SimX - Meta.OriginX) / Meta.MetresPerSample;
		const double Row = (SimZ - Meta.OriginZ) / Meta.MetresPerSample;
		const int32 C = FMath::RoundToInt32(Col);
		const int32 R = FMath::RoundToInt32(Row);
		if (!FMath::IsNearlyEqual(Col, static_cast<double>(C), 1e-6) || !FMath::IsNearlyEqual(Row, static_cast<double>(R), 1e-6)
			|| C < 0 || R < 0 || C >= Meta.Width || R >= Meta.Height)
		{
			return INDEX_NONE;
		}
		return R * Meta.Width + C;
	}
}

UDFTerrainImportCommandlet::UDFTerrainImportCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	ShowErrorCount = true;
}

FString UDFTerrainImportCommandlet::MapFolderName(const FString& MapId)
{
	FString Name = MapId;
	if (Name.Len() > 0)
	{
		Name[0] = FChar::ToUpper(Name[0]);
	}
	return Name;
}

FString UDFTerrainImportCommandlet::TerrainLevelPath(const FString& MapId)
{
	const FString Folder = MapFolderName(MapId);
	return FString::Printf(TEXT("/Game/DF/Maps/%s/L_%s_Terrain"), *Folder, *Folder);
}

FString UDFTerrainImportCommandlet::PersistentLevelPath(const FString& MapId)
{
	const FString Folder = MapFolderName(MapId);
	return FString::Printf(TEXT("/Game/DF/Maps/%s/L_%s"), *Folder, *Folder);
}

int32 UDFTerrainImportCommandlet::Main(const FString& Params)
{
	FString MapId;
	if (!FParse::Value(*Params, TEXT("map="), MapId) || MapId.IsEmpty())
	{
		UE_LOG(LogDFTerrainImport, Error, TEXT("usage: -run=DFTerrainImport -map=<id> [-terraindir=<dir>] [-verifyonly]"));
		return 1;
	}
	MapId = MapId.ToLower();
	FString TerrainDir;
	if (!FParse::Value(*Params, TEXT("terraindir="), TerrainDir) || TerrainDir.IsEmpty())
	{
		TerrainDir = FDFTerrainHeightmap::DefaultOutDir();
	}
	const bool bVerifyOnly = FParse::Param(*Params, TEXT("verifyonly"));

	const FString JsonPath = FPaths::Combine(TerrainDir, MapId + TEXT("_height.json"));
	const FString PngPath = FPaths::Combine(TerrainDir, MapId + TEXT("_height.png"));
	FDFTerrainHeightmap Heightmap;
	FString Error;
	if (!FDFTerrainHeightmap::Load(JsonPath, PngPath, Heightmap, Error))
	{
		UE_LOG(LogDFTerrainImport, Error, TEXT("%s: %s"), *MapId, *Error);
		return 1;
	}
	const FDFTerrainLandscapeLayout Layout = FDFTerrainLandscapeLayout::Compute(Heightmap.Meta);
	UE_LOG(LogDFTerrainImport, Display, TEXT("%s: heightmap %dx%d @ %.3g m, y in [%.3f, %.3f] m, origin (%.1f, %.1f); landscape %dx%d verts, %dx%d components of %d quads x %d sections, location (%.0f, %.0f, %.2f) cm, scale (%.0f, %.0f, %.4f)"),
		*MapId, Heightmap.Meta.Width, Heightmap.Meta.Height, Heightmap.Meta.MetresPerSample, Heightmap.Meta.MinZ, Heightmap.Meta.MaxZ, Heightmap.Meta.OriginX, Heightmap.Meta.OriginZ,
		Layout.SizeX, Layout.SizeY, Layout.ComponentCount.X, Layout.ComponentCount.Y, Layout.QuadsPerSection, Layout.SectionsPerComponent,
		Layout.Location.X, Layout.Location.Y, Layout.Location.Z, Layout.Scale.X, Layout.Scale.Y, Layout.Scale.Z);

	if (!bVerifyOnly)
	{
		UPhysicalMaterial* PhysMat = EnsureDefaultPhysicalMaterial();
		if (!PhysMat)
		{
			return 1;
		}
		if (!BuildTerrainLevel(MapId, Heightmap, Layout, PhysMat))
		{
			return 1;
		}
		if (!EnsurePersistentLevel(MapId, Heightmap))
		{
			return 1;
		}
	}
	if (!VerifyTerrainLevel(MapId, Heightmap, Layout))
	{
		return 1;
	}
	UE_LOG(LogDFTerrainImport, Display, TEXT("%s: terrain import OK — %s"), *MapId, *TerrainLevelPath(MapId));
	return 0;
}

UPhysicalMaterial* UDFTerrainImportCommandlet::EnsureDefaultPhysicalMaterial() const
{
	const FString ObjectPath = FString::Printf(TEXT("%s.%s"), GDefaultPhysMatPackage, GDefaultPhysMatName);
	if (FPackageName::DoesPackageExist(GDefaultPhysMatPackage))
	{
		UPhysicalMaterial* Existing = LoadObject<UPhysicalMaterial>(nullptr, *ObjectPath);
		if (Existing)
		{
			return Existing;
		}
		UE_LOG(LogDFTerrainImport, Warning, TEXT("%s exists but is not a PhysicalMaterial; recreating it"), GDefaultPhysMatPackage);
	}
	UPackage* Package = CreatePackage(GDefaultPhysMatPackage);
	Package->FullyLoad();
	UPhysicalMaterial* PhysMat = NewObject<UPhysicalMaterial>(Package, GDefaultPhysMatName, RF_Public | RF_Standalone);
	PhysMat->SurfaceType = SurfaceType3;
	PhysMat->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(PhysMat);
	if (!SaveAssetPackage(Package, PhysMat))
	{
		UE_LOG(LogDFTerrainImport, Error, TEXT("could not save %s"), GDefaultPhysMatPackage);
		return nullptr;
	}
	UE_LOG(LogDFTerrainImport, Display, TEXT("created %s (SurfaceType3 = Grass)"), *ObjectPath);
	return PhysMat;
}

bool UDFTerrainImportCommandlet::BuildTerrainLevel(const FString& MapId, const FDFTerrainHeightmap& Heightmap, const FDFTerrainLandscapeLayout& Layout, UPhysicalMaterial* PhysMat) const
{
	const FString TerrainPath = TerrainLevelPath(MapId);
	// A fresh untitled world every run: the sublevel is a product of the json, so "replace" is
	// simply "save over it" — the same idempotence make_dev_level.py has.
	UWorld* World = UEditorLoadingAndSavingUtils::NewBlankMap(false);
	if (!World)
	{
		UE_LOG(LogDFTerrainImport, Error, TEXT("could not create a blank world for %s"), *TerrainPath);
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = *FString::Printf(TEXT("Landscape_%s"), *MapFolderName(MapId));
	SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
	ALandscape* Landscape = World->SpawnActor<ALandscape>(ALandscape::StaticClass(), Layout.Location, FRotator::ZeroRotator, SpawnParams);
	if (!Landscape)
	{
		UE_LOG(LogDFTerrainImport, Error, TEXT("could not spawn the ALandscape for %s"), *TerrainPath);
		return false;
	}
	Landscape->SetActorRelativeScale3D(Layout.Scale);
	Landscape->DefaultPhysMaterial = PhysMat;
	// Nanite landscape (ADR-0012). bEnableNanite is a protected UPROPERTY with no setter; the
	// details panel flips it through reflection and so do we. The Nanite mesh itself is built by
	// the editor on load (it needs the RHI; this commandlet runs -nullrhi).
	if (FBoolProperty* NaniteProperty = FindFProperty<FBoolProperty>(ALandscapeProxy::StaticClass(), TEXT("bEnableNanite")))
	{
		NaniteProperty->SetPropertyValue_InContainer(Landscape, true);
	}
	else
	{
		UE_LOG(LogDFTerrainImport, Warning, TEXT("ALandscapeProxy::bEnableNanite not found — Nanite left off"));
	}

	TArray<uint16> HeightData;
	FDFTerrainLandscapeLayout::ToLandscapeData(Heightmap, Layout, HeightData);
	TMap<FGuid, TArray<uint16>> HeightDataPerLayers;
	HeightDataPerLayers.Add(FGuid(), MoveTemp(HeightData));
	TMap<FGuid, TArray<FLandscapeImportLayerInfo>> MaterialLayerDataPerLayers;
	MaterialLayerDataPerLayers.Add(FGuid(), TArray<FLandscapeImportLayerInfo>());

	// The same call the editor's New Landscape > Import button makes (LandscapeEditorDetailCustomization_NewLandscape.cpp).
	Landscape->Import(FGuid::NewGuid(), 0, 0, Layout.SizeX - 1, Layout.SizeY - 1, Layout.SectionsPerComponent, Layout.QuadsPerSection,
		HeightDataPerLayers, TEXT(""), MaterialLayerDataPerLayers, ELandscapeImportAlphamapType::Additive, TArrayView<const FLandscapeLayer>());

	ULandscapeInfo* Info = Landscape->GetLandscapeInfo();
	if (!Info)
	{
		UE_LOG(LogDFTerrainImport, Error, TEXT("Import left no ULandscapeInfo on %s"), *Landscape->GetName());
		return false;
	}
	Info->UpdateLayerInfoMap(Landscape);
	FActorLabelUtilities::SetActorLabelUnique(Landscape, SpawnParams.Name.ToString());

	if (!UEditorLoadingAndSavingUtils::SaveMap(World, TerrainPath))
	{
		UE_LOG(LogDFTerrainImport, Error, TEXT("could not save %s"), *TerrainPath);
		return false;
	}
	UE_LOG(LogDFTerrainImport, Display, TEXT("saved %s with %d landscape components"), *TerrainPath, Landscape->LandscapeComponents.Num());
	return true;
}

bool UDFTerrainImportCommandlet::EnsurePersistentLevel(const FString& MapId, const FDFTerrainHeightmap& Heightmap) const
{
	const FString PersistentPath = PersistentLevelPath(MapId);
	const FString TerrainPath = TerrainLevelPath(MapId);

	UWorld* World = nullptr;
	if (FPackageName::DoesPackageExist(PersistentPath))
	{
		// WS-09/WS-10x own this level; only its list of sublevels is touched.
		World = UEditorLoadingAndSavingUtils::LoadMap(PersistentPath);
		if (!World)
		{
			UE_LOG(LogDFTerrainImport, Error, TEXT("could not load %s"), *PersistentPath);
			return false;
		}
	}
	else
	{
		// The minimal persistent level, the way tools/ue-bridge/ue/make_dev_level.py builds
		// L_Dev_Empty — minus the floor, because the terrain is the floor.
		World = UEditorLoadingAndSavingUtils::NewBlankMap(false);
		if (!World)
		{
			UE_LOG(LogDFTerrainImport, Error, TEXT("could not create %s"), *PersistentPath);
			return false;
		}
		double GroundMetres = Heightmap.Meta.MinZ;
		const int32 OriginSample = SampleIndexAt(Heightmap.Meta, 0.0, 0.0);
		if (OriginSample != INDEX_NONE)
		{
			GroundMetres = Heightmap.SampleToMetres(Heightmap.Samples[OriginSample]);
		}
		// Sim (0,0) is the Unreal origin; stand the start 1.2 m over the ground there.
		APlayerStart* Start = World->SpawnActor<APlayerStart>(FVector(0.0, 0.0, GroundMetres * 100.0 + 120.0), FRotator::ZeroRotator);
		FActorLabelUtilities::SetActorLabelUnique(Start, TEXT("PlayerStart"));
		ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(FVector(0.0, 0.0, 500.0), FRotator(-55.0f, -30.0f, 0.0f));
		Sun->GetComponent()->SetIntensity(8.0f);
		FActorLabelUtilities::SetActorLabelUnique(Sun, TEXT("Sun"));
		ASkyAtmosphere* Sky = World->SpawnActor<ASkyAtmosphere>(FVector::ZeroVector, FRotator::ZeroRotator);
		FActorLabelUtilities::SetActorLabelUnique(Sky, TEXT("SkyAtmosphere"));
		ASkyLight* SkyLight = World->SpawnActor<ASkyLight>(FVector(0.0, 0.0, 400.0), FRotator::ZeroRotator);
		SkyLight->GetLightComponent()->bRealTimeCapture = true;
		FActorLabelUtilities::SetActorLabelUnique(SkyLight, TEXT("SkyLight"));
		AExponentialHeightFog* Fog = World->SpawnActor<AExponentialHeightFog>(FVector::ZeroVector, FRotator::ZeroRotator);
		FActorLabelUtilities::SetActorLabelUnique(Fog, TEXT("HeightFog"));
		if (!UEditorLoadingAndSavingUtils::SaveMap(World, PersistentPath))
		{
			UE_LOG(LogDFTerrainImport, Error, TEXT("could not save %s"), *PersistentPath);
			return false;
		}
		UE_LOG(LogDFTerrainImport, Display, TEXT("created minimal persistent level %s (WS-09 owns its content from here)"), *PersistentPath);
	}

	bool bPresent = false;
	for (ULevelStreaming* Streaming : World->GetStreamingLevels())
	{
		if (Streaming && Streaming->GetWorldAssetPackageName() == TerrainPath)
		{
			bPresent = true;
			break;
		}
	}
	if (bPresent)
	{
		UE_LOG(LogDFTerrainImport, Display, TEXT("%s already streams %s"), *PersistentPath, *TerrainPath);
		return true;
	}
	ULevelStreaming* Added = UEditorLevelUtils::AddLevelToWorld(World, *TerrainPath, ULevelStreamingAlwaysLoaded::StaticClass());
	if (!Added)
	{
		UE_LOG(LogDFTerrainImport, Error, TEXT("could not add %s to %s"), *TerrainPath, *PersistentPath);
		return false;
	}
	if (!UEditorLoadingAndSavingUtils::SaveMap(World, PersistentPath))
	{
		UE_LOG(LogDFTerrainImport, Error, TEXT("could not save %s after adding the terrain sublevel"), *PersistentPath);
		return false;
	}
	UE_LOG(LogDFTerrainImport, Display, TEXT("%s now streams %s (always loaded)"), *PersistentPath, *TerrainPath);
	return true;
}

bool UDFTerrainImportCommandlet::VerifyTerrainLevel(const FString& MapId, const FDFTerrainHeightmap& Heightmap, const FDFTerrainLandscapeLayout& Layout) const
{
	const FString TerrainPath = TerrainLevelPath(MapId);
	UWorld* World = UEditorLoadingAndSavingUtils::LoadMap(TerrainPath);
	if (!World)
	{
		UE_LOG(LogDFTerrainImport, Error, TEXT("verify: could not load %s"), *TerrainPath);
		return false;
	}
	// One ALandscape per terrain sublevel; the first one found is it.
	TActorIterator<ALandscape> LandscapeIt(World);
	ALandscape* Landscape = LandscapeIt ? *LandscapeIt : nullptr;
	if (!Landscape)
	{
		UE_LOG(LogDFTerrainImport, Error, TEXT("verify: %s has no ALandscape"), *TerrainPath);
		return false;
	}
	const FDFTerrainHeightmapMeta& Meta = Heightmap.Meta;
	bool bOk = true;

	// 1. Bounds: the whole landscape (padding included) must lie within the json's Z range and
	//    reach both ends of it, and its footprint must be where the frame mapping says.
	const FBox Bounds = Landscape->GetComponentsBoundingBox(true);
	UE_LOG(LogDFTerrainImport, Display, TEXT("verify: landscape bounds X [%.0f, %.0f] Y [%.0f, %.0f] Z [%.1f, %.1f] cm = y in [%.3f, %.3f] m (json [%.3f, %.3f])"),
		Bounds.Min.X, Bounds.Max.X, Bounds.Min.Y, Bounds.Max.Y, Bounds.Min.Z, Bounds.Max.Z, Bounds.Min.Z / 100.0, Bounds.Max.Z / 100.0, Meta.MinZ, Meta.MaxZ);
	const double ZTolerance = 5.0; // cm: one 16-bit step is Scale.Z/128 (~0.05 cm here); bounds may add a hair of slack
	if (FMath::Abs(Bounds.Min.Z - Meta.MinZ * 100.0) > ZTolerance || FMath::Abs(Bounds.Max.Z - Meta.MaxZ * 100.0) > ZTolerance)
	{
		UE_LOG(LogDFTerrainImport, Error, TEXT("verify: Z range mismatch"));
		bOk = false;
	}
	const FVector ExpectedMin = Layout.Location;
	const FVector ExpectedMax = Layout.Location + FVector((Layout.SizeX - 1) * Layout.Scale.X, (Layout.SizeY - 1) * Layout.Scale.Y, 0.0);
	if (FMath::Abs(Bounds.Min.X - ExpectedMin.X) > 1.0 || FMath::Abs(Bounds.Min.Y - ExpectedMin.Y) > 1.0
		|| FMath::Abs(Bounds.Max.X - ExpectedMax.X) > 1.0 || FMath::Abs(Bounds.Max.Y - ExpectedMax.Y) > 1.0)
	{
		UE_LOG(LogDFTerrainImport, Error, TEXT("verify: footprint mismatch — expected X [%.0f, %.0f] Y [%.0f, %.0f]"), ExpectedMin.X, ExpectedMax.X, ExpectedMin.Y, ExpectedMax.Y);
		bOk = false;
	}

	// 2. Samples: read the saved heightmap back (CPU source data, no RHI needed) at sim (0,0) — the
	//    Unreal origin — and at an asymmetric point, and compare with the PNG. This is the test of
	//    the transpose/flip: a wrong axis puts a different sample at the origin.
	ULandscapeInfo* Info = Landscape->GetLandscapeInfo();
	if (!Info)
	{
		UE_LOG(LogDFTerrainImport, Error, TEXT("verify: no landscape info"));
		return false;
	}
	FLandscapeEditDataInterface EditData(Info, false);
	const TArray<FVector2D> Probes = { FVector2D(0.0, 0.0), FVector2D(30.0, 10.0), FVector2D(-40.0, -4.0) };
	for (const FVector2D& Probe : Probes)
	{
		const int32 SampleIndex = SampleIndexAt(Meta, Probe.X, Probe.Y);
		if (SampleIndex == INDEX_NONE)
		{
			continue;
		}
		const FVector Unreal = FDFTerrainLandscapeLayout::SimToUnreal(Probe.X, 0.0, Probe.Y);
		const int32 VX = FMath::RoundToInt32((Unreal.X - Layout.Location.X) / Layout.Scale.X);
		const int32 VY = FMath::RoundToInt32((Unreal.Y - Layout.Location.Y) / Layout.Scale.Y);
		int32 X1 = VX, Y1 = VY, X2 = VX, Y2 = VY;
		uint16 Stored = 0;
		EditData.GetHeightData(X1, Y1, X2, Y2, &Stored, 0);
		const uint16 Expected = Heightmap.Samples[SampleIndex];
		const double StoredMetres = Layout.VertexToUnreal(VX, VY, Stored).Z / 100.0;
		const TOptional<float> Collision = Landscape->GetHeightAtLocation(FVector(Unreal.X, Unreal.Y, 0.0), EHeightfieldSource::Simple);
		UE_LOG(LogDFTerrainImport, Display, TEXT("verify: sim (%.0f, %.0f) -> Unreal (%.0f, %.0f) vertex (%d, %d): stored %u, png %u, y = %.3f m%s"),
			Probe.X, Probe.Y, Unreal.X, Unreal.Y, VX, VY, Stored, Expected, StoredMetres,
			Collision.IsSet() ? *FString::Printf(TEXT(", collision %.3f m"), Collision.GetValue() / 100.0) : TEXT(""));
		if (Stored != Expected)
		{
			UE_LOG(LogDFTerrainImport, Error, TEXT("verify: sample mismatch at sim (%.0f, %.0f)"), Probe.X, Probe.Y);
			bOk = false;
		}
	}
	return bOk;
}
