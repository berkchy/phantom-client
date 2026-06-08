#include "qsi_internal.h"
#include <cfloat>

#define GetViewAngles           gEngfuncs.GetViewAngles
#define SetViewAngles           gEngfuncs.SetViewAngles
#define GetEntityByIndex        gEngfuncs.GetEntityByIndex
#define GetLocalPlayer          gEngfuncs.GetLocalPlayer
#define GetMaxClients           gEngfuncs.GetMaxClients
#define GetClientTime           gEngfuncs.GetClientTime
#define IsSpectateOnly          gEngfuncs.IsSpectateOnly
#define PM_TraceLine            gEngfuncs.PM_TraceLine



struct Timer {
    float a;
    float b;

    Timer(float d = 1.0f) {
        b = d;
        a = 0.0f;
    }

    bool Ready() {
        float c = GetClientTime();

        if (a <= 0.0f) {
            a = c;
            return false;
        }

		// time reset fix
        if (c < a) {
            a = c;
            return false;
        }

        if ((c - a) >= b) {
            a = c;
            return true;
        }

        return false;
    }
};

static bool CanWallbang(Vector src, Vector dst)
{
    Vector dir = dst - src;
    float totalDist = dir.Length();
    if (totalDist < 1.0f) return false;
    dir = dir / totalDist;

    pmtrace_t firstTrace;
    {
        pmtrace_t* tr = PM_TraceLine(src, dst, PM_TRACELINE_PHYSENTSONLY, 2, -1);
        if (!tr) return false;
        if (tr->fraction >= 1.0f) return true;
        firstTrace = *tr;
    }

    if (firstTrace.allsolid || firstTrace.startsolid)
        return false;

    Vector startBeyond = firstTrace.endpos + dir * 2.0f;
    Vector endBeyond = firstTrace.endpos + dir * 100.0f;

    pmtrace_t secondTrace;
    {
        pmtrace_t* tr = PM_TraceLine(startBeyond, endBeyond, PM_TRACELINE_PHYSENTSONLY, 2, -1);
        if (!tr) return false;
        secondTrace = *tr;
    }

    float wallThickness;
    if (secondTrace.fraction >= 1.0f)
        wallThickness = (endBeyond - startBeyond).Length();
    else
        wallThickness = (secondTrace.endpos - startBeyond).Length();

    return wallThickness <= 60.0f;
}

float QSI_Client::GetFov(const Vector& viewangles, const Vector& src, const Vector& dst)
{
    Vector delta = dst - src;
    float hyp = sqrtf(delta.x * delta.x + delta.y * delta.y);
    if (hyp < 1.0f) return 360.0f;
    Vector aimAngle;
    CalcAngle(src, dst, aimAngle);
    float yawDiff = aimAngle.y - viewangles.y;
    yawDiff = fmodf(yawDiff + 540.0f, 360.0f) - 180.0f;
    float pitchDiff = aimAngle.x - viewangles.x;
    pitchDiff = fmodf(pitchDiff + 540.0f, 360.0f) - 180.0f;
    if (fabsf(yawDiff) > 90.0f) return 360.0f;
    return fabsf(yawDiff) + fabsf(pitchDiff) - 0.5f;
}

bool QSI_Client::IsVisible(cl_entity_t* ent)
{
    if (!ent) return false;
    cl_entity_t* local = GetLocalPlayer();
    if (!local) return false;
    Vector eyePos = local->origin;
    eyePos.z += (local->curstate.usehull == 1) ? kEyeHeightDucked : kEyeHeightStanding;
    Vector targetPos = AimPos(ent);
    pmtrace_t* tr = PM_TraceLine(eyePos, targetPos, PM_TRACELINE_PHYSENTSONLY, 2, -1);
    return (tr && tr->fraction > 0.8f);
}

bool QSI_Client::IsPlayerFriend(cl_entity_t* ent)
{
    if (qsi_cvars[CVAR_FRIENDLIST] && qsi_cvars[CVAR_FRIENDLIST]->value > 0.5f)
    {
        const char* name = g_PlayerInfoList[ent->index].name;
        if (name && name[0] && QSI.IsFriend(name))
        {
            return true;
        }
    }
    return false;
}

bool QSI_Client::IsValidTarget(cl_entity_t* ent)
{
    if (!ent) return false;
    if (ent->index < 1 || ent->index > MAX_PLAYERS) return false;

    cl_entity_t* local = GetLocalPlayer();
    if (!local) return false;
    if (ent == local) return false;
    if (IsGhostPlayer(ent->index)) return false;
    if (!IsAliveEntity(ent)) return false;

    if (ent->curstate.messagenum != 0)
    {
        int diff = local->curstate.messagenum - ent->curstate.messagenum;
        if (diff > 1 || diff < -5) return false;
    }

    return true;
}

