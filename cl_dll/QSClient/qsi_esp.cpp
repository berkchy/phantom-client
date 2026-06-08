#include "qsi_internal.h"
#include "wrect.h"
#include "GameStudioModelRenderer.h"
#include "r_studioint.h"
#include <cstdlib>
#include <algorithm>

extern engine_studio_api_t IEngineStudio;

#define GetViewAngles           gEngfuncs.GetViewAngles
#define GetEntityByIndex        gEngfuncs.GetEntityByIndex
#define GetLocalPlayer          gEngfuncs.GetLocalPlayer
#define GetMaxClients           gEngfuncs.GetMaxClients
#define GetClientTime           gEngfuncs.GetClientTime
#define DrawString              gEngfuncs.pfnDrawString
#define DrawConsoleString       gEngfuncs.pfnDrawConsoleString
#define hudGetModelByIndex      gEngfuncs.hudGetModelByIndex

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

constexpr float kEspBoxTopOffset = 72.0f;
constexpr float kSkeletonZOffset = 47.0f;
constexpr float kUnitsToMeters = 52.0f;
constexpr float kWorldToScreenZOffset = 64.0f;
constexpr float kEspBoxWidthRatio = 0.4f;
constexpr float kWeaponSpriteScale = 0.38f;
constexpr float kGroundWeaponSpriteScale = 0.32f;
constexpr float kCornerBoxRatio = 0.35f;
constexpr int kMaxTopPlayersY = 200;

bool QSI_Client::WorldToScreen(const Vector& world, float& screenX, float& screenY)
{
    cl_entity_t* local = GetLocalPlayer();
    if (!local) return false;
    Vector viewOrigin = local->origin;
    Vector viewAngles;
    GetViewAngles(viewAngles);
    int specMode = local->curstate.iuser1;
    int specTarget = local->curstate.iuser2;
    if (specMode == 4 || specMode == 5 || specMode == 6) {
        if (specTarget >= 1 && specTarget <= MAX_PLAYERS) {
            cl_entity_t* target = GetEntityByIndex(specTarget);
            if (target && target->player) viewOrigin = target->origin;
        }
    }
    viewOrigin.z += kWorldToScreenZOffset;
    Vector delta = world - viewOrigin;
    Vector forward, right, up;
    AngleVectors(viewAngles, forward, right, up);
    float dotForward = DotProduct(delta, forward);
    if (dotForward <= 0.01f) return false;
    float dotRight = DotProduct(delta, right);
    float dotUp    = DotProduct(delta, up);
    float fov = gHUD.m_iFOV ? gHUD.m_iFOV : 90.0f;
    float tanHalfFov = tanf(DEG2RAD(fov) * 0.5f);
    if (tanHalfFov < 0.001f) tanHalfFov = 0.001f;
    float focalLen = (gHUD.m_scrinfo.iWidth * 0.5f) / tanHalfFov;
    screenX = gHUD.m_scrinfo.iWidth  * 0.5f + (dotRight / dotForward) * focalLen;
    screenY = gHUD.m_scrinfo.iHeight * 0.5f - (dotUp   / dotForward) * focalLen;
    return true;
}

void QSI_Client::DrawLine(int x1, int y1, int x2, int y2, int thickness, int r, int g, int b, int a)
{
    int dx = abs(x2 - x1);
    int sx = x1 < x2 ? 1 : -1;
    int dy = -abs(y2 - y1);
    int sy = y1 < y2 ? 1 : -1;
    int err = dx + dy;

    for (;;)
    {
        FillRGBA(x1 - thickness / 2, y1 - thickness / 2, thickness, thickness, r, g, b, a);
        if (x1 == x2 && y1 == y2)
            break;

        const int e2 = err * 2;
        if (e2 >= dy)
        {
            err += dy;
            x1 += sx;
        }
        if (e2 <= dx)
        {
            err += dx;
            y1 += sy;
        }
    }
}

