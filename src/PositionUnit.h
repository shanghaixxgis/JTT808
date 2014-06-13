#pragma once

#include "AutoLock.h"
#include <string>
#include "IServerDefine.h"
using namespace std ; 


/**
* 
    SYSTEMTIME tm1;
    _stscanf( msg.gpstime, _T("%4d-%2d-%2d %2d:%2d:%2d"),   
	&tm1.wYear, 
	&tm1.wMonth, 
	&tm1.wDay, 
	&tm1.wHour, 
	&tm1.wMinute,
	&tm1.wSecond );

	double dt;
	SystemTimeToVariantTime(&tm1,&dt);
*/
// /*上行GPS信息结构*/
// typedef struct UP_GPSDATA 
// {
// 	CHAR  chTID[20] ;//GPSid
// 	float fLat ;
// 	float fLon ; 
// 	float fspeed ; 
// 	float fheading ; 
// };

//int BCDToInt(byte bcd) //BCD转十进制
//{
//	return (0xff & (bcd>>4))*10 +(0xf & bcd);
//}
//
//int IntToBCD(byte int) //十进制转BCD
//{
//	return ((int/10)<<4+((int%10)&0x0f);
//}

//保证灵活性 采用json是最好的选择


class CPositionUnit
{
public :
	CPositionUnit(IPositionServer *pServer);
	enum{ BUFFER_SIZE = 40960 }; /**< 缓冲区大小  */
	enum{ HEART_BEAT_LIMIT = 1 };    /**< 心跳 ，分钟 */

	virtual ~CPositionUnit(void);

public : //过程
	void SetInitParam( SOCKET s ,char* ip,USHORT port) ; 
	virtual bool ReceiveData( char *buffer , int bufferlen);
	virtual void ProcessData() ;

	bool IsLostConnect();

	SOCKET getSocket()  { return m_s ; }
	const string &getIP() { return m_ip ; }
	USHORT getPort() {return m_port ; }	

protected:
	CPositionUnit(void);
	void ScanInvalidData() ;
	void Scan(int iScan = 1) ; 	

	SOCKET m_s ; 
	string m_ip ; 
	USHORT m_port ;

	/**
	*
	*/
	time_t _lastrecvtime ;     /**< 最后接收时间 */
	time_t _lastsendtime ; /**< 最后发送时间 */
	BYTE   _buffer[BUFFER_SIZE] ; /**< 缓冲区 */
	int    _dataptr ;      /**< 数据指针 */
	BOOL   _isLogon ;      /**< 是否登录 */
	IPositionServer *m_pServer ; 

};
