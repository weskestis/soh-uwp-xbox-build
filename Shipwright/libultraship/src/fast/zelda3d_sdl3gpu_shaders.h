#pragma once

#include <cstdint>
#include <string>
#include <vector>

#ifdef ENABLE_SDL3GPU
#include <glslang/Public/ShaderLang.h>
#endif

namespace Fast::Zelda3DSdl3GpuShaders {

#ifdef ENABLE_SDL3GPU
bool Compile(EShLanguage stage, const char* source, std::vector<uint32_t>& spirv);
#endif

// Fill the renderer's deliberately small, fixed placeholder vocabulary. Returns false and names
// the missing or unconsumed token instead of emitting a partly-templated shader.
bool BuildSources(const char* genericTevFunctions, const char* tapCombiner, const char* tapPreFog,
                  std::string& vertexSource, std::string& fragmentSource, std::string& error);

const char* OverlayDepthFragment();

} // namespace Fast::Zelda3DSdl3GpuShaders
