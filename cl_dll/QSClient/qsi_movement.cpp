#include "qsi_internal.h"
#include "bot_autoplay.h"

#define GetViewAngles           gEngfuncs.GetViewAngles
#define GetLocalPlayer          gEngfuncs.GetLocalPlayer
#define GetClientTime           gEngfuncs.GetClientTime
#define IsSpectateOnly          gEngfuncs.IsSpectateOnly
#define Cvar_Set                gEngfuncs.Cvar_Set
#define PM_TraceLine            gEngfuncs.PM_TraceLine

extern int g_iWaterLevel;

void onGroundStrafe() 
{
	QSI.m_iGs = 1; 
}

void offGroundStrafe() 
{
	QSI.m_iGs = 0;
}

static bool QSI_IsNearGround( cl_entity_t* local, float distance )
{
    if( !local )
        return false;

    const float traceDist = distance > 4.0f ? distance : 4.0f;
    Vector start = local->origin;
    Vector end = start;
    start.z += 1.0f;
    end.z -= traceDist;

    pmtrace_t *tr = PM_TraceLine( start, end, PM_TRACELINE_PHYSENTSONLY, 2, local->index );
    if( !tr )
        return false;

    return tr->fraction < 1.0f && !tr->allsolid;
}

void QSI_Client::AutoScopeSniper(usercmd_t* cmd)
{
    if (!cmd || !qsi_cvars[CVAR_SNIPER_NOSCOPE] || qsi_cvars[CVAR_SNIPER_NOSCOPE]->value < 0.5f)
        return;

    const int weaponId = GetActiveWeaponId();
    if (!IsSniperWeapon(weaponId))
    {
        m_iLastAutoScopeWeaponId = weaponId;
        m_flNextAutoScopeTime = 0.0f;
        return;
    }

    if (weaponId != m_iLastAutoScopeWeaponId)
    {
        m_iLastAutoScopeWeaponId = weaponId;
        m_flNextAutoScopeTime = 0.0f;
    }

    if ((m_iRawFOV > 0 && m_iRawFOV < 90) || ShouldHideSniperScope())
        return;

    if (cmd->buttons & (IN_ATTACK | IN_ATTACK2))
        return;

    const float currentTime = GetClientTime();
    if (currentTime < m_flNextAutoScopeTime)
        return;

    cmd->buttons |= IN_ATTACK2;
    m_flNextAutoScopeTime = currentTime + kAutoScopeDelay;
}

void QSI_Client::ScreenJitter(usercmd_t* cmd)
{
    if (!cmd) return;

    if (!qsi_cvars[CVAR_JITTER] || qsi_cvars[CVAR_JITTER]->value < 0.5f)
        return;

    float currentTime = GetClientTime();

    float intensity = qsi_cvars[CVAR_JITTER_INTENSITY] ? qsi_cvars[CVAR_JITTER_INTENSITY]->value : 0.4f;   // Sallanma şiddeti
    float speed     = qsi_cvars[CVAR_JITTER_SPEED]     ? qsi_cvars[CVAR_JITTER_SPEED]->value     : 42.0f;  // Sallanma hızı

    float jitterX = sinf(currentTime * speed)          * intensity;
    float jitterY = cosf(currentTime * speed * 1.27f)  * (intensity * 0.75f);

    cmd->viewangles.x += jitterX;
    cmd->viewangles.y += jitterY;
    cmd->viewangles.z = 0.0f;
}

