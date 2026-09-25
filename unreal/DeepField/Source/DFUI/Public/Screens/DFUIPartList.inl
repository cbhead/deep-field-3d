// Parts: widgets that are visible *alongside* a screen rather than instead of it. Every part here
// is a child of WBP_HudLayout (the one screen on DF.UI.Layer.Game) and is shown or hidden by it;
// none is ever pushed to a layer. Mirrors Config/Tags/DF_UI.ini. Append-only.
//
// The five below are the play chrome the Godot client drew every frame (PROGRAMME.md A3): they all
// sit over the world at once, which is exactly what a layer stack cannot express.
//
//   DF_UI_PART(CppName, "DF.UI.Part.<Name>")
DF_UI_PART(Crosshairs, "DF.UI.Part.Crosshairs")
DF_UI_PART(Overheads,  "DF.UI.Part.Overheads")
DF_UI_PART(Prompts,    "DF.UI.Part.Prompts")
DF_UI_PART(Revive,     "DF.UI.Part.Revive")
DF_UI_PART(Endless,    "DF.UI.Part.Endless")
