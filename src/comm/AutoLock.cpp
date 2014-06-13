// AutoLock.cpp: implementation of the AutoLock class.
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "AutoLock.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CAutoLock::CAutoLock(CCritSec*pCritSec)
{
	m_pCritSec=pCritSec;
	m_pCritSec->Lock();
}

CAutoLock::~CAutoLock()
{
	m_pCritSec->Unlock();
}
