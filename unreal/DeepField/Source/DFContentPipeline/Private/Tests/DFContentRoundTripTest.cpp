#include "Content/DFContentRows.h"
#include "DFContentImporter.h"
#include "DFContentJsonShaper.h"
#include "DFContentTables.h"
#include "DFContentTestUtil.h"
#include "Engine/DataTable.h"
#include "JsonObjectConverter.h"
#include "Misc/AutomationTest.h"
#include "UObject/EnumProperty.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

// For every table: the committed DataTable, re-exported row by row with FJsonObjectConverter, agrees with the
// JSON file — same rows in the same order, every JSON key on the struct, every value equal (numbers within
// 1e-4, arrays and maps deep), and every struct field the JSON omits still at its default. The first
// difference per table is named. This is the proof that the header and the schema are one contract (C2).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDFContentRoundTripTest, "DF.Content.RoundTrip", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace
{
	constexpr double GTolerance = 1e-4;

	TSharedPtr<FJsonObject> Export(const UStruct* Struct, const void* Instance)
	{
		// SkipStandardizeCase keeps property names and enum map keys exactly as authored; the comparison
		// applies the pipeline's own key rule (FDFContentJsonShaper::JsonKeyFor) instead.
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		FJsonObjectConverter::UStructToJsonObject(Struct, Instance, Object, 0, 0, nullptr, EJsonObjectConversionFlags::SkipStandardizeCase);
		return Object;
	}

	FString Short(const TSharedPtr<FJsonValue>& Value)
	{
		if (!Value.IsValid())
		{
			return TEXT("<none>");
		}
		switch (Value->Type)
		{
		case EJson::String:  return FString::Printf(TEXT("\"%s\""), *Value->AsString());
		case EJson::Number:  return LexToString(Value->AsNumber());
		case EJson::Boolean: return Value->AsBool() ? TEXT("true") : TEXT("false");
		case EJson::Array:   return FString::Printf(TEXT("[%d items]"), Value->AsArray().Num());
		case EJson::Object:  return FString::Printf(TEXT("{%d keys}"), Value->AsObject()->Values.Num());
		default:             return TEXT("null");
		}
	}

	bool NumbersEqual(double A, double B)
	{
		return FMath::Abs(A - B) <= GTolerance + 1e-6 * FMath::Abs(A);
	}

	// Structural equality with the numeric tolerance; used for "the JSON omitted it, so it must be default".
	bool JsonEqual(const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B)
	{
		if (!A.IsValid() || !B.IsValid())
		{
			return A.IsValid() == B.IsValid();
		}
		if (A->Type != B->Type)
		{
			return false;
		}
		switch (A->Type)
		{
		case EJson::Number:  return NumbersEqual(A->AsNumber(), B->AsNumber());
		case EJson::String:  return A->AsString() == B->AsString();
		case EJson::Boolean: return A->AsBool() == B->AsBool();
		case EJson::Array:
		{
			const TArray<TSharedPtr<FJsonValue>>& ArrayA = A->AsArray();
			const TArray<TSharedPtr<FJsonValue>>& ArrayB = B->AsArray();
			if (ArrayA.Num() != ArrayB.Num())
			{
				return false;
			}
			for (int32 i = 0; i < ArrayA.Num(); ++i)
			{
				if (!JsonEqual(ArrayA[i], ArrayB[i]))
				{
					return false;
				}
			}
			return true;
		}
		case EJson::Object:
		{
			const TSharedPtr<FJsonObject>& ObjectA = A->AsObject();
			const TSharedPtr<FJsonObject>& ObjectB = B->AsObject();
			if (ObjectA->Values.Num() != ObjectB->Values.Num())
			{
				return false;
			}
			for (const auto& Pair : ObjectA->Values)
			{
				if (!JsonEqual(Pair.Value, ObjectB->TryGetField(FString(Pair.Key))))
				{
					return false;
				}
			}
			return true;
		}
		default:
			return true;
		}
	}

	class FRowComparer
	{
	public:
		/** Src is the JSON row (minus "id"), Exported the table row through Export(). */
		bool CompareStruct(const UStruct* Struct, const TSharedPtr<FJsonObject>& Src, const TSharedPtr<FJsonObject>& Exported, const FString& Path, FString& OutDiff);

	private:
		bool CompareValue(const FProperty* Property, const TSharedPtr<FJsonValue>& Src, const TSharedPtr<FJsonValue>& Exp, const FString& Path, FString& OutDiff);
		bool CompareNumberMap(const TSharedPtr<FJsonObject>& Src, const TSharedPtr<FJsonObject>& Exp, const FString& Path, FString& OutDiff);
		TSharedPtr<FJsonObject> DefaultExport(const UStruct* Struct);

		TMap<const UStruct*, TSharedPtr<FJsonObject>> Defaults;
	};

	TSharedPtr<FJsonObject> FRowComparer::DefaultExport(const UStruct* Struct)
	{
		if (TSharedPtr<FJsonObject>* Found = Defaults.Find(Struct))
		{
			return *Found;
		}
		FStructOnScope Instance(Struct);
		return Defaults.Add(Struct, Export(Struct, Instance.GetStructMemory()));
	}

	bool FRowComparer::CompareStruct(const UStruct* Struct, const TSharedPtr<FJsonObject>& Src, const TSharedPtr<FJsonObject>& Exported, const FString& Path, FString& OutDiff)
	{
		const TSharedPtr<FJsonObject> Default = DefaultExport(Struct);   // by value: nested calls may grow the map
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			const FProperty* Property = *It;
			const FString Key = FDFContentJsonShaper::JsonKeyFor(Property);
			const FString Here = Path + TEXT(".") + Key;
			const TSharedPtr<FJsonValue> Exp = Exported->TryGetField(Property->GetName());
			const TSharedPtr<FJsonValue> SrcValue = Src->TryGetField(Key);
			if (!SrcValue.IsValid())
			{
				// An optional field the JSON omits must still be at the struct default in the table: anything
				// else reached the table from somewhere other than the text source of truth.
				const TSharedPtr<FJsonValue> DefaultValue = Default->TryGetField(Property->GetName());
				if (!JsonEqual(Exp, DefaultValue))
				{
					OutDiff = FString::Printf(TEXT("%s: absent from the JSON but the table holds %s (default %s)"), *Here, *Short(Exp), *Short(DefaultValue));
					return false;
				}
				continue;
			}
			if (!CompareValue(Property, SrcValue, Exp, Here, OutDiff))
			{
				return false;
			}
		}
		for (const auto& Pair : Src->Values)
		{
			const FString Key(Pair.Key);
			if (!FDFContentJsonShaper::FindProperty(Struct, Key))
			{
				OutDiff = FString::Printf(TEXT("%s.%s: key has no field on %s"), *Path, *Key, *Struct->GetName());
				return false;
			}
		}
		return true;
	}

	bool FRowComparer::CompareNumberMap(const TSharedPtr<FJsonObject>& Src, const TSharedPtr<FJsonObject>& Exp, const FString& Path, FString& OutDiff)
	{
		for (const auto& Pair : Src->Values)
		{
			const FString Key(Pair.Key);
			const TSharedPtr<FJsonValue> ExpValue = Exp->TryGetField(Key);
			if (!ExpValue.IsValid())
			{
				OutDiff = FString::Printf(TEXT("%s.%s: in the JSON but not in the table"), *Path, *Key);
				return false;
			}
			if (Pair.Value->Type != EJson::Number || ExpValue->Type != EJson::Number || !NumbersEqual(Pair.Value->AsNumber(), ExpValue->AsNumber()))
			{
				OutDiff = FString::Printf(TEXT("%s.%s: JSON %s, table %s"), *Path, *Key, *Short(Pair.Value), *Short(ExpValue));
				return false;
			}
		}
		for (const auto& Pair : Exp->Values)
		{
			const FString Key(Pair.Key);
			if (!Src->HasField(Key))
			{
				OutDiff = FString::Printf(TEXT("%s.%s: in the table (%s) but not in the JSON"), *Path, *Key, *Short(Pair.Value));
				return false;
			}
		}
		return true;
	}

	bool FRowComparer::CompareValue(const FProperty* Property, const TSharedPtr<FJsonValue>& Src, const TSharedPtr<FJsonValue>& Exp, const FString& Path, FString& OutDiff)
	{
		if (!Exp.IsValid())
		{
			OutDiff = FString::Printf(TEXT("%s: the table row did not export this field"), *Path);
			return false;
		}

		if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			const TSharedPtr<FJsonObject>* ExpObject = nullptr;
			if (!Exp->TryGetObject(ExpObject))
			{
				OutDiff = FString::Printf(TEXT("%s: the table exported %s for a struct"), *Path, *Short(Exp));
				return false;
			}
			if (FDFContentJsonShaper::IsScrapBundle(Property))
			{
				const TSharedPtr<FJsonObject>* SrcObject = nullptr;
				if (!Src->TryGetObject(SrcObject))
				{
					OutDiff = FString::Printf(TEXT("%s: JSON has %s where a scrap bundle object is expected"), *Path, *Short(Src));
					return false;
				}
				const TSharedPtr<FJsonObject>* Amounts = nullptr;
				if (!(*ExpObject)->TryGetObjectField(TEXT("Amounts"), Amounts))
				{
					OutDiff = FString::Printf(TEXT("%s: the table's scrap bundle has no Amounts"), *Path);
					return false;
				}
				return CompareNumberMap(*SrcObject, *Amounts, Path, OutDiff);
			}
			if (FDFContentJsonShaper::IsVector2D(Property))
			{
				const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
				if (!Src->TryGetArray(Items) || Items->Num() != 2)
				{
					OutDiff = FString::Printf(TEXT("%s: JSON has %s where [x, z] is expected"), *Path, *Short(Src));
					return false;
				}
				const double X = (*ExpObject)->GetNumberField(TEXT("X"));
				const double Y = (*ExpObject)->GetNumberField(TEXT("Y"));
				if (!NumbersEqual((*Items)[0]->AsNumber(), X) || !NumbersEqual((*Items)[1]->AsNumber(), Y))
				{
					OutDiff = FString::Printf(TEXT("%s: JSON [%s, %s], table [%s, %s]"), *Path, *Short((*Items)[0]), *Short((*Items)[1]), *LexToString(X), *LexToString(Y));
					return false;
				}
				return true;
			}
			if (FDFContentJsonShaper::IsLinearColor(Property))
			{
				FString Hex;
				if (!Src->TryGetString(Hex))
				{
					OutDiff = FString::Printf(TEXT("%s: JSON has %s where \"#RRGGBB\" is expected"), *Path, *Short(Src));
					return false;
				}
				const FLinearColor Want(FColor::FromHex(Hex));   // the same sRGB -> linear step the importer takes
				const FLinearColor Have((*ExpObject)->GetNumberField(TEXT("R")), (*ExpObject)->GetNumberField(TEXT("G")), (*ExpObject)->GetNumberField(TEXT("B")), (*ExpObject)->GetNumberField(TEXT("A")));
				if (!Want.Equals(Have, 1e-3f))
				{
					OutDiff = FString::Printf(TEXT("%s: JSON %s, table %s"), *Path, *Hex, *Have.ToString());
					return false;
				}
				return true;
			}
			const TSharedPtr<FJsonObject>* SrcObject = nullptr;
			if (!Src->TryGetObject(SrcObject))
			{
				OutDiff = FString::Printf(TEXT("%s: JSON has %s where a %s object is expected"), *Path, *Short(Src), *StructProperty->Struct->GetName());
				return false;
			}
			return CompareStruct(StructProperty->Struct, *SrcObject, *ExpObject, Path, OutDiff);
		}

		if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
		{
			const TArray<TSharedPtr<FJsonValue>>* SrcItems = nullptr;
			const TArray<TSharedPtr<FJsonValue>>* ExpItems = nullptr;
			if (!Src->TryGetArray(SrcItems) || !Exp->TryGetArray(ExpItems))
			{
				OutDiff = FString::Printf(TEXT("%s: JSON %s, table %s (array expected)"), *Path, *Short(Src), *Short(Exp));
				return false;
			}
			if (SrcItems->Num() != ExpItems->Num())
			{
				OutDiff = FString::Printf(TEXT("%s: JSON has %d items, table has %d"), *Path, SrcItems->Num(), ExpItems->Num());
				return false;
			}
			for (int32 i = 0; i < SrcItems->Num(); ++i)
			{
				if (!CompareValue(ArrayProperty->Inner, (*SrcItems)[i], (*ExpItems)[i], FString::Printf(TEXT("%s[%d]"), *Path, i), OutDiff))
				{
					return false;
				}
			}
			return true;
		}

		if (const FSetProperty* SetProperty = CastField<FSetProperty>(Property))
		{
			const TArray<TSharedPtr<FJsonValue>>* SrcItems = nullptr;
			const TArray<TSharedPtr<FJsonValue>>* ExpItems = nullptr;
			if (!Src->TryGetArray(SrcItems) || !Exp->TryGetArray(ExpItems) || SrcItems->Num() != ExpItems->Num())
			{
				OutDiff = FString::Printf(TEXT("%s: JSON %s, table %s"), *Path, *Short(Src), *Short(Exp));
				return false;
			}
			for (int32 i = 0; i < SrcItems->Num(); ++i)
			{
				if (!CompareValue(SetProperty->ElementProp, (*SrcItems)[i], (*ExpItems)[i], FString::Printf(TEXT("%s[%d]"), *Path, i), OutDiff))
				{
					return false;
				}
			}
			return true;
		}

		if (const FMapProperty* MapProperty = CastField<FMapProperty>(Property))
		{
			const TSharedPtr<FJsonObject>* SrcObject = nullptr;
			const TSharedPtr<FJsonObject>* ExpObject = nullptr;
			if (!Src->TryGetObject(SrcObject) || !Exp->TryGetObject(ExpObject))
			{
				OutDiff = FString::Printf(TEXT("%s: JSON %s, table %s (map expected)"), *Path, *Short(Src), *Short(Exp));
				return false;
			}
			for (const auto& Pair : (*SrcObject)->Values)
			{
				const FString Key(Pair.Key);
				const TSharedPtr<FJsonValue> ExpValue = (*ExpObject)->TryGetField(Key);
				if (!ExpValue.IsValid())
				{
					OutDiff = FString::Printf(TEXT("%s.%s: in the JSON but not in the table"), *Path, *Key);
					return false;
				}
				if (!CompareValue(MapProperty->ValueProp, Pair.Value, ExpValue, Path + TEXT(".") + Key, OutDiff))
				{
					return false;
				}
			}
			for (const auto& Pair : (*ExpObject)->Values)
			{
				const FString Key(Pair.Key);
				if (!(*SrcObject)->HasField(Key))
				{
					OutDiff = FString::Printf(TEXT("%s.%s: in the table (%s) but not in the JSON"), *Path, *Key, *Short(Pair.Value));
					return false;
				}
			}
			return true;
		}

		if (const UEnum* Enum = FDFContentJsonShaper::EnumOf(Property))
		{
			FString SrcName;
			FString ExpName;
			if (!Src->TryGetString(SrcName) || !Exp->TryGetString(ExpName) || !SrcName.Equals(ExpName, ESearchCase::IgnoreCase))
			{
				OutDiff = FString::Printf(TEXT("%s: JSON %s, table %s (%s)"), *Path, *Short(Src), *Short(Exp), *Enum->GetName());
				return false;
			}
			return true;
		}

		if (Property->IsA<FBoolProperty>())
		{
			if (Src->Type != EJson::Boolean || Exp->Type != EJson::Boolean || Src->AsBool() != Exp->AsBool())
			{
				OutDiff = FString::Printf(TEXT("%s: JSON %s, table %s"), *Path, *Short(Src), *Short(Exp));
				return false;
			}
			return true;
		}

		if (Property->IsA<FNumericProperty>())
		{
			if (Src->Type != EJson::Number || Exp->Type != EJson::Number || !NumbersEqual(Src->AsNumber(), Exp->AsNumber()))
			{
				OutDiff = FString::Printf(TEXT("%s: JSON %s, table %s"), *Path, *Short(Src), *Short(Exp));
				return false;
			}
			return true;
		}

		if (Property->IsA<FNameProperty>())
		{
			// An empty JSON string and NAME_None are the same absence.
			FString SrcName;
			FString ExpName;
			if (!Src->TryGetString(SrcName) || !Exp->TryGetString(ExpName))
			{
				OutDiff = FString::Printf(TEXT("%s: JSON %s, table %s (name expected)"), *Path, *Short(Src), *Short(Exp));
				return false;
			}
			if (SrcName.IsEmpty())
			{
				SrcName = TEXT("None");
			}
			if (!SrcName.Equals(ExpName, ESearchCase::IgnoreCase))
			{
				OutDiff = FString::Printf(TEXT("%s: JSON \"%s\", table \"%s\""), *Path, *SrcName, *ExpName);
				return false;
			}
			return true;
		}

		if (Property->IsA<FStrProperty>() || Property->IsA<FTextProperty>())
		{
			FString SrcText;
			FString ExpText;
			if (!Src->TryGetString(SrcText) || !Exp->TryGetString(ExpText) || SrcText != ExpText)
			{
				OutDiff = FString::Printf(TEXT("%s: JSON %s, table %s"), *Path, *Short(Src), *Short(Exp));
				return false;
			}
			return true;
		}

		OutDiff = FString::Printf(TEXT("%s: %s fields are not comparable"), *Path, *Property->GetClass()->GetName());
		return false;
	}
}

