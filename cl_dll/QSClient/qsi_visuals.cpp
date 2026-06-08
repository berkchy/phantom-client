#include "qsi_internal.h"
#include "r_studioint.h"
#include <cfloat>

#define GetClientTime           gEngfuncs.GetClientTime
#define GetMaxClients           gEngfuncs.GetMaxClients
#define GetEntityByIndex        gEngfuncs.GetEntityByIndex
#define GetScreenFade           gEngfuncs.pfnGetScreenFade
#define SetScreenFade           gEngfuncs.pfnSetScreenFade
#define Cvar_Set                gEngfuncs.Cvar_Set
#define GetCvarPointer          gEngfuncs.pfnGetCvarPointer

struct screenfade_s
{
    float       fadeSpeed;
    float       fadeEnd;
    float       fadeTotalEnd;
    float       fadeReset;
    byte        fader, fadeg, fadeb, fadealpha;
    int         fadeFlags;
};

typedef struct screenfade_s screenfade_t;
extern engine_studio_api_t IEngineStudio;

void HSVtoRGB(float h, float s, float v, float* r, float* g, float* b)
{
    if (s == 0) {
        *r = *g = *b = v;
        return;
    }
    h = fmod(h, 360.0f) / 60.0f;
    int i = (int)h;
    float f = h - i;
    float p = v * (1 - s);
    float q = v * (1 - s * f);
    float t = v * (1 - s * (1 - f));
    switch (i) {
        case 0: *r = v; *g = t; *b = p; break;
        case 1: *r = q; *g = v; *b = p; break;
        case 2: *r = p; *g = v; *b = t; break;
        case 3: *r = p; *g = q; *b = v; break;
        case 4: *r = t; *g = p; *b = v; break;
        default: *r = v; *g = p; *b = q; break;
    }
}

void QSI_Client::UpdateRainbowXhairColor()
{
    cvar_t* xhair_color = GetCvarPointer("xhair_color");
    if (m_cachedXhairColor != xhair_color->string && qsi_cvars[CVAR_XHAIR_RAINBOW]->value < 0.5f) {
        Cvar_Set("xhair_color", m_cachedXhairColor.c_str());
        return;
    }
    if (!qsi_cvars[CVAR_XHAIR_RAINBOW] || !qsi_cvars[CVAR_XHAIR_RAINBOW_SPEED] || !xhair_color) return;
    if (qsi_cvars[CVAR_XHAIR_RAINBOW]->value < 0.5f) return;
    float time = GetClientTime();
    float speed = qsi_cvars[CVAR_XHAIR_RAINBOW_SPEED]->value;
    float hue = fmodf(time * speed * 60.0f, 360.0f);
    float rr, gg, bb;
    HSVtoRGB(hue, 1.0f, 1.0f, &rr, &gg, &bb);
    int r = (int)(rr * 255);
    int g = (int)(gg * 255);
    int b = (int)(bb * 255);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d %d %d", r, g, b);
    Cvar_Set("xhair_color", buf);
}

void QSI_Client::NoFlashIMP()
{
	if (qsi_cvars[CVAR_NOFLASH] && qsi_cvars[CVAR_NOFLASH]->value > 0.5f)
    {
        screenfade_t fade;
        GetScreenFade(&fade);
        if (fade.fadealpha > 0)
        {
            fade.fadealpha = 0;
            SetScreenFade(&fade);
        }
    }
}

void QSI_Client::ApplyViewModelGlow()
{
    cl_entity_t* viewModel = gEngfuncs.GetViewModel();
    if (!viewModel)
        return;

    if (!qsi_cvars[CVAR_VIEWMODEL_GLOW] || qsi_cvars[CVAR_VIEWMODEL_GLOW]->value < 0.5f)
    {
        if (viewModel->curstate.renderfx == kRenderFxGlowShell)
            viewModel->curstate.renderfx = kRenderFxNone;
        return;
    }

    viewModel->curstate.renderfx = kRenderFxGlowShell;
    viewModel->curstate.renderamt = 24;
    viewModel->curstate.rendercolor.r = m_iViewModelGlowR;
    viewModel->curstate.rendercolor.g = m_iViewModelGlowG;
    viewModel->curstate.rendercolor.b = m_iViewModelGlowB;
}

void QSI_Client::ApplyEntityGlow()
{
    if (!qsi_cvars[CVAR_ENTITY_GLOW] || qsi_cvars[CVAR_ENTITY_GLOW]->value < 0.5f)
        return;

    for (int i = 1; i <= GetMaxClients(); i++)
    {
        cl_entity_t* ent = GetEntityByIndex(i);
        if (!ent || !ent->player || !IsValidTarget(ent))
            continue;

        int team = GetPlayerTeam(i);
        if (team == 1) // TR
        {
            ent->curstate.renderfx = kRenderFxGlowShell;
            ent->curstate.renderamt = 22;
            ent->curstate.rendercolor.r = 255;
            ent->curstate.rendercolor.g = 60;
            ent->curstate.rendercolor.b = 60;
        }
        else if (team == 2) // CT
        {
            ent->curstate.renderfx = kRenderFxGlowShell;
            ent->curstate.renderamt = 22;
            ent->curstate.rendercolor.r = 60;
            ent->curstate.rendercolor.g = 160;
            ent->curstate.rendercolor.b = 255;
        }
    }
}
