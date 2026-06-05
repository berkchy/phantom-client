/*
HudFeedback.cpp - in-game HUD feedback settings
*/

#include "Framework.h"
#include "Bitmap.h"
#include "PicButton.h"
#include "Slider.h"
#include "SpinControl.h"
#include "Action.h"
#include "model/StringArrayModel.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ART_BANNER "gfx/shell/head_advoptions"

namespace
{
enum HudFeedbackPage
{
	PAGE_KILLMARK = 0,
	PAGE_DEATHNOTICE,
	PAGE_WEAPON,
	PAGE_CHAT,
	PAGE_COUNT
};

static const char *s_pageNames[PAGE_COUNT] =
{
	"Killmark",
	"Deathnotice",
	"Weapon icon",
	"Chat"
};

static CStringArrayModel s_pageModel( s_pageNames, PAGE_COUNT );

static HFont HudPreviewFont( void )
{
	return uiStatic.hConsoleFont ? uiStatic.hConsoleFont : uiStatic.hDefaultFont;
}

struct HudPreviewSpriteState
{
	bool ready;
	HIMAGE killSheet;
	HIMAGE scoreSheet;
	HIMAGE weaponSheet;
	wrect_t numStrip[10];
	wrect_t killTextRect;
	wrect_t firstBloodRect;
	wrect_t iconHeadRect;
	wrect_t iconKnifeRect;
	wrect_t iconFragRect;
	wrect_t weaponRect;
};

static HudPreviewSpriteState s_spriteState;

static bool HudPreview_LoadHudRect( const char *entryName, wrect_t *outRect )
{
	if( !entryName || !entryName[0] || !outRect )
		return false;

	int fileLen = 0;
	byte *file = EngFuncs::COM_LoadFile( "sprites/hud.txt", &fileLen );
	if( !file )
		return false;

	bool found = false;
	char *data = (char *)file;
	char token[128];
	char value[128];

	while( ( data = EngFuncs::COM_ParseFile( data, token, sizeof( token ))) != NULL )
	{
		if( !token[0] )
			continue;

		if( !strcmp( token, entryName ))
		{
			int x = 0, y = 0, w = 0, h = 0;

			data = EngFuncs::COM_ParseFile( data, value, sizeof( value )); // res
			data = EngFuncs::COM_ParseFile( data, value, sizeof( value )); // sheet
			data = EngFuncs::COM_ParseFile( data, value, sizeof( value ) ); x = atoi( value );
			data = EngFuncs::COM_ParseFile( data, value, sizeof( value ) ); y = atoi( value );
			data = EngFuncs::COM_ParseFile( data, value, sizeof( value ) ); w = atoi( value );
			data = EngFuncs::COM_ParseFile( data, value, sizeof( value ) ); h = atoi( value );

			outRect->left = x;
			outRect->top = y;
			outRect->right = x + w;
			outRect->bottom = y + h;
			found = true;
			break;
		}
	}

	EngFuncs::COM_FreeFile( file );
	return found;
}

static void HudPreview_BuildNumberStrip( const wrect_t &stripRect, wrect_t *digits, int digitCount )
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

static int HudPreview_RectWidth( const wrect_t *rect )
{
	return rect ? rect->right - rect->left : 0;
}

static int HudPreview_RectHeight( const wrect_t *rect )
{
	return rect ? rect->bottom - rect->top : 0;
}

static int HudPreview_DigitWidth( const wrect_t *rect, float scale )
{
	return rect ? Q_max( 1, (int)(( HudPreview_RectWidth( rect ) * scale ) + 0.5f )) : 0;
}

static int HudPreview_CalcNumberWidth( const wrect_t *digits, int digitCount, int number, float scale )
{
	if( !digits || digitCount <= 0 )
		return 0;

	char buffer[16];
	snprintf( buffer, sizeof( buffer ), "%d", number );

	int totalWidth = 0;
	const int len = (int)strlen( buffer );
	for( int i = 0; i < len; ++i )
	{
		const int digit = buffer[i] - '0';
		if( digit < 0 || digit >= digitCount )
			continue;

		totalWidth += HudPreview_DigitWidth( &digits[digit], scale );
		if( i + 1 < len )
			totalWidth += 1;
	}

	return totalWidth;
}

static int HudPreview_DrawNumber( HIMAGE sprite, const wrect_t *digits, int digitCount, int x, int y, int number, int alpha, float scale )
{
	if( !sprite || !digits || digitCount <= 0 )
		return x;

	char buffer[16];
	snprintf( buffer, sizeof( buffer ), "%d", number );

	int drawX = x;
	const int len = (int)strlen( buffer );
	for( int i = 0; i < len; ++i )
	{
		const int digit = buffer[i] - '0';
		if( digit < 0 || digit >= digitCount )
			continue;

		const wrect_t &rect = digits[digit];
		EngFuncs::SPR_Set( sprite, alpha, alpha, alpha, alpha );
		EngFuncs::SPR_DrawAdditiveScale( 0, drawX, y, &rect, scale );
		drawX += HudPreview_DigitWidth( &rect, scale ) + 1;
	}

	return drawX;
}

static void HudPreview_SetRect( wrect_t *dst, int left, int top, int right, int bottom )
{
	if( !dst )
		return;

	dst->left = left;
	dst->top = top;
	dst->right = right;
	dst->bottom = bottom;
}

static void HudPreview_InitSprites( void )
{
	if( s_spriteState.ready )
		return;

	memset( &s_spriteState, 0, sizeof( s_spriteState ));
	s_spriteState.killSheet = EngFuncs::SPR_Load( "sprites/kill.spr" );
	s_spriteState.scoreSheet = EngFuncs::SPR_Load( "sprites/scoreboard_text.spr" );
	s_spriteState.weaponSheet = EngFuncs::SPR_Load( "sprites/640hud1.spr" );

	wrect_t numberStripRect = { 0, 0, 0, 0 };
	if( !HudPreview_LoadHudRect( "SBNum_L", &numberStripRect ))
		HudPreview_LoadHudRect( "SBNum_S", &numberStripRect );
	HudPreview_BuildNumberStrip( numberStripRect, s_spriteState.numStrip, 10 );

	if( !HudPreview_LoadHudRect( "KM_KillText", &s_spriteState.killTextRect ))
		HudPreview_SetRect( &s_spriteState.killTextRect, 97, 97, 255, 142 );

	if( !HudPreview_LoadHudRect( "SBText_1st", &s_spriteState.firstBloodRect ))
		HudPreview_SetRect( &s_spriteState.firstBloodRect, 88, 13, 111, 24 );

	if( !HudPreview_LoadHudRect( "KM_Icon_Head", &s_spriteState.iconHeadRect ))
		HudPreview_SetRect( &s_spriteState.iconHeadRect, 0, 0, 98, 98 );

	if( !HudPreview_LoadHudRect( "KM_Icon_knife", &s_spriteState.iconKnifeRect ))
		HudPreview_SetRect( &s_spriteState.iconKnifeRect, 97, 0, 195, 98 );

	if( !HudPreview_LoadHudRect( "KM_Icon_Frag", &s_spriteState.iconFragRect ))
		HudPreview_SetRect( &s_spriteState.iconFragRect, 0, 97, 98, 195 );

	if( !HudPreview_LoadHudRect( "d_ak47", &s_spriteState.weaponRect ))
		HudPreview_SetRect( &s_spriteState.weaponRect, 192, 80, 240, 96 );

	s_spriteState.ready = true;
}

template <size_t N>
void SetVisible( CMenuBaseItem * const (&items)[N], bool visible )
{
	for( size_t i = 0; i < N; ++i )
	{
		if( !items[i] )
			continue;
		if( visible ) items[i]->Show();
		else items[i]->Hide();
	}
}
}

