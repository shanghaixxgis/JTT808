#pragma once
#include "PositionUnit.h"
#include <string>
#include "IServerDefine.h"


using namespace std;

class CPositonUnitFactory
{

private:	
	CPositonUnitFactory();  
	CPositonUnitFactory(const CPositonUnitFactory &);  
	CPositonUnitFactory& operator = (const CPositonUnitFactory &);

public:
	CPositonUnitFactory(IPositionServer *pServer);  
	virtual ~CPositonUnitFactory(void);
	CPositionUnit *CreatePositionUnit(const string &strUnitType) ; 
protected:
	CPositionUnit *CreateJT808Unit()  ;

	IPositionServer *m_pServer ; 

};
