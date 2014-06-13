#include "StdAfx.h"
#include "BasePositionServer.h"

#include "JTT808ServerDlg.h"

CBasePositionServer::CBasePositionServer(void)
{
	m_pFac = new CPositonUnitFactory(this);
}

void CBasePositionServer::SetMainFrame( CJTT808ServerDlg *pDlg) 
{
	
	m_pDlg = pDlg ; 

}
CBasePositionServer::~CBasePositionServer(void)
{
	if( m_pFac != NULL )
	{
		delete m_pFac  ; 
		m_pFac = NULL ; 
	}	
	
	ReleaseAllUnits() ;
}

/*
*	新的客户端连接
*/
void CBasePositionServer::TcpAcceptComplete(SOCKET client,char* ip,unsigned short port)
{	
	AddUnit(client, ip, port);
}
/*发送事件*/
void CBasePositionServer::TcpSendComplete(SOCKET client,char* buf,unsigned long data_len)
{
	//TRACE0(" Send Complete !\n") ; 
}
/**
* 接收报文
*/
void CBasePositionServer::TcpReceiveComplete(SOCKET client,char* buf,unsigned long data_len)
{
	//TRACE0(" Recv Complete !\n") ; 
	CPositionUnit *pUnit = FindUnit(client) ; 
	if( pUnit == NULL )
		return ;
	CAutoLock l(&m_cri);
	pUnit->ReceiveData(buf , data_len) ; 

}


void CBasePositionServer::TcpCloseComplete( SOCKET client ) 
{	
	ReleaseOneUnit(client) ;
}

/**
*  1秒钟执行
*/
void CBasePositionServer::TimerSecond() 
{	
#pragma region    处理超时终端连接
	CAutoLock l(&m_cri);
	for(Iterator  it = m_vUnits.begin() ; it != m_vUnits.end() ; it++ )
	{		
		if( (*it)->IsLostConnect() ) 
		{
			CloseSocket((*it)->getSocket()) ; 
			delete (*it);
			m_vUnits.erase(it) ; 
			return ;
		}		
	}
#pragma endregion

}

#pragma region Unit相关操作

BOOL CBasePositionServer::HasUnit(SOCKET client)
{
	for( vector<CPositionUnit*>::iterator it =m_vUnits.begin() ; it != m_vUnits.end() ; it ++ )
	{
		if( (*it)->getSocket() == client )
			return TRUE ; 
	}
	return FALSE ; 
}

void CBasePositionServer::SendToTerminal(SOCKET client , CHAR *pData , ULONG datalen )
{
	TcpSend(client , pData , datalen ) ; 
}

void   CBasePositionServer::ReceivePosition(const Common_Position &cPos)
{

	//103@117.27419299749023@31.871791818227194@0.0@0.0@2014-03-05 14:22:39@0@100@0@9939/10689@0/0@1
	/*GPSID 经度 纬度 速度 角度 时间 GPS状态 电量 卫星数内置存储卡 外置存储卡	是否充电*/
	//if(cPos.valid )
	//{
		char ss[200] = {0} ; 
		sprintf(ss , "%s@%f@%f@%4.1f@%4.1f@%s@0@100@1/1@1/1@1" ,cPos.gpsid ,  cPos.lon ,  cPos.lat ,cPos.speed ,cPos.heading,  cPos.gpstime ) ; 
		string str = ss ; 


		m_pDlg->AddLogText(ss) ; 
		m_pDlg->SendToDatabaseServer(str) ; 

	//}
	//else
	//{
	//	TRACE0("Invalid Position\n" ) ; 
	//}

}

void CBasePositionServer::ReleaseAllUnits() 
{
	CAutoLock l(&m_cri);
	for(Iterator  it = m_vUnits.begin() ; it != m_vUnits.end() ; it++ )
	{		
		CloseSocket((*it)->getSocket()) ; 
		delete (*it);
	}
	m_vUnits.clear();
}

void CBasePositionServer::AddUnit( SOCKET client, char* ip, unsigned short port )
{
	/*可以从配置文件中读取具体的Unit类型，然后创建*/
	if( HasUnit(client ))
		return ; 

	CAutoLock l(&m_cri);

	CPositionUnit *pUnit = m_pFac->CreatePositionUnit("JT808") ; 
	pUnit->SetInitParam(client , ip , port) ; 
	m_vUnits.push_back(pUnit) ; 
}

void CBasePositionServer::ReleaseOneUnit(SOCKET client)
{
	CAutoLock l(&m_cri);
	
	for(Iterator  it = m_vUnits.begin() ; it != m_vUnits.end() ; it++ )
	{		
		if( (*it)->getSocket() == client ) 
		{
			CloseSocket((*it)->getSocket()) ; 
			delete (*it);
			m_vUnits.erase(it) ; 
			return ;
		}		
	}
}

CPositionUnit *CBasePositionServer::FindUnit(SOCKET client) 
{
	for( Iterator it = m_vUnits.begin() ; it != m_vUnits.end() ; it++ )
	{
		if( (*it)->getSocket() == client  )
		{
			return (*it);
		}
	}
	return NULL ; 
}

#pragma endregion

#pragma region 数据处理相关操作
void CBasePositionServer::ProcessUpData()
{
	
	for( Iterator it = m_vUnits.begin() ; it != m_vUnits.end() ; it++ )
	{		
		CAutoLock l(&m_cri);
		(*it)->ProcessData();
	}
}


int CBasePositionServer::Run()
{
	AfxBeginThread((AFX_THREADPROC)PacketThreadFunc , this) ; 
	return CIocpServer::Run() ; 

}

UINT WINAPI CBasePositionServer::PacketThreadFunc(LPVOID lparam)
{	
	CBasePositionServer  *pServer = (CBasePositionServer* ) lparam ; 
	while(1)
	{	
		if( pServer->m_bStopServer )
		{
			AfxEndThread(0) ; 
			return 1 ; 
		}
		pServer->ProcessUpData() ; 
		Sleep(1) ; 
	}
	return  0 ;  
}

#pragma endregion