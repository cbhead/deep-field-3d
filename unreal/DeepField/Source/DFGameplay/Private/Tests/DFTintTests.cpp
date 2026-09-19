#include "DFGameplayTags.h"
#include "Misc/AutomationTest.h"
#include "Tint/DFPaletteSettings.h"
#include "Tint/DFTintComponent.h"
#include "Tint/DFTintLayout.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFTintPrecedenceTest, "DF.Unit.Tint.Precedence", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDFTintPrecedenceTest::RunTest(const FString& Parameters)
{
	// A component with no owner: it resolves and mirrors, and writes to no primitive.
	UDFTintComponent* Tint = NewObject<UDFTintComponent>(GetTransientPackage());
	const UDFPaletteSettings* Palette = GetDefault<UDFPaletteSettings>();
	const FLinearColor Chill = Palette->Status(FName(TEXT("chill")))->Tint;
	const FLinearColor Burn = Palette->Status(FName(TEXT("burn")))->Tint;
	const FLinearColor Freeze = Palette->Status(FName(TEXT("freeze")))->Tint;

	TestEqual(TEXT("no status: weight 0"), Tint->GetFloat(DFTintLayout::StatusWeight), 0.f);
	TestFalse(TEXT("no status: no winner"), Tint->GetStatusTintSource().IsValid());

	// Movement alone.
	Tint->SetStatus(EDFStatusChannel::Movement, DFTags::Status_Chill, 0.35f);
	TestEqual(TEXT("chill wins alone"), Tint->GetStatusTintSource(), FGameplayTag(DFTags::Status_Chill));
	TestTrue(TEXT("StatusTint = chill"), Tint->GetColor(DFTintLayout::StatusTintR).Equals(Chill, 1e-4f));
	TestEqual(TEXT("weight 0.7"), Tint->GetFloat(DFTintLayout::StatusWeight), 0.7f);
	TestTrue(TEXT("chill has no emissive"), Tint->GetColor(DFTintLayout::StatusEmissiveR).Equals(FLinearColor::Black, 1e-4f));

	// Thermal beats Movement, and burn glows.
	Tint->SetStatus(EDFStatusChannel::Thermal, DFTags::Status_Burn, 6.f);
	TestEqual(TEXT("burn over chill"), Tint->GetStatusTintSource(), FGameplayTag(DFTags::Status_Burn));
	TestTrue(TEXT("StatusTint = burn"), Tint->GetColor(DFTintLayout::StatusTintR).Equals(Burn, 1e-4f));
	TestTrue(TEXT("burn emissive = burn"), Tint->GetColor(DFTintLayout::StatusEmissiveR).Equals(Burn, 1e-4f));

	// Control beats both.
	Tint->SetStatus(EDFStatusChannel::Control, DFTags::Status_Freeze, 1.2f);
	TestEqual(TEXT("freeze over burn"), Tint->GetStatusTintSource(), FGameplayTag(DFTags::Status_Freeze));
	TestTrue(TEXT("StatusTint = freeze"), Tint->GetColor(DFTintLayout::StatusTintR).Equals(Freeze, 1e-4f));
	TestEqual(TEXT("FreezeAmount 1"), Tint->GetFloat(DFTintLayout::FreezeAmount), 1.f);
	TestTrue(TEXT("freeze has no emissive"), Tint->GetColor(DFTintLayout::StatusEmissiveR).Equals(FLinearColor::Black, 1e-4f));

	// Clearing walks back down the order.
	Tint->ClearStatus(EDFStatusChannel::Control);
	TestEqual(TEXT("burn again"), Tint->GetStatusTintSource(), FGameplayTag(DFTags::Status_Burn));
	TestEqual(TEXT("FreezeAmount 0"), Tint->GetFloat(DFTintLayout::FreezeAmount), 0.f);
	Tint->ClearStatus(EDFStatusChannel::Thermal);
	TestEqual(TEXT("chill again"), Tint->GetStatusTintSource(), FGameplayTag(DFTags::Status_Chill));
	Tint->ClearStatus(EDFStatusChannel::Movement);
	TestFalse(TEXT("nothing left"), Tint->GetStatusTintSource().IsValid());
	TestEqual(TEXT("weight back to 0"), Tint->GetFloat(DFTintLayout::StatusWeight), 0.f);

	// Mark and reveal are emissive-only: they never repaint and coexist with a tint.
	Tint->SetStatus(EDFStatusChannel::Movement, DFTags::Status_Chill, 0.35f);
	Tint->SetMarked(true);
	TestEqual(TEXT("MarkEmissive 1"), Tint->GetFloat(DFTintLayout::MarkEmissive), 1.f);
	TestEqual(TEXT("mark leaves the chill tint"), Tint->GetStatusTintSource(), FGameplayTag(DFTags::Status_Chill));
	Tint->SetRevealed(true);
	TestEqual(TEXT("RevealEmissive 1"), Tint->GetFloat(DFTintLayout::RevealEmissive), 1.f);
	Tint->SetMarked(false);
	Tint->SetRevealed(false);
	// ...and the Vulnerability / Detection channels drive the same flags.
	Tint->SetStatus(EDFStatusChannel::Vulnerability, DFTags::Status_Mark, 0.25f);
	TestEqual(TEXT("Vulnerability channel marks"), Tint->GetFloat(DFTintLayout::MarkEmissive), 1.f);
	TestEqual(TEXT("mark in Vulnerability does not become the tint"), Tint->GetStatusTintSource(), FGameplayTag(DFTags::Status_Chill));
	Tint->SetStatus(EDFStatusChannel::Detection, DFTags::Status_Reveal, 1.f);
	TestEqual(TEXT("Detection channel reveals"), Tint->GetFloat(DFTintLayout::RevealEmissive), 1.f);
	Tint->ClearStatus(EDFStatusChannel::Vulnerability);
	TestEqual(TEXT("MarkEmissive 0"), Tint->GetFloat(DFTintLayout::MarkEmissive), 0.f);

	// Poison in Toxin sets PoisonAmount without a tint of its own.
	Tint->ClearStatus(EDFStatusChannel::Movement);
	Tint->SetStatus(EDFStatusChannel::Toxin, DFTags::Status_Poison, 3.f);
	TestEqual(TEXT("PoisonAmount 1"), Tint->GetFloat(DFTintLayout::PoisonAmount), 1.f);
	TestFalse(TEXT("poison is not a StatusTint"), Tint->GetStatusTintSource().IsValid());

	// The other C8 setters land at their layout indices.
	Tint->SetHp(0.4f);
	TestEqual(TEXT("HpFrac"), Tint->GetFloat(DFTintLayout::HpFrac), 0.4f);
	Tint->SetElite(DFTags::Enemy_Elite_Gilded);
	TestTrue(TEXT("gilded rim"), Tint->GetColor(DFTintLayout::EliteRimColorR).Equals(Palette->EliteRim(FName(TEXT("gilded"))), 1e-4f));
	TestEqual(TEXT("rim power on"), Tint->GetFloat(DFTintLayout::EliteRimPower), 1.f);
	Tint->SetStealth(0.5f);
	TestEqual(TEXT("Stealth"), Tint->GetFloat(DFTintLayout::Stealth), 0.5f);
	Tint->SetEnergy(Palette->TowerEnergyHue(FName(TEXT("lance"))), 0.95f);
	TestTrue(TEXT("EnergyHue = lance"), Tint->GetColor(DFTintLayout::EnergyHueR).Equals(Palette->TowerEnergyHue(FName(TEXT("lance"))), 1e-4f));
	TestEqual(TEXT("EnergyIntensity"), Tint->GetFloat(DFTintLayout::EnergyIntensity), 0.95f);
	Tint->SetDamage(0.3f);
	Tint->SetWear(0.6f);
	Tint->SetPhase(2);
	Tint->SetDissolve(0.1f);
	TestEqual(TEXT("Damage"), Tint->GetFloat(DFTintLayout::Damage), 0.3f);
	TestEqual(TEXT("Wear"), Tint->GetFloat(DFTintLayout::Wear), 0.6f);
	TestEqual(TEXT("Phase"), Tint->GetFloat(DFTintLayout::Phase), 2.f);
	TestEqual(TEXT("Dissolve"), Tint->GetFloat(DFTintLayout::Dissolve), 0.1f);

	// The palette is palette.json: spot-check a transcription.
	TestTrue(TEXT("burn is #E8622B"), Burn.Equals(UDFPaletteSettings::FromHex(TEXT("#E8622B")), 1e-4f));
	TestTrue(TEXT("mark is emissive-only"), !Palette->Status(FName(TEXT("mark")))->bHasTint && Palette->Status(FName(TEXT("mark")))->bEmissive);
	TestEqual(TEXT("layout fits the CPD budget"), DFTintLayout::NumFloats <= 36, true);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
