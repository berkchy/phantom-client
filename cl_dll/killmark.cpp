#include "hud.h"
#include "cl_util.h"
#include "draw_util.h"
#include "triangleapi.h"
#include "strtools.h"

#include <string.h>

namespace
{
static float KillMark_ClampFloat( float value, float minValue, float maxValue )
{
	if( value < minValue )
		return minValue;
	if( value > maxValue )
		return maxValue;
	return value;
}

static int KillMark_ClampInt( int value, int minValue, int maxValue )
{
	if( value < minValue )
		return minValue;
	if( value > maxValue )
		return maxValue;
	return value;
}

static float KillMark_EaseOutCubic( float t )
{
	t = KillMark_ClampFloat( t, 0.0f, 1.0f );
	float inv = 1.0f - t;
	return 1.0f - inv * inv * inv;
}

static const char *KillMark_ResolveCandidate( const char *const *paths, int count )
{
	for( int i = 0; i < count; ++i )
	{
		if( paths[i] && paths[i][0] )
			return paths[i];
	}

	return ( count > 0 ) ? paths[0] : "";
}

static int KillMark_RectScaledWidth( const wrect_t *prc, float scale )
{
	if( !prc )
		return 0;

	return max( 1, (int)( ( prc->right - prc->left ) * scale + 0.5f ) );
}

static int KillMark_RectScaledHeight( const wrect_t *prc, float scale )
{
	if( !prc )
		return 0;

	return max( 1, (int)( ( prc->bottom - prc->top ) * scale + 0.5f ) );
}

static void KillMark_DrawHudSprite( HSPRITE sprite, const wrect_t *prc, int x, int y, int alpha, float scale )
{
	if( !sprite || !prc )
		return;

	alpha = KillMark_ClampInt( alpha, 0, 255 );
	SPR_Set( sprite, alpha, alpha, alpha );
	scale = KillMark_ClampFloat( scale, 0.1f, 5.0f );
	if( scale != 1.0f && gEngfuncs.pfnSPR_DrawAdditiveScale )
		gEngfuncs.pfnSPR_DrawAdditiveScale( 0, x, y, prc, scale );
	else
		SPR_DrawAdditive( 0, x, y, prc );
}

static int KillMark_DigitWidth( const wrect_t *prc, float scale )
{
	if( !prc )
		return 0;

	return max( 1, (int)( ( prc->right - prc->left ) * scale + 0.5f ) );
}

static void KillMark_BuildNumberStrip( const wrect_t &stripRect, wrect_t *digits, int digitCount )
{
	if( !digits || digitCount <= 0 )
		return;

	const int totalWidth = stripRect.right - stripRect.left;
	const int digitWidth = digitCount > 0 ? totalWidth / digitCount : 0;
	if( digitWidth <= 0 )
		return;

	for( int i = 0; i < digitCount; ++i )
	{
		digits[i].left = stripRect.left + i * digitWidth;
		digits[i].top = stripRect.top;
		digits[i].right = ( i + 1 == digitCount ) ? stripRect.right : ( digits[i].left + digitWidth );
		digits[i].bottom = stripRect.bottom;
	}
}

static int KillMark_DrawScaledNumberSprite( HSPRITE sprite, const wrect_t *digitRects, int digitCount, int x, int y, int number, int r, int g, int b, int a, float scale )
{
	if( !sprite || !digitRects || digitCount <= 0 )
		return x;

	char buffer[16];
	snprintf( buffer, sizeof( buffer ), "%d", number );

	const int digits = (int)strlen( buffer );
	if( digits <= 0 )
		return x;

	int drawX = x;
	const int fade = KillMark_ClampInt( a, 0, 255 );
	for( int i = 0; i < digits; ++i )
	{
		const int digit = buffer[i] - '0';
		if( digit < 0 || digit >= digitCount )
			continue;

		const wrect_t &rect = digitRects[digit];
		const int digitWidth = KillMark_DigitWidth( &rect, scale );

		SPR_Set( sprite, fade * r / 255, fade * g / 255, fade * b / 255 );
		if( gEngfuncs.pfnSPR_DrawAdditiveScale )
			gEngfuncs.pfnSPR_DrawAdditiveScale( 0, drawX, y, &rect, scale );
		else
			SPR_DrawAdditive( 0, drawX, y, &rect );

		drawX += digitWidth + 1;
	}

	return drawX;
}

static int KillMark_CalcScaledNumberWidth( const wrect_t *digitRects, int digitCount, int number, float scale )
{
	if( !digitRects || digitCount <= 0 )
		return 0;

	char buffer[16];
	snprintf( buffer, sizeof( buffer ), "%d", number );

	const int digits = (int)strlen( buffer );
	if( digits <= 0 )
		return 0;

	int totalWidth = 0;
	for( int i = 0; i < digits; ++i )
	{
		const int digit = buffer[i] - '0';
		if( digit < 0 || digit >= digitCount )
			continue;

		const wrect_t &rect = digitRects[digit];
		totalWidth += KillMark_DigitWidth( &rect, scale );
		if( i + 1 < digits )
			totalWidth += 1;
	}

	return totalWidth;
}

static HSPRITE KillMark_LoadSpriteNearest( const char *path )
{
	if( gRenderAPI.SPR_LoadExt )
		return gRenderAPI.SPR_LoadExt( path, TF_NEAREST | TF_NOMIPMAP );

	return SPR_Load( path );
}

static int KillMark_RectWidth( const wrect_t *prc )
{
	return prc ? ( prc->right - prc->left ) : 0;
}

static int KillMark_RectHeight( const wrect_t *prc )
{
	return prc ? ( prc->bottom - prc->top ) : 0;
}

static void KillMark_LogRect( const char *name, const wrect_t &rect )
{
	gEngfuncs.Con_Printf( "KillMark: %s = %d %d %d %d\n", name, rect.left, rect.top, rect.right, rect.bottom );
}

static void KillMark_BuildHudNumberRect( wrect_t *prc, int w, int h, int xOffset, int yOffset )
{
	if( !prc )
		return;

	int x = 0;
	int y = 0;

	for( int i = 0; i < 10; ++i )
	{
		prc[i].left = x;
		prc[i].top = 0;
		prc[i].right = prc[i].left + w + xOffset;
		prc[i].bottom = h + yOffset;

		x += w;
		y += h;
	}
}

static bool KillMark_LoadHudRect( const char *entryName, wrect_t *outRect )
{
	if( !entryName || !entryName[0] || !outRect )
		return false;

	const int index = gHUD.GetSpriteIndex( entryName );
	if( index < 0 )
	{
		gEngfuncs.Con_Printf( "KillMark: sprite %s not found in loaded hud.txt\n", entryName );
		return false;
	}

	*outRect = gHUD.GetSpriteRect( index );
	gEngfuncs.Con_Printf( "KillMark: sprite %s resolved idx=%d rect=%d %d %d %d\n",
		entryName, index, outRect->left, outRect->right, outRect->top, outRect->bottom );
	return true;
}

}

