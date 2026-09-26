#include "Status/DFReactionResolver.h"

void FDFReactionResolver::Add(FName Id, const FDFReactionRow& Row)
{
	for (FDFReactionEntry& Entry : Reactions)
	{
		if (Entry.Id == Id)
		{
			Entry.Row = Row;
			return;
		}
	}
	FDFReactionEntry& Entry = Reactions.AddDefaulted_GetRef();
	Entry.Id = Id;
	Entry.Row = Row;
}

const FDFReactionEntry* FDFReactionResolver::Match(FName Active, FName Incoming) const
{
	if (Active.IsNone() || Incoming.IsNone())
	{
		return nullptr;
	}
	for (const FDFReactionEntry& Entry : Reactions)
	{
		const FDFReactionRow& R = Entry.Row;
		if ((R.StatusA == Active && R.StatusB == Incoming) || (R.StatusB == Active && R.StatusA == Incoming))
		{
			return &Entry;
		}
	}
	return nullptr;
}
