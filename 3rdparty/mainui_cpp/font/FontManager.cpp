/*
FontManager.cpp - font manager
Copyright (C) 2017 a1batross

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/
#include <locale.h>
#include "FontManager.h"
#include "BaseMenu.h"
#include "Utils.h"

#include "BaseFontBackend.h"
#include "BitmapFont.h"
#include "utflib.h"

#define DEFAULT_WEIGHT   500

CFontManager *g_FontMgr;

CFontManager::CFontManager()
{
	m_Fonts.EnsureCapacity( 4 );
}

CFontManager::~CFontManager()
{
	DeleteAllFonts();
}

void CFontManager::VidInit( void )
{
	static float prevScale = 0;

	float scale = uiStatic.scaleY;

	if( !prevScale || fabs( scale - prevScale ) > 0.1f )
	{
		DeleteAllFonts();
		uiStatic.hConsoleFont = CFontBuilder( "ConsoleFont", UI_CONSOLE_CHAR_HEIGHT * scale, DEFAULT_WEIGHT )
			.Create();
		uiStatic.hDefaultFont = uiStatic.hConsoleFont;
		uiStatic.hSmallFont   = uiStatic.hConsoleFont;
		uiStatic.hSmallerFont = uiStatic.hConsoleFont;
		uiStatic.hTinyFont    = uiStatic.hConsoleFont;
		uiStatic.hBigFont     = uiStatic.hConsoleFont;
		uiStatic.hBoldFont    = uiStatic.hConsoleFont;
		uiStatic.hLightBlur   = uiStatic.hConsoleFont;
		uiStatic.hHeavyBlur   = uiStatic.hConsoleFont;

		prevScale = scale;
	}
}

void CFontManager::DeleteAllFonts()
{
	for( int i = 0; i < m_Fonts.Count(); i++ )
	{
		delete m_Fonts[i];
	}
	m_Fonts.RemoveAll();
}

void CFontManager::DeleteFont(HFont hFont)
{
	CBaseFont *font = GetIFontFromHandle(hFont);
	if( font )
	{
		m_Fonts[hFont-1] = NULL;

		delete font;
	}
}

CBaseFont *CFontManager::GetIFontFromHandle(HFont font)
{
	if( m_Fonts.IsValidIndex( font - 1 ) )
		return m_Fonts[font-1];

	return NULL;
}

int CFontManager::GetEllipsisWide(HFont font)
{
	if( m_Fonts.IsValidIndex( font - 1 ) )
		return m_Fonts[font-1]->GetEllipsisWide();
	return 0;
}

void CFontManager::GetCharABCWide(HFont font, int ch, int &a, int &b, int &c)
{
	CBaseFont *pFont = GetIFontFromHandle( font );
	if( pFont )
		pFont->GetCharABCWidths( ch, a, b, c );
	else
		a = b = c = 0;
}

int CFontManager::GetCharacterWidth(HFont font, int ch)
{
	int a, b, c;
	GetCharABCWide( font, ch, a, b, c );
	return a + b + c;
}

int CFontManager::GetCharacterWidthScaled(HFont font, int ch, int height)
{
	return GetCharacterWidth( font, ch );
}

HFont CFontManager::GetFontByName(const char *name)
{
	for( int i = 0; i < m_Fonts.Count(); i++ )
	{
		if( !stricmp( name, m_Fonts[i]->GetName() ) )
			return i;
	}
	return -1;
}

int CFontManager::GetFontTall(HFont font)
{
	CBaseFont *pFont = GetIFontFromHandle( font );
	if( pFont )
		return pFont->GetTall();
	return 0;
}

int CFontManager::GetFontAscent(HFont font)
{
	CBaseFont *pFont = GetIFontFromHandle( font );
	if( pFont )
		return pFont->GetAscent();
	return 0;
}

bool CFontManager::GetFontUnderlined(HFont font)
{
	CBaseFont *pFont = GetIFontFromHandle( font );
	if( pFont )
		return pFont->GetFlags() & FONT_UNDERLINE;
	return false;
}

void CFontManager::GetTextSize(HFont fontHandle, const char *text, int *wide, int *tall, int size )
{
	CBaseFont *font = GetIFontFromHandle( fontHandle );

	if( !font || !text || !text[0] )
	{
		if( wide ) *wide = 0;
		if( tall ) *tall = 0;
		return;
	}

	int fontTall = font->GetHeight(), x = 0;
	int _wide = 0, _tall;
	const char *ch = text;
	_tall = fontTall;
	int i = 0;
	utfstate_t state;

	while( *ch && ( size < 0 || i < size ) )
	{
		if( IsColorString( ch ) )
		{
			ch += 2;
			continue;
		}

		int uch = state.Decode((uint8_t)*ch );

		if( uch )
		{
			if( uch == '\n' && *( ch + 1 ) != '\0' )
			{
				_tall += fontTall;
				x = 0;
			}
			else
			{
				int a, b, c;
				font->GetCharABCWidths( uch, a, b, c );
				x += a + b + c;
				if( x > _wide )
					_wide = x;
			}
		}
		i++;
		ch++;
	}

	if( tall ) *tall = _tall;
	if( wide ) *wide = _wide;
}

int CFontManager::CutText(HFont fontHandle, const char *text, int height, int visibleSize, bool reverse, bool stopAtWhitespace, int *wide, bool *remaining )
{
	CBaseFont *font = GetIFontFromHandle( fontHandle );

	if( remaining )
		*remaining = false;

	if( !font || !text || !text[0] || visibleSize <= 0 )
		return 0;

	int _wide = 0;
	const char *ch = text;
	utfstate_t state;

	int whiteSpacePos = 0;

	while( *ch )
	{
		if( IsColorString( ch ) )
		{
			ch += 2;
			continue;
		}

		int uch = state.Decode((uint8_t)*ch );
		int x = 0;
		if( uch )
		{
			if( uch == '\n' )
			{
				ch++;
				break;
			}

			int a, b, c;
			font->GetCharABCWidths( uch, a, b, c );
			x = a + b + c;

			if( uch == ' ' )
			{
				whiteSpacePos = ch - text;
			}
		}

		if( !reverse && _wide + x >= visibleSize )
			break;

		ch++;
		_wide += x;
	}

	if( !reverse )
	{
		if( *ch && remaining ) *remaining = true;
		if( wide ) *wide = _wide;
		if( stopAtWhitespace && whiteSpacePos )
			return whiteSpacePos;
		return ch - text;
	}

	if( _wide < visibleSize )
	{
		if( remaining ) *remaining = false;
		if( wide ) *wide = _wide;
		return 0;
	}

	ch = text;

	whiteSpacePos = 0;

	state.Reset();
	while( *ch && _wide > visibleSize )
	{
		if( IsColorString( ch ) )
		{
			ch += 2;
			continue;
		}

		int uch = state.Decode((uint8_t)*ch );
		if( uch )
		{
			int a, b, c;
			font->GetCharABCWidths( uch, a, b, c );
			_wide -= a + b + c;

			if( uch == ' ' )
			{
				whiteSpacePos = ch - text;
			}
		}
		ch++;
	}

	if( remaining ) *remaining = true;
	if( wide ) *wide = _wide;
	if( stopAtWhitespace && whiteSpacePos ) return whiteSpacePos;
	return ch - text;

}

int CFontManager::GetTextWide(HFont font, const char *text, int size)
{
	int wide;

	GetTextSize( font, text, &wide, NULL, size );

	return wide;
}

int CFontManager::GetTextHeight(HFont fontHandle, const char *text, int size )
{
	CBaseFont *font = GetIFontFromHandle( fontHandle );
	if( !font || !text || !text[0] )
	{
		return 0;
	}

	int height = font->GetHeight();

	int i = 0;
	while( *text&&( size < 0 || i < size ) )
	{
		if( *text == '\n' )
			height += height;

		text++;
		i++;
	}
	return height;
}

int CFontManager::GetTextHeightExt( HFont fontHandle, const char *text, int height, int w, int size )
{
	CBaseFont *font = GetIFontFromHandle( fontHandle );
	if( !font || !text || !text[0] || !w )
	{
		return 0;
	}

	const char *text2 = text;
	int y = 0;

	while( *text2 && ( size < 0 || text2 - text < size ) )
	{
		int pos = CutText( fontHandle, text2, height, w, false, true );
		if( pos == 0 )
			break;

		y += height;
		text2 += pos;
	}

	return y;

}

int CFontManager::GetTextWideScaled(HFont font, const char *text, const int height, int size)
{
	return GetTextWide( font, text, size );
}

void CFontManager::UploadTextureForFont(CBaseFont *font)
{
}

int CFontManager::DrawCharacter(HFont fontHandle, int ch, Point pt, int charH, const unsigned int color, bool forceAdditive )
{
	CBaseFont *font = GetIFontFromHandle( fontHandle );

	if( !font )
		return 0;

	return font->DrawCharacter( ch, pt, charH, color, forceAdditive );
}

void CFontManager::DebugDraw(HFont fontHandle)
{
	CBaseFont *font = GetIFontFromHandle(fontHandle);

	font->DebugDraw();
}


HFont CFontBuilder::Create()
{
	CBaseFont *font;

	if( !m_hForceHandle )
	{
		for( int i = 0; i < g_FontMgr->m_Fonts.Count(); i++ )
		{
			font = g_FontMgr->m_Fonts[i];

			if( font->IsEqualTo( m_szName, m_iTall, m_iWeight, m_iBlur, m_iFlags ) )
				return i + 1;
		}
	}

	font = new CBitmapFont();

	double starttime = EngFuncs::DoubleTime();

	if( !font->Create( m_szName, m_iTall, m_iWeight, m_iBlur, m_fBrighten, m_iOutlineSize, m_iScanlineOffset, m_fScanlineScale, m_iFlags ) )
	{
		delete font;
		return -1;
	}

	g_FontMgr->UploadTextureForFont( font );

	double endtime = EngFuncs::DoubleTime();

	Con_DPrintf( "Rendering %s(%i, %i) took %f seconds\n", font->GetName(), m_iTall, m_iWeight, endtime - starttime );

	if( m_hForceHandle != -1 && g_FontMgr->m_Fonts.Count() != m_hForceHandle )
	{
		if( g_FontMgr->m_Fonts.IsValidIndex( m_hForceHandle ) )
		{
			g_FontMgr->m_Fonts.FastRemove( m_hForceHandle );
			return g_FontMgr->m_Fonts.InsertBefore( m_hForceHandle, font );
		}
	}

	return g_FontMgr->m_Fonts.AddToTail(font) + 1;
}