void QSI_Client::DrawSkeleton(cl_entity_t* ent, int r, int g, int b, int a)
{
    if (!ent || !ent->player || !ent->model)
        return;

    model_t* renderModel = ent->model;
    studiohdr_t* studioHeader = (studiohdr_t*)IEngineStudio.Mod_Extradata(renderModel);
    if (!studioHeader)
        return;

    static const struct {
        const char* name1;
        const char* name2;
    } bonePairs[] = {
        {"Bip01 Pelvis", "Bip01 Spine"},
        {"Bip01 Spine", "Bip01 Spine1"},
        {"Bip01 Spine1", "Bip01 Spine2"},
        {"Bip01 Spine2", "Bip01 Spine3"},
        {"Bip01 Spine3", "Bip01 Neck"},
        {"Bip01 Neck", "Bip01 Head"},
        {"Bip01 Neck", "Bip01 L UpperArm"},
        {"Bip01 L UpperArm", "Bip01 L Forearm"},
        {"Bip01 L Forearm", "Bip01 L Hand"},
        {"Bip01 Neck", "Bip01 R UpperArm"},
        {"Bip01 R UpperArm", "Bip01 R Forearm"},
        {"Bip01 R Forearm", "Bip01 R Hand"},
        {"Bip01 Pelvis", "Bip01 L Thigh"},
        {"Bip01 L Thigh", "Bip01 L Calf"},
        {"Bip01 L Calf", "Bip01 L Foot"},
        {"Bip01 Pelvis", "Bip01 R Thigh"},
        {"Bip01 R Thigh", "Bip01 R Calf"},
        {"Bip01 R Calf", "Bip01 R Foot"}
    };

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
    renderer.m_nPlayerIndex = ent->index - 1;

    if (renderer.m_nPlayerIndex >= 0 && renderer.m_nPlayerIndex < GetMaxClients())
    {
        renderModel = IEngineStudio.SetupPlayerModel(renderer.m_nPlayerIndex);
        livePlayerInfo = IEngineStudio.PlayerInfo(renderer.m_nPlayerIndex);
        if (renderModel && livePlayerInfo)
        {
            savedPlayerInfo = *livePlayerInfo;
            shouldRestorePlayerInfo = true;
            renderer.m_pPlayerInfo = livePlayerInfo;
        }
    }

    renderer.m_pRenderModel = renderModel;
    renderer.m_pStudioHeader = studioHeader;
    IEngineStudio.StudioSetHeader(studioHeader);
    IEngineStudio.SetRenderModel(renderModel);

    if (ent->curstate.gaitsequence)
    {
        vec3_t originalAngles;
        VectorCopy(ent->angles, originalAngles);
        renderer.StudioProcessGait(&ent->curstate);
        if (livePlayerInfo) livePlayerInfo->gaitsequence = ent->curstate.gaitsequence;
        renderer.m_pPlayerInfo = nullptr;
        renderer.StudioSetUpTransform(0);
        VectorCopy(originalAngles, ent->angles);
        renderer.m_pPlayerInfo = livePlayerInfo;
    }
    else
    {
        renderer.CalculatePitchBlend(&ent->curstate);
        renderer.CalculateYawBlend(&ent->curstate);
        if (livePlayerInfo) livePlayerInfo->gaitsequence = 0;
        renderer.StudioSetUpTransform(0);
    }

    renderer.StudioSetupBones();

    for (const auto& pair : bonePairs)
    {
        const char* b1Names[] = {pair.name1};
        const char* b2Names[] = {pair.name2};
        int idx1 = FindStudioBoneByName(studioHeader, b1Names, 1);
        int idx2 = FindStudioBoneByName(studioHeader, b2Names, 1);

        if (idx1 >= 0 && idx2 >= 0)
        {
            Vector v1, v2;
            v1.x = (*renderer.m_pbonetransform)[idx1][0][3];
            v1.y = (*renderer.m_pbonetransform)[idx1][1][3];
            v1.z = (*renderer.m_pbonetransform)[idx1][2][3] + kSkeletonZOffset;

            v2.x = (*renderer.m_pbonetransform)[idx2][0][3];
            v2.y = (*renderer.m_pbonetransform)[idx2][1][3];
            v2.z = (*renderer.m_pbonetransform)[idx2][2][3] + kSkeletonZOffset;

            float x1, y1, x2, y2;
            if (WorldToScreen(v1, x1, y1) && WorldToScreen(v2, x2, y2))
            {
                DrawLine((int)x1, (int)y1, (int)x2, (int)y2, 1, r, g, b, a);
            }
        }
    }

    ent->curstate = savedCurstate;
    ent->latched = savedLatched;
    VectorCopy(savedAngles, ent->angles);
    if (shouldRestorePlayerInfo && livePlayerInfo)
        *livePlayerInfo = savedPlayerInfo;
    RestoreStudioRendererSnapshot(renderer, snapshot);
}

