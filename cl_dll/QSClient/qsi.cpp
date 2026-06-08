#include "qsi.h"
#include "qsi_internal.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

enum CoordAxis { AXIS_X, AXIS_Y, AXIS_Z };

#ifdef _WIN32
    #define strcasecmp _stricmp
#endif

constexpr float kEspBoxTopOffset = 72.0f;
constexpr float kSkeletonZOffset = 47.0f;
constexpr float kUnitsToMeters = 52.0f;
constexpr float kWorldToScreenZOffset = 64.0f;
constexpr float kEspBoxWidthRatio = 0.4f;
constexpr float kWeaponSpriteScale = 0.38f;
constexpr float kGroundWeaponSpriteScale = 0.32f;
constexpr float kCornerBoxRatio = 0.35f;
constexpr int kMaxCandidates = 32;
constexpr int kMaxTopPlayersY = 200;

#define Con_Printf              gEngfuncs.Con_Printf
#define hudGetModelByIndex      gEngfuncs.hudGetModelByIndex
#define Cmd_Argc                gEngfuncs.Cmd_Argc
#define Cmd_Argv                gEngfuncs.Cmd_Argv
#define GetClientTime           gEngfuncs.GetClientTime
#define GetLocalPlayer          gEngfuncs.GetLocalPlayer
#define GetMaxClients           gEngfuncs.GetMaxClients
#define RegisterVariable        gEngfuncs.pfnRegisterVariable
#define AddCommand              gEngfuncs.pfnAddCommand
#define IsSpectateOnly          gEngfuncs.IsSpectateOnly

struct CvarDef {
    const char* name;
    const char* defval;
    int flags;
};

static const CvarDef cvarDefs[CVAR_COUNT] = {
    {"qsi_aim",            "1",           FCVAR_ARCHIVE},
    {"qsi_silentaim",      "1",           FCVAR_ARCHIVE},
    {"qsi_teamcheck",      "1",           FCVAR_ARCHIVE},
    {"qsi_debug",          "0",           FCVAR_ARCHIVE},
    {"qsi_esp",            "1",           FCVAR_ARCHIVE},
    {"qsi_z",              "4",          FCVAR_ARCHIVE},
    {"qsi_x",              "0",           FCVAR_ARCHIVE},
    {"qsi_y",              "0",           FCVAR_ARCHIVE},
    {"qsi_name_steal",     "0",           FCVAR_ARCHIVE},
    {"qsi_topplayers",     "0",           FCVAR_ARCHIVE},
    {"qsi_esp_style",      "filled",      FCVAR_ARCHIVE},
    {"qsi_aim_on_wall",    "0",           FCVAR_ARCHIVE},
    {"qsi_aim_fov",        "15.0",         FCVAR_ARCHIVE},
    {"qsi_aim_fov_draw",   "0",           FCVAR_ARCHIVE},
    {"qsi_esp_ent",        "0",           FCVAR_ARCHIVE},
    {"qsi_esp_ent_size",   "18",          FCVAR_ARCHIVE},
    {"qsi_esp_ent_color",  "255 255 100", FCVAR_ARCHIVE},
    {"qsi_triggerbot",     "0",           FCVAR_ARCHIVE},
    {"qsi_trigger_delay",  "0.020",       FCVAR_ARCHIVE},
    {"qsi_noflash",        "1",           FCVAR_ARCHIVE},
    {"qsi_lookdown",       "0",           FCVAR_ARCHIVE},
    {"qsi_jitter",         "0",           FCVAR_ARCHIVE},
    {"qsi_jitter_intensity","0.4",        FCVAR_ARCHIVE},
    {"qsi_jitter_speed",   "30",          FCVAR_ARCHIVE},
    {"qsi_friendlist",     "1",           FCVAR_ARCHIVE},
    {"qsi_targetinfo",     "0",           FCVAR_ARCHIVE},
    {"qsi_behind_arrow",   "1",           FCVAR_ARCHIVE},
    {"qsi_sniper_noscope", "0",           FCVAR_ARCHIVE},
    {"qsi_screen_fov",     "0",           FCVAR_ARCHIVE},
    {"qsi_viewmodel_fov",  "0",           FCVAR_ARCHIVE},
    {"qsi_sway",           "0.0",         FCVAR_ARCHIVE},
    {"qsi_viewmodel_glow", "0",           FCVAR_ARCHIVE},
    {"qsi_entity_glow",    "0",           FCVAR_ARCHIVE},
    {"qsi_spinbot",        "0",           FCVAR_ARCHIVE},
    {"qsi_spin_speed",     "1440",        FCVAR_ARCHIVE},
    {"qsi_spin_factor",    "2.75",        FCVAR_ARCHIVE},
    {"qsi_spin_yaw",       "180",         FCVAR_ARCHIVE},
    {"qsi_spin_pitch",     "89",          FCVAR_ARCHIVE},
    {"qsi_spin_roll",      "20",          FCVAR_ARCHIVE},
    {"qsi_spin_random",    "1",           FCVAR_ARCHIVE},
    {"qsi_aim_bone",       "0",           FCVAR_ARCHIVE},
    {"qsi_strafebooster",  "0",           FCVAR_ARCHIVE},
    {"qsi_tracer",         "1",           FCVAR_ARCHIVE},
    {"qsi_backtrack",      "0",           FCVAR_ARCHIVE},
    {"qsi_backtrack_ms",   "200",         FCVAR_ARCHIVE},
    {"qsi_nospread",       "0",           FCVAR_ARCHIVE},
    {"xhair_rainbow",      "1",           FCVAR_ARCHIVE},
    {"xhair_rainbow_speed","1.0",         FCVAR_ARCHIVE},
    {"qsi_esp_weapon_scale","1.0",        FCVAR_ARCHIVE},
    {"qsi_ground_weapon_scale","1.0",     FCVAR_ARCHIVE},
    {"qsi_bhop_assist",    "1",           FCVAR_ARCHIVE},
    {"qsi_bhop_ground_dist","24.0",       FCVAR_ARCHIVE},
};
#define GetViewAngles           gEngfuncs.GetViewAngles
#define GetEntityByIndex        gEngfuncs.GetEntityByIndex
#define SetViewAngles           gEngfuncs.SetViewAngles
#define GetCvarPointer          gEngfuncs.pfnGetCvarPointer
#define PM_TraceLine            gEngfuncs.PM_TraceLine
#define Cvar_Set                gEngfuncs.Cvar_Set
#define GetScreenFade           gEngfuncs.pfnGetScreenFade
#define SetScreenFade           gEngfuncs.pfnSetScreenFade

#include "com_weapons.h"
#include "com_model.h"
#include "parsemsg.h"
#include "hud.h"
#include "studio.h"
#include <cfloat>
#include <string>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include "cl_dll.h"
#include <map>
#include "cl_util.h"
#include "wrect.h"
#include "r_studioint.h"
#include "GameStudioModelRenderer.h"
#include "bot_autoplay.h"