class CMenuHudFeedback;

class CMenuHudPreview : public CMenuBaseItem
{
public:
	void Draw() override;
};

class CMenuHudFeedback : public CMenuFramework
{
public:
	CMenuHudFeedback() : CMenuFramework( "CMenuHudFeedback" ) {}

	bool KeyDown( int key ) override;
	void Show() override;
	void SaveAndPopMenu() override;

	int CurrentPage() { return (int)m_page.GetCurrentValue(); }
	const char *CurrentPageName()
	{
		const char *name = m_page.GetCurrentString();
		return name && name[0] ? name : "HUD";
	}

	float KillmarkComboReset() { return m_killComboReset.GetCurrentValue(); }
	float KillmarkX() { return m_killX.GetCurrentValue(); }
	float KillmarkY() { return m_killY.GetCurrentValue(); }
	float KillmarkNumScale() { return m_killNumScale.GetCurrentValue(); }
	float KillmarkTextScale() { return m_killTextScale.GetCurrentValue(); }
	float KillmarkIconScale() { return m_killIconScale.GetCurrentValue(); }
	float KillmarkFadeIn() { return m_killFadeIn.GetCurrentValue(); }
	float KillmarkHold() { return m_killHold.GetCurrentValue(); }
	float KillmarkFadeOut() { return m_killFadeOut.GetCurrentValue(); }