void QSI_Client::DrawESPBox(float left, float top, float right, float bottom, int r, int g, int b, int a, cl_entity_t* ent) {
    const char* style = qsi_cvars[CVAR_ESP_STYLE] ? qsi_cvars[CVAR_ESP_STYLE]->string : "corner-box";
    if (strcmp(style, "corner-box") == 0) {
        float cs = (right - left) * kCornerBoxRatio;
        int t = 2;
        FillRGBA(left, top, cs, t, r,g,b,a);
        FillRGBA(left, top, t, cs, r,g,b,a);
        FillRGBA(right - cs, top, cs, t, r,g,b,a);
        FillRGBA(right - t, top, t, cs, r,g,b,a);
        FillRGBA(left, bottom - t, cs, t, r,g,b,a);
        FillRGBA(left, bottom - cs, t, cs, r,g,b,a);
        FillRGBA(right - cs, bottom - t, cs, t, r,g,b,a);
        FillRGBA(right - t, bottom - cs, t, cs, r,g,b,a);
    }
    else if (strcmp(style, "2d-box") == 0) {
        int t = 2;
        FillRGBA(left, top, right-left, t, r,g,b,a);
        FillRGBA(left, top, t, bottom-top, r,g,b,a);
        FillRGBA(left, bottom-t, right-left, t, r,g,b,a);
        FillRGBA(right-t, top, t, bottom-top, r,g,b,a);
    }
    else if (strcmp(style, "outline") == 0) {
        FillRGBA(left-2, top-2, right-left+4, bottom-top+4, 0,0,0,180);
        FillRGBA(left, top, right-left, bottom-top, r,g,b,a);
    }
    else if (strcmp(style, "glow") == 0) {
        FillRGBA(left-4, top-4, right-left+8, bottom-top+8, r,g,b,80);
        FillRGBA(left, top, right-left, bottom-top, r,g,b,a);
    }
    else if (strcmp(style, "filled") == 0) {
        FillRGBA(left, top, right-left, bottom-top, r,g,b,60);
        FillRGBA(left, top, right-left, 2, r,g,b,255);
        FillRGBA(left, top, 2, bottom-top, r,g,b,255);
        FillRGBA(left, bottom-2, right-left, 2, r,g,b,255);
        FillRGBA(right-2, top, 2, bottom-top, r,g,b,255);
    }
    else if (strcmp(style, "dashed") == 0) {
        int t = 2;
        for (int i = 0; i < (right-left); i += 8) {
            FillRGBA(left+i, top, 4, t, r,g,b,a);
            FillRGBA(left+i, bottom-t, 4, t, r,g,b,a);
        }
        for (int i = 0; i < (bottom-top); i += 8) {
            FillRGBA(left, top+i, t, 4, r,g,b,a);
            FillRGBA(right-t, top+i, t, 4, r,g,b,a);
        }
    }
    else if (strcmp(style, "gradient") == 0) {
        int steps = 20;
        for (int i = 0; i < steps; i++) {
            int rr = r * (steps-i) / steps;
            int gg = g * (steps-i) / steps;
            int bb = b * (steps-i) / steps;
            FillRGBA(left, top + i*(bottom-top)/steps, right-left, (bottom-top)/steps, rr,gg,bb,a);
        }
    }
    else if (strcmp(style, "hitbox") == 0) {
        DrawSkeleton(ent, r, g, b, a);
        Vector headPos = GetBonePosition(ent, 1);
        float sx, sy;
        if (WorldToScreen(headPos, sx, sy)) {
            int hx = (int)sx, hy = (int)sy;
            int radius = (int)((right - left) * 0.35f);
            if (radius < 4) radius = 4;
            if (radius > 30) radius = 30;
            int segs = 20;
            for (int ci = 0; ci < segs; ci++) {
                float a1 = ci * (360.0f / segs) * (M_PI / 180.0f);
                float a2 = (ci + 1) * (360.0f / segs) * (M_PI / 180.0f);
                int x1 = hx + (int)(cosf(a1) * radius);
                int y1 = hy + (int)(sinf(a1) * radius);
                int x2 = hx + (int)(cosf(a2) * radius);
                int y2 = hy + (int)(sinf(a2) * radius);
                DrawLine(x1, y1, x2, y2, 2, r, g, b, a);
            }
        }
    }
    else if (strcmp(style, "arrow") == 0) {
        int cx = (int)((left+right)*0.5f), cy = (int)((top+bottom)*0.5f);
        int hs = (int)((right-left)*0.3f);
        int t = 2; int x = (int)left, y = (int)top, rx = (int)right, by = (int)bottom;
        DrawLine(x, y, x+hs, y, t, r,g,b,a); DrawLine(x, y, x, y+hs, t, r,g,b,a);
        DrawLine(rx-hs, y, rx, y, t, r,g,b,a); DrawLine(rx, y, rx, y+hs, t, r,g,b,a);
        DrawLine(x, by-hs, x, by, t, r,g,b,a); DrawLine(x, by, x+hs, by, t, r,g,b,a);
        DrawLine(rx-hs, by, rx, by, t, r,g,b,a); DrawLine(rx, by-hs, rx, by, t, r,g,b,a);
        DrawLine(cx, y-6, cx, y, 1, r,g,b,a); DrawLine(x-6, cy, x, cy, 1, r,g,b,a);
        DrawLine(cx, by, cx, by+6, 1, r,g,b,a); DrawLine(rx, cy, rx+6, cy, 1, r,g,b,a);
    }
    else if (strcmp(style, "snapbar") == 0) {
        int t = 2, bw = (int)(right-left), bh = (int)(bottom-top);
        FillRGBA((int)left, (int)top, bw, t, r,g,b,a);
        FillRGBA((int)left, (int)top, t, bh, r,g,b,a);
        FillRGBA((int)left, (int)(bottom-t), bw, t, r,g,b,a);
        FillRGBA((int)(right-t), (int)top, t, bh, r,g,b,a);
        int cx = (int)((left+right)*0.5f), scrW = gHUD.m_scrinfo.iWidth;
        int scrH = gHUD.m_scrinfo.iHeight;
        DrawLine(cx, (int)bottom, scrW/2, scrH, 1, r, g, b, a/2);
    }
    else if (strcmp(style, "cross") == 0) {
        int cx = (int)((left+right)*0.5f), cy = (int)((top+bottom)*0.5f);
        int len = (int)((right-left)*0.5f) + 4;
        DrawLine(cx-len, cy, cx+len, cy, 1, r,g,b,a);
        DrawLine(cx, cy-len, cx, cy+len, 1, r,g,b,a);
        int gap = 4;
        DrawLine(cx-len+gap, cy, cx-len/2, cy, 2, r,g,b,a);
        DrawLine(cx+len/2, cy, cx+len-gap, cy, 2, r,g,b,a);
        DrawLine(cx, cy-len+gap, cx, cy-len/2, 2, r,g,b,a);
        DrawLine(cx, cy+len/2, cx, cy+len-gap, 2, r,g,b,a);
    }
    else if (strcmp(style, "radar") == 0) {
        int cx = (int)((left+right)*0.5f), cy = (int)((top+bottom)*0.5f);
        int rdot = (int)((right-left)*0.2f); if (rdot < 3) rdot = 3;
        FillRGBA(cx-rdot, cy-rdot, rdot*2, rdot*2, 0,0,0,180);
        FillRGBA(cx-rdot+1, cy-rdot+1, rdot*2-2, rdot*2-2, r,g,b,180);
        FillRGBA(cx-1, cy-rdot+2, 2, rdot*2-4, 255,255,255,60);
        FillRGBA(cx-rdot+2, cy-1, rdot*2-4, 2, 255,255,255,60);
        DrawLine(cx, cy-rdot, cx, cy+rdot, 1, 0,0,0,100);
        DrawLine(cx-rdot, cy, cx+rdot, cy, 1, 0,0,0,100);
    }
    else if (strcmp(style, "scifi") == 0) {
        int t = 1, bw = (int)(right-left), bh = (int)(bottom-top);
        int x = (int)left, y = (int)top, rx = (int)right, by = (int)bottom;
        DrawLine(x, y, rx, y, t, r,g,b,a);
        DrawLine(x, by, rx, by, t, r,g,b,a);
        int cx = (int)((left+right)*0.5f);
        DrawLine(cx-2, y-3, cx+2, y-3, t, r,g,b,a);
        DrawLine(cx-2, by+3, cx+2, by+3, t, r,g,b,a);
        DrawLine(cx, y-6, cx, y-3, t, r,g,b,a);
        DrawLine(cx, by+3, cx, by+6, t, r,g,b,a);
        for (int i = 0; i < bh/8; i++) {
            int lx = x + (i%2)*8 + 2;
            FillRGBA(lx, y + i*8 + 4, 3, 1, r,g,b,80);
            FillRGBA(rx-3, y + i*8 + 4, 3, 1, r,g,b,80);
        }
    }
    else if (strcmp(style, "thick") == 0) {
        int t = 4;
        FillRGBA((int)left-1, (int)top-1, (int)(right-left)+2, t, r,g,b,a);
        FillRGBA((int)left-1, (int)top-1, t, (int)(bottom-top)+2, r,g,b,a);
        FillRGBA((int)left-1, (int)(bottom-t+1), (int)(right-left)+2, t, r,g,b,a);
        FillRGBA((int)(right-t-1), (int)top-1, t, (int)(bottom-top)+2, r,g,b,a);
        int hp = g_PlayerExtraInfo[ent->index].health;
        if (hp > 0 && hp <= 100) {
            int hr = 255-hp*2, hg = hp*2;
            if (hr < 0) hr = 0; if (hg > 255) hg = 255;
            float frac = hp / 100.0f;
            int barW = (int)((right-left-4) * frac);
            FillRGBA((int)left+2, (int)top-5, barW, 3, hr, hg, 0, 200);
            FillRGBA((int)left+2, (int)top-6, (int)(right-left-4)+1, 5, 0,0,0,120);
        }
    }
}

