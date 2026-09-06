#include "texture_pack.h"

#include "../../texture_pack/texture_pack_runtime.h"
#include "../zelda3d_repl.h"

#include <libultraship/bridge.h>

#include <cstdio>
#include <cstring>

bool Zelda3D_TexturePackReplCommand(const char* command, const char* line, const char* outPath) {
    if (std::strcmp(command, "texpack") != 0) {
        return false;
    }

    char action[32] = {};
    const bool hasAction = std::sscanf(line, "%*31s %31s", action) == 1;
    if (hasAction && (std::strcmp(action, "on") == 0 || std::strcmp(action, "1") == 0)) {
        Zelda3D_TexturePackRequestEnabled(1);
        CVarSave();
    } else if (hasAction && (std::strcmp(action, "off") == 0 || std::strcmp(action, "0") == 0)) {
        Zelda3D_TexturePackRequestEnabled(0);
        CVarSave();
    } else if (hasAction && std::strcmp(action, "rescan") == 0) {
        Zelda3D_TexturePackRequestRescan();
    } else if (hasAction) {
        Zelda3D_ReplReply(outPath, "texpack: usage: texpack [on|off|rescan]");
        return true;
    }

    Zelda3D_ReplReply(outPath, "%s; source=%s; install=%s; pending=%d override=%d", Zelda3D_TexturePackStatus(),
                      Zelda3D_TexturePackSource(), Zelda3D_TexturePackInstallDirectory(),
                      Zelda3D_TexturePackSwitchPending(), Zelda3D_TexturePackExternalOverride());
    return true;
}
