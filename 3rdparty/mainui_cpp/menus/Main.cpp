/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/


#include "Framework.h"
#include "Action.h"
#include "Bitmap.h"
#include "PicButton.h"
#include "YesNoMessageBox.h"
#include "keydefs.h"
#include "MenuStrings.h"
#include "PlayerIntroduceDialog.h"
#include "gameinfo.h"
#include "AnimatedBanner.h"
#include "MovieBanner.h"

#define ART_MINIMIZE_N	"gfx/shell/min_n"
#define ART_MINIMIZE_F	"gfx/shell/min_f"
#define ART_MINIMIZE_D	"gfx/shell/min_d"
#define ART_CLOSEBTN_N	"gfx/shell/cls_n"
#define ART_CLOSEBTN_F	"gfx/shell/cls_f"
#define ART_CLOSEBTN_D	"gfx/shell/cls_d"
#define MAIN_MENU_BUTTON_SHIFT_UP	42
#define MAIN_MENU_BRAND_FONT_PATH	"gfx/fonts/cs_regular.ttf"
#define MAIN_MENU_BRAND_FONT_H	18
#define MAIN_MENU_BRAND_LOGO_GLYPH	"-"

static int g_iMainMenuBrandFont = 0;

void UI_Main_LoadBrandFont()
{
	if( g_iMainMenuBrandFont > 0 )
		return;

	if( EngFuncs::textfuncs.pfnImGui_LoadFontHandle )
	{
		const char *candidates[] = {
			MAIN_MENU_BRAND_FONT_PATH,
			"gfx/fonts/FiraSans-Regular.ttf",
			"gfx/fonts/tahoma.ttf"
		};
		for( size_t i = 0; i < sizeof( candidates ) / sizeof( candidates[0] ); ++i )
		{
			g_iMainMenuBrandFont = EngFuncs::textfuncs.pfnImGui_LoadFontHandle( candidates[i], MAIN_MENU_BRAND_FONT_H );
			Con_Printf( "ImGui: main menu brand font %s handle=%d\n", candidates[i], g_iMainMenuBrandFont );
			if( g_iMainMenuBrandFont )
				break;
		}
	}
	else
	{
		Con_Printf( "ImGui: main menu brand font not loaded: pfnImGui_LoadFontHandle unavailable\n" );
	}
}

class CMenuMain: public CMenuFramework
{
public:
	CMenuMain() : CMenuFramework( "CMenuMain" ) { }

	bool KeyDown( int key ) override;
	void Draw() override;

private:
	void _Init() override;
	void _VidInit( ) override;
	void Think() override;

	void VidInit(bool connected);

	void QuitDialogCb();
	void DisconnectCb();
	void DisconnectDialogCb();
	void HazardCourseDialogCb();
	void HazardCourseCb();

	CMenuAnimatedBanner animatedBanner;
	CMenuMovieBanner movieBanner;

	CMenuPicButton	console;
	CMenuPicButton	resumeGame;
	CMenuPicButton	disconnect;
	CMenuPicButton	newGame;
	CMenuPicButton	hazardCourse;
	CMenuPicButton	configuration;
	CMenuPicButton	saveRestore;
	CMenuPicButton	multiPlayer;
	CMenuPicButton	customGame;
	CMenuPicButton	readme;
	CMenuPicButton	previews;
	CMenuPicButton	quit;

	// buttons on top right. Maybe should be drawn if fullscreen == 1?
	CMenuBitmap	minimizeBtn;
	CMenuBitmap	quitButton;

	// quit dialog
	CMenuYesNoMessageBox dialog;

	bool bTrainMap;
	bool bCustomGame;
};

void CMenuMain::QuitDialogCb()
{
	if( CL_IsActive() && EngFuncs::GetCvarFloat( "host_serverstate" ) && EngFuncs::GetCvarFloat( "maxplayers" ) == 1.0f )
		dialog.SetMessage( L( "StringsList_235" ) );
	else
		dialog.SetMessage( L( "GameUI_QuitConfirmationText" ) );

	dialog.onPositive.SetCommand( false, "quit\n" );
	dialog.Show();
}

void CMenuMain::DisconnectCb()
{
	EngFuncs::ClientCmd( false, "disconnect\n" );
	VidInit( false );
	CalcPosition();
	CalcSizes();
	VidInitItems();
}

void CMenuMain::DisconnectDialogCb()
{
	dialog.onPositive = VoidCb( &CMenuMain::DisconnectCb );
	dialog.SetMessage( L( "Really disconnect?" ) );
	dialog.Show();
}