void QSI_Client::DrawFOVCircle()
{
    if (!qsi_cvars[CVAR_AIM_FOV_DRAW] || qsi_cvars[CVAR_AIM_FOV_DRAW]->value < 0.5f) return;
    int cx = gHUD.m_scrinfo.iWidth / 2;
    int cy = gHUD.m_scrinfo.iHeight / 2;
    float fovDeg = qsi_cvars[CVAR_AIM_FOV] ? qsi_cvars[CVAR_AIM_FOV]->value : 8.0f;
    float radius = tanf(DEG2RAD(fovDeg / 2.0f)) * (gHUD.m_scrinfo.iWidth / 2.0f) * 1.8f;
    float pulse = sinf(GetClientTime() * 3.0f) * 0.2f + 1.0f;
    int pulseAlpha = (int)(pulse * 180.0f + 75.0f);
    int thickness = 4;
    const int segments = 32;
    for (int i = 0; i < segments; i++)
    {
        float angle1 = DEG2RAD(i * (360.0f / segments));
        float angle2 = DEG2RAD((i + 1) * (360.0f / segments));
        int x1 = cx + (int)(cosf(angle1) * radius);
        int y1 = cy + (int)(sinf(angle1) * radius);
        int x2 = cx + (int)(cosf(angle2) * radius);
        int y2 = cy + (int)(sinf(angle2) * radius);
        int dx = x2 - x1;
        int dy = y2 - y1;
        float len = sqrtf(dx*dx + dy*dy);
        if (len > 0) {
            FillRGBA(
                (x1 + x2) / 2 - thickness / 2,
                (y1 + y2) / 2 - thickness / 2,
                thickness, thickness,
                255, 40, 40, pulseAlpha
            );
            FillRGBA(
                (x1 + x2) / 2 - 1,
                (y1 + y2) / 2 - 1,
                2, 2,
                255, 100, 100, (int)(pulseAlpha * 0.7f)
            );
        }
    }
    char fovText[32];
    sprintf(fovText, "FOV: %.0f", fovDeg);
    DrawConsoleString(cx - 35, cy + (int)radius + 12, fovText);
    DrawConsoleString(cx - 34, cy + (int)radius + 12, (char*)"FOV:");
}

