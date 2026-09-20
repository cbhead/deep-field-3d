#include "Screens/DFUILayout.h"

#include "Screens/DFActivatableScreen.h"
#include "Screens/DFUITags.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

DEFINE_LOG_CATEGORY_STATIC(LogDFUILayout, Log, All);

bool UDFUILayout::RegisterLayer(FGameplayTag LayerTag, UCommonActivatableWidgetContainerBase* Container)
{
	if (!Container || !FDFUITags::Get().Layers().Contains(LayerTag))
	{
		UE_LOG(LogDFUILayout, Error, TEXT("%s: cannot register layer '%s' (%s)"), *GetName(), *LayerTag.ToString(), Container ? TEXT("not a DF.UI.Layer tag") : TEXT("no container"));
		return false;
	}
	if (const TObjectPtr<UCommonActivatableWidgetContainerBase>* Existing = Layers.Find(LayerTag); Existing && *Existing != Container)
	{
		UE_LOG(LogDFUILayout, Error, TEXT("%s: layer '%s' already has a container (%s)"), *GetName(), *LayerTag.ToString(), *GetNameSafe(*Existing));
		return false;
	}
	Layers.Add(LayerTag, Container);
	return true;
}

UCommonActivatableWidgetContainerBase* UDFUILayout::GetLayer(FGameplayTag LayerTag) const
{
	return Layers.FindRef(LayerTag);
}

bool UDFUILayout::HasAllLayers() const
{
	for (const FGameplayTag& Layer : FDFUITags::Get().Layers())
	{
		if (!Layers.FindRef(Layer))
		{
			return false;
		}
	}
	return true;
}

UDFActivatableScreen* UDFUILayout::PushScreen(TSubclassOf<UDFActivatableScreen> ScreenClass)
{
	const UDFActivatableScreen* Defaults = ScreenClass ? ScreenClass->GetDefaultObject<UDFActivatableScreen>() : nullptr;
	const FGameplayTag LayerTag = Defaults ? Defaults->GetLayerTag() : FGameplayTag();
	if (!LayerTag.IsValid())
	{
		UE_LOG(LogDFUILayout, Error, TEXT("%s: %s has no DF.UI.Screen tag, so no layer"), *GetName(), *GetNameSafe(ScreenClass));
		return nullptr;
	}
	return Cast<UDFActivatableScreen>(PushToLayer(LayerTag, ScreenClass));
}

UCommonActivatableWidget* UDFUILayout::PushToLayer(FGameplayTag LayerTag, TSubclassOf<UCommonActivatableWidget> WidgetClass)
{
	UCommonActivatableWidgetContainerBase* Layer = GetLayer(LayerTag);
	if (!Layer || !WidgetClass)
	{
		UE_LOG(LogDFUILayout, Error, TEXT("%s: cannot push %s to '%s' (%s)"), *GetName(), *GetNameSafe(WidgetClass), *LayerTag.ToString(), Layer ? TEXT("no class") : TEXT("layer not registered"));
		return nullptr;
	}
	return Layer->AddWidget<UCommonActivatableWidget>(WidgetClass);
}

bool UDFUILayout::PopScreen(UCommonActivatableWidget* Screen)
{
	if (!Screen)
	{
		return false;
	}
	for (const TPair<FGameplayTag, TObjectPtr<UCommonActivatableWidgetContainerBase>>& Layer : Layers)
	{
		if (Layer.Value && Layer.Value->GetWidgetList().Contains(Screen))
		{
			Layer.Value->RemoveWidget(*Screen);
			return true;
		}
	}
	return false;
}

void UDFUILayout::ClearLayer(FGameplayTag LayerTag)
{
	if (UCommonActivatableWidgetContainerBase* Layer = GetLayer(LayerTag))
	{
		Layer->ClearWidgets();
	}
}

UDFActivatableScreen* UDFUILayout::FindOpenScreen(FGameplayTag ScreenTag) const
{
	const UCommonActivatableWidgetContainerBase* Layer = GetLayer(FDFUITags::Get().LayerOf(ScreenTag));
	if (!Layer)
	{
		return nullptr;
	}
	for (UCommonActivatableWidget* Widget : Layer->GetWidgetList())
	{
		UDFActivatableScreen* Screen = Cast<UDFActivatableScreen>(Widget);
		if (Screen && Screen->GetScreenTag() == ScreenTag)
		{
			return Screen;
		}
	}
	return nullptr;
}