extern engine_studio_api_t IEngineStudio;
extern struct client_static_s cls;
extern int g_iWaterLevel;

BacktrackRecord g_BacktrackHistory[AIMBOT_MAX_PLAYERS][BACKTRACK_FRAMES];
PlayerInfo_t g_Player[MAX_PLAYERS + 1];
float g_fLastESPUpdate[MAX_PLAYERS + 1];
float g_playerBoneTransforms[AIMBOT_MAX_PLAYERS][MAXSTUDIOBONES][3][4] = {};
bool g_playerBoneTransformValid[AIMBOT_MAX_PLAYERS] = {};

int ClampQsiFOV(int fov)
{
    if (fov < 1)
        return 1;

    if (fov > 179)
        return 179;

    return fov;
}

float ClampQsiFOV(float fov)
{
    if (fov < 1.0f)
        return 1.0f;

    if (fov > 179.0f)
        return 179.0f;

    return fov;
}

float NormalizeQsiAngle(float angle)
{
    while (angle > 180.0f)
        angle -= 360.0f;

    while (angle < -180.0f)
        angle += 360.0f;

    return angle;
}

static float LerpQsiValue(float a, float b, float t)
{
    return a + ( b - a ) * t;
}

void FixQsiMovementForYaw(usercmd_t* cmd, float originalYaw, float newYaw)
{
    if (!cmd)
        return;

    const float delta = DEG2RAD(NormalizeQsiAngle(newYaw - originalYaw));
    const float forward = cmd->forwardmove;
    const float side = cmd->sidemove;

    cmd->forwardmove = cosf(delta) * forward - sinf(delta) * side;
    cmd->sidemove = sinf(delta) * forward + cosf(delta) * side;
}

WeaponOffset g_WeaponOffsets[] =
{
	// Name          X      Y     Z
    // --- PISTOL ---
    { "usp",         4.0f,  -1.0f, 0.6f },
    { "glock18",     4.0f,  -1.0f, 0.6f },
    { "p228",        4.0f,  -1.0f, 0.6f },
    { "fiveseven",   4.0f,  -1.0f, 0.6f },
    { "elite",       1.5f,  -2.0f, 0.6f },
    { "deagle",      4.0f,  -1.0f, 0.4f },
	
	// --- SHOTGUN ---
    { "m3",          4.0f,  -4.0f, 0.3f },
    { "xm1014",      2.0f,  -2.0f, 0.3f },
    
    // --- SMG ---
    { "mp5navy",     3.8f,  5.0f, 0.5f },
    { "tmp",         3.8f,  5.0f, 0.5f },
    { "mac10",       3.8f,  6.0f, 0.5f },
    { "ump45",       4.8f,  4.0f, 0.5f },
    { "p90",         6.3f,  -1.0f, 0.5f },

    // --- RIFLE ---
    { "ak47",        3.2f,  -4.0f, 0.9f },
    { "m4a1",        6.2f,  -1.5f, 0.9f },
    { "galil",       3.2f,  -4.0f, 0.9f },
    { "famas",       4.2f,  7.0f, 0.9f },
    { "aug",         4.2f,  4.0f, 0.9f },
    { "sg552",       4.7f,  -3.0f, 0.9f },

    // --- SNIPER (RIFLE ALT) ---
    { "awp",         3.5f,  -7.0f, 1.4f },
    { "scout",       4.5f,  4.0f, 1.4f },
    { "g3sg1",       4.5f,  -2.0f, 1.4f },
    { "sg550",       4.5f,  -4.0f, 1.4f },

    // --- MACHINE GUN ---
    { "m249",        1.3f,  4.0f, 1.6f },

    // --- OTHER ---
    { "knife",       7.0f,  7.0f, -0.6f },
    { "hegrenade",   2.0f,  0.0f, 1.4f },
    { "flashbang",   2.0f,  0.0f, 1.4f },
    { "smokegrenade",2.0f,  0.0f, 1.4f },
    { "c4",          1.0f,  3.0f, 0.2f },
    { "backpack",    1.0f,  3.0f, 0.2f },

    { NULL,          0.5f,  0.0f, 0.7f }
};

void CaptureStudioRendererSnapshot(CGameStudioModelRenderer& renderer, StudioRendererSnapshot& snapshot)
{
    snapshot.currentEntity = renderer.m_pCurrentEntity;
    snapshot.renderModel = renderer.m_pRenderModel;
    snapshot.playerInfo = renderer.m_pPlayerInfo;
    snapshot.studioHeader = renderer.m_pStudioHeader;
    snapshot.bodyPart = renderer.m_pBodyPart;
    snapshot.subModel = renderer.m_pSubModel;
    snapshot.playerState = renderer.m_pplayer;
    snapshot.playerIndex = renderer.m_nPlayerIndex;
    snapshot.frameCount = renderer.m_nFrameCount;
    snapshot.topColor = renderer.m_nTopColor;
    snapshot.bottomColor = renderer.m_nBottomColor;
    snapshot.clientTime = renderer.m_clTime;
    snapshot.oldClientTime = renderer.m_clOldTime;
    snapshot.gaitMovement = renderer.m_flGaitMovement;
    snapshot.softwareXScale = renderer.m_fSoftwareXScale;
    snapshot.softwareYScale = renderer.m_fSoftwareYScale;

    memcpy(snapshot.viewUp, renderer.m_vUp, sizeof(snapshot.viewUp));
    memcpy(snapshot.viewRight, renderer.m_vRight, sizeof(snapshot.viewRight));
    memcpy(snapshot.viewNormal, renderer.m_vNormal, sizeof(snapshot.viewNormal));
    memcpy(snapshot.renderOrigin, renderer.m_vRenderOrigin, sizeof(snapshot.renderOrigin));
    memcpy(snapshot.boneTransform, (*renderer.m_pbonetransform), sizeof(snapshot.boneTransform));
    memcpy(snapshot.lightTransform, (*renderer.m_plighttransform), sizeof(snapshot.lightTransform));
}

