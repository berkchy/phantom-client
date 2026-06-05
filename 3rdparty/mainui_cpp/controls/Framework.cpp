/*
Framework.cpp -- base menu fullscreen root window
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
#include "Framework.h"
#include "PicButton.h"

// menu banners used fiexed rectangle (virtual screenspace at 640x480)
#define UI_BANNER_POSX		72
#define UI_BANNER_POSY		72
#define UI_BANNER_WIDTH		736
#define UI_BANNER_HEIGHT		128

CMenuFramework::CMenuFramework( const char *name ) : BaseClass( name )
{
	memset( m_apBtns, 0, sizeof( m_apBtns ) );
	m_iBtnsNum = 0;
	bannerAnimDirection = ANIM_NO;
}

CMenuFramework::~CMenuFramework()
{
	for( int i = 0; i < m_iBtnsNum; i++ )
	{
		RemoveItem( *( m_apBtns[i] ) );
		delete m_apBtns[i];
		m_apBtns[i] = NULL;
	}
}

void CMenuFramework::Show()
{
	BaseClass::Show();
}

void CMenuFramework::Draw()
{
	static int statusFadeTime;
	static CMenuBaseItem *lastItem;
	CMenuBaseItem *item;

	BaseClass::Draw();

	item = ItemAtCursor();
	// a1ba: maybe this should be somewhere else?
	if( item != lastItem )
	{
		if( item ) item->m_iLastFocusTime = uiStatic.realTime;
		statusFadeTime = uiStatic.realTime;

		lastItem = item;
	}

	// status text removed per user request
}

void CMenuFramework::Hide()
{
	BaseClass::Hide();
}

void CMenuFramework::Init()
{
	BaseClass::Init();
	pos.x = uiStatic.xOffset;
	pos.y = uiStatic.yOffset;
	size.w = uiStatic.width;
	size.h = 768;
}

void CMenuFramework::VidInit()
{
	pos.x = uiStatic.xOffset;
	pos.y = uiStatic.yOffset;
	size.w = uiStatic.width;
	size.h = 768;
	BaseClass::VidInit();
}

CMenuPicButton * CMenuFramework::AddButton(const char *szName, const char *szStatus, EDefaultBtns buttonPicId, CEventCallback onReleased, int iFlags)
{
	if( m_iBtnsNum >= MAX_FRAMEWORK_PICBUTTONS )
	{
		Host_Error( "Too many pic buttons in framework!" );
		return NULL;
	}

	CMenuPicButton *btn = new CMenuPicButton();

	btn->SetNameAndStatus( szName, szStatus );
	btn->SetPicture( buttonPicId );
	btn->iFlags |= iFlags;
	btn->onReleased = onReleased;
	btn->SetCoord( 72, 230 + m_iBtnsNum * 36 );
	AddItem( btn );

	m_apBtns[m_iBtnsNum++] = btn;

	return btn;
}

CMenuPicButton * CMenuFramework::AddButton( const char *szName, const char *szStatus, const char *szButtonPath, CEventCallback onReleased, int iFlags, int hotkey )
{
	if( m_iBtnsNum >= MAX_FRAMEWORK_PICBUTTONS )
	{
		Host_Error( "Too many pic buttons in framework!" );
		return NULL;
	}

	CMenuPicButton *btn = new CMenuPicButton();

	btn->SetNameAndStatus( szName, szStatus );
	btn->SetPicture( szButtonPath, hotkey );
	btn->iFlags |= iFlags;
	btn->onReleased = onReleased;
	btn->SetCoord( 72, 230 + m_iBtnsNum * 36 );
	AddItem( btn );

	m_apBtns[m_iBtnsNum++] = btn;

	return btn;
}

void CMenuFramework::RealignButtons( void )
{
	for( int i = 0, j = 0; i < m_iBtnsNum; i++ )
	{
		if( !m_apBtns[i]->IsVisible())
			continue;

		m_apBtns[i]->SetCoord( 72, 230 + j * 36 );
		m_apBtns[i]->CalcPosition();
		j++;
	}
}

bool CMenuFramework::KeyDown( int key )
{
	bool b = BaseClass::KeyDown( key );

	if( UI::Key::IsEscape( key ))
	{
		const CMenuBaseWindow *newWindow = WindowStack()->Current();

		if( newWindow != nullptr && newWindow != this && newWindow->IsRoot() )
		{
			PrepareBannerAnimation( ANIM_CLOSING, nullptr );
		}
	}

	return b;
}

void CMenuFramework::PrepareBannerAnimation( EAnimation direction, CMenuPicButton *initiator )
{
	// banner is not present, ignore
	if( banner.Parent() != this )
		return;

	bannerAnimDirection = direction;
	if( initiator )
	{
		bannerRects[0].pt = initiator->GetRenderPosition();
		bannerRects[0].sz = initiator->GetRenderSize();

		bannerRects[1].pt = banner.GetRenderPosition();
		bannerRects[1].sz = banner.GetRenderSize();

		banner.szName = initiator->szName;
	}
}

bool CMenuFramework::DrawAnimation()
{
	float period = ui_menutransition_time ? ui_menutransition_time->value : 200.0f;
	float alpha;

	if( eTransitionType == ANIM_OPENING )
		alpha = ( uiStatic.realTime - m_iTransitionStartTime ) / period;
	else if( eTransitionType == ANIM_CLOSING )
		alpha = 1.0f - ( uiStatic.realTime - m_iTransitionStartTime ) / period;
	else
		return true;

	alpha = bound( 0.0f, alpha, 1.0f );

	if( ( eTransitionType == ANIM_OPENING && alpha >= 1.0f ) ||
		( eTransitionType == ANIM_CLOSING && alpha <= 0.0f ) )
	{
		if( bannerAnimDirection != ANIM_NO )
			bannerAnimDirection = ANIM_NO;
		return true;
	}

	// Slide transition: content slides in from right (opening) or out to right (closing)
	int slideOffset = (int)( ( 1.0f - alpha ) * uiStatic.width );
	int screenSlideOffset = (int)( ( 1.0f - alpha ) * ScreenWidth );

	int savedPos[256];
	int count = m_pItems.Count();
	for( int i = 0; i < count; i++ )
	{
		savedPos[i] = m_pItems[i]->pos.x;
		m_pItems[i]->pos.x += slideOffset;
		m_pItems[i]->CalcPosition();
	}

	UI_EnableAlphaFactor( alpha );

	Draw();

	if( bannerAnimDirection != ANIM_NO )
	{
		float bFrac = bound( 0.0f, alpha, 1.0f );
		Rect r = Rect::Lerp( bannerRects[0], bannerRects[1], bFrac );
		r.pt.x += screenSlideOffset;
		banner.Draw( r.pt, r.sz );
	}

	UI_DisableAlphaFactor();

	for( int i = 0; i < count; i++ )
	{
		m_pItems[i]->pos.x = savedPos[i];
		m_pItems[i]->CalcPosition();
	}

	return false;
}

CMenuFramework::CMenuBannerBitmap::CMenuBannerBitmap()
{
	SetRect( UI_BANNER_POSX, UI_BANNER_POSY, UI_BANNER_WIDTH, UI_BANNER_HEIGHT );
}

void CMenuFramework::CMenuBannerBitmap::SetPicture(const char *pic)
{
	image.Load( pic );
}

void CMenuFramework::CMenuBannerBitmap::Draw( Point pt, Size sz )
{
	if( image.IsValid() )
	{
		UI_DrawPic( pt, sz, uiColorWhite, image, QM_DRAWADDITIVE );
	}
	else
	{
		UI_DrawString( font, pt,
			sz,
			szName,
			uiPromptTextColor, m_scChSize,
			QM_LEFT, ETF_SHADOW | ETF_NOSIZELIMIT | ETF_FORCECOL );
	}
}

void CMenuFramework::CMenuBannerBitmap::Draw()
{
	CMenuFramework *pParent = (CMenuFramework *)Parent();

	if (pParent && (pParent->eTransitionType == ANIM_OPENING || pParent->bannerAnimDirection != ANIM_NO))
		return;

	Draw( m_scPos, m_scSize );
}

