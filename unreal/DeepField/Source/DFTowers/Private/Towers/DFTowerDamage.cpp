#include "Towers/DFTowerDamage.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "DFGameplayTags.h"
#include "Damage/DFDamageContext.h"
#include "Effects/DFGE_Damage.h"
#include "GameFramework/Actor.h"
#include "Status/DFStatusComponent.h"

namespace DFTowerDamage
{
	void ApplyDamage(AActor* Instigator, AActor* Target, float Amount, const FGameplayTag& DamageType, const FGameplayTag& DamageSource)
	{
		if (!IsValid(Instigator) || !IsValid(Target) || Amount <= 0.f)
		{
			return;
		}
		UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target, /*LookForComponent*/ true);
		if (!ASC)
		{
			return;
		}
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddInstigator(Instigator, Instigator);
		UDFDamageContext* Damage = UDFDamageContext::Make(Instigator, DamageType, DamageSource);
		Damage->SetSourceLocation(Instigator->GetActorLocation());
		Damage->AttachTo(Context);
		FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UDFGE_Damage::StaticClass(), 1.f, Context);
		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(DFTags::SetByCaller_Damage, Amount);
			ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
		}
	}

	void ApplyStatuses(AActor* Instigator, AActor* Target, TConstArrayView<FName> StatusIds)
	{
		if (!IsValid(Target) || StatusIds.Num() == 0)
		{
			return;
		}
		if (UDFStatusComponent* Status = Target->FindComponentByClass<UDFStatusComponent>())
		{
			for (const FName& StatusId : StatusIds)
			{
				if (!StatusId.IsNone())
				{
					Status->ApplyById(StatusId, Instigator);
				}
			}
		}
	}
}