int QSI_Client::GetPlayerTeam(int index)
{
    if (index < 1 || index > MAX_PLAYERS) return 0;
    return g_PlayerExtraInfo[index].teamnumber;
}

void QSI_Client::CalcAngle(const Vector& src, const Vector& dst, Vector& angles)
{
    Vector delta = dst - src;
    float hyp = sqrtf(delta.x * delta.x + delta.y * delta.y);
    if (hyp < 1.0f) hyp = 1.0f;
    angles.x = RAD2DEG(atan2f(-delta.z, hyp));
    angles.y = RAD2DEG(atan2f(delta.y, delta.x));
    angles.z = 0.0f;
}

float QSI_Client::GetDistance(const Vector& src, const Vector& dst)
{
    Vector delta = dst - src;
    return sqrtf(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
}

int QSI_Client::FindBestTargetByFov(const Vector& eyePos, const Vector& viewAngles, float maxFov)
{
    cl_entity_t* local = GetLocalPlayer();
    if (!local) return -1;

    int bestTarget = -1;
    float bestFov = FLT_MAX;

    for (int i = 1; i <= GetMaxClients(); i++)
    {
        cl_entity_t* ent = GetEntityByIndex(i);
        if (!IsValidTarget(ent)) continue;
        if (IsPlayerFriend(ent)) continue;

        if (qsi_cvars[CVAR_TEAMCHECK] && qsi_cvars[CVAR_TEAMCHECK]->value > 0.5f)
            if (GetPlayerTeam(local->index) == GetPlayerTeam(i)) continue;

        if (!IsVisible(ent))
        {
            if (!(qsi_cvars[CVAR_AIM_ON_WALL] && qsi_cvars[CVAR_AIM_ON_WALL]->value > 0.5f
                && CanWallbang(eyePos, AimPos(ent))))
                continue;
        }

        Vector aimPos = AimPos(ent);
        float fovDist = GetFov(viewAngles, eyePos, aimPos);
        if (fovDist > maxFov + 1.0f) continue;
        if (fovDist < bestFov)
        {
            bestFov = fovDist;
            bestTarget = i;
        }
    }

    return bestTarget;
}

int QSI_Client::FindBestTargetByDistance(const Vector& eyePos)
{
    cl_entity_t* local = GetLocalPlayer();
    if (!local) return -1;

    int bestTarget = -1;
    float bestDistance = FLT_MAX;

    for (int i = 1; i <= GetMaxClients(); i++)
    {
        cl_entity_t* ent = GetEntityByIndex(i);
        if (!IsValidTarget(ent)) continue;
        if (IsPlayerFriend(ent)) continue;

        if (qsi_cvars[CVAR_TEAMCHECK] && qsi_cvars[CVAR_TEAMCHECK]->value > 0.5f)
            if (GetPlayerTeam(local->index) == GetPlayerTeam(i)) continue;

        if (!IsVisible(ent))
        {
            if (!(qsi_cvars[CVAR_AIM_ON_WALL] && qsi_cvars[CVAR_AIM_ON_WALL]->value > 0.5f
                && CanWallbang(eyePos, AimPos(ent))))
                continue;
        }

        float distance = GetDistance(eyePos, ent->origin);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestTarget = i;
        }
    }

    return bestTarget;
}

void QSI_Client::SilentAim(usercmd_t* cmd)
{
    if (!qsi_cvars[CVAR_AIM] || qsi_cvars[CVAR_AIM]->value < 0.5f) return;
    if (!qsi_cvars[CVAR_SILENTAIM] || qsi_cvars[CVAR_SILENTAIM]->value < 0.5f) return;

    if (gHUD.m_iIntermission || IsSpectateOnly()) return;

    cl_entity_t* local = GetLocalPlayer();
    if (!local) return;

    Vector eyePos = local->origin;
    eyePos.z += (local->curstate.usehull == 1) ? kEyeHeightDucked : kEyeHeightStanding;

    int bestTarget = FindBestTargetByDistance(eyePos);

    m_iBestTarget = bestTarget;

    if (bestTarget == -1) return;

    cl_entity_t* targetEnt = GetEntityByIndex(bestTarget);
    if (!targetEnt) return;

    Vector finalAimPos = AimPos(targetEnt);
    Vector aimAngle;
    CalcAngle(eyePos, finalAimPos, aimAngle);

    if (qsi_cvars[CVAR_TRIGGERBOT] && qsi_cvars[CVAR_TRIGGERBOT]->value > 0.5f)
        TriggerbotFire(cmd);

    if (cmd->buttons & IN_ATTACK)
        cmd->viewangles = aimAngle;
}