bool FDFContentRoundTripTest::RunTest(const FString& Parameters)
{
	const FString JsonDir = DFContentTables::DefaultJsonDir();
	const TArray<FString> Tables = DFContentTables::TablesIn(JsonDir);
	if (Tables.Num() == 0)
	{
		AddError(FString::Printf(TEXT("no content JSON under %s"), *JsonDir));
		return false;
	}

	TMap<FString, TArray<FString>> ContentIds;
	DFContentTest::LoadContentIds(*this, ContentIds);

	int32 RowsChecked = 0;
	for (const FString& Table : Tables)
	{
		UScriptStruct* RowStruct = DFContentTables::RowStructFor(Table);
		if (!RowStruct)
		{
			AddError(FString::Printf(TEXT("%s: no row struct is mapped (DFContentTables.cpp)"), *Table));
			continue;
		}
		TArray<TSharedPtr<FJsonObject>> Rows;
		FString Error;
		if (!FDFContentImporter::LoadTableJson(JsonDir, Table, Rows, Error))
		{
			AddError(Error);
			continue;
		}
		UDataTable* DataTable = DFContentTest::LoadTable(*this, Table);
		if (!DataTable)
		{
			continue;
		}
		if (DataTable->GetRowStruct() != RowStruct)
		{
			AddError(FString::Printf(TEXT("%s: the table's row struct is %s, expected %s"), *Table, *GetNameSafe(DataTable->GetRowStruct()), *RowStruct->GetName()));
			continue;
		}

		TArray<FString> Ids;
		bool bIdsOk = true;
		for (int32 i = 0; i < Rows.Num() && bIdsOk; ++i)
		{
			FString Id;
			bIdsOk = FDFContentImporter::RowId(Rows[i], Id, FString::Printf(TEXT("%s.json rows[%d]"), *Table, i), Error);
			Ids.Add(Id);
		}
		if (!bIdsOk)
		{
			AddError(Error);
			continue;
		}

		const TArray<FName> Names = DataTable->GetRowNames();
		if (Names.Num() != Ids.Num())
		{
			AddError(FString::Printf(TEXT("%s: JSON has %d rows, table has %d"), *Table, Ids.Num(), Names.Num()));
			continue;
		}
		bool bOrderOk = true;
		for (int32 i = 0; i < Names.Num(); ++i)
		{
			if (Names[i] != FName(*Ids[i]))
			{
				AddError(FString::Printf(TEXT("%s: row %d is '%s' in the table but '%s' in the JSON (rows must match in order)"), *Table, i, *Names[i].ToString(), *Ids[i]));
				bOrderOk = false;
				break;
			}
		}
		if (!bOrderOk)
		{
			continue;
		}
		if (const TArray<FString>* Listed = ContentIds.Find(Table))
		{
			if (*Listed != Ids)
			{
				AddError(FString::Printf(TEXT("%s: content-ids.json lists different ids (or a different order) than %s.json"), *Table, *Table));
			}
		}
		else
		{
			AddError(FString::Printf(TEXT("content-ids.json has no entry for %s"), *Table));
		}

		FRowComparer Comparer;
		for (int32 i = 0; i < Names.Num(); ++i)
		{
			TSharedRef<FJsonObject> Src = MakeShared<FJsonObject>();
			Src->Values = Rows[i]->Values;
			Src->RemoveField(TEXT("id"));
			if (RowStruct == FDFBalanceRow::StaticStruct())
			{
				// C2: the balance row's dials live in one name -> float map.
				TSharedRef<FJsonObject> Dials = MakeShared<FJsonObject>();
				Dials->Values = Src->Values;
				Src = MakeShared<FJsonObject>();
				Src->SetObjectField(TEXT("dials"), Dials);
			}
			const TSharedPtr<FJsonObject> Exported = Export(RowStruct, DataTable->FindRowUnchecked(Names[i]));
			FString Diff;
			if (!Comparer.CompareStruct(RowStruct, Src, Exported, Table + TEXT("/") + Ids[i], Diff))
			{
				AddError(Diff);
				break;   // the first difference per table is the useful one
			}
			++RowsChecked;
		}
	}
	AddInfo(FString::Printf(TEXT("round-tripped %d rows across %d tables"), RowsChecked, Tables.Num()));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
