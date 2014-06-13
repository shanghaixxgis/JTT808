#pragma once
#include "positionunit.h"

#include <algorithm>


template <class T>
void endswap(T *objp)
{
	unsigned char *memp = reinterpret_cast<unsigned char*>(objp);
	std::reverse(memp, memp + sizeof(T));
}




#pragma pack( push ,1 )
enum {

	/**
	*	终端注册
	*/
	JTT_REGISTER = 0x0100,
	JTT_REGISTER_REPLY = 0X8100,

	/**
	*	注销
	*/
	JTT_REGOUT = 0x0003,
	/**
	*  鉴权
	*/
	JTT_CHECK_AUTH = 0x0102 ,

	/**
	* 平台通用消息
	*/
	JTT_PLAT_GENERAL_REPLY = 0x8001,
	JTT_TERM_GENERAL_REPLY = 0x0001 ,
	/**
	* 心跳消息
	*/
	JTT_TERM_HEART = 0X0002 ,

	/*终端位置信息上报*/
	JTT_TERM_POSITION = 0x0200 

};



/**
* MsgProp说明
* 15	14 | 13	| 12	11	10	   | 9	8	7	6	5	4	3	2	1	0
保留	    分包  数据加密方式	           消息体长度
*/
/*JTT协议头*/
typedef struct JT808Protocol
{
	enum { HEADERSIZE = 13 };
	BYTE    Sign[1] ;     /**<协议头0x7e*/
	WORD    Cmd;          /**<消息ID*/  
	WORD    Mask ;        /**<消息体属性*/
	BYTE    SimNo[6] ;   /**<手机号*/ 
	WORD    SeqNo   ;    /**<消息流水号*/

	BYTE    MsgBd[2] ;   /*消息体*/	
	/**
	* 消息体长度
	*/
	WORD    GetMsgBodyLength()
	{	
		WORD ulen = Mask ; 
		endswap(&ulen) ; 
		ulen =  0X01FF & ulen  ; 
		return ulen;
	}
	/**小字节序设置长度*/
	void   SetMsgBodyLength(WORD wLen)
	{		
		endswap(&wLen) ; 
		WORD uProp = 0x00FC & Mask ; 
		Mask = uProp | wLen ; 
	}

	WORD    GetCmd()
	{
		WORD wcmd = Cmd ; 
		endswap(&wcmd) ;
		return wcmd ; 
	}
	void SetCmd(WORD cmd) 
	{
		Cmd = cmd ; 
		endswap(&Cmd) ; 
	}

	BYTE    GetCheckCode()
	{
		if(IsMultiPacket())
		{
			return 0 ; //暂时不处理分包的情况
		}
		else  //不分包
		{
			return  *( Sign + 13 + GetMsgBodyLength()) ;
		}
	}

	/**
	* 是否分包
	*/
	bool IsMultiPacket()
	{
		WORD uData = Mask ; 
		endswap(&uData) ; 

		WORD uVal = 0x2000 & uData ; 
		if( uVal == 0 )
			return false ; 
		return true ; 
	}
	/**
	* 是否加密
	*/
	bool IsEncrypted()
	{
		WORD uData = Mask ; 
		endswap(&uData) ; 
		WORD uVal = 0x1c00 & uData ; 
		if( uVal == 0 )
			return false ; 
		return true ; 
	}

	
}JT808Protocol;


/**终端注册结构*/
typedef struct JT808Body_RegisterUP
{
	WORD shen ;            //省
	WORD shi ;             //市县
	BYTE manufacturer[5] ; //制造商id
	BYTE terminaltype[8]; //终端类型
	BYTE terminalID[7] ; //终端ID
	BYTE color[1] ;   //车牌颜色
	BYTE plateID[1] ; //车牌号
}JT808Body_RegisterUP;


/**位置基本信息*/
typedef struct JT808Body_PositionUP
{

	DWORD alarmstate ; //报警标志位
	DWORD posstate ;   //状态标志位
	DWORD lat ; //纬度
	DWORD lon ; //经度
	WORD  height ; //高程
	WORD  speed ;   //速度
	WORD  heading ;//方向
	BYTE  bcdtime[6] ;//时间

}JT808Body_PositionUP;


BYTE MakeCheck(BYTE *psrc, UINT16 ilen) ;
BYTE MakeCheckProtocol(JT808Protocol *pData ) ; 

#pragma pack( pop )


class CJT808Unit :
	public CPositionUnit
{
public:

	/** GPS数据 */
	enum { MAX_PACKET_LEN  = 1024 };

	CJT808Unit(IPositionServer *pServer);
	virtual ~CJT808Unit(void);
	virtual bool ReceiveData( char *buffer , int bufferlen)  ;
	virtual void ProcessData();


protected:
	
private:
	CJT808Unit(void);

    /*预定义缓冲区，提高处理效率*/
	char mchOneBuffer[MAX_PACKET_LEN];      // 一个转义后的包缓冲
	int  miOnePtr ;                         //转义后指针
	char mchRawOneBuffer[MAX_PACKET_LEN]  ; //一个未转义原始包缓冲
	int  miRawPtr ;                         //原始指针

	char mchReplyOneBuffer[MAX_PACKET_LEN]  ;    //一个未转义下行包缓冲
	int  miReplyOnePtr ;                            //未转义下行包指针
	char mchReplyRawOneBuffer[MAX_PACKET_LEN]  ; //一个转义后下行包缓冲
	int  miReplyRawPtr ;                         //转义后下行包指针
	
	/*终端ID*/
	char TerminalID[8] ;  //
	
private:
	bool GetOneRawPacket();
	bool GetOnePacket() ;    
	void UpBufferTransfer();  //上行包去掉转义
	void DownBufferTransfer();//下行包加入转义
	void ProcessPacket(JT808Protocol *pData) ; 
	void SendTerminal();

private:
	/*终端注册应答*/
	void PacketRegisterReply();
	/*平台通信应答*/
	void PacketGeneralReply();

	/*位置信息处理*/
	void ProcessPosition(JT808Protocol *pData);

	/*Util  : BCD->NUM */
	BYTE BCDtoNumber(const BYTE& bcd) ; 


};