void CMenuMain::HazardCourseDialogCb()
{
	dialog.onPositive = VoidCb( &CMenuMain::HazardCourseCb );;
	dialog.SetMessage( L( "StringsList_234" ) );
	dialog.Show();
}

/*
=================
CMenuMain::Key
=================
*/
bool CMenuMain::KeyDown( int key )
{
	if( UI::Key::IsEscape( key ) )
	{
		if ( CL_IsActive( ))
		{
			if( !dialog.IsVisible() )
				UI_CloseMenu();
		}
		else
		{
			QuitDialogCb( );
		}
		return true;
	}
	return CMenuFramework::KeyDown( key );
}

/*
=================
UI_Main_HazardCourse
=================
*/
void CMenuMain::HazardCourseCb()
{
	if( EngFuncs::GetCvarFloat( "host_serverstate" ) && EngFuncs::GetCvarFloat( "maxplayers" ) > 1 )
		EngFuncs::HostEndGame( "end of the game" );

	EngFuncs::CvarSetValue( "skill", 1.0f );
	EngFuncs::CvarSetValue( "deathmatch", 0.0f );
	EngFuncs::CvarSetValue( "teamplay", 0.0f );
	EngFuncs::CvarSetValue( "pausable", 1.0f ); // singleplayer is always allowing pause
	EngFuncs::CvarSetValue( "coop", 0.0f );
	EngFuncs::CvarSetValue( "maxplayers", 1.0f ); // singleplayer

	EngFuncs::PlayBackgroundTrack( NULL, NULL );

	EngFuncs::ClientCmd( false, "hazardcourse\n" );
}

