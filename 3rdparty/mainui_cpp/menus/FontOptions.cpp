/*
FontOptions.cpp - ImGui menu font configuration
*/

#include "Framework.h"
#include "Bitmap.h"
#include "PicButton.h"
#include "SpinControl.h"
#include "Field.h"
#include "Action.h"
#include "Utils.h"
#include "utlvector.h"
#include "utlstring.h"

#include <stdlib.h>
#include <string.h>

#define ART_BANNER		"gfx/shell/head_config"
#define FONT_INI_PATH		"fonts/font.ini"

extern void UI_Main_LoadBrandFont();

struct fontEntry_t
{
	CUtlString display;
	CUtlString path;
};

class CMenuFontListModel : public CMenuBaseArrayModel, public CUtlVector<fontEntry_t>
{
public:
	void Update() override
	{
		RemoveAll();
		AddPattern( "gfx/fonts/*.ttf" );
		AddPattern( "gfx/fonts/*.otf" );

		if( Count() == 0 )
		{
			fontEntry_t entry;
			entry.display = "cs_regular.ttf";
			entry.path = "gfx/fonts/cs_regular.ttf";
			AddToTail( entry );
		}
	}

	int GetRows() const override
	{
		return Count();
	}

	const char *GetText( int line ) override
	{
		if( !IsValidIndex( line ) )
			return "";
		return Element( line ).display.Get();
	}

	int FindPath( const char *path ) const
	{
		for( int i = 0; i < Count(); i++ )
		{
			if( !stricmp( Element( i ).path.Get(), path ) )
				return i;
		}
		return -1;
	}

	const char *PathAt( int i ) const
	{
		return IsValidIndex( i ) ? Element( i ).path.Get() : "";
	}

private:
	void AddPattern( const char *pattern )
	{
		int numFiles = 0;
		char **filenames = EngFuncs::GetFilesList( pattern, &numFiles, true );
		for( int i = 0; filenames && i < numFiles; i++ )
		{
			if( !filenames[i] || !filenames[i][0] )
				continue;

			char display[128];
			COM_FileBase( filenames[i], display, sizeof( display ));

			fontEntry_t entry;
			entry.display = display;
			entry.path = filenames[i];
			if( FindPath( entry.path.Get() ) < 0 )
				AddToTail( entry );
		}
	}
};

class CMenuFontOptions : public CMenuFramework
{
public:
	CMenuFontOptions() : CMenuFramework( "CMenuFontOptions" ), currentSize( 18.0f ), syncingSize( false ) { }

	bool KeyDown( int key ) override;
	void Show() override;

private:
	void _Init() override;
	void LoadConfig();
	void SaveCb();
	void CancelCb();
	void ApplySelection();
	void SizeSpinChanged();
	void SizeInputChanged();
	void SetSizeControls( float value );

	CMenuFontListModel fontsModel;
	CMenuSpinControl font;
	CMenuSpinControl size;
	CMenuField sizeInput;
	CMenuAction preview;
	CMenuPicButton done;
	CMenuPicButton cancel;

	CUtlString currentPath;
	float currentSize;
	bool syncingSize;
};

bool CMenuFontOptions::KeyDown( int key )
{
	if( UI::Key::IsEscape( key ) )
		CancelCb();
	return CMenuFramework::KeyDown( key );
}

void CMenuFontOptions::LoadConfig()
{
	currentPath = "gfx/fonts/cs_regular.ttf";
	currentSize = 18.0f;

	int iniSize = 0;
	char *iniData = (char *)EngFuncs::COM_LoadFile( FONT_INI_PATH, &iniSize );
	if( !iniData )
		return;

	char *pos = iniData;
	char *end = iniData + iniSize;
	while( pos < end )
	{
		while( pos < end && ( *pos == ' ' || *pos == '\t' || *pos == '\r' )) pos++;
		if( pos >= end ) break;

		if( *pos == '#' || *pos == ';' || *pos == '\n' )
		{
			while( pos < end && *pos != '\n' ) pos++;
			if( pos < end ) pos++;
			continue;
		}

		char key[64] = "";
		char value[256] = "";
		int ki = 0, vi = 0;

		while( pos < end && *pos != '=' && *pos != '\n' && ki < (int)sizeof( key ) - 1 )
			key[ki++] = *pos++;
		key[ki] = 0;

		if( pos < end && *pos == '=' ) pos++;
		while( pos < end && ( *pos == ' ' || *pos == '\t' )) pos++;

		while( pos < end && *pos != '\n' && *pos != '\r' && vi < (int)sizeof( value ) - 1 )
			value[vi++] = *pos++;
		value[vi] = 0;

		while( ki > 0 && ( key[ki - 1] == ' ' || key[ki - 1] == '\t' )) key[--ki] = 0;
		while( vi > 0 && ( value[vi - 1] == ' ' || value[vi - 1] == '\t' )) value[--vi] = 0;
		if( pos < end && *pos == '\r' ) pos++;
		if( pos < end && *pos == '\n' ) pos++;

		if( !stricmp( key, "font" ) && value[0] )
			currentPath = value;
		else if( !stricmp( key, "size" ) && value[0] )
			currentSize = (float)atof( value );
	}

	EngFuncs::COM_FreeFile( iniData );
}

