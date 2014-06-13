#pragma once

#include "autolock.h"
#include "iocpserver.h"

#include "IServerDefine.h"

#include "PositonUnitFactory.h"
#include <vector>
using namespace std ; 

class CJTT808ServerDlg ; 

class CBasePositionServer :
	public CIocpServer , public IPositionServer
{
public:
	typedef vector<CPositionUnit*>::iterator Iterator ; 

	CBasePositionServer(void);
	virtual ~CBasePositionServer(void);

	void SetMainFrame( CJTT808ServerDlg *pDlg) ; 

public :
	void ProcessUpData() ; 	
	virtual void   ReceivePosition(const Common_Position &cPos); //IServerPosition
	virtual void SendToTerminal(SOCKET client , CHAR *pData , ULONG datalen ) ; 
	int Run()throw(CSrvException);//启动服务器

public :
	/**
	* Units相关操作
	*/	
	BOOL HasUnit(SOCKET client) ;
	CPositionUnit *FindUnit(SOCKET client) ; 
	void ReleaseAllUnits() ;
	void AddUnit( SOCKET client, char* ip, unsigned short port );
	void ReleaseOneUnit(SOCKET client);
protected:
	/**
	* tcp回调
	*/
	void TcpAcceptComplete(SOCKET client,char* ip,unsigned short port);
	void TcpSendComplete(SOCKET client,char* buf,unsigned long data_len);
	void TcpReceiveComplete(SOCKET client,char* buf,unsigned long data_len);
	void TcpCloseComplete( SOCKET client )  ; 
	void TimerSecond() ;                              /**<1秒定时器*/

	

	/**
	* 
	*/
	CPositonUnitFactory    *m_pFac ; 
	CCritSec m_cri ; 
	vector<CPositionUnit*>  m_vUnits ; 

	static UINT WINAPI PacketThreadFunc(LPVOID lparam) ; 

	/**
	*
	* 
	*/

	CJTT808ServerDlg *m_pDlg ; 
	

};
