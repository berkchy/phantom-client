#pragma once

#include "hud.h"
#include "cl_util.h"
#include "com_weapons.h"
#include "pm_defs.h"
#include "entity_types.h"
#include "cdll_int.h"
#include "util_vector.h"
#include "com_model.h"

#include <cmath>
#include <vector>
#include <string>
#include "studio.h"

#define AIMBOT_MAX_PLAYERS 33

struct WeaponOffset
{
    const char* name;
    float x;
    float y;
    float z;
};

extern WeaponOffset g_WeaponOffsets[];
constexpr int MAX_GROUND_ENTITIES = 512;
struct PlayerInfo_t { bool bAlive = false; };
extern float g_playerBoneTransforms[AIMBOT_MAX_PLAYERS][MAXSTUDIOBONES][3][4];
extern bool  g_playerBoneTransformValid[AIMBOT_MAX_PLAYERS];

enum CvarIndex {
    CVAR_AIM,
    CVAR_SILENTAIM,
    CVAR_TEAMCHECK,
    CVAR_DEBUG,
    CVAR_ESP,
    CVAR_Z,
    CVAR_X,
    CVAR_Y,
    CVAR_NAME_STEAL,
    CVAR_TOPPLAYERS,
    CVAR_ESP_STYLE,
    CVAR_AIM_ON_WALL,
    CVAR_AIM_FOV,
    CVAR_AIM_FOV_DRAW,
    CVAR_ESP_ENT,
    CVAR_ESP_ENT_SIZE,
    CVAR_ESP_ENT_COLOR,
    CVAR_TRIGGERBOT,
    CVAR_TRIGGER_DELAY,
    CVAR_NOFLASH,
    CVAR_LOOKDOWN,
    CVAR_JITTER,
    CVAR_JITTER_INTENSITY,
    CVAR_JITTER_SPEED,
    CVAR_FRIENDLIST,
    CVAR_TARGETINFO,
    CVAR_BEHIND_ARROW,
    CVAR_SNIPER_NOSCOPE,
    CVAR_SCREEN_FOV,
    CVAR_VIEWMODEL_FOV,
    CVAR_SWAY,
    CVAR_VIEWMODEL_GLOW,
    CVAR_ENTITY_GLOW,
    CVAR_CRAZY_SPIN,
    CVAR_SPIN_SPEED,
    CVAR_SPIN_FACTOR,
    CVAR_SPIN_YAW,
    CVAR_SPIN_PITCH,
    CVAR_SPIN_ROLL,
    CVAR_SPIN_RANDOM,
    CVAR_AIM_BONE,
    CVAR_STRAFEBOOSTER,
    CVAR_TRACER,
    CVAR_BACKTRACK,
    CVAR_BACKTRACK_MS,
    CVAR_NOSPREAD,
    CVAR_XHAIR_RAINBOW,
    CVAR_XHAIR_RAINBOW_SPEED,
    CVAR_ESP_WEAPON_SCALE,
    CVAR_GROUND_WEAPON_SCALE,
    CVAR_BHOP_ASSIST,
    CVAR_BHOP_GROUND_DIST,
    CVAR_COUNT
};

class QSI_Client
{
public:
    QSI_Client();

    void InitCVars();
    void Initialize();
    void AimAtEnemy(usercmd_t* cmd);
    void SilentAim(usercmd_t* cmd);
    void ScreenJitter(usercmd_t* cmd);
    void LookDown(usercmd_t* cmd);
    void CreateMove(usercmd_t* cmd);
    void HUD_Redraw(float time, int intermission);
    void ClearESPData();

    void DrawESP();
    void DrawSkeleton(cl_entity_t* ent, int r, int g, int b, int a);
    void DrawLine(int x1, int y1, int x2, int y2, int thickness, int r, int g, int b, int a);
    void DrawTopPlayers();
    void DrawESPBox(float left, float top, float right, float bottom, int r, int g, int b, int a, cl_entity_t* ent);
    void DrawFOVCircle();
    void DrawGroundEntities();
    void DrawWeaponSpriteOnGround(float sx, float sy, int boxSize, const char* mdl, int r, int g, int b, float pulseAlpha);
    void DrawTargetInfo();
    void DrawBehindArrows();
    void DrawTracer(float bx, float by);
    void SaveConfig();
    void LoadConfig();
    void TriggerbotFire(usercmd_t* cmd);
    void UpdateBacktrack();
    Vector GetBacktrackPos(int entityIndex);
    void BunnyHop(usercmd_t* cmd);
    void GroundStrafe(usercmd_t* cmd);
    void AutoStrafe(usercmd_t* cmd);
    void StrafeBooster(usercmd_t* cmd);
    void AutoScopeSniper(usercmd_t* cmd);
    void UpdateRainbowXhairColor();
    void NoFlashIMP();
    void ApplyViewModelGlow();
    void ApplyEntityGlow();
    void CrazySpin(usercmd_t* cmd);
    void SetRawFOV(int rawFov);
    int GetVisibleFOV(int rawFov, int defaultFov) const;
    float GetMouseSensitivity(int rawFov, int defaultFov) const;
    bool ShouldHideSniperScope() const;
    bool ShouldKeepSniperViewmodel() const;
    float GetViewModelFOV() const;
    float GetViewModelSway() const;

    void LoadFriendsList();
    void SaveFriendsList();
    bool IsFriend(const char* name);

public:
    cvar_t* qsi_cvars[CVAR_COUNT];

    std::vector<std::string> g_Friends;

    int m_iBestTarget = -1;
    std::string m_cachedXhairColor = "255 255 255";
    int m_iGs = 0;
    int m_iViewModelGlowR = 255;
    int m_iViewModelGlowG = 80;
    int m_iViewModelGlowB = 80;

private:
    void CalcAngle(const Vector& src, const Vector& dst, Vector& angles);
    int FindBestTargetByFov(const Vector& eyePos, const Vector& viewAngles, float maxFov);
    int FindBestTargetByDistance(const Vector& eyePos);
    bool IsValidTarget(cl_entity_t* ent);
    bool IsPlayerFriend(cl_entity_t* ent);
    bool IsVisible(cl_entity_t* ent);
    bool WorldToScreen(const Vector& world, float& screenX, float& screenY);
    int  GetPlayerTeam(int index);
    float GetDistance(const Vector& src, const Vector& dst);
    float GetFov(const Vector& viewangles, const Vector& src, const Vector& dst);
    int GetBaseScreenFOV(int defaultFov) const;
    int GetActiveWeaponId() const;
    bool IsSniperWeapon(int weaponId) const;

private:
    int m_iRawFOV = 0;
    float m_flNextAutoScopeTime = 0.0f;
    int m_iLastAutoScopeWeaponId = WEAPON_NONE;
    Vector m_vecSpinRandomTarget;
    Vector m_vecSpinRandomCurrent;
    float m_flNextSpinRandomTime = 0.0f;
    float m_flLastSpinUpdateTime = 0.0f;
};

extern QSI_Client QSI;
void PrintESPStyles();
float GetHeadOffset(cl_entity_t* ent, int xyz);
Vector GetBonePosition(cl_entity_t* ent, int boneIndex);
void StealName();
void HSVtoRGB(float h, float s, float v, float* r, float* g, float* b);
void Cmd_AddFriend();
void Cmd_DelFriend();
void Cmd_ListFriends();
