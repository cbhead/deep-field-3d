#include "Rig/DFTowerRigComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Rig/DFTowerDefinition.h"
#include "Towers/DFTowerMath.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DFTowerRigComponent)

namespace
{
	/** Socket names (naming.md). */
	const FName SocketYaw(TEXT("S_Yaw"));
	const FName SocketPitch(TEXT("S_Pitch"));
	const FName SocketMuzzle(TEXT("S_Muzzle"));
	const FName SocketSpin(TEXT("S_Spin"));

	/** The search sweep's half-width, degrees: enough to read as looking, small enough not to swing wide. */
	constexpr float WobbleAmplitudeDeg = 6.f;
}

UDFTowerRigComponent::UDFTowerRigComponent()
{
	PrimaryComponentTick.bCanEverTick = false;   // the tower drives it
	SetIsReplicatedByDefault(false);             // cosmetic: every machine turns its own turret
}

void UDFTowerRigComponent::Configure(FName TowerId, EDFTowerKind Kind, const UDFTowerDefinition* InDefinition)
{
	switch (Kind)
	{
	case EDFTowerKind::Barricade:
		Style = EDFTowerRigStyle::None;
		break;
	case EDFTowerKind::Aura:
	case EDFTowerKind::Tesla:
	case EDFTowerKind::Support:
		Style = EDFTowerRigStyle::Spin;
		break;
	default:
		Style = EDFTowerRigStyle::Turret;
		break;
	}
	Definition = InDefinition;
	Limits = InDefinition ? InDefinition->Rig : DFTowerRig::ManifestLimitsFor(TowerId);
	YawDeg = 0.f;
	PitchDeg = FMath::Clamp(0.f, Limits.PitchMinDeg, Limits.PitchMaxDeg);   // rest level, or at the floor (nova rests at 22)
	SpinDeg = 0.f;
	WobbleRemaining = 0.f;

	DestroyParts();
	if (InDefinition)
	{
		Foot = MakePart(TEXT("Foot"), InDefinition->FootMesh, this, NAME_None);
		if (Style == EDFTowerRigStyle::Turret)
		{
			Yaw = MakePart(TEXT("Yaw"), InDefinition->YawMesh, Foot ? static_cast<USceneComponent*>(Foot) : this, SocketYaw);
			Pitch = MakePart(TEXT("Pitch"), InDefinition->PitchMesh, Yaw ? static_cast<USceneComponent*>(Yaw) : this, SocketPitch);
			if (Pitch && GetOwner())
			{
				Muzzle = NewObject<USceneComponent>(GetOwner(), MakeUniqueObjectName(GetOwner(), USceneComponent::StaticClass(), TEXT("Muzzle")));
				Muzzle->SetupAttachment(Pitch, SocketMuzzle);
				Muzzle->RegisterComponent();
			}
		}
		else if (Style == EDFTowerRigStyle::Spin)
		{
			Spin = MakePart(TEXT("Spin"), InDefinition->SpinMesh, Foot ? static_cast<USceneComponent*>(Foot) : this, SocketSpin);
		}
	}
	ApplyAngles();
}

UStaticMeshComponent* UDFTowerRigComponent::MakePart(FName Name, const TSoftObjectPtr<UStaticMesh>& Mesh, USceneComponent* Parent, FName Socket)
{
	AActor* Owner = GetOwner();
	if (!Owner || Mesh.IsNull())
	{
		return nullptr;
	}
	UStaticMesh* Loaded = Mesh.LoadSynchronous();   // a handful of small meshes, once per build
	if (!Loaded)
	{
		return nullptr;
	}
	UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(Owner, MakeUniqueObjectName(Owner, UStaticMeshComponent::StaticClass(), Name));
	Part->SetStaticMesh(Loaded);
	Part->SetMobility(EComponentMobility::Movable);
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);   // looks only; the pad and the structure's hit volume do the colliding
	Part->SetupAttachment(Parent, Socket);
	Part->RegisterComponent();
	return Part;
}

void UDFTowerRigComponent::DestroyParts()
{
	for (TPair<FName, TArray<TObjectPtr<UStaticMeshComponent>>>& Pair : StageParts)
	{
		for (UStaticMeshComponent* Module : Pair.Value)
		{
			if (Module)
			{
				Module->DestroyComponent();
			}
		}
	}
	StageParts.Reset();
	for (USceneComponent* Part : { static_cast<USceneComponent*>(Muzzle), static_cast<USceneComponent*>(Pitch),
		static_cast<USceneComponent*>(Yaw), static_cast<USceneComponent*>(Spin), static_cast<USceneComponent*>(Foot) })
	{
		if (Part)
		{
			Part->DestroyComponent();
		}
	}
	Muzzle = nullptr;
	Pitch = nullptr;
	Yaw = nullptr;
	Spin = nullptr;
	Foot = nullptr;
}

USceneComponent* UDFTowerRigComponent::PartByName(FName Part) const
{
	if (Part == TEXT("Pitch") && Pitch) { return Pitch; }
	if (Part == TEXT("Yaw") && Yaw) { return Yaw; }
	if (Part == TEXT("Spin") && Spin) { return Spin; }
	return Foot;
}