void QSI_Client::DrawESP()
{
    if (!qsi_cvars[CVAR_ESP] || qsi_cvars[CVAR_ESP]->value < 0.5f) return;
    cl_entity_t* local = GetLocalPlayer();
    if (!local) return;
    for (int i = 1; i <= GetMaxClients(); i++)
    {
        cl_entity_t* ent = GetEntityByIndex(i);
        if (!ent || !ent->player) continue;
        hud_player_info_t info;
        GetPlayerInfo(i, &info);
        if (!info.name || !info.name[0]) continue;
        if (!IsValidTarget(ent)) continue;
        bool isFriend = false;
        if (qsi_cvars[CVAR_FRIENDLIST] && qsi_cvars[CVAR_FRIENDLIST]->value > 0.5f)
        {
            isFriend = IsPlayerFriend(ent);
        }
        Vector origin = ent->origin;
        Vector bottomPos = origin;
        Vector topPos = origin;
        topPos.z += kEspBoxTopOffset;
        float sbx, sby, stx, sty;
        if (!WorldToScreen(bottomPos, sbx, sby)) continue;
        if (!WorldToScreen(topPos, stx, sty)) continue;
        float h = sby - sty;
        float w = h * kEspBoxWidthRatio;
        float left   = stx - w / 2;
        float top    = sty;
        float right  = stx + w / 2;
        float bottom = sby;
        int sw = gHUD.m_scrinfo.iWidth;
        int sh = gHUD.m_scrinfo.iHeight;
        if (right < 0 || left > sw || bottom < 0 || top > sh) continue;
        bool visible = IsVisible(ent);
        int team = GetPlayerTeam(i);
        int r, g, b;
        r = (team == 1) ? (visible ? 255 : 150) : (team == 2) ? 0 : 255;
        g = (team == 1) ? (visible ? 100 : 60) : (team == 2) ? 150 : 255;
        b = (team == 1) ? 0 : (team == 2) ? 255 : 0;
        int a = visible ? 255 : 40;
        DrawESPBox(left, top, right, bottom, r, g, b, a, ent);
        DrawTracer(stx, sty);
        char text[128] = {0};
        strcpy(text, info.name);
        if (ent->curstate.weaponmodel > 0)
        {
            model_t* pModel = hudGetModelByIndex(ent->curstate.weaponmodel);
            if (pModel && pModel->name[0] != '\0')
            {
                char wpn[64] = "";
                ExtractWeaponName(pModel->name, wpn, sizeof(wpn));
                char spriteName[64] = "";
                WeaponModelToSpriteName(wpn, spriteName, sizeof(spriteName));
                int iSprite = gHUD.GetSpriteIndex(spriteName);
                if (iSprite > 0)
                {
                    wrect_t rect = gHUD.GetSpriteRect(iSprite);
                    int w = rect.right - rect.left;
                    int h = rect.bottom - rect.top;
                    if (w > 0 && h > 0)
                    {
                        float scale = qsi_cvars[CVAR_ESP_WEAPON_SCALE] ? qsi_cvars[CVAR_ESP_WEAPON_SCALE]->value : 1.0f;
                        scale = std::max(0.10f, std::min(scale, 4.0f));
                        int drawW = std::max(1, (int)(w * scale + 0.5f));
                        int drawH = std::max(1, (int)(h * scale + 0.5f));
                        int iconX = (int)stx - drawW / 2;
                        int iconY = (int)sty - drawH - 20;
                        SPR_Set(gHUD.GetSprite(iSprite), r, g, b);
                        if (gEngfuncs.pfnSPR_DrawAdditiveScale)
                            SPR_DrawAdditiveScale(0, iconX - 5, iconY, &rect, scale);
                        else
                            SPR_DrawAdditive(0, iconX - 5, iconY, &rect);
                    }
                }
                strcat(text, " | ");
                char* dot = strstr(wpn, ".mdl");
                if (dot) *dot = 0;
                if (wpn[0] >= 'a' && wpn[0] <= 'z') wpn[0] -= 32;
                strcat(text, wpn);
            }
        }
        char distStr[16];
        sprintf(distStr, " %.0fm", GetDistance(local->origin, ent->origin) / kUnitsToMeters);
        strcat(text, distStr);
        int tx = (int)stx - (strlen(text) * 4);
        int ty = (int)bottom + 5;
        DrawConsoleString(tx, ty, text);
        const char* teamName;
        int teamR = 255, teamG = 255, teamB = 255;
        if (team == 1) {
            teamName = "[TR]";
            teamR = 255; teamG = 0; teamB = 0;
        } else if (team == 2) {
            teamName = "[CT]";
            teamR = 0; teamG = 150; teamB = 255;
        } else {
            teamName = "[SPEC]";
            teamR = 128; teamG = 128; teamB = 128;
        }
        int teamTx = (int)stx - (strlen(teamName) * 4);
        int teamTy = ty + 18;
        DrawConsoleString(teamTx, teamTy + 7, (char*)teamName);
        if (isFriend)
        {
            int friendTy = teamTy + 15;
            DrawConsoleString(teamTx - 9, friendTy + 16, (char*)"FRIEND");
        }
    }
}

void QSI_Client::DrawWeaponSpriteOnGround(float sx, float sy, int boxSize,
                                       const char* mdl, int r, int g, int b, float pulseAlpha)
{
    if (!mdl || !mdl[0]) return;
    char wpnName[64] = "";
    ExtractWeaponName(mdl, wpnName, sizeof(wpnName));
    if (!wpnName[0]) return;
    char spriteName[64] = "";
    if (!WeaponModelToSpriteName(wpnName, spriteName, sizeof(spriteName)))
        return;
    int iSprite = gHUD.GetSpriteIndex(spriteName);
    if (iSprite <= 0)
    {
        char fallback[32];
        snprintf(fallback, sizeof(fallback), "%s", wpnName);
        DrawConsoleString((int)sx - (strlen(fallback) * 3),
                                      (int)sy - boxSize - 30,
                                      fallback);
        return;
    }
    wrect_t rect = gHUD.GetSpriteRect(iSprite);
    int iWeaponWidth  = rect.right  - rect.left;
    int iWeaponHeight = rect.bottom - rect.top;
    if (iWeaponWidth <= 0 || iWeaponHeight <= 0) return;
    float scale = qsi_cvars[CVAR_GROUND_WEAPON_SCALE] ? qsi_cvars[CVAR_GROUND_WEAPON_SCALE]->value : 1.0f;
    scale = std::max(0.10f, std::min(scale, 4.0f));
    int drawW = std::max(1, (int)(iWeaponWidth * scale + 0.5f));
    int drawH = std::max(1, (int)(iWeaponHeight * scale + 0.5f));
    int iconX = (int)sx - drawW / 2;
    int iconY = (int)sy - boxSize - drawH - 17;
    int alpha = (int)(pulseAlpha * 255.0f);
    if (alpha < 80) alpha = 80;
    SPR_Set(gHUD.GetSprite(iSprite), r, g, b);
    if (gEngfuncs.pfnSPR_DrawAdditiveScale)
        SPR_DrawAdditiveScale(0, iconX - 5, iconY, &rect, scale);
    else
        SPR_DrawAdditive(0, iconX - 5, iconY, &rect);
}

