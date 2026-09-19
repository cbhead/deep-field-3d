#include "Content/DFContentDefinition.h"

#include "DFGameplayTags.h"

void UDFContentDefinition::PostLoad()
{
	Super::PostLoad();
	if (!ContentId.IsNone() && PrimaryType.IsValid())
	{
		ContentTag = DFTags::ForContentId(TEXT("DF.") + PrimaryType.ToString(), ContentId);
	}
}