void RestoreStudioRendererSnapshot(CGameStudioModelRenderer& renderer, const StudioRendererSnapshot& snapshot)
{
    renderer.m_pCurrentEntity = snapshot.currentEntity;
    renderer.m_pRenderModel = snapshot.renderModel;
    renderer.m_pPlayerInfo = snapshot.playerInfo;
    renderer.m_pStudioHeader = snapshot.studioHeader;
    renderer.m_pBodyPart = snapshot.bodyPart;
    renderer.m_pSubModel = snapshot.subModel;
    renderer.m_pplayer = snapshot.playerState;
    renderer.m_nPlayerIndex = snapshot.playerIndex;
    renderer.m_nFrameCount = snapshot.frameCount;
    renderer.m_nTopColor = snapshot.topColor;
    renderer.m_nBottomColor = snapshot.bottomColor;
    renderer.m_clTime = snapshot.clientTime;
    renderer.m_clOldTime = snapshot.oldClientTime;
    renderer.m_flGaitMovement = snapshot.gaitMovement;
    renderer.m_fSoftwareXScale = snapshot.softwareXScale;
    renderer.m_fSoftwareYScale = snapshot.softwareYScale;

    memcpy(renderer.m_vUp, snapshot.viewUp, sizeof(snapshot.viewUp));
    memcpy(renderer.m_vRight, snapshot.viewRight, sizeof(snapshot.viewRight));
    memcpy(renderer.m_vNormal, snapshot.viewNormal, sizeof(snapshot.viewNormal));
    memcpy(renderer.m_vRenderOrigin, snapshot.renderOrigin, sizeof(snapshot.renderOrigin));
    memcpy((*renderer.m_pbonetransform), snapshot.boneTransform, sizeof(snapshot.boneTransform));
    memcpy((*renderer.m_plighttransform), snapshot.lightTransform, sizeof(snapshot.lightTransform));

    if (snapshot.studioHeader)
        IEngineStudio.StudioSetHeader(snapshot.studioHeader);

    if (snapshot.renderModel)
        IEngineStudio.SetRenderModel(snapshot.renderModel);
}

int FindStudioBoneByName(studiohdr_t* studioHeader, const char* const* boneNames, size_t boneNameCount)
{
    if (!studioHeader || !boneNames || !boneNameCount)
        return -1;

    mstudiobone_t* bones = reinterpret_cast<mstudiobone_t*>(reinterpret_cast<byte*>(studioHeader) + studioHeader->boneindex);

    for (size_t nameIndex = 0; nameIndex < boneNameCount; ++nameIndex)
    {
        if (!boneNames[nameIndex] || !boneNames[nameIndex][0])
            continue;

        for (int i = 0; i < studioHeader->numbones; ++i)
        {
            if (!strcasecmp(bones[i].name, boneNames[nameIndex]))
                return i;
        }
    }

    return -1;
}

int ResolveStudioBoneIndex(studiohdr_t* studioHeader, cl_entity_t* ent, int boneIndex)
{
    if (!studioHeader)
        return -1;

    if (ent && ent->player)
    {
        switch (boneIndex)
        {
        case BONE_HEAD:
        {
            static const char* kBoneNames[] = { "Bip01 Head", "ValveBiped.Bip01_Head1", "Bip01 Neck" };
            int idx = FindStudioBoneByName(studioHeader, kBoneNames, sizeof(kBoneNames) / sizeof(kBoneNames[0]));
            if (idx >= 0) return idx;
            break;
        }
        case BONE_PELVIS:
        {
            static const char* kBoneNames[] = { "Bip01 Pelvis" };
            int idx = FindStudioBoneByName(studioHeader, kBoneNames, sizeof(kBoneNames) / sizeof(kBoneNames[0]));
            if (idx >= 0) return idx;
            break;
        }
        case BONE_SPINE1:
        {
            static const char* kBoneNames[] = { "Bip01 Spine", "Bip01 Spine1" };
            int idx = FindStudioBoneByName(studioHeader, kBoneNames, sizeof(kBoneNames) / sizeof(kBoneNames[0]));
            if (idx >= 0) return idx;
            break;
        }
        case BONE_SPINE2:
        {
            static const char* kBoneNames[] = { "Bip01 Spine1", "Bip01 Spine2" };
            int idx = FindStudioBoneByName(studioHeader, kBoneNames, sizeof(kBoneNames) / sizeof(kBoneNames[0]));
            if (idx >= 0) return idx;
            break;
        }
        case BONE_SPINE3:
        {
            static const char* kBoneNames[] = { "Bip01 Spine3", "Bip01 Spine2" };
            int idx = FindStudioBoneByName(studioHeader, kBoneNames, sizeof(kBoneNames) / sizeof(kBoneNames[0]));
            if (idx >= 0) return idx;
            break;
        }
        default:
            break;
        }
    }

    if (boneIndex >= 0 && boneIndex < studioHeader->numbones)
        return boneIndex;

    return -1;
}

Vector GetBonePosition(cl_entity_t* ent, int boneIndex)
{
    if (!ent)
        return Vector(0, 0, 0);

    Vector bonePos = ent->origin;
    model_t* renderModel = ent->model;

    if (!renderModel)
        return bonePos;

    CGameStudioModelRenderer& renderer = g_StudioRenderer;
    StudioRendererSnapshot snapshot;
    CaptureStudioRendererSnapshot(renderer, snapshot);

    entity_state_t savedCurstate = ent->curstate;
    latchedvars_t savedLatched = ent->latched;
    vec3_t savedAngles;
    player_info_t savedPlayerInfo = {};
    player_info_t* livePlayerInfo = nullptr;
    bool shouldRestorePlayerInfo = false;

    VectorCopy(ent->angles, savedAngles);

    IEngineStudio.GetTimes(&renderer.m_nFrameCount, &renderer.m_clTime, &renderer.m_clOldTime);
    IEngineStudio.GetViewInfo(renderer.m_vRenderOrigin, renderer.m_vUp, renderer.m_vRight, renderer.m_vNormal);
    IEngineStudio.GetAliasScale(&renderer.m_fSoftwareXScale, &renderer.m_fSoftwareYScale);

    renderer.m_pCurrentEntity = ent;
    renderer.m_pplayer = &ent->curstate;
    renderer.m_pPlayerInfo = nullptr;
    renderer.m_nPlayerIndex = -1;

    if (ent->player)
    {
        renderer.m_nPlayerIndex = ent->index - 1;

        if (renderer.m_nPlayerIndex < 0 || renderer.m_nPlayerIndex >= GetMaxClients())
            goto cleanup;

        renderModel = IEngineStudio.SetupPlayerModel(renderer.m_nPlayerIndex);
        livePlayerInfo = IEngineStudio.PlayerInfo(renderer.m_nPlayerIndex);

        if (!renderModel || !livePlayerInfo)
            goto cleanup;

        savedPlayerInfo = *livePlayerInfo;
        shouldRestorePlayerInfo = true;
        renderer.m_pPlayerInfo = livePlayerInfo;
    }

    renderer.m_pRenderModel = renderModel;
    renderer.m_pStudioHeader = (studiohdr_t*)IEngineStudio.Mod_Extradata(renderModel);

    if (!renderer.m_pStudioHeader)
        goto cleanup;

    boneIndex = ResolveStudioBoneIndex(renderer.m_pStudioHeader, ent, boneIndex);
    if (boneIndex < 0)
        goto cleanup;

    IEngineStudio.StudioSetHeader(renderer.m_pStudioHeader);
    IEngineStudio.SetRenderModel(renderModel);

    if (ent->player)
    {
        if (ent->curstate.sequence >= renderer.m_pStudioHeader->numseq)
            ent->curstate.sequence = 0;

        if (ent->curstate.gaitsequence >= renderer.m_pStudioHeader->numseq)
            ent->curstate.gaitsequence = 0;

        if (ent->curstate.gaitsequence)
        {
            vec3_t originalAngles;

            VectorCopy(ent->angles, originalAngles);
            renderer.StudioProcessGait(&ent->curstate);
            livePlayerInfo->gaitsequence = ent->curstate.gaitsequence;
            renderer.m_pPlayerInfo = nullptr;
            renderer.StudioSetUpTransform(0);
            VectorCopy(originalAngles, ent->angles);
            renderer.m_pPlayerInfo = livePlayerInfo;
        }
        else
        {
            ent->curstate.controller[0] = 127;
            ent->curstate.controller[1] = 127;
            ent->curstate.controller[2] = 127;
            ent->curstate.controller[3] = 127;
            ent->latched.prevcontroller[0] = ent->curstate.controller[0];
            ent->latched.prevcontroller[1] = ent->curstate.controller[1];
            ent->latched.prevcontroller[2] = ent->curstate.controller[2];
            ent->latched.prevcontroller[3] = ent->curstate.controller[3];

            renderer.CalculatePitchBlend(&ent->curstate);
            renderer.CalculateYawBlend(&ent->curstate);
            livePlayerInfo->gaitsequence = 0;
            renderer.StudioSetUpTransform(0);
        }
    }
    else
    {
        renderer.StudioSetUpTransform(ent->trivial_accept);
    }

    renderer.StudioSetupBones();

    bonePos.x = (*renderer.m_pbonetransform)[boneIndex][0][3];
    bonePos.y = (*renderer.m_pbonetransform)[boneIndex][1][3];
    bonePos.z = (*renderer.m_pbonetransform)[boneIndex][2][3];

cleanup:
    ent->curstate = savedCurstate;
    ent->latched = savedLatched;
    VectorCopy(savedAngles, ent->angles);

    if (shouldRestorePlayerInfo && livePlayerInfo)
        *livePlayerInfo = savedPlayerInfo;

    RestoreStudioRendererSnapshot(renderer, snapshot);
    return bonePos;
}

