#include "StdAfx.h"
#include "PositonUnitFactory.h"
#include "JT808Unit.h"
#include "BasePositionServer.h"

CPositonUnitFactory::CPositonUnitFactory(IPositionServer *pServer)
{
	m_pServer = pServer ; 
}

CPositonUnitFactory::~CPositonUnitFactory(void)
{
}

CPositionUnit *CPositonUnitFactory::CreatePositionUnit(const string &strUnitType)
{
	if( strUnitType == "JT808")
	{
		return CreateJT808Unit();
	}
	return NULL;
}

CPositionUnit *CPositonUnitFactory::CreateJT808Unit() 
{
	return new CJT808Unit(m_pServer);
}