// Screens and the layer each one lives on. Mirrors Config/Tags/DF_UI.ini (DF.UI.Screens.TagsResolve
// checks both directions). Append-only.
//
// A layer is one UCommonActivatableWidgetContainerBase, and such a container shows exactly ONE of
// its widgets at a time ("Only the widget at the top of the stack is displayed and activated. All
// others are deactivated." - CommonActivatableWidgetContainer.h). So two screens on the same layer
// are a statement that they are mutually exclusive: opening one hides the other.
//
// That is why the always-on play chrome is NOT here. Crosshairs, overheads, prompts, the revive
// column and the endless strip are visible *at the same time* as the HUD, so they are parts inside
// the HUD layout (DFUIPartList.inl), not screens sharing Layer.Game. Layer.Game holds the HUD and
// nothing else, and DF.UI.Screens.LayersAndInput fails if that stops being true.
//
//   DF_UI_SCREEN(CppName, "DF.UI.Screen.<Name>", Layer)
DF_UI_SCREEN(Hud,            "DF.UI.Screen.Hud",            Game)
DF_UI_SCREEN(Wheel,          "DF.UI.Screen.Wheel",          GameMenu)
DF_UI_SCREEN(Upgrade,        "DF.UI.Screen.Upgrade",        GameMenu)
DF_UI_SCREEN(Armory,         "DF.UI.Screen.Armory",         GameMenu)
DF_UI_SCREEN(Blueprints,     "DF.UI.Screen.Blueprints",     GameMenu)
DF_UI_SCREEN(TeleportPicker, "DF.UI.Screen.TeleportPicker", GameMenu)
DF_UI_SCREEN(Lobby,          "DF.UI.Screen.Lobby",          Menu)
DF_UI_SCREEN(Sector,         "DF.UI.Screen.Sector",         Menu)
DF_UI_SCREEN(Intermission,   "DF.UI.Screen.Intermission",   Menu)
DF_UI_SCREEN(EndMatch,       "DF.UI.Screen.EndMatch",       Menu)
DF_UI_SCREEN(Pause,          "DF.UI.Screen.Pause",          Menu)
DF_UI_SCREEN(HowTo,          "DF.UI.Screen.HowTo",          Menu)
DF_UI_SCREEN(Connection,     "DF.UI.Screen.Connection",     Modal)