static const char *const kKillCountSoundCandidates[13][2] =
{
	{ "sound/vox/kill1.wav", "kill1.wav" },
	{ "sound/vox/kill2.wav", "kill2.wav" },
	{ "sound/vox/kill3.wav", "kill3.wav" },
	{ "sound/vox/kill4.wav", "kill4.wav" },
	{ "sound/vox/kill5.wav", "kill5.wav" },
	{ "sound/vox/kill6.wav", "kill6.wav" },
	{ "sound/vox/kill7.wav", "kill7.wav" },
	{ "sound/vox/kill8.wav", "kill8.wav" },
	{ "sound/vox/kill9.wav", "kill9.wav" },
	{ "sound/vox/kill10.wav", "kill10.wav" },
	{ "sound/vox/kill11.wav", "kill11.wav" },
	{ "sound/vox/kill12.wav", "kill12.wav" },
	{ "sound/vox/kill13.wav", "kill13.wav" }
};

static const char *const kHeadshotSoundCandidates[] =
{
	"sound/vox/kill_headshot.wav",
	"kill_headshot.wav"
};

static const char *const kKnifeSoundCandidates[] =
{
	"sound/vox/kill_knife.wav",
	"kill_knife.wav"
};

static const char *const kGrenadeSoundCandidates[] =
{
	"sound/vox/kill_grenade.wav",
	"kill_grenade.wav"
};