void UDFTowerRigComponent::ApplyAngles()
{
	if (Yaw)
	{
		Yaw->SetRelativeRotation(FRotator(0.f, YawDeg, 0.f));
	}
	if (Pitch)
	{
		Pitch->SetRelativeRotation(FRotator(PitchDeg * Limits.PitchSign, 0.f, 0.f));
	}
	if (Spin)
	{
		Spin->SetRelativeRotation(FRotator(0.f, SpinDeg, 0.f));
	}
}

FVector UDFTowerRigComponent::GetPivotLocation() const
{
	if (Yaw)
	{
		return Yaw->GetComponentLocation();
	}
	const AActor* Owner = GetOwner();
	return (Owner ? Owner->GetActorLocation() : GetComponentLocation()) + FVector(0.f, 0.f, DFTowerMath::ShotMuzzleHeightCm);
}

FVector UDFTowerRigComponent::GetMuzzleLocation() const
{
	return Muzzle ? Muzzle->GetComponentLocation() : GetPivotLocation();
}

bool UDFTowerRigComponent::AimAt(const FVector& TargetCm, float DeltaSeconds)
{
	switch (Style)
	{
	case EDFTowerRigStyle::None:
		return true;
	case EDFTowerRigStyle::Spin:
		SpinDeg = FRotator::NormalizeAxis(SpinDeg + DFTowerRig::SpinEngagedDegPerSec * DeltaSeconds);
		ApplyAngles();
		return true;
	default:
		break;
	}
	const AActor* Owner = GetOwner();
	FVector2D Desired = DFTowerRig::DesiredAngles(GetPivotLocation(), Owner ? static_cast<float>(Owner->GetActorRotation().Yaw) : 0.f, TargetCm);
	bool bSearching = false;
	if (WobbleRemaining > 0.f)
	{
		WobbleRemaining = FMath::Max(0.f, WobbleRemaining - DeltaSeconds);
		const float Phase = WobbleTotal > 0.f ? 1.f - WobbleRemaining / WobbleTotal : 1.f;
		Desired.X += WobbleAmplitudeDeg * FMath::Sin(Phase * 2.f * PI);   // one sweep left and right, ending on the aim
		bSearching = WobbleRemaining > 0.f;
	}
	const bool bSettled = DFTowerRig::Slew(YawDeg, PitchDeg, Desired, Limits, DeltaSeconds);
	ApplyAngles();
	return bSettled && !bSearching;
}

void UDFTowerRigComponent::Idle(float DeltaSeconds)
{
	if (Style == EDFTowerRigStyle::Spin)
	{
		SpinDeg = FRotator::NormalizeAxis(SpinDeg + DFTowerRig::SpinIdleDegPerSec * DeltaSeconds);
		ApplyAngles();
	}
	WobbleRemaining = 0.f;
}

void UDFTowerRigComponent::StartSearchWobble(float Seconds)
{
	if (Style == EDFTowerRigStyle::Turret && Seconds > 0.f)
	{
		WobbleRemaining = Seconds;
		WobbleTotal = Seconds;
	}
}

void UDFTowerRigComponent::SetPathLevels(const FDFTowerRow& Row, TConstArrayView<int32> PathLevels)
{
	const UDFTowerDefinition* Def = Definition.Get();
	for (int32 PathIndex = 0; PathIndex < Row.UpgradePaths.Num(); ++PathIndex)
	{
		const FName PathId = Row.UpgradePaths[PathIndex].Id;
		const int32 Purchases = DFTowerMath::PurchasesOn(PathLevels, PathIndex);   // level N shows S02..SN: N-1 modules
		const FDFTowerStageSet* Set = Def ? Def->Stages.Find(PathId) : nullptr;
		TArray<TObjectPtr<UStaticMeshComponent>>& Attached = StageParts.FindOrAdd(PathId);
		const int32 Want = Set ? FMath::Min(Purchases, Set->Modules.Num()) : 0;
		while (Attached.Num() > Want)   // a sale or a rebuild takes modules off
		{
			if (UStaticMeshComponent* Module = Attached.Pop())
			{
				Module->DestroyComponent();
			}
		}
		while (Attached.Num() < Want)
		{
			const int32 Stage = Attached.Num();   // 0 = S02
			USceneComponent* Mount = PartByName(Set->MountPart);
			const FName Socket(*FString::Printf(TEXT("S_Stage_%s"), *PathId.ToString()));
			UStaticMeshComponent* Module = MakePart(FName(*FString::Printf(TEXT("Stage_%s_S%02d"), *PathId.ToString(), Stage + 2)),
				Set->Modules[Stage], Mount ? Mount : static_cast<USceneComponent*>(this), Socket);
			Attached.Add(Module);   // a null (mesh not imported) still counts, so the levels stay aligned
		}
	}
}

int32 UDFTowerRigComponent::GetAttachedStageCount(FName PathId) const
{
	const TArray<TObjectPtr<UStaticMeshComponent>>* Attached = StageParts.Find(PathId);
	return Attached ? Attached->Num() : 0;
}