void CMenuFontOptions::ApplySelection()
{
	const int index = (int)font.GetCurrentValue();
	const char *path = fontsModel.PathAt( index );
	if( path && path[0] )
		currentPath = path;

	if( currentSize < 10.0f )
		currentSize = 10.0f;
	if( currentSize > 48.0f )
		currentSize = 48.0f;

	char text[256];
	snprintf( text, sizeof( text ), "Preview: %s, %.0f px", currentPath.Get(), currentSize );
	preview.SetText( text );
}

void CMenuFontOptions::SetSizeControls( float value )
{
	if( value < 10.0f )
		value = 10.0f;
	if( value > 48.0f )
		value = 48.0f;

	currentSize = value;

	char buffer[16];
	snprintf( buffer, sizeof( buffer ), "%.0f", currentSize );

	syncingSize = true;
	size.SetCurrentValue( currentSize );
	sizeInput.SetBuffer( buffer );
	syncingSize = false;
}

void CMenuFontOptions::SizeSpinChanged()
{
	if( syncingSize )
		return;

	SetSizeControls( size.GetCurrentValue() );
	ApplySelection();
}

void CMenuFontOptions::SizeInputChanged()
{
	if( syncingSize )
		return;

	SetSizeControls( (float)atoi( sizeInput.GetBuffer() ) );
	ApplySelection();
}

void CMenuFontOptions::SaveCb()
{
	ApplySelection();

	char buffer[384];
	int len = snprintf( buffer, sizeof( buffer ),
		"font = %s\n"
		"size = %.0f\n",
		currentPath.Get(), currentSize );

	if( len > 0 )
		EngFuncs::COM_SaveFile( FONT_INI_PATH, buffer, len );

	if( EngFuncs::textfuncs.pfnImGui_LoadFont )
	{
		int result = EngFuncs::textfuncs.pfnImGui_LoadFont( currentPath.Get(), currentSize );
		Con_Printf( "ImGui: Font Options saved %s size=%.0f reload=%d\n", currentPath.Get(), currentSize, result );
	}

	UI_Main_LoadBrandFont();

	SaveAndPopMenu();
}

void CMenuFontOptions::CancelCb()
{
	Hide();
}

void CMenuFontOptions::Show()
{
	CMenuFramework::Show();

	fontsModel.Update();
	font.Setup( &fontsModel );

	LoadConfig();

	int index = fontsModel.FindPath( currentPath.Get() );
	if( index < 0 )
		index = 0;

	font.SetCurrentValue( (float)index );
	SetSizeControls( currentSize );
	ApplySelection();
}

void CMenuFontOptions::_Init()
{
	banner.SetPicture( ART_BANNER );

	fontsModel.Update();

	font.SetNameAndStatus( L( "Font" ), L( "Select the ImGui menu font." ) );
	font.Setup( &fontsModel );
	font.SetRect( 360, 260, 300, 32 );
	font.onChanged = VoidCb( &CMenuFontOptions::ApplySelection );

	sizeInput.SetNameAndStatus( L( "Size" ), L( "Type menu font size." ) );
	sizeInput.SetRect( 360, 340, 90, 32 );
	sizeInput.iMaxLength = 3;
	sizeInput.bNumbersOnly = true;
	sizeInput.onChanged = VoidCb( &CMenuFontOptions::SizeInputChanged );

	size.SetNameAndStatus( L( "Adjust" ), L( "Adjust menu font size." ) );
	size.Setup( 10, 48, 1 );
	size.SetRect( 470, 340, 160, 32 );
	size.onChanged = VoidCb( &CMenuFontOptions::SizeSpinChanged );

	preview.iFlags = QMF_INACTIVE|QMF_DROPSHADOW;
	preview.colorBase = uiColorHelp;
	preview.SetCharSize( QM_BOLDFONT );
	preview.SetRect( 360, 420, 480, 32 );
	preview.SetText( "Preview" );

	done.SetNameAndStatus( L( "Done" ), nullptr );
	done.SetPicture( PC_DONE );
	done.iFlags |= QMF_NOTIFY;
	done.SetCoord( 72, 520 );
	done.onReleased = VoidCb( &CMenuFontOptions::SaveCb );

	cancel.SetNameAndStatus( L( "GameUI_Cancel" ), nullptr );
	cancel.SetPicture( PC_CANCEL );
	cancel.iFlags |= QMF_NOTIFY;
	cancel.SetCoord( 72, 570 );
	cancel.onReleased = VoidCb( &CMenuFontOptions::CancelCb );

	AddItem( banner );
	AddItem( done );
	AddItem( cancel );
	AddItem( font );
	AddItem( sizeInput );
	AddItem( size );
	AddItem( preview );
}

ADD_MENU( menu_fontoptions, CMenuFontOptions, UI_FontOptions_Menu );