void CMenuMain::_Init( void )
{
	if( gMenu.m_gameinfo.trainmap[0] && stricmp( gMenu.m_gameinfo.trainmap, gMenu.m_gameinfo.startmap ) != 0 )
		bTrainMap = true;
	else bTrainMap = false;

	if( EngFuncs::GetCvarFloat( "host_allow_changegame" ))
		bCustomGame = true;
	else bCustomGame = false;

	// console
	console.SetNameAndStatus( L( "GameUI_Console" ), NULL );
	console.iFlags |= QMF_NOTIFY;
	console.SetPicture( PC_CONSOLE );
	console.SetVisibility( gpGlobals->developer );
	SET_EVENT_MULTI( console.onReleased,
	{
		UI_SetActiveMenu( false );
		EngFuncs::KEY_SetDest( KEY_CONSOLE );
	});

	resumeGame.SetNameAndStatus( L( "GameUI_GameMenu_ResumeGame" ), L( "StringsList_188" ) );
	resumeGame.SetPicture( PC_RESUME_GAME );
	resumeGame.iFlags |= QMF_NOTIFY;
	resumeGame.onReleased = UI_CloseMenu;

	disconnect.SetNameAndStatus( L( "GameUI_GameMenu_Disconnect" ), L( "Disconnect from server." ) );
	disconnect.SetPicture( PC_DISCONNECT );
	disconnect.iFlags |= QMF_NOTIFY;
	disconnect.onReleased = VoidCb( &CMenuMain::DisconnectDialogCb );

	newGame.SetNameAndStatus( L( "GameUI_NewGame" ), L( "StringsList_189" ) );
	newGame.SetPicture( PC_NEW_GAME );
	newGame.iFlags |= QMF_NOTIFY;
	newGame.onReleased = UI_NewGame_Menu;

	hazardCourse.SetNameAndStatus( L( "GameUI_TrainingRoom" ), L( "StringsList_190" ) );
	hazardCourse.SetPicture( PC_HAZARD_COURSE );
	hazardCourse.iFlags |= QMF_NOTIFY;
	hazardCourse.onReleasedClActive = VoidCb( &CMenuMain::HazardCourseDialogCb );
	hazardCourse.onReleased = VoidCb( &CMenuMain::HazardCourseCb );

	multiPlayer.SetNameAndStatus( L( "GameUI_Multiplayer" ), L( "StringsList_198" ) );
	multiPlayer.SetPicture( PC_MULTIPLAYER );
	multiPlayer.iFlags |= QMF_NOTIFY;
	multiPlayer.onReleased = UI_MultiPlayer_Menu;

	configuration.SetNameAndStatus( L( "GameUI_Options" ), L( "StringsList_193" ) );
	configuration.SetPicture( PC_CONFIG );
	configuration.iFlags |= QMF_NOTIFY;
	configuration.onReleased = UI_Options_Menu;

	saveRestore.iFlags |= QMF_NOTIFY;

	customGame.SetNameAndStatus( L( "GameUI_ChangeGame" ), L( "StringsList_530" ) );
	customGame.SetPicture( PC_CUSTOM_GAME );
	customGame.iFlags |= QMF_NOTIFY;
	customGame.onReleased = UI_CustomGame_Menu;

	// TODO: add readme screen later
	readme.SetNameAndStatus( L("View Readme"), L( "StringsList_194" ) );
	readme.SetPicture( PC_VIEW_README );
	readme.iFlags |= QMF_NOTIFY;

	previews.SetNameAndStatus( L( "Previews" ), L( "StringsList_400" ) );
	previews.SetPicture( PC_PREVIEWS );
	previews.iFlags |= QMF_NOTIFY;
	SET_EVENT( previews.onReleased, EngFuncs::ShellExecute( MenuStrings[ IDS_MEDIA_PREVIEWURL ], NULL, false ) );

	quit.SetNameAndStatus( L( "GameUI_GameMenu_Quit" ), L( "GameUI_QuitConfirmationText" ) );
	quit.SetPicture( PC_QUIT );
	quit.iFlags |= QMF_NOTIFY;
	quit.onReleased = VoidCb( &CMenuMain::QuitDialogCb );

	quitButton.SetPicture( ART_CLOSEBTN_N, ART_CLOSEBTN_F, ART_CLOSEBTN_D );
	quitButton.iFlags = QMF_MOUSEONLY;
	quitButton.eFocusAnimation = QM_HIGHLIGHTIFFOCUS;
	quitButton.onReleased = VoidCb( &CMenuMain::QuitDialogCb );

	minimizeBtn.SetPicture( ART_MINIMIZE_N, ART_MINIMIZE_F, ART_MINIMIZE_D );
	minimizeBtn.iFlags = QMF_MOUSEONLY;
	minimizeBtn.eFocusAnimation = QM_HIGHLIGHTIFFOCUS;
	minimizeBtn.onReleased.SetCommand( false, "minimize\n" );

	if ( gMenu.m_gameinfo.gamemode == GAME_MULTIPLAYER_ONLY || gMenu.m_gameinfo.startmap[0] == 0 )
		newGame.SetGrayed( true );

	if ( gMenu.m_gameinfo.gamemode == GAME_SINGLEPLAYER_ONLY )
		multiPlayer.SetGrayed( true );

	// if ( gMenu.m_gameinfo.gamemode == GAME_MULTIPLAYER_ONLY )
	// {
	// 	saveRestore.SetGrayed( true );
	// 	hazardCourse.SetGrayed( true );
	// }

	// too short execute string - not a real command
	if( strlen( MenuStrings[IDS_MEDIA_PREVIEWURL] ) <= 3 )
	{
		previews.SetGrayed( true );
	}

	// server.dll needs for reading savefiles or startup newgame
	if( !EngFuncs::CheckGameDll( ))
	{
		saveRestore.SetGrayed( true );
		hazardCourse.SetGrayed( true );
		newGame.SetGrayed( true );
	}

	if( FBitSet( gMenu.m_gameinfo.flags, GFL_ANIMATED_TITLE ))
	{
		if( animatedBanner.TryLoad())
			AddItem( animatedBanner );
	}
	else
	{
		AddItem( movieBanner );
	}

	dialog.Link( this );

	AddItem( banner );
	AddItem( console );
	AddItem( disconnect );
	AddItem( resumeGame );
	AddItem( newGame );

	AddItem( configuration );
	AddItem( multiPlayer );
	AddItem( quit );
}

/*
=================
UI_Main_Init
=================
*/
void CMenuMain::VidInit( bool connected )
{
	int hoffset = ( 70 / 640.0 ) * 1024.0;

	int bottom_voffset = ( 404 / 480.0 ) * 768.0 - MAIN_MENU_BUTTON_SHIFT_UP;

	// no visible console button gap
	int ygap = 36;

	// statically positioned items
	quit.SetCoord( hoffset, bottom_voffset );

	// let's start calculating positions
	int yoffset = bottom_voffset - ygap;

	multiPlayer.SetCoord( hoffset, yoffset );
	yoffset -= ygap;

	bool single = gpGlobals->maxClients < 2;

	configuration.SetCoord( hoffset, yoffset );
	yoffset -= ygap;

	newGame.SetCoord( hoffset, yoffset );
	yoffset -= ygap;

	if( connected )
	{
		resumeGame.SetCoord( hoffset, yoffset );
		yoffset -= ygap;

		if( !single )
		{
			disconnect.SetCoord( hoffset, yoffset );
			yoffset -= ygap;
		}
	}

	console.SetCoord( hoffset, yoffset );
	yoffset -= ygap;

	// now figure out what's visible
	resumeGame.SetVisibility( connected );
	disconnect.SetVisibility( connected && !single );

	if( connected && single )
	{
		saveRestore.SetNameAndStatus( L( "Save\\Load Game" ), L( "StringsList_192" ) );
		saveRestore.SetPicture( PC_SAVE_LOAD_GAME );
		saveRestore.onReleased = UI_SaveLoad_Menu;
	}
	else
	{
		saveRestore.SetNameAndStatus( L( "GameUI_LoadGame" ), L( "StringsList_191" ) );
		saveRestore.SetPicture( PC_LOAD_GAME );
		saveRestore.onReleased = UI_LoadGame_Menu;
	}
}

