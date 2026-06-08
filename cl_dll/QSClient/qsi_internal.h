#pragma once

#include "qsi.h"
#include "studio.h"
#include "GameStudioModelRenderer.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

constexpr float kEyeHeightDucked = 12.0f;
constexpr float kEyeHeightStanding = 17.0f;
constexpr float kAutoScopeDelay = 0.75f;

struct StudioRendererSnapshot
{
    cl_entity_t* currentEntity;
    model_t* renderModel;
    player_info_t* playerInfo;
    studiohdr_t* studioHeader;
    mstudiobodyparts_t* bodyPart;
    mstudiomodel_t* subModel;
    entity_state_t* playerState;
    int playerIndex;
    int frameCount;
    int topColor;
    int bottomColor;
    double clientTime;
    double oldClientTime;
    float gaitMovement;
    float softwareXScale;
    float softwareYScale;
    float viewUp[3];
    float viewRight[3];
    float viewNormal[3];
    float renderOrigin[3];
    float boneTransform[MAXSTUDIOBONES][3][4];
    float lightTransform[MAXSTUDIOBONES][3][4];
};

void CaptureStudioRendererSnapshot(CGameStudioModelRenderer& renderer, StudioRendererSnapshot& snapshot);
void RestoreStudioRendererSnapshot(CGameStudioModelRenderer& renderer, const StudioRendererSnapshot& snapshot);

int FindStudioBoneByName(studiohdr_t* studioHeader, const char* const* boneNames, size_t boneNameCount);
int ResolveStudioBoneIndex(studiohdr_t* studioHeader, cl_entity_t* ent, int boneIndex);
Vector GetBonePosition(cl_entity_t* ent, int boneIndex);

float NormalizeQsiAngle(float angle);
void FixQsiMovementForYaw(usercmd_t* cmd, float originalYaw, float newYaw);
int ClampQsiFOV(int fov);
float ClampQsiFOV(float fov);

bool ContainsNoCase(const char* text, const char* find);
bool WeaponModelToSpriteName(const char* wpnName, char* spriteName, size_t spriteSize);
void ExtractWeaponName(const char* modelPath, char* outBuf, size_t outSize);
void DebugGroundEntity(cl_entity_t* ent);

bool IsPlayerAlive(const cl_entity_t* ent);
bool IsAliveEntity(cl_entity_t* Entity);

float GetHeadOffset(cl_entity_t* ent, int xyz);
inline bool IsGhostPlayer(int index)
{
    if (index < 1 || index > MAX_PLAYERS) return true;
    if (g_PlayerExtraInfo[index].teamnumber == 0) return true;
    const char* name = g_PlayerInfoList[index].name;
    if (!name || name[0] == '\0' || strlen(name) < 1) return true;
    return false;
}
Vector AimPos(cl_entity_t* ent);
void HSVtoRGB(float h, float s, float v, float* r, float* g, float* b);
void StealName();

#define BACKTRACK_FRAMES 16
struct BacktrackRecord { Vector pos; float time; };
extern BacktrackRecord g_BacktrackHistory[AIMBOT_MAX_PLAYERS][BACKTRACK_FRAMES];

extern PlayerInfo_t g_Player[MAX_PLAYERS + 1];
extern float g_fLastESPUpdate[MAX_PLAYERS + 1];
