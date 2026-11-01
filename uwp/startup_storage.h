#pragma once

// SDL has already created the selected game's private data directory. Create
// its logs child directly; walking ancestors can leave the app's sandbox.
bool Zelda3DUwp_PrepareLogDirectory(const char* directory, void (*trace)(const char*));
