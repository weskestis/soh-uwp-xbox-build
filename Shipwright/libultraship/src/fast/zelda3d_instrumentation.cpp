// Native-renderer diagnostics and per-model submission counts.

#include "fast/zelda3d_instrumentation.h"

#include "zelda3d_instrumentation_state.h"

#include <map>

namespace Zelda3DFast {
namespace {

std::map<int, long> submissionCounts;
void (*progressCallback)() = nullptr;

} // namespace

void RecordSubmission(int modelId) {
    ++submissionCounts[modelId];
}

void ReportProgress() {
    if (progressCallback != nullptr) {
        progressCallback();
    }
}

} // namespace Zelda3DFast

extern "C" int gZelda3dTraceModelId = -1;
extern "C" int gZelda3dStateCheck = -1;
extern "C" int g_sgDumpModel = -1;
extern "C" int gZelda3dSgDrawOnly = -1;
extern "C" int gZelda3dSgDrawSkip = -1;
extern "C" int gZelda3dSgDrawSkipAfter = -1;
extern "C" int gZelda3dSgModelOnly = -1;
extern "C" int gZelda3dSgDrawList = 0;

extern "C" int Zelda3D_SgDrawIsolationIncludes(int modelId, int drawIndex) {
    return (gZelda3dSgModelOnly < 0 || modelId == gZelda3dSgModelOnly) &&
           (gZelda3dSgDrawOnly < 0 || drawIndex == gZelda3dSgDrawOnly) && drawIndex != gZelda3dSgDrawSkip &&
           (gZelda3dSgDrawSkipAfter < 0 || drawIndex <= gZelda3dSgDrawSkipAfter);
}

extern "C" long Zelda3D_GL_SubmitCount(int modelId) {
    const auto model = Zelda3DFast::submissionCounts.find(modelId);
    return model == Zelda3DFast::submissionCounts.end() ? 0 : model->second;
}

extern "C" void Zelda3D_GL_SetProgressCallback(void (*callback)(void)) {
    Zelda3DFast::progressCallback = callback;
}
