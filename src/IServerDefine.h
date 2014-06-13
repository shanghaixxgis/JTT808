#pragma once

#include <string>
#include <map> 
#include <iostream>
using namespace std;

// std::string Util::bytes_to_hexstr(unsigned char* first, unsigned char* last)
// {    
// 	std::ostringstream oss;    
// 	oss << std::hex << std::setfill('0');    
// 	while(first<last) 
// 		oss << std::setw(2) <<  int(*first++) << " ";    
// 	return oss.str();
// }


typedef struct Common_Position
{
	char gpsid[20] ;//GPSID
	bool valid ;   //有效性
	bool alarm ;   //报警
	float lat ;    //纬度
	float lon ;    //经度
	float height ; //高程
	float speed ;  //速度
	float heading; //方向
	char  gpstime[20] ;  //时间


}Common_Position;


/**
* 回调接口声明
*/
interface IPositionServer
{
	virtual void ReceivePosition(const Common_Position &cPos) = 0  ; 
	virtual void SendToTerminal(SOCKET client , CHAR *pData , ULONG datalen ) = 0  ;
};