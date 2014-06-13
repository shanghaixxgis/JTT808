// CritSec.cpp: implementation of the CCritSec class.
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "CritSec.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CCritSec::CCritSec(void)
{
    InitializeCriticalSection(&m_CritSec);
}

CCritSec::~CCritSec(void)
{
    DeleteCriticalSection(&m_CritSec);
}

void CCritSec::Lock(void)
{
    EnterCriticalSection(&m_CritSec);
}

void CCritSec::Unlock(void)
{
    LeaveCriticalSection(&m_CritSec);
}
