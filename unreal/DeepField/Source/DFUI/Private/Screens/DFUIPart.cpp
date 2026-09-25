#include "Screens/DFUIPart.h"

#include "ViewModels/DFViewModelSubsystem.h"

UDFMatchViewModel* UDFUIPart::GetMatch() const
{
	const UDFViewModelSubsystem* ViewModels = UDFViewModelSubsystem::Get(this);
	return ViewModels ? ViewModels->GetMatch() : nullptr;
}
