
// MFCApplication1.h: file di intestazione principale per l'applicazione PROJECT_NAME
//

#pragma once

#ifndef __AFXWIN_H__
	#error "includere 'pch.h' prima di includere questo file per PCH"
#endif

#include "resource.h"		// simboli principali


// CMFCApplication1App:
// Per l'implementazione di questa classe, vedere MFCApplication1.cpp
//

class CMFCApplication1App : public CWinApp
{
public:
	CMFCApplication1App();

// Override
public:
	virtual BOOL InitInstance();

// Implementazione

	DECLARE_MESSAGE_MAP()
};

extern CMFCApplication1App theApp;