void CMenuMain::_VidInit()
{
	VidInit( CL_IsActive() );
	UI_Main_LoadBrandFont();
}

void CMenuMain::Think()
{
	if( gpGlobals->developer )
	{
		if( !console.IsVisible( ))
			console.Show();
	}
	else
	{
		if( console.IsVisible( ))
			console.Hide();
	}

	CMenuFramework::Think();
}

void CMenuMain::Draw()
{
	CMenuFramework::Draw();

	if( !g_iMainMenuBrandFont || !EngFuncs::textfuncs.pfnImGui_DrawTextFont || !EngFuncs::textfuncs.pfnImGui_GetTextWidthFont )
		return;

	const int charH = Q_max( 12, (int)( MAIN_MENU_BRAND_FONT_H * uiStatic.scaleY ));
	const int bottomMargin = Q_max( 18, (int)( 20 * uiStatic.scaleY ));
	const int y = ScreenHeight - bottomMargin - charH;
	const int shadowOffset = Q_max( 1, (int)( 2 * uiStatic.scaleX ));
	const unsigned int textColor = PackRGBA( 255, 220, 64, 255 );
	const unsigned int logoColor = PackRGBA( 255, 255, 255, 230 );
	const unsigned int shadowColor = PackRGBA( 0, 0, 0, 160 );

	int x = Q_max( 18, (int)( 28 * uiStatic.scaleX ));
	int sx = x + shadowOffset;
	int sy = y + shadowOffset;
	const float fontSize = (float)charH;

	if( EngFuncs::textfuncs.pfnImGui_SetScreenSize )
		EngFuncs::textfuncs.pfnImGui_SetScreenSize( ScreenWidth, ScreenHeight );

	EngFuncs::textfuncs.pfnImGui_DrawTextFont( g_iMainMenuBrandFont, sx, sy, Red( shadowColor ), Green( shadowColor ), Blue( shadowColor ), Alpha( shadowColor ), "Counter", fontSize );
	sx += EngFuncs::textfuncs.pfnImGui_GetTextWidthFont( g_iMainMenuBrandFont, "Counter", fontSize );
	EngFuncs::textfuncs.pfnImGui_DrawTextFont( g_iMainMenuBrandFont, sx, sy, Red( shadowColor ), Green( shadowColor ), Blue( shadowColor ), Alpha( shadowColor ), MAIN_MENU_BRAND_LOGO_GLYPH, fontSize );
	sx += EngFuncs::textfuncs.pfnImGui_GetTextWidthFont( g_iMainMenuBrandFont, MAIN_MENU_BRAND_LOGO_GLYPH, fontSize );
	EngFuncs::textfuncs.pfnImGui_DrawTextFont( g_iMainMenuBrandFont, sx, sy, Red( shadowColor ), Green( shadowColor ), Blue( shadowColor ), Alpha( shadowColor ), "Strike", fontSize );

	EngFuncs::textfuncs.pfnImGui_DrawTextFont( g_iMainMenuBrandFont, x, y, Red( textColor ), Green( textColor ), Blue( textColor ), Alpha( textColor ), "Counter", fontSize );
	x += EngFuncs::textfuncs.pfnImGui_GetTextWidthFont( g_iMainMenuBrandFont, "Counter", fontSize );
	EngFuncs::textfuncs.pfnImGui_DrawTextFont( g_iMainMenuBrandFont, x, y, Red( logoColor ), Green( logoColor ), Blue( logoColor ), Alpha( logoColor ), MAIN_MENU_BRAND_LOGO_GLYPH, fontSize );
	x += EngFuncs::textfuncs.pfnImGui_GetTextWidthFont( g_iMainMenuBrandFont, MAIN_MENU_BRAND_LOGO_GLYPH, fontSize );
	EngFuncs::textfuncs.pfnImGui_DrawTextFont( g_iMainMenuBrandFont, x, y, Red( textColor ), Green( textColor ), Blue( textColor ), Alpha( textColor ), "Strike", fontSize );
}

ADD_MENU( menu_main, CMenuMain, UI_Main_Menu );