void QSI_Client::DrawGroundEntities()
{
    if (!qsi_cvars[CVAR_ESP_ENT] || qsi_cvars[CVAR_ESP_ENT]->value < 0.5f) return;
    cl_entity_t* local = GetLocalPlayer();
    if (!local) return;
    int localTeam = g_PlayerExtraInfo[local->index].teamnumber;
    int boxSize = qsi_cvars[CVAR_ESP_ENT_SIZE] ? (int)qsi_cvars[CVAR_ESP_ENT_SIZE]->value : 18;
    int localMsg = local->curstate.messagenum;
    for (int i = GetMaxClients() + 1; i <= MAX_GROUND_ENTITIES; i++)
    {
        cl_entity_t* ent = GetEntityByIndex(i);
        if (!ent || ent->index >= 1024) continue;
        if (ent->player || (ent->curstate.effects & EF_NODRAW)) continue;
        if (!ent->model || !ent->model->name[0]) continue;
        if (ent->curstate.messagenum == 0) continue;
        int diff = localMsg - ent->curstate.messagenum;
        if (diff > 8 || diff < -8) continue;
        if (qsi_cvars[CVAR_DEBUG]->value >= 2.0)
        	DebugGroundEntity(ent);
        const char* mdl = ent->model->name;
        char itemName[64] = "Unknown";
        if (strstr(mdl, "w_")) {
            const char* fname = strrchr(mdl, '/') ? strrchr(mdl, '/') + 1 : mdl;
            strncpy(itemName, fname + 2, 60);
            char* dot = strstr(itemName, ".mdl");
            if (dot) *dot = '\0';
            if (itemName[0] >= 'a' && itemName[0] <= 'z') itemName[0] -= 32;
        }
        else if (strstr(mdl, "c4") || strstr(mdl, "ied")) {
            strcpy(itemName, "C4");
        }
        else if (strstr(mdl, "hostage")) {
            strcpy(itemName, "Hostage");
        }
        else continue;
        int r = 255, g = 255, b = 100;
        if (strstr(mdl, "usp") || strstr(mdl, "glock") || strstr(mdl, "deagle") ||
            strstr(mdl, "p228") || strstr(mdl, "fiveseven") || strstr(mdl, "elite")) {
            r=0; g=100; b=255;
        }
        else if (strstr(mdl, "ak47") || strstr(mdl, "m4a1") || strstr(mdl, "galil") ||
                 strstr(mdl, "famas") || strstr(mdl, "mp5") || strstr(mdl, "tmp") ||
                 strstr(mdl, "mac10") || strstr(mdl, "ump45") || strstr(mdl, "p90")) {
            r=0; g=255; b=0;
        }
        else if (strstr(mdl, "awp") || strstr(mdl, "scout") || strstr(mdl, "sg550") ||
                 strstr(mdl, "g3sg1")) {
            r=255; g=255; b=0;
        }
        else if (strstr(mdl, "m249") || strstr(mdl, "m3") || strstr(mdl, "xm1014")) {
            r=255; g=100; b=0;
        }
        else if (strstr(mdl, "hegrenade") || strstr(mdl, "flashbang") ||
                 strstr(mdl, "smokegrenade")) {
            r=180; g=0; b=255;
        }
        else if (strstr(mdl, "c4") || strstr(mdl, "ied")) {
            r=255; g=0; b=0;
        }
        else if (strstr(mdl, "hostage")) {
            r=0; g=255; b=255;
        }
        char ownerStr[64] = "";
        int owner = ent->curstate.owner;
        if (owner >= 1 && owner <= GetMaxClients()) {
            const char* ownerName = g_PlayerInfoList[owner].name;
            if (ownerName && ownerName[0]) {
                snprintf(ownerStr, sizeof(ownerStr), " by %s", ownerName);
            }
        }
        bool isPlantedC4 = (strcmp(itemName, "C4") == 0);
        float pulse = sinf(GetClientTime() * 6.0f) * 0.5f + 0.5f;
        int alpha = (int)(180.0f + 75.0f * pulse);
        Vector pos = ent->origin;
        pos.z += 50.0f;
        float sx, sy;
        if (!WorldToScreen(pos, sx, sy)) continue;
        int half = boxSize / 2;
        FillRGBA((int)sx - half, (int)sy - half, boxSize, 2, r, g, b, alpha);
        FillRGBA((int)sx - half, (int)sy + half, boxSize, 2, r, g, b, alpha);
        FillRGBA((int)sx - half, (int)sy - half, 2, boxSize, r, g, b, alpha);
        FillRGBA((int)sx + half, (int)sy - half, 2, boxSize, r, g, b, alpha);
        char text[128];
        float dist = GetDistance(local->origin, ent->origin) / kUnitsToMeters;
        if (isPlantedC4 && localTeam == 2) {
            sprintf(text, "DEFUSE %.1fm", dist);
        } else {
            sprintf(text, "%s%s %.0fm", itemName, ownerStr, dist);
        }
        DrawWeaponSpriteOnGround(sx - 3, sy, boxSize, mdl, r, g, b, pulse);
        DrawConsoleString((int)sx - (strlen(text) * 4) / 2, (int)sy + boxSize + 8, text);
    }
}