	float DeathNoticeTime() { return m_dnTime.GetCurrentValue(); }
	float DeathNoticeX() { return m_dnX.GetCurrentValue(); }
	float DeathNoticeY() { return m_dnY.GetCurrentValue(); }
	float DeathNoticeBgAlpha() { return m_dnBgAlpha.GetCurrentValue(); }
	float DeathNoticeBgSoftness() { return m_dnBgSoftness.GetCurrentValue(); }
	float DeathNoticeAnimTime() { return m_dnAnimTime.GetCurrentValue(); }
	float DeathNoticeRowGap() { return m_dnRowGap.GetCurrentValue(); }

	float WeaponIconX() { return m_weaponX.GetCurrentValue(); }
	float WeaponIconY() { return m_weaponY.GetCurrentValue(); }
	float WeaponIconScale() { return m_weaponScale.GetCurrentValue(); }

	float ChatTime() { return m_chatTime.GetCurrentValue(); }
	float ChatAnimTime() { return m_chatAnimTime.GetCurrentValue(); }
	float ChatX() { return m_chatX.GetCurrentValue(); }
	float ChatY() { return m_chatY.GetCurrentValue(); }

private:
	void _Init() override;
	void UpdatePageVisibility();
	void PageChanged();
	void CancelCb();
	void Restore();

	CMenuSpinControl m_page;
	CMenuHudPreview   m_preview;
	CMenuPicButton    m_done;
	CMenuPicButton    m_cancel;

	CMenuSlider m_killComboReset;
	CMenuSlider m_killX;
	CMenuSlider m_killY;
	CMenuSlider m_killNumScale;
	CMenuSlider m_killTextScale;
	CMenuSlider m_killIconScale;
	CMenuSlider m_killFadeIn;
	CMenuSlider m_killHold;
	CMenuSlider m_killFadeOut;

	CMenuSlider m_dnTime;
	CMenuSlider m_dnX;
	CMenuSlider m_dnY;
	CMenuSlider m_dnBgAlpha;
	CMenuSlider m_dnBgSoftness;
	CMenuSlider m_dnAnimTime;
	CMenuSlider m_dnRowGap;

	CMenuSlider m_weaponX;
	CMenuSlider m_weaponY;
	CMenuSlider m_weaponScale;

	CMenuSlider m_chatTime;
	CMenuSlider m_chatAnimTime;
	CMenuSlider m_chatX;
	CMenuSlider m_chatY;
};

bool CMenuHudFeedback::KeyDown( int key )
{
	if( UI::Key::IsEscape( key ) )
	{
		Restore();
		Hide();
		return true;
	}
	return CMenuFramework::KeyDown( key );
}

void CMenuHudFeedback::Restore()
{
	m_killComboReset.DiscardChanges();
	m_killX.DiscardChanges();
	m_killY.DiscardChanges();
	m_killNumScale.DiscardChanges();
	m_killTextScale.DiscardChanges();
	m_killIconScale.DiscardChanges();
	m_killFadeIn.DiscardChanges();
	m_killHold.DiscardChanges();
	m_killFadeOut.DiscardChanges();

	m_dnTime.DiscardChanges();
	m_dnX.DiscardChanges();
	m_dnY.DiscardChanges();
	m_dnBgAlpha.DiscardChanges();
	m_dnBgSoftness.DiscardChanges();
	m_dnAnimTime.DiscardChanges();
	m_dnRowGap.DiscardChanges();

	m_weaponX.DiscardChanges();
	m_weaponY.DiscardChanges();
	m_weaponScale.DiscardChanges();

	m_chatTime.DiscardChanges();
	m_chatAnimTime.DiscardChanges();
	m_chatX.DiscardChanges();
	m_chatY.DiscardChanges();
}

void CMenuHudFeedback::SaveAndPopMenu()
{
	m_killComboReset.WriteCvar();
	m_killX.WriteCvar();
	m_killY.WriteCvar();
	m_killNumScale.WriteCvar();
	m_killTextScale.WriteCvar();
	m_killIconScale.WriteCvar();
	m_killFadeIn.WriteCvar();
	m_killHold.WriteCvar();
	m_killFadeOut.WriteCvar();

	m_dnTime.WriteCvar();
	m_dnX.WriteCvar();
	m_dnY.WriteCvar();
	m_dnBgAlpha.WriteCvar();
	m_dnBgSoftness.WriteCvar();
	m_dnAnimTime.WriteCvar();
	m_dnRowGap.WriteCvar();

	m_weaponX.WriteCvar();
	m_weaponY.WriteCvar();
	m_weaponScale.WriteCvar();

	m_chatTime.WriteCvar();
	m_chatAnimTime.WriteCvar();
	m_chatX.WriteCvar();
	m_chatY.WriteCvar();

	CMenuFramework::SaveAndPopMenu();
}

