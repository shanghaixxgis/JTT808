// CritSec.h: interface for the CCritSec class.
//
//////////////////////////////////////////////////////////////////////
#pragma once
#ifndef __CRITSEC_H__
#define __CRITSEC_H__

class CCritSec  
{
public:
	CCritSec();
	virtual ~CCritSec();
public:
    void Lock(void);
    void Unlock(void);
protected:
    CRITICAL_SECTION m_CritSec;
};

#endif 