float GetHeadOffset(cl_entity_t* ent, int xyz)
{
    if (!ent || ent->curstate.weaponmodel <= 0)
        return 0.0f;

    model_t* wmodel = hudGetModelByIndex(ent->curstate.weaponmodel);
    if (!wmodel || !wmodel->name[0])
        return 0.0f;

    const char* name = wmodel->name;

    for (int i = 0; g_WeaponOffsets[i].name != NULL; i++)
    {
        if (strstr(name, g_WeaponOffsets[i].name))
        {
            if (xyz == AXIS_X) return g_WeaponOffsets[i].x;
            if (xyz == AXIS_Y) return g_WeaponOffsets[i].y;
            if (xyz == AXIS_Z) return g_WeaponOffsets[i].z;
        }
    }

    if (xyz == AXIS_X) return 0.5f;
    if (xyz == AXIS_Y) return 0.0f;
    if (xyz == AXIS_Z) return 0.7f;

    return 0.0f;
}

static float g_SpinLastYaw[AIMBOT_MAX_PLAYERS];
static float g_SpinLastTime[AIMBOT_MAX_PLAYERS];
static bool  g_SpinDetected[AIMBOT_MAX_PLAYERS];
static constexpr float SPIN_THRESHOLD = 25.0f;
static constexpr float SPIN_WINDOW = 0.2f;

static bool IsPlayerSpinning(int entityIndex)
{
    if (entityIndex < 1 || entityIndex >= AIMBOT_MAX_PLAYERS)
        return false;

    cl_entity_t* ent = GetEntityByIndex(entityIndex);
    if (!ent) return false;

    float curTime = GetClientTime();
    float dt = curTime - g_SpinLastTime[entityIndex];

    if (dt >= SPIN_WINDOW)
    {
        float yawDelta = fabs(ent->curstate.angles[1] - g_SpinLastYaw[entityIndex]);
        if (yawDelta > 180.0f) yawDelta = 360.0f - yawDelta;
        g_SpinDetected[entityIndex] = (yawDelta > SPIN_THRESHOLD);
        g_SpinLastYaw[entityIndex] = ent->curstate.angles[1];
        g_SpinLastTime[entityIndex] = curTime;
    }

    return g_SpinDetected[entityIndex];
}

Vector AimPos(cl_entity_t *ent)
{
    if (!ent) 
        return Vector(0, 0, 0);

    int idx = ent->index;

    if (idx >= 1 && idx < AIMBOT_MAX_PLAYERS && IsPlayerSpinning(idx))
    {
        bool ducking = (ent->curstate.usehull == 1);
        float hullMinZ = ducking ? -18.0f : -36.0f;
        float hullHeight = ducking ? 36.0f : 72.0f;
        Vector aimPos = ent->origin;
        aimPos.z += hullMinZ + hullHeight * 0.65f;
        return aimPos;
    }

    Vector btPos = QSI.GetBacktrackPos(ent->index);
    if (btPos.Length() > 1.0f)
        return btPos;
	
    Vector gAimboxRes = GetBonePosition(ent, QSI.qsi_cvars[CVAR_AIM_BONE] ? QSI.qsi_cvars[CVAR_AIM_BONE]->value : 1);
	
    if (QSI.qsi_cvars[CVAR_X]) gAimboxRes.x += QSI.qsi_cvars[CVAR_X]->value;
    if (QSI.qsi_cvars[CVAR_Y]) gAimboxRes.y += QSI.qsi_cvars[CVAR_Y]->value;
    if (QSI.qsi_cvars[CVAR_Z]) gAimboxRes.z += QSI.qsi_cvars[CVAR_Z]->value;
	
    return gAimboxRes;
}

void DebugGroundEntity(cl_entity_t* ent)
{
	if(QSI.qsi_cvars[CVAR_DEBUG]->value < 3.0f)
		return;
		
    if (!ent || !ent->model || !ent->model->name[0])
    {
        Con_Printf("[DEBUG] Entity: NULL or there is no model\n");
        return;
    }

    const char* modelName = ent->model->name;
    int index = ent->index;
    int owner = ent->curstate.owner;

    Con_Printf("[DEBUG] Index: %d | Owner: %d | Model: %s\n", 
                         index, owner, modelName);
}

bool ContainsNoCase(const char* text, const char* find)
{
    if (!text || !find || !find[0]) return false;

    while (*text)
    {
        const char* t = text;
        const char* f = find;

        while (*t && *f && tolower(*t) == tolower(*f))
        {
            ++t;
            ++f;
        }

        if (!*f)
            return true;

        ++text;
    }

    return false;
}

