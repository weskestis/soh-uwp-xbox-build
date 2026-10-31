#pragma once

#include <string>

namespace MmArchiveBoot {

struct PortArchive {
    std::string path;
    bool compatible;
};

// Desktop returns status for the extraction UI. Archive-only builds require
// both archives to be valid here, before the constructor creates a window.
PortArchive PreparePortArchive();
bool NeedsRomRegeneration();
void CreateModFolder();

} // namespace MmArchiveBoot