static const wrect_t kKillTextRect = { 131, 169, 11, 22 };
static const wrect_t kFirstBloodTextRect = { 88, 111, 13, 24 };
static const wrect_t kKillIconHeadRect = { 0, 0, 98, 98 };
static const wrect_t kKillIconKnifeRect = { 97, 195, 0, 98 };
static const wrect_t kKillIconFragRect = { 0, 98, 97, 195 };

int CHudKillMark::Init( void )
{
	gHUD.AddHudElem( this );

	hud_killmark_combo_reset = CVAR_CREATE( "hud_killmark_combo_reset", "3.0", FCVAR_ARCHIVE );
	hud_killmark_x = CVAR_CREATE( "hud_killmark_x", "0", FCVAR_ARCHIVE );
	hud_killmark_y = CVAR_CREATE( "hud_killmark_y", "0", FCVAR_ARCHIVE );
	hud_killmark_num_scale = CVAR_CREATE( "hud_killmark_num_scale", "1.18", FCVAR_ARCHIVE );
	hud_killmark_text_scale = CVAR_CREATE( "hud_killmark_text_scale", "1.0", FCVAR_ARCHIVE );
	hud_killmark_icon_scale = CVAR_CREATE( "hud_killmark_icon_scale", "1.0", FCVAR_ARCHIVE );
	hud_killmark_fadein_time = CVAR_CREATE( "hud_killmark_fadein_time", "0.16", FCVAR_ARCHIVE );
	hud_killmark_hold_time = CVAR_CREATE( "hud_killmark_hold_time", "1.13", FCVAR_ARCHIVE );
	hud_killmark_fadeout_time = CVAR_CREATE( "hud_killmark_fadeout_time", "0.16", FCVAR_ARCHIVE );

	m_iFlags = 0;
	m_hNumSprite = 0;
	m_hTextSprite = 0;
	m_hKillSprite = 0;
	m_rcKillText = kKillTextRect;
	m_rcFirstBloodText = kFirstBloodTextRect;
	m_rcIconHead = kKillIconHeadRect;
	m_rcIconKnife = kKillIconKnifeRect;
	m_rcIconFrag = kKillIconFragRect;
	memset( m_rcNumber, 0, sizeof( m_rcNumber ) );
	ResetState();
	m_iLastLoggedKillCount = -1;
	m_bFirstBloodAvailable = true;

	return 1;
}

void CHudKillMark::Reset( void )
{
	ResetState();
	m_bFirstBloodAvailable = true;
}

void CHudKillMark::InitHUDData( void )
{
	Reset();
}

void CHudKillMark::ResetState( void )
{
	m_iKillCount = 0;
	m_iLastLoggedKillCount = -1;
	m_flComboResetTime = 0.0f;
	m_flDisplayEndTime = 0.0f;
	m_flCreateTime = 0.0f;
	m_bLastHeadshot = false;
	m_bCurrentFirstBlood = false;
	m_szLastWeapon[0] = 0;
	m_iFlags &= ~HUD_DRAW;
}

const char *CHudKillMark::ResolveSoundCandidate( const char *const *paths, int count ) const
{
	return KillMark_ResolveCandidate( paths, count );
}

void CHudKillMark::BuildHudNumberRect( wrect_t *prc, int w, int h, int xOffset, int yOffset )
{
	KillMark_BuildHudNumberRect( prc, w, h, xOffset, yOffset );
}

