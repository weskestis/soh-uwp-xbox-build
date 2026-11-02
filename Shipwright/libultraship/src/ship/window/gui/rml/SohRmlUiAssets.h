#pragma once

namespace Ship {
#ifdef ZELDA_FUSION
inline constexpr const char* kRmlMenuDocument = "assets/rml/fusion_menu.rml";
inline constexpr const char* kRmlLauncherDocument = "assets/rml/fusion_launcher.rml";
#else
inline constexpr const char* kRmlMenuDocument = "assets/rml/zelda3d_test.rml";
inline constexpr const char* kRmlLauncherDocument = "assets/rml/zelda3d_launcher.rml";
#endif
} // namespace Ship