void CMenuHudFeedback::PageChanged()
{
	UpdatePageVisibility();
}

void CMenuHudFeedback::CancelCb()
{
	Restore();
	Hide();
}

void CMenuHudFeedback::Show()
{
	CMenuFramework::Show();
	m_page.SetCurrentValue( (float)PAGE_KILLMARK );
	UpdatePageVisibility();
}

void CMenuHudFeedback::_Init()
{
	banner.SetPicture( ART_BANNER );

	m_page.Setup( &s_pageModel );
	m_page.SetNameAndStatus( L( "Section" ), L( "Select HUD feedback section." ) );
	m_page.SetRect( 72, 250, 240, 32 );
	m_page.SetCurrentValue( (float)PAGE_KILLMARK );
	m_page.onChanged = VoidCb( &CMenuHudFeedback::PageChanged );

	m_preview.iFlags = QMF_INACTIVE | QMF_DROPSHADOW;
	m_preview.SetCharSize( QM_SMALLFONT );
	m_preview.SetRect( 500, 250, 390, 430 );

	m_done.SetNameAndStatus( L( "GameUI_OK" ), nullptr );
	m_done.SetPicture( PC_DONE );
	m_done.iFlags |= QMF_NOTIFY;
	m_done.SetCoord( 72, 690 );
	m_done.onReleased = VoidCb( &CMenuHudFeedback::SaveAndPopMenu );

	m_cancel.SetNameAndStatus( L( "GameUI_Cancel" ), nullptr );
	m_cancel.SetPicture( PC_CANCEL );
	m_cancel.iFlags |= QMF_NOTIFY;
	m_cancel.SetCoord( 72, 740 );
	m_cancel.onReleased = VoidCb( &CMenuHudFeedback::CancelCb );

	// Killmark
	m_killComboReset.SetNameAndStatus( L( "Combo reset" ), L( "Seconds before the kill chain resets after no new kill." ) );
	m_killComboReset.SetRect( 72, 300, 260, 32 );
	m_killComboReset.Setup( 0.25f, 10.0f, 0.05f );
	m_killComboReset.LinkCvar( "hud_killmark_combo_reset" );

	m_killX.SetNameAndStatus( L( "Killmark X" ), L( "Horizontal offset for killmark block." ) );
	m_killX.SetRect( 72, 342, 260, 32 );
	m_killX.Setup( -200.0f, 200.0f, 1.0f );
	m_killX.LinkCvar( "hud_killmark_x" );

	m_killY.SetNameAndStatus( L( "Killmark Y" ), L( "Vertical offset for killmark block." ) );
	m_killY.SetRect( 72, 384, 260, 32 );
	m_killY.Setup( -200.0f, 200.0f, 1.0f );
	m_killY.LinkCvar( "hud_killmark_y" );

	m_killNumScale.SetNameAndStatus( L( "Count scale" ), L( "Scale for kill count digits." ) );
	m_killNumScale.SetRect( 72, 426, 260, 32 );
	m_killNumScale.Setup( 0.5f, 3.0f, 0.01f );
	m_killNumScale.LinkCvar( "hud_killmark_num_scale" );

	m_killTextScale.SetNameAndStatus( L( "Kill text scale" ), L( "Scale for the Kill text." ) );
	m_killTextScale.SetRect( 72, 468, 260, 32 );
	m_killTextScale.Setup( 0.5f, 3.0f, 0.01f );
	m_killTextScale.LinkCvar( "hud_killmark_text_scale" );

	m_killIconScale.SetNameAndStatus( L( "Icon scale" ), L( "Scale for headshot/knife/grenade icons." ) );
	m_killIconScale.SetRect( 72, 510, 260, 32 );
	m_killIconScale.Setup( 0.5f, 3.0f, 0.01f );
	m_killIconScale.LinkCvar( "hud_killmark_icon_scale" );

	m_killFadeIn.SetNameAndStatus( L( "Fade in" ), L( "Killmark fade-in time." ) );
	m_killFadeIn.SetRect( 72, 552, 260, 32 );
	m_killFadeIn.Setup( 0.01f, 5.0f, 0.01f );
	m_killFadeIn.LinkCvar( "hud_killmark_fadein_time" );

	m_killHold.SetNameAndStatus( L( "Hold" ), L( "Killmark hold time." ) );
	m_killHold.SetRect( 72, 594, 260, 32 );
	m_killHold.Setup( 0.01f, 30.0f, 0.01f );
	m_killHold.LinkCvar( "hud_killmark_hold_time" );

	m_killFadeOut.SetNameAndStatus( L( "Fade out" ), L( "Killmark fade-out time." ) );
	m_killFadeOut.SetRect( 72, 636, 260, 32 );
	m_killFadeOut.Setup( 0.01f, 5.0f, 0.01f );
	m_killFadeOut.LinkCvar( "hud_killmark_fadeout_time" );

	// Death notice
	m_dnTime.SetNameAndStatus( L( "Display time" ), L( "Death notice lifetime." ) );
	m_dnTime.SetRect( 72, 300, 260, 32 );
	m_dnTime.Setup( 1.0f, 15.0f, 0.1f );
	m_dnTime.LinkCvar( "hud_deathnotice_time" );

	m_dnX.SetNameAndStatus( L( "Notice X" ), L( "Right pad for death notice panel." ) );
	m_dnX.SetRect( 72, 342, 260, 32 );
	m_dnX.Setup( 0.0f, 200.0f, 1.0f );
	m_dnX.LinkCvar( "hud_deathnotice_x" );

	m_dnY.SetNameAndStatus( L( "Notice Y" ), L( "Vertical offset for death notice rows." ) );
	m_dnY.SetRect( 72, 384, 260, 32 );
	m_dnY.Setup( -200.0f, 200.0f, 1.0f );
	m_dnY.LinkCvar( "hud_deathnotice_y" );

	m_dnBgAlpha.SetNameAndStatus( L( "Background alpha" ), L( "Death notice background opacity." ) );
	m_dnBgAlpha.SetRect( 72, 426, 260, 32 );
	m_dnBgAlpha.Setup( 0.0f, 255.0f, 1.0f );
	m_dnBgAlpha.LinkCvar( "hud_deathnotice_bg_alpha" );

	m_dnBgSoftness.SetNameAndStatus( L( "Background softness" ), L( "Soft edge strength for death notice panel." ) );
	m_dnBgSoftness.SetRect( 72, 468, 260, 32 );
	m_dnBgSoftness.Setup( 0.0f, 100.0f, 1.0f );
	m_dnBgSoftness.LinkCvar( "hud_deathnotice_bg_softness" );

	m_dnAnimTime.SetNameAndStatus( L( "Animation time" ), L( "Death notice slide animation time." ) );
	m_dnAnimTime.SetRect( 72, 510, 260, 32 );
	m_dnAnimTime.Setup( 0.01f, 1.0f, 0.01f );
	m_dnAnimTime.LinkCvar( "hud_deathnotice_anim_time" );

	m_dnRowGap.SetNameAndStatus( L( "Row gap" ), L( "Vertical gap between death notice rows." ) );
	m_dnRowGap.SetRect( 72, 552, 260, 32 );
	m_dnRowGap.Setup( 12.0f, 48.0f, 1.0f );
	m_dnRowGap.LinkCvar( "hud_deathnotice_row_gap" );

	// Weapon icon
	m_weaponX.SetNameAndStatus( L( "Weapon icon X" ), L( "Horizontal offset for the active weapon sprite." ) );
	m_weaponX.SetRect( 72, 300, 260, 32 );
	m_weaponX.Setup( -200.0f, 200.0f, 1.0f );
	m_weaponX.LinkCvar( "hud_weaponicon_x" );

	m_weaponY.SetNameAndStatus( L( "Weapon icon Y" ), L( "Vertical offset for the active weapon sprite." ) );
	m_weaponY.SetRect( 72, 342, 260, 32 );
	m_weaponY.Setup( -200.0f, 200.0f, 1.0f );
	m_weaponY.LinkCvar( "hud_weaponicon_y" );

	m_weaponScale.SetNameAndStatus( L( "Weapon icon scale" ), L( "Scale of the active weapon sprite." ) );
	m_weaponScale.SetRect( 72, 384, 260, 32 );
	m_weaponScale.Setup( 0.25f, 3.0f, 0.01f );
	m_weaponScale.LinkCvar( "hud_weaponicon_scale" );

	// Chat
	m_chatTime.SetNameAndStatus( L( "Chat time" ), L( "Time to keep chat messages visible." ) );
	m_chatTime.SetRect( 72, 300, 260, 32 );
	m_chatTime.Setup( 1.0f, 15.0f, 0.1f );
	m_chatTime.LinkCvar( "hud_saytext_time" );

	m_chatAnimTime.SetNameAndStatus( L( "Chat animation" ), L( "Chat fade/slide animation time." ) );
	m_chatAnimTime.SetRect( 72, 342, 260, 32 );
	m_chatAnimTime.Setup( 0.01f, 1.0f, 0.01f );
	m_chatAnimTime.LinkCvar( "hud_saytext_anim_time" );

	m_chatX.SetNameAndStatus( L( "Chat X offset" ), L( "Horizontal chat offset." ) );
	m_chatX.SetRect( 72, 384, 260, 32 );
	m_chatX.Setup( -100.0f, 100.0f, 1.0f );
	m_chatX.LinkCvar( "hud_saytext_x_offset" );

	m_chatY.SetNameAndStatus( L( "Chat Y offset" ), L( "Vertical chat offset." ) );
	m_chatY.SetRect( 72, 426, 260, 32 );
	m_chatY.Setup( -100.0f, 100.0f, 1.0f );
	m_chatY.LinkCvar( "hud_saytext_y_offset" );

	AddItem( banner );
	AddItem( m_page );
	AddItem( m_preview );
	AddItem( m_done );
	AddItem( m_cancel );

	AddItem( m_killComboReset );
	AddItem( m_killX );
	AddItem( m_killY );
	AddItem( m_killNumScale );
	AddItem( m_killTextScale );
	AddItem( m_killIconScale );
	AddItem( m_killFadeIn );
	AddItem( m_killHold );
	AddItem( m_killFadeOut );

	AddItem( m_dnTime );
	AddItem( m_dnX );
	AddItem( m_dnY );
	AddItem( m_dnBgAlpha );
	AddItem( m_dnBgSoftness );
	AddItem( m_dnAnimTime );
	AddItem( m_dnRowGap );

	AddItem( m_weaponX );
	AddItem( m_weaponY );
	AddItem( m_weaponScale );

	AddItem( m_chatTime );
	AddItem( m_chatAnimTime );
	AddItem( m_chatX );
	AddItem( m_chatY );

	UpdatePageVisibility();
}