int CHudKillMark::DrawHudNumber( HSPRITE sprite, const wrect_t *prc, int x, int y, int iNumber, int r, int g, int b, int a, float scale )
{
	if( !sprite || !prc )
		return x;

	scale = KillMark_ClampFloat( scale, 0.5f, 3.0f );
	char buffer[16];
	snprintf( buffer, sizeof( buffer ), "%d", iNumber );

	const int digits = (int)strlen( buffer );
	if( digits <= 0 )
		return x;

	int totalWidth = 0;
	int maxHeight = 0;
	for( int i = 0; i < digits; ++i )
	{
		const int frame = buffer[i] - '0';
		const int frameWidth = max( 1, (int)( ( prc[frame].right - prc[frame].left ) * scale + 0.5f ) );
		totalWidth += frameWidth;
		if( i + 1 < digits )
			totalWidth += 1;

		const int frameHeight = max( 1, (int)( ( prc[frame].bottom - prc[frame].top ) * scale + 0.5f ) );
		if( frameHeight > maxHeight )
			maxHeight = frameHeight;
	}

	int drawX = x;
	for( int i = 0; i < digits; ++i )
	{
		const int frame = buffer[i] - '0';
		const int frameWidth = max( 1, (int)( ( prc[frame].right - prc[frame].left ) * scale + 0.5f ) );
		const int frameHeight = max( 1, (int)( ( prc[frame].bottom - prc[frame].top ) * scale + 0.5f ) );
		const int frameY = y + ( maxHeight - frameHeight ) / 2;
		SPR_Set( sprite, r, g, b );
		if( gEngfuncs.pfnSPR_DrawGeneric )
			gEngfuncs.pfnSPR_DrawGeneric( 0, drawX, frameY, &prc[frame], kRenderTransAdd, kRenderNormal, frameWidth, frameHeight );
		else
			SPR_DrawAdditive( 0, drawX, frameY, &prc[frame] );
		drawX += frameWidth + 1;
	}

	return x + totalWidth;
}

int CHudKillMark::VidInit( void )
{
	m_hKillSprite = KillMark_LoadSpriteNearest( "sprites/kill.spr" );
	m_hNumSprite = KillMark_LoadSpriteNearest( "sprites/scoreboard_text.spr" );

	m_rcKillText = kKillTextRect;
	m_rcFirstBloodText = kFirstBloodTextRect;
	m_rcIconHead = kKillIconHeadRect;
	m_rcIconKnife = kKillIconKnifeRect;
	m_rcIconFrag = kKillIconFragRect;

	wrect_t numberStripRect = { 0, 0, 0, 0 };
	if( !KillMark_LoadHudRect( "SBNum_L", &numberStripRect ) )
		KillMark_LoadHudRect( "SBNum_S", &numberStripRect );
	KillMark_BuildNumberStrip( numberStripRect, m_rcNumber, 10 );

	KillMark_LoadHudRect( "SBText_1st", &m_rcFirstBloodText );
	KillMark_LoadHudRect( "KM_KillText", &m_rcKillText );
	KillMark_LoadHudRect( "KM_Icon_Head", &m_rcIconHead );
	KillMark_LoadHudRect( "KM_Icon_knife", &m_rcIconKnife );
	KillMark_LoadHudRect( "KM_Icon_Frag", &m_rcIconFrag );

	for( int i = 0; i < 13; ++i )
		m_pszKillSounds[i] = ResolveSoundCandidate( kKillCountSoundCandidates[i], 2 );

	m_pszHeadshotSound = ResolveSoundCandidate( kHeadshotSoundCandidates, sizeof( kHeadshotSoundCandidates ) / sizeof( kHeadshotSoundCandidates[0] ) );
	m_pszKnifeSound = ResolveSoundCandidate( kKnifeSoundCandidates, sizeof( kKnifeSoundCandidates ) / sizeof( kKnifeSoundCandidates[0] ) );
	m_pszGrenadeSound = ResolveSoundCandidate( kGrenadeSoundCandidates, sizeof( kGrenadeSoundCandidates ) / sizeof( kGrenadeSoundCandidates[0] ) );

	m_bLastHeadshot = false;
	m_bCurrentFirstBlood = false;
	m_szLastWeapon[0] = 0;

	return 1;
}

