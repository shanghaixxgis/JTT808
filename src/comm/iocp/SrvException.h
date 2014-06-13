#pragma once

#pragma warning( disable : 4290 )  //vc++编译器还不支持异常规范，所以忽略此警告
#include <iostream>
#include <sstream>
using namespace std;

class CSrvException
{
public:
	CSrvException(void);
	CSrvException(const char* expDescription,int expCode=0);
	~CSrvException(void);
public:
	string GetExpDescription(){return this->m_strExpDescription;};
	int GetExpCode(){return this->m_iExpCode;};
	long GetExpLine(){return this->m_iExpLineNumber;};
private:
	string m_strExpDescription;
	int m_iExpCode;//异常代码
	long m_iExpLineNumber;//异常源代码的行数，对调试版本有效
};

