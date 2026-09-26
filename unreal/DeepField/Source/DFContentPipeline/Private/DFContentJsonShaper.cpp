#include "DFContentJsonShaper.h"

#include "Content/DFContentRows.h"
#include "UObject/EnumProperty.h"
#include "UObject/UnrealType.h"

namespace
{
	FString Describe(const TSharedPtr<FJsonValue>& Value)
	{
		if (!Value.IsValid())
		{
			return TEXT("nothing");
		}
		switch (Value->Type)
		{
		case EJson::String:  return TEXT("a string");
		case EJson::Number:  return TEXT("a number");
		case EJson::Boolean: return TEXT("a bool");
		case EJson::Array:   return TEXT("an array");
		case EJson::Object:  return TEXT("an object");
		case EJson::Null:    return TEXT("null");
		default:             return TEXT("nothing");
		}
	}

	FString Join(const FString& Path, const FString& Key)
	{
		return Path.IsEmpty() ? Key : Path + TEXT(".") + Key;
	}

	FString EnumNames(const UEnum* Enum)
	{
		TArray<FString> Names;
		for (int32 i = 0; i < Enum->NumEnums() - 1; ++i)   // the last entry is the generated _MAX
		{
			Names.Add(Enum->GetNameStringByIndex(i));
		}
		return FString::Join(Names, TEXT(", "));
	}
}

FString FDFContentJsonShaper::JsonKeyFor(const FProperty* Property)
{
	FString Key = Property->GetName();
	if (Property->IsA<FBoolProperty>() && Key.Len() > 1 && Key[0] == TEXT('b') && FChar::IsUpper(Key[1]))
	{
		Key.RightChopInline(1);
	}
	Key[0] = FChar::ToLower(Key[0]);
	return Key;
}

const FProperty* FDFContentJsonShaper::FindProperty(const UStruct* Struct, const FString& JsonKey)
{
	for (TFieldIterator<FProperty> It(Struct); It; ++It)
	{
		// Exact match: C3 says keys are camelCase, and the schema validator rejects "Cost", so the importer must
		// too — a key that only matches by case is a typo this pipeline exists to catch, not a lenient success.
		if (JsonKeyFor(*It).Equals(JsonKey, ESearchCase::CaseSensitive))
		{
			return *It;
		}
	}
	return nullptr;
}

bool FDFContentJsonShaper::IsScrapBundle(const FProperty* Property)
{
	const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
	return StructProperty && StructProperty->Struct == FDFScrapBundle::StaticStruct();
}

bool FDFContentJsonShaper::IsVector2D(const FProperty* Property)
{
	const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
	return StructProperty && StructProperty->Struct == TBaseStructure<FVector2D>::Get();
}

bool FDFContentJsonShaper::IsLinearColor(const FProperty* Property)
{
	const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
	return StructProperty && StructProperty->Struct == TBaseStructure<FLinearColor>::Get();
}