void CHudKillMark::Shutdown( void )
{
	m_hNumSprite = 0;
	m_hKillSprite = 0;
}

void CHudKillMark::TriggerKillSound( int count, bool headshot, bool knife, bool grenade )
{
	const char *sound = nullptr;

	if( headshot )
		sound = m_pszHeadshotSound;
	else if( knife )
		sound = m_pszKnifeSound;
	else if( grenade )
		sound = m_pszGrenadeSound;
	else
	{
		count = KillMark_ClampInt( count, 1, 13 );
		sound = m_pszKillSounds[count - 1];
	}

	if( sound && sound[0] )
		PlaySound( sound, 1.0f );
}

void CHudKillMark::NotifyDeath( int killer, int victim, bool killerIsLocal, bool victimIsLocal, bool suicide, bool teamkill, bool nonPlayerKill, bool headshot, const char *weaponName )
{
	(void)killer;
	(void)victim;
	const float now = gHUD.m_flTime;

	if( victimIsLocal || suicide || teamkill || nonPlayerKill )
	{
		ResetState();
		return;
	}

	if( !killerIsLocal )
		return;

	const float comboReset = hud_killmark_combo_reset ? hud_killmark_combo_reset->value : 3.0f;
	const float fadeInTime = KillMark_ClampFloat( hud_killmark_fadein_time ? hud_killmark_fadein_time->value : 0.16f, 0.01f, 5.0f );
	const float holdTime = KillMark_ClampFloat( hud_killmark_hold_time ? hud_killmark_hold_time->value : 1.13f, 0.01f, 30.0f );
	const float fadeOutTime = KillMark_ClampFloat( hud_killmark_fadeout_time ? hud_killmark_fadeout_time->value : 0.16f, 0.01f, 5.0f );
	if( now > m_flComboResetTime )
		m_iKillCount = 0;

	m_iKillCount++;
	if( m_iKillCount < 1 )
		m_iKillCount = 1;

	m_bLastHeadshot = headshot;
	if( weaponName && weaponName[0] )
	{
		strncpy( m_szLastWeapon, weaponName, sizeof( m_szLastWeapon ) );
		m_szLastWeapon[ sizeof( m_szLastWeapon ) - 1 ] = 0;
	}
	else
	{
		m_szLastWeapon[0] = 0;
	}

	m_flCreateTime = now;
	m_flDisplayEndTime = now + fadeInTime + holdTime + fadeOutTime;
	m_flComboResetTime = now + max( comboReset, 0.25f );
	m_iFlags |= HUD_DRAW;

	const bool firstBlood = m_bFirstBloodAvailable && m_iKillCount == 1;
	m_bCurrentFirstBlood = firstBlood;
	m_bFirstBloodAvailable = false;

	const bool knife = weaponName && !stricmp( weaponName, "knife" );
	const bool grenade = weaponName && ( !stricmp( weaponName, "grenade" ) || !stricmp( weaponName, "hegrenade" ) );
	TriggerKillSound( m_iKillCount, headshot, knife, grenade );
}

