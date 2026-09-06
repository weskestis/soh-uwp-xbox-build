// Zelda3D engine lifecycle composition. Domain state and policy live in focused subsystem owners.
#include "zelda3d_runtime.h"

#include "../behaviors/actor/boss_fd2_bridge.h"
#include "../behaviors/actor/boss_goma_bridge.h"
#include "../behaviors/actor/cucco_control.h"
#include "../behaviors/actor_behavior_bridge.h"
#include "../behaviors/title/title_rider_state.h"
#include "../diagnostics/actor_motion_capture.h"
#include "../diagnostics/actor_selection.h"
#include "../render/fog_render.h"
#include "../render/scene_lighting_submission.h"
#include "../player/zelda3d_link.h"
#include "../texture_pack/texture_pack_runtime.h"

#include "fast/zelda3d_submission.h"

void Zelda3D_ActorPostUpdate(PlayState* play, Actor* actor) {
    Zelda3D_ActorBehaviorPostUpdate(play, actor);
    Zelda3D_LinkApplyPin(play, actor);
    Zelda3D_Title_RiderApply(play, actor);
    Zelda3D_ActorMotionCapturePostUpdate(play, actor);
    Zelda3D_BossGomaClimbTick(actor);
    Zelda3D_BossFd2IdleTick(play, actor);
    Zelda3D_ActorSelectionPostUpdate(actor);
}

void Zelda3D_FrameEndUpdate(PlayState* play) {
    // This must run even while Original SoH graphics are active; otherwise there would be no frame
    // path capable of observing a request to turn OoT3D graphics back on.
    Zelda3D_ProcessGraphicsModeRequest(play);
    Zelda3D_TexturePackProcessRequest(play);
    if (!Zelda3D_Enabled()) {
        return;
    }
    Zelda3D_CuccoAdvanceFrame();
    Zelda3D_UpdateSceneLighting(play);
    Zelda3D_UpdateFog(play);
}

void Zelda3D_FrameBegin(void) {
    if (Zelda3D_Enabled()) {
        Zelda3D_GL_FrameBegin();
    }
}