void QSI_Client::DrawTopPlayers()
{
    if (!qsi_cvars[CVAR_TOPPLAYERS] || qsi_cvars[CVAR_TOPPLAYERS]->value < 0.5f) return;
    int sw = gHUD.m_scrinfo.iWidth;
    int xCT = sw / 4;
    int xTR = sw * 3 / 4;
    int y = 10;
    DrawString(xCT - 40, y, "CT", 0, 150, 255);
    y += 20;
    for (int i = 1; i <= GetMaxClients(); i++)
    {
        if (g_PlayerExtraInfo[i].teamnumber != 2) continue;
        const char* name = g_PlayerInfoList[i].name;
        if (!name || !name[0]) continue;
        DrawString(xCT - 40, y, name, 0, 150, 255);
        y += 15;
        if (y > kMaxTopPlayersY) break;
    }
    y = 10;
    DrawString(xTR - 40, y, "TR", 255, 0, 0);
    y += 20;
    for (int i = 1; i <= GetMaxClients(); i++)
    {
        if (g_PlayerExtraInfo[i].teamnumber != 1) continue;
        const char* name = g_PlayerInfoList[i].name;
        if (!name || !name[0]) continue;
        DrawString(xTR - 40, y, name, 255, 0, 0);
        y += 15;
        if (y > kMaxTopPlayersY) break;
    }
}

void QSI_Client::ClearESPData()
{
    memset(g_Player, 0, sizeof(g_Player));
    memset(g_fLastESPUpdate, 0, sizeof(g_fLastESPUpdate));
}

void QSI_Client::DrawTracer(float bx, float by)
{
    if (!qsi_cvars[CVAR_TRACER] || qsi_cvars[CVAR_TRACER]->value < 0.5f) return;
    int cx = gHUD.m_scrinfo.iWidth / 2;
    int cy = gHUD.m_scrinfo.iHeight / 2;
    DrawLine(cx, cy, (int)bx, (int)by, 1, 255, 255, 255, 160);
}

void QSI_Client::DrawTargetInfo()
{
    if (!qsi_cvars[CVAR_TARGETINFO] || qsi_cvars[CVAR_TARGETINFO]->value < 0.5f)
        return;

    if (m_iBestTarget <= 0)
        return;

    cl_entity_t* target = GetEntityByIndex(m_iBestTarget);
    if (!target || !IsValidTarget(target))
        return;

    hud_player_info_t pinfo;
    GetPlayerInfo(m_iBestTarget, &pinfo);

    if (!pinfo.name || !pinfo.name[0])
        return;

    int screenWidth = gHUD.m_scrinfo.iWidth;
    int screenHeight = gHUD.m_scrinfo.iHeight;

    int startX = screenWidth / 2 - 155;
    int startY = screenHeight / 2 + (screenHeight * 15 / 100);

    int team = GetPlayerTeam(m_iBestTarget);
    int bgR = (team == 1) ? 110 : 0;
    int bgG = (team == 1) ? 15  : 55;
    int bgB = (team == 1) ? 15  : 130;

    FillRGBA(startX - 12, startY - 9, 285, 108, bgR, bgG, bgB, 88);

    int borderR = (team == 1) ? 255 : 0;
    int borderG = (team == 1) ? 80  : 200;
    int borderB = (team == 1) ? 80  : 255;

    FillRGBA(startX - 14, startY - 11, 289, 2, borderR, borderG, borderB, 230);
    FillRGBA(startX - 14, startY + 97, 289, 2, borderR, borderG, borderB, 230);

    int lineY = startY;
    int labelX = startX + 8;
    int valueX = startX + 98;

    DrawConsoleString(labelX, lineY, (char*)"Target:");
    lineY += 18;
    DrawConsoleString(labelX, lineY, (char*)"Location:");
    lineY += 18;
    DrawConsoleString(labelX, lineY, (char*)"Team:");
    lineY += 18;
    DrawConsoleString(labelX, lineY, (char*)"Weapon:");
    lineY += 18;
    DrawConsoleString(labelX, lineY, (char*)"Distance:");
    lineY += 18;

    lineY = startY;

    char nameBuf[64];
    snprintf(nameBuf, sizeof(nameBuf), "%s", pinfo.name);
    DrawConsoleString(valueX, lineY, nameBuf);
    lineY += 18;

    Vector pos = target->origin;
    char locBuf[64];
    snprintf(locBuf, sizeof(locBuf), "%.0f, %.0f, %.0f", pos.x, pos.y, pos.z);
    DrawConsoleString(valueX, lineY, locBuf);
    lineY += 18;

    const char* teamStr = (team == 1) ? "TERRORIST" : (team == 2) ? "COUNTER-TERRORIST" : "SPECTATOR";
    DrawConsoleString(valueX, lineY, (char*)teamStr);
    lineY += 18;

    char weaponName[64] = "Unknown";
    if (target->curstate.weaponmodel > 0)
    {
        model_t* pModel = hudGetModelByIndex(target->curstate.weaponmodel);
        if (pModel && pModel->name[0])
        {
            ExtractWeaponName(pModel->name, weaponName, sizeof(weaponName));
            if (weaponName[0] >= 'a' && weaponName[0] <= 'z')
                weaponName[0] -= 32;
        }
    }
    DrawConsoleString(valueX, lineY, weaponName);
    lineY += 18;

    float dist = GetDistance(GetLocalPlayer()->origin, target->origin) / kUnitsToMeters;
    char distBuf[32];
    snprintf(distBuf, sizeof(distBuf), "%.1f m", dist);
    DrawConsoleString(valueX, lineY, distBuf);
}

