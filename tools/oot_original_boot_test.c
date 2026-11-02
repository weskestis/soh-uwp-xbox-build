// Engine-service fixture for the actual title and horse setup functions.
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

typedef int s32;
typedef int16_t s16;
typedef float f32;
typedef uint8_t u8;
typedef struct { float x, y, z; } Vec3f;
typedef struct { int16_t x, y, z; } Vec3s;
typedef struct GameState GameState;
struct GameState {
    bool running;
    void* gfxCtx;
    void (*main)(GameState*);
    void (*destroy)(GameState*);
    void (*next)(GameState*);
};
typedef struct { GameState state; } OpeningContext;
typedef struct { GameState state; int unused; } PlayContext;
typedef struct {
    GameState state;
    void* staticSegment;
    int view, exit, ult, unk_1D4, coverAlpha, addAlpha, visibleDuration;
} TitleContext;
typedef struct Actor {
    struct { Vec3f pos; Vec3s rot; } world;
    struct { Vec3s rot; } shape;
    int room;
    Vec3f velocity;
    float gravity, yDistToWater;
} Actor;
typedef struct { float unk_28; } AgeProperties;
typedef struct {
    Actor actor; Actor* rideActor;
    int currentBoots, currentTunic, stateFlags1, stateFlags2;
    struct { void* animation; } skelAnime;
    AgeProperties* ageProperties;
} Player;
typedef struct {
    int sceneNum, actorCtx, activeCamera;
    struct { struct { int behaviorType1; } curRoom; } roomCtx;
} PlayState;
typedef struct { unsigned int epoch; } Zelda3DOnce;
typedef struct {
    int scene, cutsceneIndex;
    Vec3s pos;
    int angle, type;
} struct_8011F9B8;

enum {
#define DEFINE_SCENE(name, title, id, ...) id,
#include "tables/scene_table.h"
#undef DEFINE_SCENE
    SCENE_ID_MAX,
    SCENE_UNUSED_6E = SCENE_ID_MAX,
};
enum {
#define DEFINE_ENTRANCE(id, ...) id,
#include "tables/entrance_table.h"
#undef DEFINE_ENTRANCE
};
static const struct { int scene; } entrances[] = {
#define DEFINE_ENTRANCE(id, scene, ...) [id] = { scene },
#include "tables/entrance_table.h"
#undef DEFINE_ENTRANCE
};
enum { GAMEMODE_TITLE_SCREEN = 1, NA_BGM_DISABLED = 0xFFFF, ACTOR_EN_HORSE = 1,
       EVENTCHKINF_EPONA_OBTAINED = 1 };
static struct {
    int gameMode, linkAge, fileNum, cutsceneIndex, sceneSetupIndex, entranceIndex;
    int seqId, natureAmbienceId, respawnFlag, eventInf[4];
    struct { int scene; } horseData;
} gSaveContext;
static int gWalkSpeedToggle, R_UPDATE_RATE, enabled, debugSaveCalls;
static int spawnCalls, mountCalls, riderInitCalls, gameplayHorseCalls, horseType;
static int matrixCalls, viewCalls, titleHookCalls, customLogo;
static unsigned int epoch = 1;
static int fastChests, onePoint, story, cvarWrites;
static Actor horse;
enum { PLAYER_BOOTS_KOKIRI, PLAYER_BOOTS_IRON, PLAYER_BOOTS_HOVER, PLAYER_BOOTS_INDOOR,
       PLAYER_BOOTS_IRON_UNDERWATER, PLAYER_BOOTS_KOKIRI_CHILD, PLAYER_BOOTS_MAX };
enum { PLAYER_TUNIC_ZORA = 2, PLAYER_STATE1_IN_WATER = 1, PLAYER_STATE1_DEAD = 2,
       PLAYER_STATE2_UNDERWATER = 1, ROOM_BEHAVIOR_TYPE1_2 = 2 };