void Cmd_AddFriend()
{
    if (Cmd_Argc() < 2)
    {
        Con_Printf("Usage: qsi_add_friend <nick>\n");
        return;
    }
    const char* nick = Cmd_Argv(1);
    if (!nick || !nick[0]) return;
    if (QSI.IsFriend(nick))
    {
        Con_Printf("[QSI] %s is already in the friend list.\n", nick);
        return;
    }
    QSI.g_Friends.push_back(nick);
    QSI.SaveFriendsList();
    Con_Printf("[QSI] %s added to the friend list.\n", nick);
}

void Cmd_DelFriend()
{
    if (Cmd_Argc() < 2)
    {
        Con_Printf("Usage: qsi_del_friend <nick>\n");
        return;
    }
    const char* nick = Cmd_Argv(1);
    if (!nick || !nick[0]) return;
    auto& vec = QSI.g_Friends;
    for (auto it = vec.begin(); it != vec.end(); ++it)
    {
        if (strcasecmp(it->c_str(), nick) == 0)
        {
            vec.erase(it);
            QSI.SaveFriendsList();
            Con_Printf("[QSI] %s removed from the friend list.\n", nick);
            return;
        }
    }
    Con_Printf("[QSI] %s not found in the list.\n", nick);
}

void Cmd_ListFriends()
{
    Con_Printf("\n\x02=== Friend List (%d) ===\x01\n", (int)QSI.g_Friends.size());
    if (QSI.g_Friends.empty())
    {
        Con_Printf("The list is empty.\n");
        return;
    }
    for (const auto& name : QSI.g_Friends)
        Con_Printf(" - %s\n", name.c_str());
}

void PrintESPStyles(void) {
    Con_Printf("\n\x02=== ESP Styles List ===\x01\n");
    Con_Printf("Style          | Description\n");
    Con_Printf("---------------|----------------------------\n");
    Con_Printf("corner-box     | Corner box (default)\n");
    Con_Printf("2d-box         | Full frame rectangle\n");
    Con_Printf("outline        | Thick black outline\n");
    Con_Printf("glow           | Glowing colored border\n");
    Con_Printf("filled         | Semi-transparent filled box\n");
    Con_Printf("dashed         | Dashed line frame\n");
    Con_Printf("gradient       | Vertical color gradient\n");
    Con_Printf("hitbox         | Skeleton bone drawing\n");
    Con_Printf("arrow          | Arrow brackets pointing inward\n");
    Con_Printf("snapbar        | Box with snap line to bottom\n");
    Con_Printf("cross          | Crosshair overlay on target\n");
    Con_Printf("radar          | Mini radar dot indicator\n");
    Con_Printf("scifi          | Sci-fi bracket style\n");
    Con_Printf("thick          | Thick colored outline\n");
    Con_Printf("\nUse: qsi_cvars[CVAR_ESP_STYLE] <style>\n");
}

static bool ParseGlowColorArg(const char* arg, char channel, int& outValue)
{
    if (!arg || arg[0] != channel || arg[1] != '=' || arg[2] == '\0')
        return false;

    char* end = nullptr;
    long value = strtol(arg + 2, &end, 10);
    if (!end || *end != '\0' || value < 0 || value > 255)
        return false;

    outValue = (int)value;
    return true;
}

void Cmd_ViewModelGlowColor()
{
    if (Cmd_Argc() != 4)
    {
        Con_Printf("Usage: qsi_viewmodel_glow_color r=0-255 g=0-255 b=0-255\n");
        return;
    }

    bool seenR = false, seenG = false, seenB = false;
    int newR = QSI.m_iViewModelGlowR, newG = QSI.m_iViewModelGlowG, newB = QSI.m_iViewModelGlowB;

    for (int i = 1; i < 4; ++i)
    {
        const char* arg = Cmd_Argv(i);
        int parsed = 0;

        if (ParseGlowColorArg(arg, 'r', parsed))
        {
            if (seenR) { Con_Printf("Error: duplicate parameter 'r'.\n"); return; }
            seenR = true; newR = parsed; continue;
        }
        if (ParseGlowColorArg(arg, 'g', parsed))
        {
            if (seenG) { Con_Printf("Error: duplicate parameter 'g'.\n"); return; }
            seenG = true; newG = parsed; continue;
        }
        if (ParseGlowColorArg(arg, 'b', parsed))
        {
            if (seenB) { Con_Printf("Error: duplicate parameter 'b'.\n"); return; }
            seenB = true; newB = parsed; continue;
        }

        Con_Printf("Error: invalid parameter '%s'. Only r=, g=, b= are allowed.\n", arg ? arg : "");
        return;
    }

    if (!seenR || !seenG || !seenB)
    {
        Con_Printf("Error: all parameters are required: r=, g=, b=.\n");
        return;
    }

    QSI.m_iViewModelGlowR = newR;
    QSI.m_iViewModelGlowG = newG;
    QSI.m_iViewModelGlowB = newB;
    Con_Printf("[QSI] ViewModel glow color set to r=%d g=%d b=%d\n", QSI.m_iViewModelGlowR, QSI.m_iViewModelGlowG, QSI.m_iViewModelGlowB);
}

void ExtractWeaponName(const char* modelPath, char* outBuf, size_t outSize)
{
    if (!modelPath || !outBuf || !outSize)
        return;
    const char* modelName = strrchr(modelPath, '/');
    if (!modelName) modelName = strrchr(modelPath, '\\');
    if (modelName) modelName++;
    else modelName = modelPath;
    strncpy(outBuf, modelName, outSize - 1);
    outBuf[outSize - 1] = '\0';
    char* dot = strstr(outBuf, ".mdl");
    if (dot)
        *dot = '\0';
    if (outBuf[1] == '_')
        memmove(outBuf, outBuf + 2, strlen(outBuf + 2) + 1);
}

bool WeaponModelToSpriteName(const char* wpnName, char* spriteName, size_t spriteSize)
{
    if (!wpnName || !wpnName[0] || !spriteName || !spriteSize)
        return false;

    if( !strcmp( wpnName, "hegrenade" ) || !strcmp( wpnName, "grenade" ) || !strcmp( wpnName, "flashbang" ) || !strcmp( wpnName, "smokegrenade" ) )
        snprintf( spriteName, spriteSize, "d_grenade" );
    else if( !strcmp( wpnName, "c4" ) )
        snprintf( spriteName, spriteSize, "c4" );
    else if( !strncmp( wpnName, "d_", 2 ) )
        snprintf( spriteName, spriteSize, "%s", wpnName );
    else
        snprintf( spriteName, spriteSize, "d_%s", wpnName );

    spriteName[spriteSize - 1] = '\0';
    return true;
}