void QSI_Client::DrawBehindArrows()
{
    if (!qsi_cvars[CVAR_BEHIND_ARROW] || qsi_cvars[CVAR_BEHIND_ARROW]->value < 0.5f)
        return;

    cl_entity_t* local = GetLocalPlayer();
    if (!local)
        return;

    Vector localAngles;
    GetViewAngles(localAngles);

    Vector forward;
    AngleVectors(localAngles, forward, NULL, NULL);

    int centerX = gHUD.m_scrinfo.iWidth / 2;
    int centerY = gHUD.m_scrinfo.iHeight / 2;

    for (int i = 1; i <= GetMaxClients(); i++)
    {
        cl_entity_t* ent = GetEntityByIndex(i);
        if (!ent || !IsValidTarget(ent) || ent->index == local->index)
            continue;

        if (IsPlayerFriend(ent))
            continue;

        Vector dir = ent->origin - local->origin;
        float len = dir.Length();
        if (len < 30.0f) continue;

        dir.x = dir.x / len;
        dir.y = dir.y / len;
        dir.z = dir.z / len;

        float dot = forward.x * dir.x + forward.y * dir.y;

        if (dot > -0.10f)
            continue;

        int team = GetPlayerTeam(i);
        int r = (team == 1) ? 255 : 100;
        int g = (team == 1) ? 90  : 255;
        int b = (team == 1) ? 90  : 255;

        float angle = atan2(dir.y, dir.x) * 180.0f / M_PI;
        float diff = localAngles.y - angle;

        while (diff > 180.0f) diff -= 360.0f;
        while (diff < -180.0f) diff += 360.0f;

        float rad = diff * (M_PI / 180.0f);
        int radius = gHUD.m_scrinfo.iHeight / 2 - 75;

        int x = centerX + (int)(sin(rad) * radius);
        int y = centerY - (int)(cos(rad) * radius);

        const float dirX = sinf(rad);
        const float dirY = -cosf(rad);
        const float perpX = -dirY;
        const float perpY = dirX;

        const float lenTri = 16.0f;
    	const float halfBase = 8.0f;
        const float baseOffset = 12.0f;

        const float tipX = (float)x;
        const float tipY = (float)y;
        const float baseCX = tipX - dirX * baseOffset;
        const float baseCY = tipY - dirY * baseOffset;

        auto DrawTriangle = [&](float ax, float ay, float bx, float by, float cx, float cy, int cr, int cg, int cb, int ca)
        {
            int minX = (int)floorf(fminf(ax, fminf(bx, cx)));
            int maxX = (int)ceilf(fmaxf(ax, fmaxf(bx, cx)));
            int minY = (int)floorf(fminf(ay, fminf(by, cy)));
            int maxY = (int)ceilf(fmaxf(ay, fmaxf(by, cy)));

            float area = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
            if (fabsf(area) < 0.001f)
                return;

            for (int py = minY; py <= maxY; ++py)
            {
                for (int px = minX; px <= maxX; ++px)
                {
                    float w0 = (bx - ax) * (py - ay) - (by - ay) * (px - ax);
                    float w1 = (cx - bx) * (py - by) - (cy - by) * (px - bx);
                    float w2 = (ax - cx) * (py - cy) - (ay - cy) * (px - cx);

                    bool inside = (w0 >= 0 && w1 >= 0 && w2 >= 0) || (w0 <= 0 && w1 <= 0 && w2 <= 0);
                    if (inside)
                        FillRGBA(px, py, 1, 1, cr, cg, cb, ca);
                }
            }
        };

        // glow
        float gTipX = tipX + dirX * 2.0f;
        float gTipY = tipY + dirY * 2.0f;
        float gLeftX = baseCX + perpX * (halfBase + 3.0f) - dirX * (lenTri * 0.25f);
        float gLeftY = baseCY + perpY * (halfBase + 3.0f) - dirY * (lenTri * 0.25f);
        float gRightX = baseCX - perpX * (halfBase + 3.0f) - dirX * (lenTri * 0.25f);
        float gRightY = baseCY - perpY * (halfBase + 3.0f) - dirY * (lenTri * 0.25f);
        DrawTriangle(gTipX, gTipY, gLeftX, gLeftY, gRightX, gRightY, r, g, b, 55);

        // outline
        float oTipX = tipX + dirX * 1.0f;
        float oTipY = tipY + dirY * 1.0f;
        float oLeftX = baseCX + perpX * (halfBase + 1.5f);
        float oLeftY = baseCY + perpY * (halfBase + 1.5f);
        float oRightX = baseCX - perpX * (halfBase + 1.5f);
        float oRightY = baseCY - perpY * (halfBase + 1.5f);
        DrawTriangle(oTipX, oTipY, oLeftX, oLeftY, oRightX, oRightY, 0, 0, 0, 220);

        // fill
        float fTipX = tipX;
        float fTipY = tipY;
        float fLeftX = baseCX + perpX * (halfBase - 1.0f);
        float fLeftY = baseCY + perpY * (halfBase - 1.0f);
        float fRightX = baseCX - perpX * (halfBase - 1.0f);
        float fRightY = baseCY - perpY * (halfBase - 1.0f);
        DrawTriangle(fTipX, fTipY, fLeftX, fLeftY, fRightX, fRightY, r, g, b, 245);
    }
}
