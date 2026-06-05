/*
BitmapFont.cpp - bitmap font backend
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

#include "BaseMenu.h"
#include "BaseFontBackend.h"
#include "BitmapFont.h"
#include "utflib.h"

CBitmapFont::CBitmapFont() : CBaseFont() { }
CBitmapFont::~CBitmapFont() { }

bool CBitmapFont::Create(const char *name, int tall, int weight, int blur, float brighten, int outlineSize, int scanlineOffset, float scanlineScale, int flags)
{
	Q_strncpy( m_szName, name, sizeof( m_szName ) );
	m_iHeight = m_iTall = tall;
	m_iWeight = weight;
	m_iFlags = flags;

	m_iBlur = blur;
	m_fBrighten = brighten;

	m_iOutlineSize = outlineSize;

	m_iScanlineOffset = scanlineOffset;
	m_fScanlineScale = scanlineScale;
	m_iAscent = 0;
	m_iMaxCharWidth = 0;

	int a, c;
	GetCharABCWidths( '.', a, m_iEllipsisWide, c );
	m_iEllipsisWide *= 3;

	return true;
}

void CBitmapFont::GetCharRGBA(int ch, Point pt, Size sz, byte *rgba, Size &drawSize)
{
}

void CBitmapFont::GetCharABCWidthsNoCache( int ch, int &a, int &b, int &c )
{
}

void CBitmapFont::GetCharABCWidths(int ch, int &a, int &b, int &c)
{
	a = c = 0;
	b = m_iHeight / 2;
}

bool CBitmapFont::HasChar(int ch) const
{
	return true;
}

void CBitmapFont::UploadGlyphsForRanges(charRange_t *range, int rangeSize)
{
}

int CBitmapFont::DrawCharacter(int ch, Point pt, int charH, const unsigned int color, bool forceAdditive)
{
	char str[2] = {(char)ch, 0};

	EngFuncs::engfuncs.pfnDrawSetTextColor( Red( color ), Green( color ), Blue( color ), Alpha( color ) );
	return EngFuncs::engfuncs.pfnDrawConsoleString( pt.x, pt.y, str ) - pt.x;
}