void QSI_Client::CrazySpin(usercmd_t* cmd)
{
    if (!cmd)
        return;

    if (!qsi_cvars[CVAR_CRAZY_SPIN] || qsi_cvars[CVAR_CRAZY_SPIN]->value < 0.5f)
        return;

    if (gHUD.m_iIntermission || IsSpectateOnly())
        return;

    if (cmd->buttons & (IN_ATTACK | IN_ATTACK2))
        return;

    const float time = GetClientTime();
    Vector cameraAngles;
    GetViewAngles(cameraAngles);

    Vector angles = cameraAngles;
    const float originalYaw = cameraAngles.y;
    const float spinSpeed = qsi_cvars[CVAR_SPIN_SPEED] ? qsi_cvars[CVAR_SPIN_SPEED]->value : 1440.0f;
    const float spinFactor = qsi_cvars[CVAR_SPIN_FACTOR] ? qsi_cvars[CVAR_SPIN_FACTOR]->value : 2.75f;
    const float yawRange = qsi_cvars[CVAR_SPIN_YAW] ? qsi_cvars[CVAR_SPIN_YAW]->value : 180.0f;
    const float pitchRange = qsi_cvars[CVAR_SPIN_PITCH] ? qsi_cvars[CVAR_SPIN_PITCH]->value : 89.0f;
    const float rollRange = qsi_cvars[CVAR_SPIN_ROLL] ? qsi_cvars[CVAR_SPIN_ROLL]->value : 20.0f;
    const bool randomTargets = qsi_cvars[CVAR_SPIN_RANDOM] && qsi_cvars[CVAR_SPIN_RANDOM]->value >= 0.5f;

    const float safeFactor = spinFactor < 0.1f ? 0.1f : spinFactor;
    const float clampedYawRange = yawRange < 0.0f ? 0.0f : ( yawRange > 360.0f ? 360.0f : yawRange );
    const float clampedPitchRange = pitchRange < 0.0f ? 0.0f : ( pitchRange > 89.0f ? 89.0f : pitchRange );
    const float clampedRollRange = rollRange < 0.0f ? 0.0f : ( rollRange > 45.0f ? 45.0f : rollRange );
    float frameTime = time - m_flLastSpinUpdateTime;
    if (frameTime < 0.0f || frameTime > 0.25f)
        frameTime = 0.016f;
    m_flLastSpinUpdateTime = time;

    if (randomTargets)
    {
        float baseYaw = (rand() % 2) ? originalYaw : NormalizeQsiAngle(originalYaw + 180.0f);
        angles.y = NormalizeQsiAngle(baseYaw + (rand() % 1201 - 600) / 10.0f);
        angles.x = (rand() % 2) ? 89.0f : -89.0f;
        angles.x += (rand() % 401 - 200) / 10.0f;
        if (angles.x > 89) angles.x = 89;
        if (angles.x < -89) angles.x = -89;
        angles.z = (rand() % 2) ? 45.0f : -45.0f;
        angles.z += (rand() % 301 - 150) / 10.0f;
        if (angles.z > 45) angles.z = 45;
        if (angles.z < -45) angles.z = -45;
    }
    else
    {
        const float spinYaw = time * spinSpeed;
        const float yawWave = sinf( time * safeFactor * 1.17f ) * clampedYawRange
            + cosf( time * safeFactor * 0.53f ) * ( clampedYawRange * 0.35f );
        float pitchWave = sinf( time * safeFactor * 1.93f ) * clampedPitchRange;
        float rollWave = cosf( time * safeFactor * 1.31f ) * clampedRollRange;

        angles.y = NormalizeQsiAngle(originalYaw + 180.0f + spinYaw + yawWave);
        angles.x = pitchWave;
        angles.z = rollWave;
    }

    FixQsiMovementForYaw(cmd, originalYaw, angles.y);
    cmd->viewangles = angles;
}

void QSI_Client::LookDown(usercmd_t* cmd)
{
    if (!cmd) return;

    if (!qsi_cvars[CVAR_LOOKDOWN] || qsi_cvars[CVAR_LOOKDOWN]->value < 0.5f)
        return;
    
    Vector vangles;
    GetViewAngles(vangles);
    
    cmd->viewangles.x = 89.0f;
    cmd->viewangles.y = vangles.y;
    cmd->viewangles.z = 0.0f;
}