static int regs[100], iregs[100], mregs[100], gPlayerAnim_link_swimer_swim_wait;
#define REG(n) regs[n]
#define IREG(n) iregs[n]
#define MREG(n) mregs[n]

#define ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define LINK_IS_ADULT (gSaveContext.linkAge == 0)
#define SET_NEXT_GAMESTATE(state, init, type) ((state)->next = init)
#define DREG(n) 0
#define VT_COL(a, b) ""
#define VT_RST ""
#define osSyncPrintf(...) ((void)0)
#define CVAR_ENHANCEMENT(name) name

static int Zelda3D_Enabled(void) { return enabled; }
static int Zelda3D_Once(Zelda3DOnce* once) {
    if (once->epoch == epoch) return 0;
    once->epoch = epoch;
    return 1;
}
static int* cvar(const char* name) {
    if (!strcmp(name, "FastChests")) return &fastChests;
    if (!strcmp(name, "TimeSavers.SkipCutscene.OnePoint")) return &onePoint;
    assert(!strcmp(name, "TimeSavers.SkipCutscene.Story"));
    return &story;
}
static int CVarGetInteger(const char* name, int defaultValue) { return *cvar(name); }
static void CVarSetInteger(const char* name, int value) { *cvar(name) = value; ++cvarWrites; }
static void Play_Init(GameState* state) {}
static void Opening_Init(GameState* state) {}
static void Title_Main(GameState* state) {}
static void Title_Destroy(GameState* state) {}
static void CustomLogo_Main(GameState* state) {}
static void Matrix_Init(GameState* state) { ++matrixCalls; }
static void View_Init(int* view, void* gfx) { ++viewCalls; }
static void GameInteractor_ExecuteOnZTitleInit(TitleContext* title) {
    ++titleHookCalls;
    if (customLogo) title->state.main = CustomLogo_Main;
}
static void Sram_InitDebugSave(void) {
    ++debugSaveCalls;
    gSaveContext.entranceIndex = ENTR_HYRULE_FIELD_PAST_BRIDGE_SPAWN;
    gSaveContext.horseData.scene = SCENE_HYRULE_FIELD;
}
static int Flags_GetEventChkInf(int flag) { return 0; }
static void Horse_ResetHorseData(PlayState* play) { gSaveContext.horseData.scene = SCENE_HYRULE_FIELD; }
static void Horse_SetupInGameplay(PlayState* play, Player* player) { ++gameplayHorseCalls; }
static Actor* Actor_Spawn(int* ctx, PlayState* play, int id, float x, float y, float z,
                          int rx, int ry, int rz, int type) {
    assert(id == ACTOR_EN_HORSE);
    horse.world.pos = (Vec3f){x, y, z};
    horseType = type;
    ++spawnCalls;
    return &horse;
}
static void Actor_MountHorse(PlayState* play, Player* player, Actor* actor) {
    assert(actor == &horse && player->rideActor == actor);
    ++mountCalls;
}
static void func_8002DE74(PlayState* play, Player* player) { ++riderInitCalls; }
static void Play_CameraSetAtEye(PlayState* play, int cam, Vec3f* at, Vec3f* eye) {}

/* PRODUCTION_FUNCTIONS */

