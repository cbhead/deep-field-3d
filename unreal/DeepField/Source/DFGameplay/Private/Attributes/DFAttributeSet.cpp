#include "Attributes/DFAttributeSet.h"

UWorld* UDFAttributeSet::GetOwnerWorld() const
{
	const UObject* Outer = GetOuter();
	return Outer ? Outer->GetWorld() : nullptr;
}