const UEnum* FDFContentJsonShaper::EnumOf(const FProperty* Property)
{
	if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
	{
		return EnumProperty->GetEnum();
	}
	if (const FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
	{
		return ByteProperty->Enum;
	}
	return nullptr;
}

TSharedPtr<FJsonObject> FDFContentJsonShaper::ShapeRow(const UScriptStruct* RowStruct, const TSharedRef<FJsonObject>& Row, const FString& Path, FString& OutError)
{
	TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
	if (RowStruct == FDFBalanceRow::StaticStruct())
	{
		// balance.json is one flat row of dials; the struct keeps them as a name -> float map so that a new
		// dial is not a schema change (DFContentRows.h). Every key is therefore a dial, and must be a number.
		TSharedRef<FJsonObject> Dials = MakeShared<FJsonObject>();
		for (const auto& Pair : Row->Values)
		{
			if (!Pair.Value.IsValid() || Pair.Value->Type != EJson::Number)
			{
				OutError = FString::Printf(TEXT("%s: dial '%s' must be a number, got %s"), *Path, *Pair.Key, *Describe(Pair.Value));
				return nullptr;
			}
			Dials->SetField(Pair.Key, Pair.Value);
		}
		Out->SetObjectField(TEXT("Dials"), Dials);
		return Out;
	}
	return ShapeObject(RowStruct, Row, Out, Path, OutError) ? TSharedPtr<FJsonObject>(Out) : nullptr;
}

bool FDFContentJsonShaper::ShapeObject(const UStruct* Struct, const TSharedRef<FJsonObject>& In, const TSharedRef<FJsonObject>& Out, const FString& Path, FString& OutError)
{
	for (const auto& Pair : In->Values)
	{
		const FProperty* Property = FindProperty(Struct, FString(Pair.Key));
		if (!Property)
		{
			OutError = FString::Printf(TEXT("%s: unknown key '%s' (%s has no such field)"), *Path, *Pair.Key, *Struct->GetName());
			return false;
		}
		TSharedPtr<FJsonValue> Shaped;
		if (!ShapeValue(Property, Pair.Value, Shaped, Join(Path, FString(Pair.Key)), OutError))
		{
			return false;
		}
		Out->SetField(Property->GetName(), Shaped);
	}
	return true;
}

bool FDFContentJsonShaper::ShapeScrapBundle(const TSharedPtr<FJsonValue>& In, TSharedPtr<FJsonValue>& Out, const FString& Path, FString& OutError)
{
	const TSharedPtr<FJsonObject>* Bundle = nullptr;
	if (!In->TryGetObject(Bundle))
	{
		OutError = FString::Printf(TEXT("%s: expected a scrap bundle {\"Alloy\": n, ...}, got %s"), *Path, *Describe(In));
		return false;
	}
	const UEnum* ScrapEnum = StaticEnum<EDFScrapType>();
	for (const auto& Pair : (*Bundle)->Values)
	{
		if (!CheckEnumName(ScrapEnum, FString(Pair.Key), Join(Path, FString(Pair.Key)), OutError))
		{
			return false;
		}
		if (!Pair.Value.IsValid() || Pair.Value->Type != EJson::Number)
		{
			OutError = FString::Printf(TEXT("%s: scrap count must be a number, got %s"), *Join(Path, FString(Pair.Key)), *Describe(Pair.Value));
			return false;
		}
	}
	// {"Alloy": 2} is the JSON spelling of FDFScrapBundle{ Amounts = {Alloy: 2} }.
	TSharedRef<FJsonObject> Wrapped = MakeShared<FJsonObject>();
	Wrapped->SetObjectField(TEXT("Amounts"), *Bundle);
	Out = MakeShared<FJsonValueObject>(Wrapped);
	return true;
}

bool FDFContentJsonShaper::CheckEnumName(const UEnum* Enum, const FString& Name, const FString& Path, FString& OutError)
{
	if (Enum->GetIndexByNameString(Name) == INDEX_NONE)
	{
		OutError = FString::Printf(TEXT("%s: '%s' is not a %s (one of: %s)"), *Path, *Name, *Enum->GetName(), *EnumNames(Enum));
		return false;
	}
	return true;
}

bool FDFContentJsonShaper::CheckMapKey(const FProperty* KeyProperty, const FString& Key, const FString& Path, FString& OutError)
{
	if (const UEnum* Enum = EnumOf(KeyProperty))
	{
		return CheckEnumName(Enum, Key, Join(Path, Key), OutError);
	}
	if (const FNumericProperty* Numeric = CastField<FNumericProperty>(KeyProperty))
	{
		// JSON object keys are strings; "4" is how an int32 key is spelled (towers.upgradePaths.breakpointRecipes).
		if (!Key.IsNumeric() || (Numeric->IsInteger() && Key.Contains(TEXT("."))))
		{
			OutError = FString::Printf(TEXT("%s: map key '%s' must be an integer"), *Path, *Key);
			return false;
		}
		return true;
	}
	// FName / FString keys accept anything.
	return true;
}

bool FDFContentJsonShaper::ShapeValue(const FProperty* Property, const TSharedPtr<FJsonValue>& In, TSharedPtr<FJsonValue>& Out, const FString& Path, FString& OutError)
{
	if (!In.IsValid() || In->Type == EJson::Null)
	{
		OutError = FString::Printf(TEXT("%s: null is not a value (omit the key instead)"), *Path);
		return false;
	}

	if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
	{
		if (IsScrapBundle(Property))
		{
			return ShapeScrapBundle(In, Out, Path, OutError);
		}
		if (IsVector2D(Property))
		{
			// C3: [x, z] metres. A field size, not a position, so no frame conversion: FVector2D(x, z).
			const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
			if (!In->TryGetArray(Items) || Items->Num() != 2 || (*Items)[0]->Type != EJson::Number || (*Items)[1]->Type != EJson::Number)
			{
				OutError = FString::Printf(TEXT("%s: expected [x, z] (two numbers), got %s"), *Path, *Describe(In));
				return false;
			}
			TSharedRef<FJsonObject> Vector = MakeShared<FJsonObject>();
			Vector->SetNumberField(TEXT("X"), (*Items)[0]->AsNumber());
			Vector->SetNumberField(TEXT("Y"), (*Items)[1]->AsNumber());
			Out = MakeShared<FJsonValueObject>(Vector);
			return true;
		}
		if (IsLinearColor(Property))
		{
			// "#RRGGBB" (C3). FJsonObjectConverter decodes the hex itself; only the shape is checked here.
			FString Hex;
			if (!In->TryGetString(Hex) || !Hex.StartsWith(TEXT("#")) || (Hex.Len() != 7 && Hex.Len() != 9))
			{
				OutError = FString::Printf(TEXT("%s: expected a \"#RRGGBB\" colour, got %s"), *Path, *Describe(In));
				return false;
			}
			Out = In;
			return true;
		}
		const TSharedPtr<FJsonObject>* Object = nullptr;
		if (!In->TryGetObject(Object))
		{
			OutError = FString::Printf(TEXT("%s: expected an object for %s, got %s"), *Path, *StructProperty->Struct->GetName(), *Describe(In));
			return false;
		}
		TSharedRef<FJsonObject> Shaped = MakeShared<FJsonObject>();
		if (!ShapeObject(StructProperty->Struct, Object->ToSharedRef(), Shaped, Path, OutError))
		{
			return false;
		}
		Out = MakeShared<FJsonValueObject>(Shaped);
		return true;
	}

	if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
	{
		const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
		if (!In->TryGetArray(Items))
		{
			OutError = FString::Printf(TEXT("%s: expected an array, got %s"), *Path, *Describe(In));
			return false;
		}
		TArray<TSharedPtr<FJsonValue>> Shaped;
		Shaped.Reserve(Items->Num());
		for (int32 i = 0; i < Items->Num(); ++i)
		{
			TSharedPtr<FJsonValue> Item;
			if (!ShapeValue(ArrayProperty->Inner, (*Items)[i], Item, FString::Printf(TEXT("%s[%d]"), *Path, i), OutError))
			{
				return false;
			}
			Shaped.Add(Item);
		}
		Out = MakeShared<FJsonValueArray>(Shaped);
		return true;
	}

	if (const FSetProperty* SetProperty = CastField<FSetProperty>(Property))
	{
		const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
		if (!In->TryGetArray(Items))
		{
			OutError = FString::Printf(TEXT("%s: expected an array, got %s"), *Path, *Describe(In));
			return false;
		}
		TArray<TSharedPtr<FJsonValue>> Shaped;
		for (int32 i = 0; i < Items->Num(); ++i)
		{
			TSharedPtr<FJsonValue> Item;
			if (!ShapeValue(SetProperty->ElementProp, (*Items)[i], Item, FString::Printf(TEXT("%s[%d]"), *Path, i), OutError))
			{
				return false;
			}
			Shaped.Add(Item);
		}
		Out = MakeShared<FJsonValueArray>(Shaped);
		return true;
	}

	if (const FMapProperty* MapProperty = CastField<FMapProperty>(Property))
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		if (!In->TryGetObject(Object))
		{
			OutError = FString::Printf(TEXT("%s: expected an object (a map), got %s"), *Path, *Describe(In));
			return false;
		}
		TSharedRef<FJsonObject> Shaped = MakeShared<FJsonObject>();
		for (const auto& Pair : (*Object)->Values)
		{
			if (!CheckMapKey(MapProperty->KeyProp, FString(Pair.Key), Path, OutError))
			{
				return false;
			}
			TSharedPtr<FJsonValue> Value;
			if (!ShapeValue(MapProperty->ValueProp, Pair.Value, Value, Join(Path, FString(Pair.Key)), OutError))
			{
				return false;
			}
			Shaped->SetField(Pair.Key, Value);
		}
		Out = MakeShared<FJsonValueObject>(Shaped);
		return true;
	}

	// Enums before numerics: a TEnumAsByte is an FByteProperty, which is numeric.
	if (const UEnum* Enum = EnumOf(Property))
	{
		FString Name;
		if (!In->TryGetString(Name))
		{
			OutError = FString::Printf(TEXT("%s: expected a %s name (one of: %s), got %s"), *Path, *Enum->GetName(), *EnumNames(Enum), *Describe(In));
			return false;
		}
		if (!CheckEnumName(Enum, Name, Path, OutError))
		{
			return false;
		}
		Out = In;
		return true;
	}

	if (Property->IsA<FBoolProperty>())
	{
		if (In->Type != EJson::Boolean)
		{
			OutError = FString::Printf(TEXT("%s: expected true/false, got %s"), *Path, *Describe(In));
			return false;
		}
		Out = In;
		return true;
	}

	if (const FNumericProperty* Numeric = CastField<FNumericProperty>(Property))
	{
		if (In->Type != EJson::Number)
		{
			OutError = FString::Printf(TEXT("%s: expected a number, got %s"), *Path, *Describe(In));
			return false;
		}
		const double Value = In->AsNumber();
		if (Numeric->IsInteger() && !FMath::IsNearlyEqual(Value, FMath::RoundToDouble(Value)))
		{
			OutError = FString::Printf(TEXT("%s: expected an integer, got %g"), *Path, Value);
			return false;
		}
		Out = In;
		return true;
	}

	if (Property->IsA<FStrProperty>() || Property->IsA<FNameProperty>() || Property->IsA<FTextProperty>())
	{
		if (In->Type != EJson::String)
		{
			OutError = FString::Printf(TEXT("%s: expected a string, got %s"), *Path, *Describe(In));
			return false;
		}
		Out = In;
		return true;
	}

	OutError = FString::Printf(TEXT("%s: %s fields are not importable from JSON"), *Path, *Property->GetClass()->GetName());
	return false;
}