void StealName() {
    if (!QSI.qsi_cvars[CVAR_NAME_STEAL] || QSI.qsi_cvars[CVAR_NAME_STEAL]->value < 0.5f) return;
    static float last = 0;
    float now = GetClientTime();
    if (now - last < 0) last = now;
    if (now - last < 3) return;
    last = now;
    cl_entity_t* local = GetLocalPlayer();
    if (!local) return;
    int max = GetMaxClients();
    int candidates[kMaxCandidates];
    int count = 0;
    for (int i = 1; i <= max; i++) {
        if (i == local->index) continue;
        hud_player_info_t pi;
        GetPlayerInfo(i, &pi);
        if (pi.name && pi.name[0]) candidates[count++] = i;
    }
    if (count == 0) return;
    int rnd = rand() % count;
    int chosen = candidates[rnd];
    hud_player_info_t pi;
    GetPlayerInfo(chosen, &pi);
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "name \"%s^0\"", pi.name);
    ClientCmd(cmd);
}

bool IsPlayerAlive(const cl_entity_t* ent)
{
    if (!ent)
        return false;

    if (!ent->player)
        return false;

    cl_entity_t* local = GetLocalPlayer();
    if (local && ent == local && gHUD.m_Health.m_iHealth <= 0)
        return false;

    return true;
}

bool IsAliveEntity(cl_entity_t* Entity)
{
    return (Entity && IsPlayerAlive(Entity) &&
            Entity->curstate.movetype != 6 && Entity->curstate.movetype != 0);
}

extern void onGroundStrafe();
extern void offGroundStrafe();

QSI_Client QSI;
QSI_Client::QSI_Client()
{
    memset(g_Player, 0, sizeof(g_Player));
    memset(g_fLastESPUpdate, 0, sizeof(g_fLastESPUpdate));
    m_iRawFOV = 0;
    m_flNextAutoScopeTime = 0.0f;
    m_iLastAutoScopeWeaponId = WEAPON_NONE;
    m_vecSpinRandomTarget = Vector(0.0f, 0.0f, 0.0f);
    m_vecSpinRandomCurrent = Vector(0.0f, 0.0f, 0.0f);
    m_flNextSpinRandomTime = 0.0f;
    m_flLastSpinUpdateTime = 0.0f;
}

void QSI_Client::InitCVars()
{
    for (int i = 0; i < CVAR_COUNT; i++)
        qsi_cvars[i] = RegisterVariable(cvarDefs[i].name, cvarDefs[i].defval, cvarDefs[i].flags);
    
    AddCommand("qsi_add_friend",   Cmd_AddFriend);
    AddCommand("qsi_del_friend",   Cmd_DelFriend);
    AddCommand("qsi_list_friends", Cmd_ListFriends);
    AddCommand("qsi_esp_styles",   PrintESPStyles);
    AddCommand("qsi_viewmodel_glow_color", Cmd_ViewModelGlowColor);
    AddCommand("+gs", 			 onGroundStrafe);
    AddCommand("-gs", 			 offGroundStrafe);
    
    LoadConfig();
    Initialize();
}

void QSI_Client::Initialize() {
    srand(static_cast<unsigned>(time(nullptr)));
    ClientCmd("play sound/vox/dadeda.wav");
    
    m_iGs = 0;

    cvar_t* c_xhair_color = GetCvarPointer("xhair_color");
    if (c_xhair_color && c_xhair_color->string) {
        m_cachedXhairColor = c_xhair_color->string;
    } else {
        m_cachedXhairColor = "255 255 255";
    }
    
    LoadFriendsList();
}

