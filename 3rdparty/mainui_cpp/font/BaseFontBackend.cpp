/*
BaseFontBackend.cpp - common font renderer backend code
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
#include "BaseFontBackend.h"
#include "FontManager.h"

CBaseFont::CBaseFont()
	: m_iTall(), m_iWeight(), m_iFlags(),
	m_iHeight(), m_iMaxCharWidth(), m_iAscent(),
	m_iBlur(), m_fBrighten(),
	m_iEllipsisWide( 0 )
{
	m_szName[0] = 0;
}

CBaseFont::~CBaseFont()
{
}


void CBaseFont::GetTextureName( char *dst, size_t len ) const
{
	dst[0] = 0;
}

void CBaseFont::UploadGlyphsForRanges(charRange_t *range, int rangeSize)
{
}

int CBaseFont::DrawCharacter(int ch, Point pt, int charH, const unsigned int color, bool forceAdditive)
{
	return 0;
}

bool CBaseFont::IsEqualTo( const char *name, int tall, int weight, int blur, int flags ) const
{
	if( !stricmp( name, GetName() ) && tall == GetTall() && weight == GetWeight() && blur == m_iBlur && flags == GetFlags() )
		return true;

	return false;
}

void CBaseFont::DebugDraw()
{
}

void CBaseFont::GetCharABCWidths( int ch, int &a, int &b, int &c )
{
	// monospace console font: 9px wide per char
	a = 0;
	b = m_iHeight / 2;
	c = 0;
}
