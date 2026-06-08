#pragma once
#ifndef BASEFONT_H
#define BASEFONT_H

#include "BaseMenu.h"
#include "miniutl/utlrbtree.h"

#if defined(_WIN32)
#undef GetCharABCWidths
#endif // defined(_WIN32)

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

	inline int GetHeight() const       { return m_iHeight + GetEfxOffset(); }
	inline int GetTall() const         { return m_iTall; }
	inline const char *GetName() const { return m_szName; }
	inline int GetAscent() const       { return m_iAscent; }
	inline int GetMaxCharWidth() const { return m_iMaxCharWidth; }
	inline int GetFlags() const        { return m_iFlags; }
	inline int GetWeight() const       { return m_iWeight; }
	inline int GetEfxOffset() const    { return m_iBlur + m_iOutlineSize; }

	bool IsEqualTo( const char *name, int tall, int weight, int blur, int flags ) const;

	void DebugDraw();

	void GetTextureName(char *dst, size_t len) const;

	inline int GetEllipsisWide( ) { return m_iEllipsisWide; }

protected:
	void ApplyBlur( Size rgbaSz, byte *rgba );
	void ApplyOutline(Point pt, Size rgbaSz, byte *rgba );
	void ApplyScanline( Size rgbaSz, byte *rgba );
	void ApplyStrikeout( Size rgbaSz, byte *rgba );

	char m_szName[32];
	int	 m_iTall, m_iWeight, m_iFlags, m_iHeight, m_iMaxCharWidth;
	int  m_iAscent;
	bool m_bAdditive;

	// blurring
	int  m_iBlur;
	float m_fBrighten;

	// Scanlines
	int  m_iScanlineOffset;
	float m_fScanlineScale;

	// Outlines
	int  m_iOutlineSize;
	int m_iEllipsisWide;

private:
	bool ReadFromCache( const char *filename, charRange_t *range, size_t rangeSize );
	void SaveToCache( const char *filename, charRange_t *range, size_t rangeSize, CBMP *bmp );

	void GetBlurValueForPixel( double *distribution, const byte *src, Point srcPt, Size srcSz, byte *dest );

	struct glyph_t
	{
		glyph_t() : ch( 0 ), texture( 0 ), rect() { }
		glyph_t( int ch ) : ch( ch ), texture( 0 ), rect() { }
		int ch;
		HIMAGE texture;
		wrect_t rect;

		bool operator< (const glyph_t &a) const
		{
			return ch < a.ch;
		}
	};

	struct abc_t
	{
		int ch;
		int a, b, c;

		bool operator< ( const abc_t &a ) const
		{
			return ch < a.ch;
		}
	};

	CUtlRBTree<glyph_t, int> m_glyphs;
	CUtlRBTree<abc_t, int>   m_ABCCache;

	char m_szTextureName[256];
	friend class CFontManager;
};

#endif // BASEFONT_H