void CMenuHudFeedback::UpdatePageVisibility()
{
	CMenuBaseItem *killItems[] =
	{
		&m_killComboReset, &m_killX, &m_killY, &m_killNumScale,
		&m_killTextScale, &m_killIconScale, &m_killFadeIn,
		&m_killHold, &m_killFadeOut
	};

	CMenuBaseItem *deathItems[] =
	{
		&m_dnTime, &m_dnX, &m_dnY, &m_dnBgAlpha,
		&m_dnBgSoftness, &m_dnAnimTime, &m_dnRowGap
	};

	CMenuBaseItem *weaponItems[] = { &m_weaponX, &m_weaponY, &m_weaponScale };
	CMenuBaseItem *chatItems[] = { &m_chatTime, &m_chatAnimTime, &m_chatX, &m_chatY };

	SetVisible( killItems, false );
	SetVisible( deathItems, false );
	SetVisible( weaponItems, false );
	SetVisible( chatItems, false );

	switch( CurrentPage() )
	{
	case PAGE_DEATHNOTICE:
		SetVisible( deathItems, true );
		break;
	case PAGE_WEAPON:
		SetVisible( weaponItems, true );
		break;
	case PAGE_CHAT:
		SetVisible( chatItems, true );
		break;
	case PAGE_KILLMARK:
	default:
		SetVisible( killItems, true );
		break;
	}
}