int CHudKillMark::Draw( float flTime )
{
	if( !m_iKillCount )
		return 1;

	if( m_flComboResetTime > 0.0f && flTime > m_flComboResetTime )
	{
		ResetState();
		return 1;
	}

	if( m_flDisplayEndTime <= flTime )
		return 1;

	const float fadeInTime = KillMark_ClampFloat( hud_killmark_fadein_time ? hud_killmark_fadein_time->value : 0.16f, 0.01f, 5.0f );
	const float holdTime = KillMark_ClampFloat( hud_killmark_hold_time ? hud_killmark_hold_time->value : 1.13f, 0.01f, 30.0f );
	const float fadeOutTime = KillMark_ClampFloat( hud_killmark_fadeout_time ? hud_killmark_fadeout_time->value : 0.16f, 0.01f, 5.0f );
	const float age = flTime - m_flCreateTime;
	const float fadeIn = KillMark_EaseOutCubic( age / fadeInTime );
	const float fadeOutStart = fadeInTime + holdTime;
	float fadeOut = 1.0f;
	if( age > fadeOutStart )
		fadeOut = 1.0f - KillMark_EaseOutCubic( ( age - fadeOutStart ) / fadeOutTime );
	const float fade = fadeIn < fadeOut ? fadeIn : fadeOut;
	const int alpha = (int)( 255.0f * fade + 0.5f );

	const wrect_t *iconRect = NULL;
	if( m_bLastHeadshot )
		iconRect = &m_rcIconHead;
	else if( m_szLastWeapon[0] )
	{
		if( !stricmp( m_szLastWeapon, "knife" ) )
			iconRect = &m_rcIconKnife;
		else if( !stricmp( m_szLastWeapon, "grenade" ) || !stricmp( m_szLastWeapon, "hegrenade" ) )
			iconRect = &m_rcIconFrag;
	}

	const wrect_t *killTextRect = &m_rcKillText;
	const HSPRITE killTextSprite = m_hKillSprite;
	const float killTextScale = KillMark_ClampFloat( hud_killmark_text_scale ? hud_killmark_text_scale->value : 1.0f, 0.25f, 5.0f );
	const int killTextWidth = KillMark_RectScaledWidth( killTextRect, killTextScale );
	const int killTextHeight = KillMark_RectScaledHeight( killTextRect, killTextScale );
	const float numberScale = KillMark_ClampFloat( hud_killmark_num_scale ? hud_killmark_num_scale->value : 1.0f, 0.5f, 3.0f );
	const int countWidth = KillMark_CalcScaledNumberWidth( m_rcNumber, 10, m_iKillCount, numberScale );
	const int countHeight = KillMark_RectScaledHeight( &m_rcNumber[0], numberScale );
	const float iconScale = KillMark_ClampFloat( hud_killmark_icon_scale ? hud_killmark_icon_scale->value : 1.0f, 0.25f, 5.0f );
	const int iconWidth = iconRect ? KillMark_RectScaledWidth( iconRect, iconScale ) : 0;
	const int iconHeight = iconRect ? KillMark_RectScaledHeight( iconRect, iconScale ) : 0;

	const int iconGap = iconRect ? 2 : 0;
	const int countGap = countWidth > 0 ? 3 : 0;
	const int topWidth = killTextWidth + countGap + countWidth;
	const int topHeight = max( killTextHeight, countHeight );
	const int totalHeight = topHeight + iconGap + iconHeight;
	const int centerX = ( ScreenWidth / 2 ) + ( hud_killmark_x ? (int)hud_killmark_x->value : 0 );
	const int baseY = ( ScreenHeight / 2 ) - YRES( 45 ) - (int)( YRES( 12 ) * ( 1.0f - fadeIn ) + 0.5f )
		+ ( hud_killmark_y ? (int)hud_killmark_y->value : 0 );
	const int yTop = baseY - totalHeight / 2;

	int xTop = centerX - topWidth / 2;

	if( countWidth > 0 )
	{
		const int countY = yTop + ( topHeight - countHeight ) / 2;
		KillMark_DrawScaledNumberSprite( m_hNumSprite, m_rcNumber, 10, xTop, countY, m_iKillCount, 255, 255, 255, alpha, numberScale );
	}

	const int killTextX = xTop + countWidth + countGap;
	const int killTextY = yTop + ( topHeight - killTextHeight ) / 2;
	KillMark_DrawHudSprite( killTextSprite, killTextRect, killTextX, killTextY, alpha, killTextScale );

	const int iconX = centerX - iconWidth / 2;
	const int iconY = yTop + topHeight + iconGap;
	if( iconRect )
	{
		const int iconDrawX = centerX - iconWidth / 2;
		const int iconDrawY = yTop + topHeight + iconGap;
		KillMark_DrawHudSprite( m_hKillSprite, iconRect, iconDrawX, iconDrawY, alpha, iconScale );
	}

	return 1;
}