void QSI_Client::SaveConfig()
{
    FILE* f = fopen("qsi.cfg", "w");
    if (!f) return;
    fprintf(f, "// QSI Config - Auto-generated on game shutdown\n");
    fprintf(f, "// Categories: Aim, Visual, Motion, Misc\n\n");
    fprintf(f, "// ===== AIM =====\n");
    fprintf(f, "QSI_AIM = \"%s\";\n", qsi_cvars[CVAR_AIM] ? qsi_cvars[CVAR_AIM]->string : "1");
    fprintf(f, "QSI_SILENTAIM = \"%s\";\n", qsi_cvars[CVAR_SILENTAIM] ? qsi_cvars[CVAR_SILENTAIM]->string : "0");
    fprintf(f, "QSI_TEAMCHECK = \"%s\";\n", qsi_cvars[CVAR_TEAMCHECK] ? qsi_cvars[CVAR_TEAMCHECK]->string : "1");
    fprintf(f, "QSI_AIM_ON_WALL = \"%s\";\n", qsi_cvars[CVAR_AIM_ON_WALL] ? qsi_cvars[CVAR_AIM_ON_WALL]->string : "0");
    fprintf(f, "QSI_AIM_FOV = \"%s\";\n", qsi_cvars[CVAR_AIM_FOV] ? qsi_cvars[CVAR_AIM_FOV]->string : "8.0");
    fprintf(f, "QSI_AIM_FOV_DRAW = \"%s\";\n", qsi_cvars[CVAR_AIM_FOV_DRAW] ? qsi_cvars[CVAR_AIM_FOV_DRAW]->string : "0");
    fprintf(f, "QSI_AIM_BONE = \"%s\";\n", qsi_cvars[CVAR_AIM_BONE] ? qsi_cvars[CVAR_AIM_BONE]->string : "0");
    fprintf(f, "QSI_TRIGGERBOT = \"%s\";\n", qsi_cvars[CVAR_TRIGGERBOT] ? qsi_cvars[CVAR_TRIGGERBOT]->string : "0");
    fprintf(f, "QSI_TRIGGER_DELAY = \"%s\";\n", qsi_cvars[CVAR_TRIGGER_DELAY] ? qsi_cvars[CVAR_TRIGGER_DELAY]->string : "0.020");
    fprintf(f, "QSI_SPINBOT = \"%s\";\n", qsi_cvars[CVAR_CRAZY_SPIN] ? qsi_cvars[CVAR_CRAZY_SPIN]->string : "1");
    fprintf(f, "QSI_SPIN_SPEED = \"%s\";\n", qsi_cvars[CVAR_SPIN_SPEED] ? qsi_cvars[CVAR_SPIN_SPEED]->string : "1440");
    fprintf(f, "QSI_SPIN_FACTOR = \"%s\";\n", qsi_cvars[CVAR_SPIN_FACTOR] ? qsi_cvars[CVAR_SPIN_FACTOR]->string : "2.75");
    fprintf(f, "QSI_SPIN_YAW = \"%s\";\n", qsi_cvars[CVAR_SPIN_YAW] ? qsi_cvars[CVAR_SPIN_YAW]->string : "180");
    fprintf(f, "QSI_SPIN_PITCH = \"%s\";\n", qsi_cvars[CVAR_SPIN_PITCH] ? qsi_cvars[CVAR_SPIN_PITCH]->string : "89");
    fprintf(f, "QSI_SPIN_ROLL = \"%s\";\n", qsi_cvars[CVAR_SPIN_ROLL] ? qsi_cvars[CVAR_SPIN_ROLL]->string : "20");
    fprintf(f, "QSI_SPIN_RANDOM = \"%s\";\n", qsi_cvars[CVAR_SPIN_RANDOM] ? qsi_cvars[CVAR_SPIN_RANDOM]->string : "0");
    fprintf(f, "\n// ===== VISUAL =====\n");
    fprintf(f, "QSI_ESP = \"%s\";\n", qsi_cvars[CVAR_ESP] ? qsi_cvars[CVAR_ESP]->string : "1");
    fprintf(f, "QSI_ESP_STYLE = \"%s\";\n", qsi_cvars[CVAR_ESP_STYLE] ? qsi_cvars[CVAR_ESP_STYLE]->string : "filled");
    fprintf(f, "QSI_ESP_ENT = \"%s\";\n", qsi_cvars[CVAR_ESP_ENT] ? qsi_cvars[CVAR_ESP_ENT]->string : "1");
    fprintf(f, "QSI_ESP_ENT_SIZE = \"%s\";\n", qsi_cvars[CVAR_ESP_ENT_SIZE] ? qsi_cvars[CVAR_ESP_ENT_SIZE]->string : "18");
    fprintf(f, "QSI_ESP_ENT_COLOR = \"%s\";\n", qsi_cvars[CVAR_ESP_ENT_COLOR] ? qsi_cvars[CVAR_ESP_ENT_COLOR]->string : "255 255 100");
    fprintf(f, "QSI_TOPPLAYERS = \"%s\";\n", qsi_cvars[CVAR_TOPPLAYERS] ? qsi_cvars[CVAR_TOPPLAYERS]->string : "1");
    fprintf(f, "QSI_TARGETINFO = \"%s\";\n", qsi_cvars[CVAR_TARGETINFO] ? qsi_cvars[CVAR_TARGETINFO]->string : "1");
    fprintf(f, "QSI_FRIENDLIST = \"%s\";\n", qsi_cvars[CVAR_FRIENDLIST] ? qsi_cvars[CVAR_FRIENDLIST]->string : "1");
    fprintf(f, "QSI_BEHIND_ARROW = \"%s\";\n", qsi_cvars[CVAR_BEHIND_ARROW] ? qsi_cvars[CVAR_BEHIND_ARROW]->string : "1");
    fprintf(f, "QSI_NOFLASH = \"%s\";\n", qsi_cvars[CVAR_NOFLASH] ? qsi_cvars[CVAR_NOFLASH]->string : "1");
    fprintf(f, "QSI_ENTITY_GLOW = \"%s\";\n", qsi_cvars[CVAR_ENTITY_GLOW] ? qsi_cvars[CVAR_ENTITY_GLOW]->string : "1");
    fprintf(f, "XHAIR_RAINBOW = \"%s\";\n", qsi_cvars[CVAR_XHAIR_RAINBOW] ? qsi_cvars[CVAR_XHAIR_RAINBOW]->string : "1");
    fprintf(f, "XHAIR_RAINBOW_SPEED = \"%s\";\n", qsi_cvars[CVAR_XHAIR_RAINBOW_SPEED] ? qsi_cvars[CVAR_XHAIR_RAINBOW_SPEED]->string : "1.0");
    fprintf(f, "QSI_TRACER = \"%s\";\n", qsi_cvars[CVAR_TRACER] ? qsi_cvars[CVAR_TRACER]->string : "0");
    fprintf(f, "QSI_BACKTRACK = \"%s\";\n", qsi_cvars[CVAR_BACKTRACK] ? qsi_cvars[CVAR_BACKTRACK]->string : "0");
    fprintf(f, "QSI_BACKTRACK_MS = \"%s\";\n", qsi_cvars[CVAR_BACKTRACK_MS] ? qsi_cvars[CVAR_BACKTRACK_MS]->string : "200");
    fprintf(f, "QSI_NOSPREAD = \"%s\";\n", qsi_cvars[CVAR_NOSPREAD] ? qsi_cvars[CVAR_NOSPREAD]->string : "0");
    fprintf(f, "QSI_ESP_WEAPON_SCALE = \"%s\";\n", qsi_cvars[CVAR_ESP_WEAPON_SCALE] ? qsi_cvars[CVAR_ESP_WEAPON_SCALE]->string : "1.0");
    fprintf(f, "QSI_GROUND_WEAPON_SCALE = \"%s\";\n", qsi_cvars[CVAR_GROUND_WEAPON_SCALE] ? qsi_cvars[CVAR_GROUND_WEAPON_SCALE]->string : "1.0");
    fprintf(f, "QSI_BHOP_ASSIST = \"%s\";\n", qsi_cvars[CVAR_BHOP_ASSIST] ? qsi_cvars[CVAR_BHOP_ASSIST]->string : "1");
    fprintf(f, "QSI_BHOP_GROUND_DIST = \"%s\";\n", qsi_cvars[CVAR_BHOP_GROUND_DIST] ? qsi_cvars[CVAR_BHOP_GROUND_DIST]->string : "24.0");
    fprintf(f, "\n// ===== MOTION =====\n");
    fprintf(f, "QSI_SNIPER_NOSCOPE = \"%s\";\n", qsi_cvars[CVAR_SNIPER_NOSCOPE] ? qsi_cvars[CVAR_SNIPER_NOSCOPE]->string : "1");
    fprintf(f, "QSI_VIEWMODEL_GLOW = \"%s\";\n", qsi_cvars[CVAR_VIEWMODEL_GLOW] ? qsi_cvars[CVAR_VIEWMODEL_GLOW]->string : "1");
    fprintf(f, "QSI_VIEWMODEL_FOV = \"%s\";\n", qsi_cvars[CVAR_VIEWMODEL_FOV] ? qsi_cvars[CVAR_VIEWMODEL_FOV]->string : "0");
    fprintf(f, "QSI_SCREEN_FOV = \"%s\";\n", qsi_cvars[CVAR_SCREEN_FOV] ? qsi_cvars[CVAR_SCREEN_FOV]->string : "0");
    fprintf(f, "QSI_JITTER = \"%s\";\n", qsi_cvars[CVAR_JITTER] ? qsi_cvars[CVAR_JITTER]->string : "0");
    fprintf(f, "QSI_JITTER_INTENSITY = \"%s\";\n", qsi_cvars[CVAR_JITTER_INTENSITY] ? qsi_cvars[CVAR_JITTER_INTENSITY]->string : "0.4");
    fprintf(f, "QSI_JITTER_SPEED = \"%s\";\n", qsi_cvars[CVAR_JITTER_SPEED] ? qsi_cvars[CVAR_JITTER_SPEED]->string : "30");
    fprintf(f, "QSI_LOOKDOWN = \"%s\";\n", qsi_cvars[CVAR_LOOKDOWN] ? qsi_cvars[CVAR_LOOKDOWN]->string : "1");
    fprintf(f, "QSI_SWAY = \"%s\";\n", qsi_cvars[CVAR_SWAY] ? qsi_cvars[CVAR_SWAY]->string : "1.0");
    fprintf(f, "QSI_STRAFEBOOSTER = \"%s\";\n", qsi_cvars[CVAR_STRAFEBOOSTER] ? qsi_cvars[CVAR_STRAFEBOOSTER]->string : "0");
    fprintf(f, "\n// ===== MISC =====\n");
    fprintf(f, "QSI_DEBUG = \"%s\";\n", qsi_cvars[CVAR_DEBUG] ? qsi_cvars[CVAR_DEBUG]->string : "0");
    fprintf(f, "QSI_NAME_STEAL = \"%s\";\n", qsi_cvars[CVAR_NAME_STEAL] ? qsi_cvars[CVAR_NAME_STEAL]->string : "0");
    fprintf(f, "QSI_X = \"%s\";\n", qsi_cvars[CVAR_X] ? qsi_cvars[CVAR_X]->string : "2");
    fprintf(f, "QSI_Y = \"%s\";\n", qsi_cvars[CVAR_Y] ? qsi_cvars[CVAR_Y]->string : "0");
    fprintf(f, "QSI_Z = \"%s\";\n", qsi_cvars[CVAR_Z] ? qsi_cvars[CVAR_Z]->string : "18");
    fclose(f);
}

