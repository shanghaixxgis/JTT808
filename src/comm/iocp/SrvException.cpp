#include "StdAfx.h"
#include "srvexception.h"

CSrvException::CSrvException(void)
{
}
CSrvException::CSrvException(const char* expDescription,int expCode/*=0*/)
{
	m_strExpDescription = expDescription;

	if(expCode != 0)
	{
		ostringstream formatString;
		formatString<<"´íÎó´úÂëÊÇ:"<<expCode;
		m_strExpDescription += formatString.str();
	}


	this->m_iExpCode = expCode;
}
CSrvException::~CSrvException(void)
{
	m_strExpDescription.empty();
}
