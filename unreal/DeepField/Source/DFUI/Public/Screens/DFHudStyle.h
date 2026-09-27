#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"

class UDFUITokens;

/**
 * The first HUD's look, as values: every colour and size is a design token (Appendix C§6,
 * docs/design-system/*.css) read through UDFUITokens by its DFTokens:: name.
 *
 * Where the tokens come from at runtime: DA_UITokens (/Game/DF/UI) once the importer has saved it;
 * until then the design system's CSS parsed in place (editor and uncooked -game run from a clone,
 * which has docs/). Where neither is there (a cooked build before DA_UITokens exists), the values
 * below are the design system's own as of 2026-09. A token missing from a real token set falls back
 * the same way and silently: the HUD is not where a missing token is caught
 * (DF.UI.Tokens.NamesMatchDesignSystem is), and a HUD that logs an error a frame is a HUD that fails
 * every test run that happens to show it.
 *
 * Text uses the engine's default face until the design fonts (Oxanium, Barlow) are imported as font
 * assets; sizes are the tokens' px at the 1080p reference, which UMG's DPI curve scales for 720p.
 */
struct DFUI_API FDFHudStyle
{
	FLinearColor Panel;        // surface-glass
	FLinearColor PanelEdge;    // border-panel
	FLinearColor Scrim;        // surface-overlay (behind the victory/defeat banner)
	FLinearColor Text;         // text-primary
	FLinearColor TextSecondary;// text-secondary (labels, the prompt; text-muted is too faint on glass at 720p)
	FLinearColor Accent;       // text-accent (countdown, victory)
	FLinearColor Money;        // res-gold
	FLinearColor Lives;        // lives
	FLinearColor Danger;       // state-danger (low lives, defeat, refusals)
	FLinearColor WaveIdle;     // wave-idle
	FLinearColor WaveActive;   // wave-active
	FLinearColor HpBar;        // bar-hp
	FLinearColor HpLow;        // bar-hp-low
	FLinearColor BarTrack;     // bar-track

	float Edge = 16.f;         // hud-edge: inset of every corner panel
	float PadPanel = 12.f;     // pad-panel-tight
	float Gap = 8.f;           // gap-inline
	float Stroke = 1.f;        // stroke-hairline

	float SizeLabel = 16.f;    // size-body-lg: labels and the prompt (the caption sizes are unreadable at 720p)
	float SizeValue = 22.f;    // size-display-sm: the numbers
	float SizeTitle = 30.f;    // size-display-md: the wave title
	float SizeToast = 18.f;    // size-title
	float SizeBanner = 56.f;   // size-display-xl

	/** Where the values came from: "DA_UITokens", "docs/design-system" or "built-in". */
	FString Source;

	/** Every value from Tokens where it has it (null = none), the built-in value otherwise. Logs nothing. */
	static FDFHudStyle From(const UDFUITokens* Tokens);

	/** The runtime style (see the class comment), resolved once per process. */
	static const FDFHudStyle& Get();

	/** The engine's default face at a design-system px size (1080p reference). */
	static FSlateFontInfo Font(float CssPx, bool bBold = true);

	/** A HUD panel: InFill with an InEdge hairline (the chamfer material, M_UI_Chamfer, is WS-12's later work). */
	FSlateBrush PanelBrush(const FLinearColor& InFill, const FLinearColor& InEdge) const;
};