void QSI_Client::LoadConfig()
{
    FILE* f = fopen("qsi.cfg", "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char* p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '/' || *p == '\n' || *p == '\0') continue;
        char cvarName[128] = {0};
        char cvarValue[128] = {0};
        if (sscanf(p, " %127[^ ] = \"%127[^\"]\"", cvarName, cvarValue) >= 2) {
            for (int ci = 0; cvarName[ci]; ci++)
                if (cvarName[ci] >= 'A' && cvarName[ci] <= 'Z')
                    cvarName[ci] += 32;
            Cvar_Set(cvarName, cvarValue);
        }
    }
    fclose(f);
}

void QSI_Client::SetRawFOV(int rawFov)
{
    m_iRawFOV = rawFov;
}

int QSI_Client::GetBaseScreenFOV(int defaultFov) const
{
    const int fallbackFov = ClampQsiFOV(defaultFov > 0 ? defaultFov : 90);

    if (!qsi_cvars[CVAR_SCREEN_FOV] || qsi_cvars[CVAR_SCREEN_FOV]->value <= 0.0f)
        return fallbackFov;

    return ClampQsiFOV(static_cast<int>(qsi_cvars[CVAR_SCREEN_FOV]->value + 0.5f));
}

int QSI_Client::GetActiveWeaponId() const
{
    return HUD_GetWeapon();
}

bool QSI_Client::IsSniperWeapon(int weaponId) const
{
    switch (weaponId)
    {
    case WEAPON_SCOUT:
    case WEAPON_AWP:
    case WEAPON_G3SG1:
    case WEAPON_SG550:
        return true;
    default:
        return false;
    }
}

int QSI_Client::GetVisibleFOV(int rawFov, int defaultFov) const
{
    const int baseFov = GetBaseScreenFOV(defaultFov);

    if (rawFov <= 0 || rawFov == 90)
        return baseFov;

    if (qsi_cvars[CVAR_SNIPER_NOSCOPE] && qsi_cvars[CVAR_SNIPER_NOSCOPE]->value >= 0.5f && rawFov < 90 && IsSniperWeapon(GetActiveWeaponId()))
        return baseFov;

    return ClampQsiFOV(rawFov);
}

float QSI_Client::GetMouseSensitivity(int rawFov, int defaultFov) const
{
    const int baseFov = GetBaseScreenFOV(defaultFov);
    const int visibleFov = GetVisibleFOV(rawFov, defaultFov);

    if (!sensitivity || visibleFov == baseFov)
        return 0.0f;

    cvar_t* zoomRatio = GetCvarPointer("zoom_sensitivity_ratio");
    const float ratio = zoomRatio ? zoomRatio->value : 1.0f;

    return sensitivity->value * (static_cast<float>(visibleFov) / static_cast<float>(baseFov)) * ratio;
}

bool QSI_Client::ShouldHideSniperScope() const
{
    return qsi_cvars[CVAR_SNIPER_NOSCOPE] && qsi_cvars[CVAR_SNIPER_NOSCOPE]->value >= 0.5f
        && m_iRawFOV > 0 && m_iRawFOV < 90
        && IsSniperWeapon(GetActiveWeaponId());
}

bool QSI_Client::ShouldKeepSniperViewmodel() const
{
    return ShouldHideSniperScope();
}

float QSI_Client::GetViewModelFOV() const
{
    if (!qsi_cvars[CVAR_VIEWMODEL_FOV] || qsi_cvars[CVAR_VIEWMODEL_FOV]->value <= 0.0f)
        return 0.0f;

    return ClampQsiFOV(qsi_cvars[CVAR_VIEWMODEL_FOV]->value);
}

float QSI_Client::GetViewModelSway() const
{
    if (!qsi_cvars[CVAR_SWAY] || qsi_cvars[CVAR_SWAY]->value <= 0.0f)
        return 0.0f;

    return qsi_cvars[CVAR_SWAY]->value;
}

void QSI_Client::LoadFriendsList()
{
    g_Friends.clear();
    FILE* f = fopen("friends.lst", "rt");
    if (!f) {
        f = fopen("friends.lst", "wt");
        if (f) fclose(f);
        return;
    }
    char line[128];
    while (fgets(line, sizeof(line), f))
    {
        char* p = strtok(line, "\r\n\t ");
        if (p && p[0])
            g_Friends.push_back(p);
    }
    fclose(f);
    Con_Printf("[QSI] %d Friend list loaded.\n", (int)g_Friends.size());
}

void QSI_Client::SaveFriendsList()
{
    FILE* f = fopen("friends.lst", "wt");
    if (!f) return;
    for (const auto& name : g_Friends)
        fprintf(f, "%s\n", name.c_str());
    fclose(f);
}

bool QSI_Client::IsFriend(const char* name)
{
    if (!name || !name[0]) return false;
    for (const auto& f : g_Friends)
    {
        if (strcasecmp(f.c_str(), name) == 0)
            return true;
    }
    return false;
}

void QSI_Client::HUD_Redraw(float time, int intermission)
{
    for (int i = 1; i <= GetMaxClients(); i++) {
        GetPlayerInfo(i, &g_PlayerInfoList[i]);
    }
    
    StealName();
    DrawTopPlayers();
    UpdateRainbowXhairColor();
    AutoPlay_DrawStatus();
    
    cl_entity_t* local = GetLocalPlayer();
    
    if (!local)
    	return;
    
    if (!IsPlayerAlive(local))
        return;
    
    DrawFOVCircle();
    DrawESP();
    DrawGroundEntities();
    DrawTargetInfo();
    NoFlashIMP();
    DrawBehindArrows();
    ApplyViewModelGlow();
}