void QSI_Client::BunnyHop(usercmd_t* cmd)
{
    if (!cmd) return;
    if (!(cmd->buttons & IN_JUMP))
        return;

    cl_entity_t* local = GetLocalPlayer();
    if (!local)
        return;

    const bool onGround = (g_iPlayerFlags & FL_ONGROUND) != 0;
    const bool bhopAssist = !qsi_cvars[CVAR_BHOP_ASSIST] || qsi_cvars[CVAR_BHOP_ASSIST]->value >= 0.5f;
    float groundDist = qsi_cvars[CVAR_BHOP_GROUND_DIST] ? qsi_cvars[CVAR_BHOP_GROUND_DIST]->value : 24.0f;
    if( groundDist < 4.0f ) groundDist = 4.0f;
    if( groundDist > 64.0f ) groundDist = 64.0f;

    if( onGround )
        return;

    if( bhopAssist && QSI_IsNearGround( local, groundDist ))
        return;

    if( !bhopAssist )
        cmd->buttons &= ~IN_JUMP;
    else
        cmd->buttons &= ~IN_JUMP;
}

void QSI_Client::GroundStrafe(usercmd_t* cmd)
{
	static bool wasOnGround = false;
	static bool releaseDuckNextFrame = false;
	static bool wasEnabled = false;
	static bool armedForLanding = false;

	if (!cmd) return;
    cl_entity_t* local = GetLocalPlayer();
    bool onGround = (g_iPlayerFlags & FL_ONGROUND) != 0;

    if (!local)
    {
        wasOnGround = onGround;
        releaseDuckNextFrame = false;
        wasEnabled = false;
        armedForLanding = false;
        return;
    }

    if (!m_iGs)
    {
        wasOnGround = onGround;
        releaseDuckNextFrame = false;
        wasEnabled = false;
        armedForLanding = false;
        return;
    }

    bool activatedThisFrame = !wasEnabled;

    if (!wasEnabled)
    {
        // Ignore mid-air activation; only arm immediately if +gs was pressed on ground.
        armedForLanding = onGround;
    }

    if (releaseDuckNextFrame)
    {
        cmd->buttons &= ~IN_DUCK;
        releaseDuckNextFrame = false;
    }

    if (activatedThisFrame && onGround)
    {
        cmd->buttons |= IN_DUCK;
        releaseDuckNextFrame = true;
        armedForLanding = true;
    }
    else if (onGround && !wasOnGround)
    {
        if (armedForLanding)
        {
            cmd->buttons |= IN_DUCK;
            releaseDuckNextFrame = true;
        }
        else
        {
            // First landing after enabling in air should not trigger the tap.
            armedForLanding = true;
        }
    }
    else if (onGround)
    {
        armedForLanding = true;
    }
    else if (!releaseDuckNextFrame)
    {
        cmd->buttons &= ~IN_DUCK;
    }

    wasEnabled = true;
    wasOnGround = onGround;
}

void QSI_Client::StrafeBooster(usercmd_t* cmd)
{
    if (!cmd) return;
    cl_entity_t* local = GetLocalPlayer();
    if (!local) return;
    if (!qsi_cvars[CVAR_STRAFEBOOSTER] || qsi_cvars[CVAR_STRAFEBOOSTER]->value < 0.5f) return;

    bool onGround = (g_iPlayerFlags & FL_ONGROUND) != 0;
    bool onLadder = (local->curstate.movetype == MOVETYPE_FLY);
    bool inWater = (g_iWaterLevel >= 2);
    if (onLadder || inWater) return;

    float intensity = qsi_cvars[CVAR_STRAFEBOOSTER]->value;
    if (intensity > 10.0f) intensity = 10.0f;
    int sm;
    float vx, vy, speed2d, curTime;

    if (!onGround)
    {
        vx = local->curstate.velocity[0];
        vy = local->curstate.velocity[1];
        speed2d = sqrtf(vx * vx + vy * vy);
        curTime = GetClientTime();

        cmd->forwardmove = 400;

        if (speed2d > 15.0f)
        {
            sm = (short)(sinf(curTime * 3.0f) * 400.0f);
            cmd->sidemove = sm;

            cmd->viewangles.y -= cosf(curTime * 3.0f) * 2.0f * (intensity / 5.0f);
        }
        else
        {
            cmd->sidemove = 0;
        }
        return;
    }

    if (onGround)
    {
        if (!m_iGs) return;

        vx = local->curstate.velocity[0];
        vy = local->curstate.velocity[1];
        speed2d = sqrtf(vx * vx + vy * vy);

        cmd->forwardmove = 400;

        float yaw = cmd->viewangles.y * (M_PI / 180.0f);
        float vnx = vx / fmaxf(speed2d, 1.0f), vny = vy / fmaxf(speed2d, 1.0f);
        float dotForward = vnx * cosf(yaw) + vny * sinf(yaw);

        if (dotForward < 0.85f)
        {
            sm = (short)(-vny * 400.0f);
            cmd->sidemove = sm;
        }
        else
        {
            static float gSwitchTime = 0;
            static int gDir = 1;
            curTime = GetClientTime();

            if (curTime - gSwitchTime > 0.3f)
            {
                gDir = -gDir;
                gSwitchTime = curTime;
            }

            cmd->sidemove = (short)(gDir * 400);

            cmd->viewangles.y -= gDir * 0.5f * (intensity / 5.0f);
        }
    }
}