void QSI_Client::AimAtEnemy(usercmd_t* cmd)
{
	if (!(qsi_cvars[CVAR_AIM] && qsi_cvars[CVAR_AIM]->value > 0.5f)) return;
    if (gHUD.m_iIntermission || IsSpectateOnly()) return;
    cl_entity_t* local = GetLocalPlayer();
    if (!local) return;
    Vector eyePos = local->origin;
    eyePos.z += (local->curstate.usehull == 1) ? kEyeHeightDucked : kEyeHeightStanding;
    Vector viewAngles;
    GetViewAngles(viewAngles);
    float maxFov = qsi_cvars[CVAR_AIM_FOV] ? qsi_cvars[CVAR_AIM_FOV]->value : 180.0f;
    int bestTarget = FindBestTargetByFov(eyePos, viewAngles, maxFov);

    m_iBestTarget = bestTarget;

    if (bestTarget == -1) return;

    cl_entity_t* targetEnt = GetEntityByIndex(bestTarget);
    if (!targetEnt) return;
    Vector finalAimPos = AimPos(targetEnt);
    Vector aimAngle;
    CalcAngle(eyePos, finalAimPos, aimAngle);
    if (qsi_cvars[CVAR_TRIGGERBOT] && qsi_cvars[CVAR_TRIGGERBOT]->value > 0.5f)
        TriggerbotFire(cmd);
    cmd->viewangles = aimAngle;
    SetViewAngles(aimAngle);
}

void QSI_Client::TriggerbotFire(usercmd_t* cmd)
{
    if (!cmd) return;
    cl_entity_t* local = GetLocalPlayer();
    if (!local) return;
    if (!QSI.qsi_cvars[CVAR_TRIGGERBOT] || QSI.qsi_cvars[CVAR_TRIGGERBOT]->value < 0.5f) return;
    
	static Timer triggerbot((float)qsi_cvars[CVAR_TRIGGER_DELAY]->value);
    if (triggerbot.Ready())
    	cmd->buttons |= IN_ATTACK;
    else
    	cmd->buttons &= ~IN_ATTACK;
}

void QSI_Client::UpdateBacktrack()
{
    if (!qsi_cvars[CVAR_BACKTRACK] || qsi_cvars[CVAR_BACKTRACK]->value < 0.5f) return;
    float now = GetClientTime();
    float window = qsi_cvars[CVAR_BACKTRACK_MS] ? qsi_cvars[CVAR_BACKTRACK_MS]->value : 200.0f;
    if (window < 1.0f) window = 1.0f;
    if (window > 500.0f) window = 500.0f;
    for (int i = 1; i <= GetMaxClients(); i++) {
        cl_entity_t* ent = GetEntityByIndex(i);
        if (!ent || !ent->player) continue;
        Vector headPos = GetBonePosition(ent, 1);
        for (int j = BACKTRACK_FRAMES - 1; j > 0; j--)
            g_BacktrackHistory[i][j] = g_BacktrackHistory[i][j - 1];
        g_BacktrackHistory[i][0] = {headPos, now};
    }
}

Vector QSI_Client::GetBacktrackPos(int entityIndex)
{
    if (!qsi_cvars[CVAR_BACKTRACK] || qsi_cvars[CVAR_BACKTRACK]->value < 0.5f)
        return Vector(0, 0, 0);
    float now = GetClientTime();
    float window = qsi_cvars[CVAR_BACKTRACK_MS] ? qsi_cvars[CVAR_BACKTRACK_MS]->value : 200.0f;
    if (window < 1.0f) window = 1.0f;
    float bestDist = FLT_MAX;
    Vector bestPos(0, 0, 0);
    cl_entity_t* local = GetLocalPlayer();
    if (!local) return Vector(0, 0, 0);
    Vector eyePos = local->origin;
    eyePos.z += (local->curstate.usehull == 1) ? kEyeHeightDucked : kEyeHeightStanding;
    for (int j = 0; j < BACKTRACK_FRAMES; j++) {
        float age = now - g_BacktrackHistory[entityIndex][j].time;
        if (age < 0 || age > window / 1000.0f) continue;
        float dist = (g_BacktrackHistory[entityIndex][j].pos - eyePos).Length();
        if (dist < bestDist) {
            bestDist = dist;
            bestPos = g_BacktrackHistory[entityIndex][j].pos;
        }
    }
    return bestPos;
}
