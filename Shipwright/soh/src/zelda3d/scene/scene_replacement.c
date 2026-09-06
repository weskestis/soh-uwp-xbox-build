#include "scene_draw.h"

#include "../core/zelda3d_runtime.h"
#include "../render/model_queries.h"
#include "../render/room_geometry_queries.h"

#include "../tables/zelda3d_scene_names.inc"

const char* Zelda3D_SceneName(PlayState* play) {
    s32 sceneNumber = play->sceneNum;
    if (sceneNumber < 0 || sceneNumber >= (s32)ARRAY_COUNT(kZelda3dSceneNames)) {
        return NULL;
    }
    return kZelda3dSceneNames[sceneNumber];
}

int Zelda3D_ShouldSuppressBgImageSkybox(PlayState* play) {
    if (play == NULL || !Zelda3D_Enabled()) {
        return 0;
    }
    const char* sceneName = Zelda3D_SceneName(play);
    if (sceneName == NULL || play->roomCtx.curRoom.num < 0) {
        return 0;
    }
    const int modelId = Zelda3D_RoomModelId(sceneName, play->roomCtx.curRoom.num);
    return modelId >= 0 && Zelda3D_ModelReady(modelId);
}
