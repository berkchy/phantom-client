#pragma once
#ifndef BASEFONT_H
#define BASEFONT_H

#include "BaseMenu.h"

struct charRange_t
{
	uint32_t chMin;
	uint32_t chMax;
	const uint32_t *sequence;
	size_t size;

	size_t Length() const
	{
		if( sequence )
			return size;
		return chMax - chMin;
	}

	int Character( size_t pos )
	{
		if( sequence )
			return sequence[pos];
		return chMin + pos;
	}
};

class CBaseFont
{
public:
	CBaseFont();
	virtual ~CBaseFont( );

	virtual bool Create(
		const char *name,
		int tall, int weight,
		int blur, float brighten,
		int outlineSize,
		int scanlineOffset, float scanlineScale,
		int flags ) = 0;
	virtual void GetCharRGBA( int ch, Point pt, Size sz, byte *rgba, Size &drawSize ) = 0;
	virtual void GetCharABCWidthsNoCache( int ch, int &a, int &b, int &c ) = 0;
	virtual bool HasChar( int ch ) const = 0;
	virtual const char *GetBackendName() const = 0;
	virtual void GetCharABCWidths( int ch, int &a, int &b, int &c );
	virtual void UploadGlyphsForRanges( charRange_t *range, int rangeSize );
	virtual int  DrawCharacter(int ch, Point pt, int charH, const unsigned int color, bool forceAdditive = false);

	inline int GetHeight() const       { return m_iHeight; }
	inline int GetTall() const         { return m_iTall; }
	inline const char *GetName() const { return m_szName; }
	inline int GetAscent() const       { return m_iAscent; }
	inline int GetMaxCharWidth() const { return m_iMaxCharWidth; }
	inline int GetFlags() const        { return m_iFlags; }
	inline int GetWeight() const       { return m_iWeight; }

	bool IsEqualTo( const char *name, int tall, int weight, int blur, int flags ) const;

	void DebugDraw();

	void GetTextureName(char *dst, size_t len) const;

	inline int GetEllipsisWide( ) { return m_iEllipsisWide; }

protected:
	char m_szName[32];
	int	 m_iTall, m_iWeight, m_iFlags, m_iHeight, m_iMaxCharWidth;
	int  m_iAscent;

	// blurring
	int  m_iBlur;
	float m_fBrighten;

	// Scanlines
	int  m_iScanlineOffset;
	float m_fScanlineScale;

	// Outlines
	int  m_iOutlineSize;
	int m_iEllipsisWide;

	friend class CFontManager;
};

#endif // BASEFONT_H
