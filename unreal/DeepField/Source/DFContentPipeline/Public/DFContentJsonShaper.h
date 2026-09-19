#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

class FProperty;
class UEnum;
class UScriptStruct;
class UStruct;

// Turns one JSON row (camelCase keys, the C3 shapes) into the object FJsonObjectConverter::JsonObjectToUStruct
// expects for a row struct: keys renamed to the exact property names (a bool's JSON key has no 'b' prefix,
// which the converter does not strip on its own), scrap bundles wrapped into FDFScrapBundle.Amounts, [x, z]
// into FVector2D, the balance row's dials into FDFBalanceRow.Dials. Every key is checked against the struct
// on the way through: an unknown key is an error naming its path, because a silently dropped typo is the
// failure mode this pipeline exists to prevent (CONTRACTS/content-json.md).
class DFCONTENTPIPELINE_API FDFContentJsonShaper
{
public:
	/** Shapes a row whose "id" has already been removed. Returns null and fills OutError on the first problem. */
	static TSharedPtr<FJsonObject> ShapeRow(const UScriptStruct* RowStruct, const TSharedRef<FJsonObject>& Row, const FString& Path, FString& OutError);

	/** The JSON key a property is written under: bools drop their 'b' prefix, then the first letter is lowered. */
	static FString JsonKeyFor(const FProperty* Property);

	/** The property a JSON key names in a struct (an exact, case-sensitive match on JsonKeyFor), or null. */
	static const FProperty* FindProperty(const UStruct* Struct, const FString& JsonKey);

	static bool IsScrapBundle(const FProperty* Property);
	static bool IsVector2D(const FProperty* Property);
	static bool IsLinearColor(const FProperty* Property);

	/** The UEnum behind an enum-class or TEnumAsByte property, or null. */
	static const UEnum* EnumOf(const FProperty* Property);

private:
	static bool ShapeObject(const UStruct* Struct, const TSharedRef<FJsonObject>& In, const TSharedRef<FJsonObject>& Out, const FString& Path, FString& OutError);
	static bool ShapeValue(const FProperty* Property, const TSharedPtr<FJsonValue>& In, TSharedPtr<FJsonValue>& Out, const FString& Path, FString& OutError);
	static bool ShapeScrapBundle(const TSharedPtr<FJsonValue>& In, TSharedPtr<FJsonValue>& Out, const FString& Path, FString& OutError);
	static bool CheckMapKey(const FProperty* KeyProperty, const FString& Key, const FString& Path, FString& OutError);
	static bool CheckEnumName(const UEnum* Enum, const FString& Name, const FString& Path, FString& OutError);
};