void CMenuHudPreview::Draw()
{
	auto *menu = static_cast<CMenuHudFeedback *>( Parent() );
	if( !menu )
		return;

	UI_FillRect( m_scPos, m_scSize, PackRGBA( 18, 18, 18, 232 ) );
	UI_DrawRectangle( m_scPos, m_scSize, uiInputFgColor );

	char caption[128];
	snprintf( caption, sizeof( caption ), "%s preview", menu->CurrentPageName() );
	UI_DrawString( font, m_scPos.x + 16, m_scPos.y + 14, m_scSize.w - 32, m_scChSize * 2,
		caption, uiColorHelp, m_scChSize, QM_LEFT, ETF_SHADOW | ETF_FORCECOL );

	const int panelX = m_scPos.x + 16;
	const int panelY = m_scPos.y + 54;
	const int panelW = m_scSize.w - 32;

	switch( menu->CurrentPage() )
	{
	case PAGE_DEATHNOTICE:
	{
		const HFont hudFont = HudPreviewFont();
		const int alpha = (int)Q_max( 0.0f, Q_min( 255.0f, menu->DeathNoticeBgAlpha() ));
		const int soft = (int)Q_max( 0.0f, Q_min( 100.0f, menu->DeathNoticeBgSoftness() ));
		const int rowGap = (int)Q_max( 12.0f, Q_min( 48.0f, menu->DeathNoticeRowGap() ));
		const int rowAlpha = alpha > 0 ? alpha : 1;
		const int outerAlpha = rowAlpha * soft * 40 / 10000;
		const int middleAlpha = rowAlpha * soft * 70 / 10000;
		const int innerAlpha = rowAlpha;
		int rowY = panelY + 10;
		const char *rows[] =
		{
			"Fo0zer killed Ethan",
			"[NFE] Fo0zer killed Albert",
			"Ulysses killed [NFE] Fo0zer"
		};

		for( int i = 0; i < 3; ++i )
		{
			int x = panelX + (int)menu->DeathNoticeX();
			int w = panelW - 24;
			int h = 16;

			EngFuncs::FillRGBA( x, rowY, w, h, 0, 0, 0, outerAlpha );
			if( w > 2 && h > 2 )
				EngFuncs::FillRGBA( x + 1, rowY + 1, w - 2, h - 2, 0, 0, 0, middleAlpha );
			if( w > 4 && h > 4 )
				EngFuncs::FillRGBA( x + 2, rowY + 2, w - 4, h - 4, 0, 0, 0, innerAlpha );

			UI_DrawString( hudFont, x + 7, rowY + 1, w - 14, h, rows[i], uiColorWhite, m_scChSize - 2, QM_LEFT, ETF_SHADOW | ETF_FORCECOL | ETF_NOSIZELIMIT );
			rowY += rowGap;
		}

		char info[160];
		snprintf( info, sizeof( info ), "time %.1f  anim %.2f  alpha %.0f  softness %.0f  gap %.0f",
			menu->DeathNoticeTime(), menu->DeathNoticeAnimTime(),
			menu->DeathNoticeBgAlpha(), menu->DeathNoticeBgSoftness(), menu->DeathNoticeRowGap() );
		UI_DrawString( font, panelX, m_scPos.y + m_scSize.h - 32, panelW, 24, info, uiColorHelp, m_scChSize, QM_LEFT, ETF_SHADOW | ETF_FORCECOL );
		break;
	}
	case PAGE_WEAPON:
	{
		HudPreview_InitSprites();

		const float scale = menu->WeaponIconScale();
		const int alpha = 255;
		const int iconW = Q_max( 1, (int)( HudPreview_RectWidth( &s_spriteState.weaponRect ) * scale + 0.5f ) );
		const int iconH = Q_max( 1, (int)( HudPreview_RectHeight( &s_spriteState.weaponRect ) * scale + 0.5f ) );
		const int iconX = panelX + panelW / 2 - iconW / 2 + (int)menu->WeaponIconX();
		const int iconY = panelY + 92 + (int)menu->WeaponIconY();

		EngFuncs::FillRGBA( panelX + 24, panelY + 56, panelW - 48, 128, 0, 0, 0, 160 );
		UI_DrawRectangle( panelX + 24, panelY + 56, panelW - 48, 128, uiInputFgColor );

		if( s_spriteState.weaponSheet )
		{
			EngFuncs::SPR_Set( s_spriteState.weaponSheet, 255, 255, 255, alpha );
			EngFuncs::SPR_DrawAdditiveScale( 0, iconX, iconY, &s_spriteState.weaponRect, scale );
		}
		else
		{
			UI_DrawString( font, panelX, panelY + 104, panelW, 20, "d_ak47", uiColorWhite, m_scChSize, QM_CENTER, ETF_SHADOW | ETF_FORCECOL );
		}

		UI_DrawString( font, panelX, panelY + 186, panelW, 20, "weapon icon preview", uiColorHelp, m_scChSize, QM_CENTER, ETF_SHADOW | ETF_FORCECOL );

		char info[128];
		snprintf( info, sizeof( info ), "weapon icon x %.0f  y %.0f  scale %.2f",
			menu->WeaponIconX(), menu->WeaponIconY(), menu->WeaponIconScale() );
		UI_DrawString( font, panelX, m_scPos.y + m_scSize.h - 32, panelW, 24, info, uiColorHelp, m_scChSize, QM_LEFT, ETF_SHADOW | ETF_FORCECOL );
		break;
	}
	case PAGE_CHAT:
	{
		const HFont hudFont = HudPreviewFont();
		int chatX = panelX + 8 + (int)menu->ChatX();
		int chatY = panelY + 12 + (int)menu->ChatY();
		const char *msgs[] =
		{
			"Player: hello",
			"Player: one more line",
			"Player: fade and slide preview"
		};

		for( int i = 0; i < 3; ++i )
		{
			int y = chatY + i * 22;
			EngFuncs::FillRGBA( chatX, y, panelW - 32, 16, 0, 0, 0, 145 );
			UI_DrawString( hudFont, chatX + 7, y + 1, panelW - 46, 16, msgs[i], uiColorWhite, m_scChSize - 2, QM_LEFT, ETF_SHADOW | ETF_FORCECOL | ETF_NOSIZELIMIT );
		}

		char info[128];
		snprintf( info, sizeof( info ), "time %.1f  anim %.2f  x %.0f  y %.0f",
			menu->ChatTime(), menu->ChatAnimTime(), menu->ChatX(), menu->ChatY() );
		UI_DrawString( font, panelX, m_scPos.y + m_scSize.h - 32, panelW, 24, info, uiColorHelp, m_scChSize, QM_LEFT, ETF_SHADOW | ETF_FORCECOL );
		break;
	}
	case PAGE_KILLMARK:
	default:
	{
		HudPreview_InitSprites();

		const float numScale = menu->KillmarkNumScale();
		const float textScale = menu->KillmarkTextScale();
		const float iconScale = menu->KillmarkIconScale();
		const int alpha = 255;
		const int countW = HudPreview_CalcNumberWidth( s_spriteState.numStrip, 10, 13, numScale );
		const int countH = HudPreview_RectHeight( &s_spriteState.numStrip[0] ) > 0 ? Q_max( 1, (int)( HudPreview_RectHeight( &s_spriteState.numStrip[0] ) * numScale + 0.5f )) : 0;
		const int textW = Q_max( 1, (int)( HudPreview_RectWidth( &s_spriteState.killTextRect ) * textScale + 0.5f ));
		const int textH = Q_max( 1, (int)( HudPreview_RectHeight( &s_spriteState.killTextRect ) * textScale + 0.5f ));
		const int iconW = Q_max( 1, (int)( HudPreview_RectWidth( &s_spriteState.iconHeadRect ) * iconScale + 0.5f ));
		const int iconH = Q_max( 1, (int)( HudPreview_RectHeight( &s_spriteState.iconHeadRect ) * iconScale + 0.5f ));
		const int gap = 4;
		const int topW = countW + gap + textW;
		const int topH = Q_max( countH, textH );
		const int iconGap = 6;
		const int blockW = Q_max( topW, iconW );
		const int blockH = topH + iconGap + iconH;
		const int blockX = panelX + panelW / 2 - blockW / 2 + (int)menu->KillmarkX();
		const int blockY = panelY + 54 + (int)menu->KillmarkY();
		const int topX = blockX + ( blockW - topW ) / 2;
		const int countY = blockY + ( topH - countH ) / 2;
		const int textY = blockY + ( topH - textH ) / 2;
		const int iconX = blockX + ( blockW - iconW ) / 2;
		const int iconY = blockY + topH + iconGap;

		EngFuncs::FillRGBA( blockX - 12, blockY - 10, blockW + 24, blockH + 20, 0, 0, 0, 120 );
		UI_DrawRectangle( blockX - 12, blockY - 10, blockW + 24, blockH + 20, uiInputFgColor );

		if( s_spriteState.scoreSheet )
		{
			EngFuncs::SPR_Set( s_spriteState.scoreSheet, 255, 255, 255, alpha );
			HudPreview_DrawNumber( s_spriteState.scoreSheet, s_spriteState.numStrip, 10, topX, countY, 13, alpha, numScale );
		}
		else
		{
			UI_DrawString( font, topX, countY, countW, countH, "13", uiColorWhite, m_scChSize, QM_LEFT, ETF_SHADOW | ETF_FORCECOL );
		}

		if( s_spriteState.killSheet )
		{
			EngFuncs::SPR_Set( s_spriteState.killSheet, 255, 255, 255, alpha );
			EngFuncs::SPR_DrawAdditiveScale( 0, topX + countW + gap, textY, &s_spriteState.killTextRect, textScale );
		}
		else
		{
			UI_DrawString( font, topX + countW + gap, textY, textW, textH, "Kill", uiColorWhite, m_scChSize, QM_LEFT, ETF_SHADOW | ETF_FORCECOL );
		}

		if( s_spriteState.killSheet )
		{
			const wrect_t *iconRect = &s_spriteState.iconHeadRect;
			EngFuncs::SPR_Set( s_spriteState.killSheet, 255, 255, 255, alpha );
			EngFuncs::SPR_DrawAdditiveScale( 0, iconX, iconY, iconRect, iconScale );
		}

		char info[192];
		snprintf( info, sizeof( info ), "combo reset %.2f  num %.2f  text %.2f  icon %.2f",
			menu->KillmarkComboReset(), menu->KillmarkNumScale(), menu->KillmarkTextScale(), menu->KillmarkIconScale() );
		UI_DrawString( font, panelX, m_scPos.y + m_scSize.h - 32, panelW, 24, info, uiColorHelp, m_scChSize, QM_LEFT, ETF_SHADOW | ETF_FORCECOL );
		break;
	}
	}
}

ADD_MENU( menu_hudfeedback, CMenuHudFeedback, UI_HudFeedback_Menu );