void QSI_Client::CreateMove(usercmd_t* cmd)
{
    if (!cmd) return;
    cl_entity_t* local = GetLocalPlayer();
    
    if (!local)
    	return;

    if (!IsPlayerAlive(local))
        return;

    AutoScopeSniper(cmd);

    if (AutoPlay_CreateMove(cmd))
        return;

    ScreenJitter(cmd);
    LookDown(cmd);
    BunnyHop(cmd);
    StrafeBooster(cmd);
    GroundStrafe(cmd);
    ApplyEntityGlow();
    UpdateBacktrack();

    // Nospread: set engine cvars for both CL_SharedRandomFloat bypass and NOSPRAD angle compensation
    if (qsi_cvars[CVAR_NOSPREAD] && qsi_cvars[CVAR_NOSPREAD]->value > 0.5f) {
        Cvar_Set("cl_nospread", "1");
        Cvar_Set("sv_nospread", "1");

        // populate weapon state cvars for engine nospread compensation (nospread.c / NOSPRAD_Apply)
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", GetActiveWeaponId());
        Cvar_Set("nospread_wpn_id", buf);

        {
            static int s_lastShotsFired = 0;
            static double s_lastFireGameTime = 0.0;

            int flags = 0;
            if (g_iPlayerFlags & FL_ONGROUND)
                flags |= (1U << 9);
            if (g_iPlayerFlags & FL_DUCKING)
                flags |= (1U << 14);

            snprintf(buf, sizeof(buf), "%d", flags);
            Cvar_Set("nospread_player_flags", buf);

            float speed2d = g_vPlayerVelocity.Length2D();
            snprintf(buf, sizeof(buf), "%.2f", speed2d);
            Cvar_Set("nospread_player_speed", buf);

            int fov = (m_iRawFOV > 0 && m_iRawFOV <= 179) ? m_iRawFOV : 90;
            snprintf(buf, sizeof(buf), "%d", fov);
            Cvar_Set("nospread_player_fov", buf);

            snprintf(buf, sizeof(buf), "%d", g_iShotsFired);
            Cvar_Set("nospread_shots_fired", buf);

            snprintf(buf, sizeof(buf), "%d", g_iWeaponFlags);
            Cvar_Set("nospread_weapon_state", buf);

            if (g_iShotsFired != s_lastShotsFired)
            {
                s_lastFireGameTime = GetClientTime();
                s_lastShotsFired = g_iShotsFired;
                snprintf(buf, sizeof(buf), "-1.0");
            }
            else
            {
                float dt = GetClientTime() - s_lastFireGameTime;
                if (dt < 0.0f) dt = 0.0f;
                snprintf(buf, sizeof(buf), "%.4f", dt);
            }
            Cvar_Set("nospread_lastfire_time", buf);
        }
    } else {
        Cvar_Set("cl_nospread", "0");
        Cvar_Set("sv_nospread", "0");
    }
    
    if (qsi_cvars[CVAR_SILENTAIM] && qsi_cvars[CVAR_SILENTAIM]->value > 0.5f)
    	SilentAim(cmd);
    else
    	AimAtEnemy(cmd);

    CrazySpin(cmd);
}