int main(void) {
    OpeningContext opening = {0};
    enabled = 0;
    Opening_SetupTitleScreen(&opening);
    assert(debugSaveCalls == 1 && opening.state.next == Play_Init && !opening.state.running);
    assert(gSaveContext.entranceIndex == ENTR_HYRULE_FIELD_PAST_BRIDGE_SPAWN);
    assert(gSaveContext.cutsceneIndex == 0xFFF3 && gSaveContext.sceneSetupIndex == 7);
    assert(gSaveContext.gameMode == GAMEMODE_TITLE_SCREEN && LINK_IS_ADULT);
    PlayState play = { .sceneNum = entrances[gSaveContext.entranceIndex + 7].scene };
    assert(play.sceneNum == SCENE_HYRULE_FIELD);
    for (int cutscene = 0xFFF3; cutscene <= 0xFFF6; ++cutscene) {
        Player player = {0};
        gSaveContext.cutsceneIndex = cutscene;
        spawnCalls = mountCalls = riderInitCalls = gameplayHorseCalls = 0;
        Horse_InitPlayerHorse(&play, &player);
        assert(spawnCalls == 1 && mountCalls == 1 && riderInitCalls == 1);
        assert(gameplayHorseCalls == 0 && horseType == 7 && player.rideActor == &horse);
    }
    enabled = 1;
    Opening_SetupTitleScreen(&opening);
    assert(gSaveContext.entranceIndex == ENTR_TITLE_0);
    assert(entrances[gSaveContext.entranceIndex + 7].scene == SCENE_TITLE);

    enabled = 0;
    for (customLogo = 0; customLogo <= 1; ++customLogo) {
        TitleContext title = { .state.running = true };
        Title_Init(&title.state);
        assert(title.state.running && title.state.next == NULL);
        assert(title.state.main == (customLogo ? CustomLogo_Main : Title_Main));
        assert(title.state.destroy == Title_Destroy && title.exit == 0);
        assert(title.coverAlpha == 255 && title.addAlpha == -3 && title.visibleDuration == 60);
    }
    assert(matrixCalls == 2 && viewCalls == 2 && titleHookCalls == 2 && R_UPDATE_RATE == 1);
    enabled = 1;
    TitleContext remakeTitle = { .state.running = true };
    Title_Init(&remakeTitle.state);
    assert(!remakeTitle.state.running && remakeTitle.state.next == Opening_Init);

    enabled = 0;
    Zelda3D_SkipInit();
    assert(cvarWrites == 0 && fastChests == 0 && onePoint == 0 && story == 0);
    story = 1; // Preserve an existing user's preference in Original mode.
    Zelda3D_SkipInit();
    assert(cvarWrites == 0 && story == 1);
    enabled = 1;
    Zelda3D_SkipInit();
    assert(cvarWrites == 2 && fastChests == 1 && onePoint == 1 && story == 1);
    Zelda3D_SkipInit();
    assert(cvarWrites == 2);
    // Native movement values must survive without any remake assets installed.
    enabled = 0;
    Player player = {0};
    Player_SetBootData(&play, &player);
    assert(REG(38) == 350);
    player.currentBoots = PLAYER_BOOTS_IRON;
    player.currentTunic = PLAYER_TUNIC_ZORA;
    player.stateFlags1 = PLAYER_STATE1_IN_WATER;
    Player_SetBootData(&play, &player);
    assert(REG(19) == 80 && REG(27) == 500 && REG(48) == 100);
    player.stateFlags1 = 0;
    play.roomCtx.curRoom.behaviorType1 = ROOM_BEHAVIOR_TYPE1_2;
    Player_SetBootData(&play, &player);
    assert(REG(45) == 500);
    player.ageProperties = &(AgeProperties){40.0f};
    player.currentBoots = PLAYER_BOOTS_KOKIRI;
    player.actor.yDistToWater = 120.0f;
    player.actor.velocity.y = -1.0f;
    func_8084B000(&player);
    assert(fabsf(player.actor.velocity.y - -0.6f) < 0.00001f);
    player.currentBoots = PLAYER_BOOTS_IRON;
    player.skelAnime.animation = &gPlayerAnim_link_swimer_swim_wait;
    player.actor.velocity.y = -4.0f;
    func_8084B000(&player);
    assert(fabsf(player.actor.velocity.y - -2.7f) < 0.00001f);
    player.currentBoots = PLAYER_BOOTS_KOKIRI;
    player.actor.yDistToWater = 40.8f;
    player.actor.velocity.y = -0.2f;
    func_8084B000(&player);
    assert(fabsf(player.actor.velocity.y - -0.3f) < 0.00001f);
    puts("OoT original boot: native entrance, four mounted-rider sequences, logo hooks and skip preferences PASS");
    puts("OoT native player: boots, underwater tunic, room override and buoyancy cases PASS");
    return 0;
}
