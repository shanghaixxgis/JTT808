
#include "stdafx.h"
#include "IocpServer.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CIocpServer::CIocpServer()
{
	m_pLogFile = new CLogFileEx(".\\SrvLog", CLogFileEx :: DAY);
	try
	{
	    m_pTcpRevCallbackFuc = NULL; 
		m_pTcpSendCallbackFuc = NULL;
		m_pTcpAcceptCallbackFuc = NULL ; 
	    m_pUdpRevCallbackFuc = NULL; 
	    m_pUdpSendCallbackFuc = NULL;

		m_bExceptionStop = false;
		m_bInitOk = true;
		m_bStopServer = false;
		m_bServerIsRunning = false;
		m_iWorkerThreadNumbers = 2;
		m_iTimerId = 0;
		m_hTcpServerSocket = NULL;
		m_hUdpServerSocket = NULL;

	    m_hTcpClientSocket = NULL;
	    m_hUdpClientSocket = NULL;

		ZeroMemory(&m_TcpServerAdd,sizeof(struct sockaddr_in));
		ZeroMemory(&m_UdpServerAdd,sizeof(struct sockaddr_in));

		m_hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE,NULL,0,0);
		if(m_hCompletionPort == NULL)
		{
			PrintDebugMessage("完成端口对象创建失败,程序无法启动.");
		}

		for(int i=0;i<MAX_PROCESSER_NUMBERS;i++)
			m_hWorkThread[i] = NULL;

		m_hListenThread = NULL;
		m_hPostAcceptEvent = CreateEvent(NULL,FALSE,FALSE,NULL);
		
		InitializeCriticalSectionAndSpinCount(&m_struCriSec,4000);

		m_pfAcceptEx = NULL;				  //AcceptEx函数地址
		m_pfGetAddrs = NULL;	//GetAcceptExSockaddrs函数的地址

		m_pUdpContextManager = NULL;
		m_pTcpAcceptContextManager = NULL; //接收内存池管理对象
		m_pTcpReceiveContextManager = NULL; //数据接收内存池管理对象
		m_pTcpSendContextManager = NULL;//数据发送内存池管理对象



		m_iOpCounter = 0;

		WSADATA wsData;
		if(WSAStartup(MAKEWORD(2, 2), &wsData)!=0)
			PrintDebugMessage("初始化sockte库错误.");
	}
	catch(CSrvException& e)
	{
		m_bInitOk = false;
		PrintDebugMessage(e);
		//if(m_bExceptionStop)
		//	throw e;
	}
	catch(...)
	{
		m_bInitOk = false;
		string message("CIocpServer 构造函数发生未知异常.");
		PrintDebugMessage(message.c_str());
		//if(m_bExceptionStop)
		//	throw message;
	}
}

CIocpServer::~CIocpServer()
{
	try
	{
		if(m_bServerIsRunning)
			this->Stop(); //停止服务器

		delete m_pLogFile;
		m_pLogFile = NULL;

		m_pTcpRevCallbackFuc = NULL; 
		m_pTcpSendCallbackFuc = NULL;
		m_pTcpAcceptCallbackFuc = NULL ; 
		m_pUdpRevCallbackFuc = NULL; 
		m_pUdpSendCallbackFuc = NULL;

		m_bInitOk = true;
		m_bStopServer = false;
		m_bServerIsRunning = false;
		m_iWorkerThreadNumbers = 0;
		m_iTimerId = 0;
		m_hTcpServerSocket = NULL;
		m_hUdpServerSocket = NULL;

		m_hTcpClientSocket = NULL;
		m_hUdpClientSocket = NULL;

		ZeroMemory(&m_TcpServerAdd,sizeof(struct sockaddr_in));
		ZeroMemory(&m_UdpServerAdd,sizeof(struct sockaddr_in));

		CloseHandle(m_hCompletionPort);
		for(int i=0;i<MAX_PROCESSER_NUMBERS;i++)
			m_hWorkThread[i] = NULL;

		m_hListenThread = NULL;

		CloseHandle(m_hPostAcceptEvent);
		m_hPostAcceptEvent = NULL;

		DeleteCriticalSection(&m_struCriSec);

		m_pfAcceptEx = NULL;				//AcceptEx函数地址
		m_pfGetAddrs = NULL;	//GetAcceptExSockaddrs函数的地址

		m_pUdpContextManager = NULL;
		m_pTcpAcceptContextManager = NULL; //接收内存池管理对象
		m_pTcpReceiveContextManager = NULL; //数据接收内存池管理对象
		m_pTcpSendContextManager = NULL;//数据发送内存池管理对象

		m_iOpCounter = 0;

		WSACleanup();
	}
	catch(CSrvException& e)
	{
		PrintDebugMessage(e);
		//if(m_bExceptionStop)
			//throw e;
	}
	catch(...)
	{
		string message("~CIocpServer 发生未知异常.");
		PrintDebugMessage(message.c_str());
		//if(m_bExceptionStop)
			//throw message;
	}
}
int CIocpServer::InitServer(char* ip,unsigned short tcp_port,unsigned short udp_port)
{
	BOOL rc = FALSE;
	try
	{
		if( ip == NULL)
			throw CSrvException("服务器ip 地址非法.",-1);
		if(tcp_port == udp_port)
			throw CSrvException("tcp 服务器端口号和udp 服务器端口号重复.",-1);

		if(InitTcpServer(ip,tcp_port)==FALSE)
			throw CSrvException("tcp 服务器初始化失败.",-1);
		if(InitUdpServer(ip,udp_port)==FALSE) //初始化服务器
			throw CSrvException("udp 服务器初始化失败.",-1);

		rc = TRUE;
	}
	catch(CSrvException& e)
	{
		PrintDebugMessage(e);
		//if(m_bExceptionStop)
		//	throw e;
	}
	catch(...)
	{
		string message("InitServer 发生未知异常.");
		PrintDebugMessage(message.c_str());
		if(m_bExceptionStop)
			throw message;
	}

	return rc;
}

//创建监听套接字
int CIocpServer::InitTcpServer(char* ip,unsigned short tcp_port)//初始化tcp服务器
{
    BOOL rc = FALSE;
	try
	{
		int errorCode = 1;
		if(ip==NULL)
		{
			throw CSrvException("非法的tcp ip地址.",-1);
		}
		if(m_hTcpServerSocket)
		{
			throw CSrvException("已经有socket运行为tcp服务器模式.",-1);
		}
		//创建套接字
		m_hTcpServerSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
		if(m_hTcpServerSocket == INVALID_SOCKET)
		{
			throw CSrvException("错误的tcp socket套接字.",-1);
		}
		//把accetpex设置为异步非阻塞模式
		ULONG ul = 1;
		errorCode = ioctlsocket(m_hTcpServerSocket, FIONBIO, &ul);
		if(SOCKET_ERROR == errorCode)
		{
			throw CSrvException("Set listen socket to FIONBIO mode error.",WSAGetLastError());
		}

		//设置为socket重用,防止服务器崩溃后端口能够尽快再次使用或共其他的进程使用
		int nOpt = 1;
		errorCode = setsockopt(m_hTcpServerSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&nOpt, sizeof(nOpt));
		if(SOCKET_ERROR == errorCode)
		{
			throw CSrvException("Set listen socket to SO_REUSEADDR mode error.",WSAGetLastError());
		}

		//关闭系统的接收与发送缓冲区
		int nBufferSize = 0;
		setsockopt(m_hTcpServerSocket,SOL_SOCKET,SO_SNDBUF,(char*)&nBufferSize,sizeof(int));
		nBufferSize = DEFAULT_TCP_RECEIVE_BUFFER_SIZE;
		setsockopt(m_hTcpServerSocket,SOL_SOCKET,SO_RCVBUF,(char*)&nBufferSize,sizeof(int)); 

		unsigned long dwBytes = 0;
		GUID guidAcceptEx = WSAID_ACCEPTEX;
		if (NULL == m_pfAcceptEx)
		{
			errorCode = WSAIoctl(m_hTcpServerSocket,SIO_GET_EXTENSION_FUNCTION_POINTER, &guidAcceptEx,sizeof(guidAcceptEx)
				, &m_pfAcceptEx, sizeof(m_pfAcceptEx), &dwBytes, NULL, NULL);
		}
		if (NULL == m_pfAcceptEx || SOCKET_ERROR == errorCode)
		{
			throw CSrvException("Invalid fuction pointer.",WSAGetLastError());
		}

		//加载GetAcceptExSockaddrs函数
		GUID guidGetAddr = WSAID_GETACCEPTEXSOCKADDRS;
		if (NULL == m_pfGetAddrs)
		{
			errorCode = WSAIoctl(m_hTcpServerSocket, SIO_GET_EXTENSION_FUNCTION_POINTER, &guidGetAddr, sizeof(guidGetAddr)
				, &m_pfGetAddrs, sizeof(m_pfGetAddrs), &dwBytes, NULL, NULL);
		}
		if (NULL == m_pfGetAddrs || SOCKET_ERROR == errorCode)
		{
			throw CSrvException("Invalid fuction pointer.",WSAGetLastError());
		}

		//填充服务器地址信息
		m_TcpServerAdd.sin_family = AF_INET;
		m_TcpServerAdd.sin_addr.s_addr = inet_addr(ip);
		m_TcpServerAdd.sin_port = htons(tcp_port);

		//邦定套接字和服务器地址
		errorCode = bind(m_hTcpServerSocket,(struct sockaddr*)&m_TcpServerAdd,sizeof(m_TcpServerAdd));
		if(errorCode == SOCKET_ERROR)
		{
			throw CSrvException("Error socket bind operation.",WSAGetLastError());
		}
		//把监听socket和完成端口邦定
		if(NULL == CreateIoCompletionPort((HANDLE)m_hTcpServerSocket,m_hCompletionPort,m_hTcpServerSocket,0))
		{
			throw CSrvException("Invalid completion port handle.",WSAGetLastError());
		}
		//当连接请求不足时，os将激发此事件
		WSAEventSelect(m_hTcpServerSocket,m_hPostAcceptEvent,FD_ACCEPT); //此事件将由系统触发
		//设置监听等待句柄
		errorCode = listen(m_hTcpServerSocket,5);//第二个参数设置在连接队列里面的等待句柄数目.并发量大的server需要调大此值
		if(errorCode)
		{
			throw CSrvException("Error socket lister operation.",WSAGetLastError());
		}
	    
		if(m_pTcpAcceptContextManager==NULL)
			m_pTcpAcceptContextManager = new CContextManager<TCP_ACCEPT_CONTEXT>(256); //接收内存池管理对象
		if(m_pTcpReceiveContextManager==NULL)
			m_pTcpReceiveContextManager = new CContextManager<TCP_RECEIVE_CONTEXT>(512); //数据接收内存池管理对象
		if(m_pTcpSendContextManager==NULL)
			m_pTcpSendContextManager = new CContextManager<TCP_SEND_CONTEXT>(512);//数据发送内存池管理对象

		PostTcpAcceptOp(32);//投递链接操作到完成端口,应付高并发量,即初始阶段有32个连接在等待client的接入,在关闭client的时候完成端口将会释放相应的context

		//printf("tcp 服务器初始化成功.\n");
		return TRUE;
	}
	catch(CSrvException& e)
	{
		m_bInitOk = false;
		this->PrintDebugMessage(e);
		if(m_bExceptionStop)
			throw e;
	}
	catch(...)
	{
		m_bInitOk = false;
		string message("InitTcpServer 发生未知异常.");
		PrintDebugMessage(message.c_str());
		//if(m_bExceptionStop)
			//throw message;
	}
	return FALSE;
}
int CIocpServer::InitUdpServer(char* ip,unsigned short udp_prot)//初始化udp服务器
{
	try
	{
		if(!m_bInitOk)
            PrintDebugMessage("服务器初始化失败.");

		int errorCode = 1;
		if(ip==NULL)
			PrintDebugMessage("服务器ip地址不能为空.");

		if(m_hUdpServerSocket)
			PrintDebugMessage("已经有socket初始化为udp服务器模式.");

		//创建监听套接字
		m_hUdpServerSocket = WSASocket(AF_INET,SOCK_DGRAM,IPPROTO_UDP, NULL, 0, WSA_FLAG_OVERLAPPED);
		if(m_hUdpServerSocket == INVALID_SOCKET)
			PrintDebugMessage("初始化udp监听套接字句柄失败.",WSAGetLastError());

		//把receiveform及sendto设置为异步非阻塞模式,因为用WSAStartup初始化默认是同步
		ULONG ul = 1;
		errorCode = ioctlsocket(m_hUdpServerSocket, FIONBIO, &ul);
		if(SOCKET_ERROR == errorCode)
		{
			PrintDebugMessage("Set listen socket to FIONBIO mode error.");
		}
		
		//设置为地址重用，优点在于服务器关闭后可以立即启用		
		int nOpt = 1;
		errorCode = setsockopt(m_hUdpServerSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&nOpt, sizeof(nOpt));
		if(SOCKET_ERROR == errorCode)
		{
			PrintDebugMessage("Set listen socket to SO_REUSEADDR mode error.");
		}
		
		//关闭接收与发送缓冲区,直接使用用户空间缓冲区
		int nBufferSize = 0;
		setsockopt(m_hUdpServerSocket,SOL_SOCKET,SO_SNDBUF,(char*)&nBufferSize,sizeof(int));
		nBufferSize = 64*10240;
		setsockopt(m_hUdpServerSocket,SOL_SOCKET,SO_RCVBUF,(char*)&nBufferSize,sizeof(int)); 
		
		unsigned long dwBytesReturned = 0;
		int bNewBehavior = FALSE;
		
		//下面的函数用于解决远端突然关闭会导致WSARecvFrom返回10054错误导致服务器完成队列中没有reeceive操作而设置
		errorCode  = WSAIoctl(m_hUdpServerSocket, SIO_UDP_CONNRESET,&bNewBehavior, sizeof(bNewBehavior),NULL, 0, &dwBytesReturned,NULL, NULL);
		if (SOCKET_ERROR == errorCode)
		{
			PrintDebugMessage("Set listen socket to SIO_UDP_CONNRESET mode error.");
		}
		//填充服务器地址信息
		m_UdpServerAdd.sin_family = AF_INET;
		m_UdpServerAdd.sin_addr.s_addr = inet_addr(ip);
		m_UdpServerAdd.sin_port = htons(udp_prot);
		
		//邦定套接字和服务器地址
		errorCode = bind(m_hUdpServerSocket,(struct sockaddr*)&m_UdpServerAdd,sizeof(m_UdpServerAdd));
		if(errorCode)
		{
			PrintDebugMessage("Error socket bind operation.");
		}
		
		//把监听线程和完成端口邦定
		if(NULL == CreateIoCompletionPort((HANDLE)m_hUdpServerSocket,m_hCompletionPort,m_hUdpServerSocket,0))
			PrintDebugMessage("Invalid completion port handle.");

		if(m_pUdpContextManager==NULL)
			m_pUdpContextManager = new CContextManager<UDP_CONTEXT>(256);

		//投递reveive操作，等待client的udp数据包到达,应付高并发udp数据报的到来
		if(PostUdpReceiveOp(m_hUdpServerSocket,64)!=64)
		{
			PrintDebugMessage("投递receive 操作有失败.");
		}
         
		PrintDebugMessage("udp 服务器初始化成功.");

		return TRUE;
	}
	catch(CSrvException& e)
	{
       	m_bInitOk = false;
		this->PrintDebugMessage(e);
		//if(m_bExceptionStop)
		//	throw e;
	}
	catch(...)
	{
		m_bInitOk = false;

		string message("InitUdpServer 发生未知异常.");
		PrintDebugMessage(message.c_str());
		if(m_bExceptionStop)
			throw message;
	}
	return FALSE;
}
int CIocpServer::InitTcpClient(char* remote_ip,unsigned short remote_port)
{
	int rc = FALSE;
	try
	{
	    m_hTcpClientSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
		if(m_hTcpServerSocket == INVALID_SOCKET)
		{
			PrintDebugMessage("初始化tcp套接字失败.");
		}
		rc = this->AssociateAsTcpClient(m_hTcpClientSocket,remote_ip,remote_port);
		if(rc != 1) m_hTcpClientSocket = NULL;
	}
	catch(CSrvException& e)
	{
		this->PrintDebugMessage(e);
		if(m_bExceptionStop)
			throw e;
	}
	catch(...)
	{
		string message("InitTcpClient 发生未知异常.");
		PrintDebugMessage(message.c_str());
		if(m_bExceptionStop)
			throw message;	
	}
	return rc;
}
int CIocpServer::InitUdpClient(char* local_ip,unsigned short local_port)
{
	int rc = FALSE;
	try
	{
	    m_hUdpClientSocket = WSASocket(AF_INET,SOCK_DGRAM,IPPROTO_UDP, NULL, 0, WSA_FLAG_OVERLAPPED);

		if(m_hUdpClientSocket == INVALID_SOCKET)
			PrintDebugMessage("初始化udp套接字失败.");

		rc = this->AssociateAsUdpClient(m_hUdpClientSocket,local_ip,local_port);
	}
	catch(CSrvException& e)
	{
		this->PrintDebugMessage(e);
		//if(m_bExceptionStop)
			//throw e;
	}
	catch(...)
	{
		string message("InitUdpClient 发生未知异常.");
		PrintDebugMessage(message.c_str());
		//if(m_bExceptionStop)
		//	throw message;
	}
	return rc;
}

//把传入的socket与完成端口关联
//由完成端口负责告知其他模块数据的接受与发送工作
int CIocpServer::AssociateAsUdpClient(SOCKET socket,char* local_ip,unsigned short local_port)
{
	BOOL rc = FALSE;
	try
	{
		if(socket == NULL)
			PrintDebugMessage("AssociateSocket() 错误的套接字句柄.");

		if(local_ip == NULL)
			PrintDebugMessage("AssociateSocket() 错误的IP地址.");

		struct sockaddr_in local_addr;
		local_addr.sin_family = AF_INET;
		local_addr.sin_addr.s_addr = inet_addr(local_ip);
		local_addr.sin_port = htons(local_port);

		//把receiveform及sendto设置为异步非阻塞模式,因为用WSAStartup初始化默认是同步
		//设置套接字的属性
		ULONG ul = 1;
		int errorCode = ioctlsocket(socket, FIONBIO, &ul);
		if(SOCKET_ERROR == errorCode)
		{
			PrintDebugMessage("Set listen socket to FIONBIO mode error.");
		}

		//设置为地址重用，优点在于服务器关闭后可以立即启用		
		int nOpt = 1;
		errorCode = setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, (char*)&nOpt, sizeof(nOpt));
		if(SOCKET_ERROR == errorCode)
		{
			PrintDebugMessage("Set listen socket to SO_REUSEADDR mode error.");
		}

		//关闭接收与发送缓冲区,直接使用用户空间缓冲区,减少内存拷贝操作
		int nBufferSize = 0;
		setsockopt(socket,SOL_SOCKET,SO_SNDBUF,(char*)&nBufferSize,sizeof(int));
		nBufferSize = 64*10240;
		setsockopt(socket,SOL_SOCKET,SO_RCVBUF,(char*)&nBufferSize,sizeof(int)); 

		unsigned long dwBytesReturned = 0;
		int bNewBehavior = FALSE;

		//下面的函数用于解决远端突然关闭会导致WSARecvFrom返回10054错误导致服务器完成队列中没有reeceive操作而设置
		errorCode  = WSAIoctl(socket, SIO_UDP_CONNRESET,&bNewBehavior, sizeof(bNewBehavior),NULL, 0, &dwBytesReturned,NULL, NULL);
		if (SOCKET_ERROR == errorCode)
		{
			PrintDebugMessage("Set listen socket to SIO_UDP_CONNRESET mode error.");
		}

		//邦定套接字和传入的ip地址
		errorCode = bind(socket,(struct sockaddr*)&local_addr,sizeof(local_addr));
		if(errorCode)
		{
			PrintDebugMessage("Error socket bind operation.",-1);
		}
		//把端口和完成端口邦定
		if(NULL == CreateIoCompletionPort((HANDLE)socket,m_hCompletionPort,socket,0))
			PrintDebugMessage("Invalid completion port handle.");

		if(m_pUdpContextManager==NULL)
			m_pUdpContextManager = new CContextManager<UDP_CONTEXT>(256);

		//投递reveive操作，等待client的udp数据包到达,主要为应付高并发udp数据报的到来
		if(PostUdpReceiveOp(socket,2)!=2)
		{
			PrintDebugMessage("投递receive 操作有失败.");
		}

		rc = TRUE;
	}
	catch(CSrvException& e)
	{
		this->PrintDebugMessage(e);
		//if(m_bExceptionStop)
		//	throw e;
	}
	catch(...)
	{
		string message("AssociateAsUdpClient 发生未知异常.");
		PrintDebugMessage(message.c_str());
		if(m_bExceptionStop)
			throw message;
	}
	return rc;
}
int CIocpServer::AssociateAsTcpClient(SOCKET socket,char* remote_ip,unsigned short remote_port)
{
	BOOL rc = FALSE;
	try
	{
		if(socket == NULL)
			PrintDebugMessage("AssociateSocket() 错误的套接字句柄.");

		if(remote_ip == NULL)
			PrintDebugMessage("AssociateSocket() 错误的IP地址.");

		struct sockaddr_in serv_addr;
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_addr.s_addr = inet_addr(remote_ip);
		serv_addr.sin_port = htons(remote_port);

		//设置为socket重用,防止服务器崩溃后端口能够尽快再次使用或共其他的进程使用
		int nOpt = 1;
		int errorCode = setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, (char*)&nOpt, sizeof(nOpt));
		if(SOCKET_ERROR == errorCode)
		{
			PrintDebugMessage("Set client socket to SO_REUSEADDR mode error.");
		}

		//关闭系统的接收与发送缓冲区
		int nBufferSize = 0;
		setsockopt(socket,SOL_SOCKET,SO_SNDBUF,(char*)&nBufferSize,sizeof(int));
		nBufferSize = DEFAULT_TCP_RECEIVE_BUFFER_SIZE;
		setsockopt(socket,SOL_SOCKET,SO_RCVBUF,(char*)&nBufferSize,sizeof(int)); 

		//邦定套接字和服务器地址
		errorCode = connect(socket,(struct sockaddr*)&serv_addr,sizeof(serv_addr));
		if(errorCode == SOCKET_ERROR)
		{
			PrintDebugMessage("AssociateSocket(),连接到服务器的时候错误.");
		}
		//把socket和完成端口邦定
		if(NULL == CreateIoCompletionPort((HANDLE)socket,m_hCompletionPort,socket,0))
		{
			PrintDebugMessage("Invalid completion port handle.");
		}

		if(m_pTcpReceiveContextManager==NULL)
			m_pTcpReceiveContextManager = new CContextManager<TCP_RECEIVE_CONTEXT>(512); //数据接收内存池管理对象
		if(m_pTcpSendContextManager==NULL)
			m_pTcpSendContextManager = new CContextManager<TCP_SEND_CONTEXT>(512);//数据发送内存池管理对象

		if(IssueTcpReceiveOp(socket,2)!=TRUE)
			PrintDebugMessage("AssociateAsClinet投递有错误.");

		rc = TRUE;
	}
	catch(CSrvException& e)
	{
		this->PrintDebugMessage(e);
		PrintDebugMessage(e);
		//if(m_bExceptionStop)
			//throw e;
	}
	catch(...)
	{
		string message("AssociateAsTcpClient 发生未知异常.");
		PrintDebugMessage(message.c_str());
		//if(m_bExceptionStop)
		//	throw message;
	}
	return rc;
}
int CIocpServer::CreateThread()
{
	if(m_bServerIsRunning)
	{
		PrintDebugMessage("启动队列线程失败,队列线程已经在运行当中.");
		return FALSE;
	}

	SYSTEM_INFO  sysinfo;
	GetSystemInfo(&sysinfo);
	m_iWorkerThreadNumbers = sysinfo.dwNumberOfProcessors*2+2;//最佳线程数量是CPU数量的2倍
	
	if(m_iWorkerThreadNumbers>=MAX_PROCESSER_NUMBERS) //注意此处很可能会引起性能的剧烈下降
		m_iWorkerThreadNumbers = MAX_PROCESSER_NUMBERS-1;
	
	m_iTimerId = timeSetEvent(1000,1000,TimerProc,(DWORD)this,TIME_PERIODIC);//1秒钟定时器

	//创建监听线程
	m_hListenThread = (HANDLE)_beginthreadex(NULL,0,ListenThread,this,0,NULL);
	if(!m_hListenThread)
	{
		PrintDebugMessage("创建队列线程失败.");
		return FALSE;
	}

	int counter = 0;
	for (int i=0; i<m_iWorkerThreadNumbers; i++)
	{
		m_hWorkThread[i] = (HANDLE)_beginthreadex(NULL,0,WorkThread,this,0,NULL);
		if(!m_hWorkThread[i]) //将少创建一个线程
		{
			m_hWorkThread[i] = NULL;
			continue;
		}	
		counter++;
	}
	m_iWorkerThreadNumbers = counter;
	return counter;
}
UINT WINAPI CIocpServer::ListenThread(LPVOID lpParama)
{
	CIocpServer* pThis = (CIocpServer*)lpParama;
	if(!pThis)
		return 0; 

	if(pThis->m_hTcpServerSocket==NULL)
	{
		pThis->PrintDebugMessage("启动tcp 监听线程失败,请确认监听socket已经创建.");
		return 0;
	}

	unsigned long rc = 0;
	unsigned short postAcceptCnt = 1;
	while(!pThis->m_bStopServer)
	{

		rc = WSAWaitForMultipleEvents(1, &pThis->m_hPostAcceptEvent, FALSE,1000, FALSE);		
		if(pThis->m_bStopServer)
			break;

		if(WSA_WAIT_FAILED == rc)
		{
			continue;
		}
		else if(rc ==WSA_WAIT_TIMEOUT)//超时
		{
			continue;
		}
		else  
		{
			rc = rc - WSA_WAIT_EVENT_0;
			if(rc == 0)
				postAcceptCnt = 32;
			else
				postAcceptCnt = 1;

			pThis->PostTcpAcceptOp(postAcceptCnt);
		}
	}
	return 0;
}
UINT WINAPI CIocpServer::WorkThread(LPVOID lpParam)
{
	CIocpServer* p = (CIocpServer*)lpParam;
	if(!p)
		return 0;
	
	p->m_iOpCounter = 0;
	
	int bResult = FALSE;;
	unsigned long NumTransferred = 0;
	SOCKET socket = 0 ;
	IO_PER_HANDLE_STRUCT*  pOverlapped = NULL;
	HANDLE hCompletionPort = p->m_hCompletionPort;
	
	while(true)
	{
		bResult = FALSE;
		NumTransferred = 0;
		socket = NULL;
		pOverlapped = NULL;
		
		bResult = GetQueuedCompletionStatus(hCompletionPort,(LPDWORD)&NumTransferred,(LPDWORD)&socket,(LPOVERLAPPED *)&pOverlapped,INFINITE);
		
		if(bResult == FALSE)
		{
			if(pOverlapped !=NULL)
			{
				unsigned long errorCode = GetLastError();
				
				if(ERROR_INVALID_HANDLE == errorCode)   
				{   
					p->PrintDebugMessage("完成端口被关闭,退出\n",errorCode);
					return 0;   
				}   
				else if(ERROR_NETNAME_DELETED == errorCode || ERROR_OPERATION_ABORTED == errorCode)   
				{   
					p->PrintDebugMessage("socket被关闭 或者 操作被取消\n",errorCode);    
				}   
				else if(ERROR_MORE_DATA == errorCode)//此错误发生的概率较高，但视网络MTU值，太大的UDP包会在网络上被丢弃
				{
					if(pOverlapped->operate_type == IO_UDP_RECEIVE)
					{
						p->IssueUdpReceiveOp((UDP_CONTEXT*)pOverlapped);
					}
					p->PrintDebugMessage("Udp 数据包被截断,请减小数据包的长度.");
					continue;
				}
				else if(ERROR_PORT_UNREACHABLE == errorCode)//远程系统异常终止
				{
					p->PrintDebugMessage("客户端终止数据发送,错误代码:%d.\n",errorCode);
				}
				else
				{
					p->PrintDebugMessage("未定义错误",errorCode);
                }
				p->ReleaseContext(pOverlapped);
			}
			else //超时
			{
				if(WAIT_TIMEOUT == GetLastError() && p->m_bStopServer)
					break;
			}
			continue;
		}
		else//正常操作
		{
			if(pOverlapped)
			{
				p->CompleteIoProcess(pOverlapped,NumTransferred); //IO操作
				::InterlockedExchangeAdd(&p->m_iOpCounter,1);
				continue;
			}
			else
			{
				p->PrintDebugMessage("工作线程正常退出.",GetLastError());
				if(socket == IO_EXIT && p->m_bStopServer)
					break;
			}
		}
	}
	return 0;
}
//完成处理例程,主要是释放资源,保存数据或投递新的io操作
//还没有检测失败
void CIocpServer::CompleteIoProcess(IO_PER_HANDLE_STRUCT* pOverlapped,unsigned long data_len)
{
	try
	{
		if(pOverlapped==NULL)
		{
			PrintDebugMessage("CompleteIoProcess 参数pOverlapped非法.");
			return ;
		}

		switch(pOverlapped->operate_type)
		{
		case IO_TCP_ACCEPT: //连接完成
			{
				TCP_ACCEPT_CONTEXT* pTcpAcceptContext = (TCP_ACCEPT_CONTEXT*)pOverlapped;

				CompleteTcpAcceptOp(pTcpAcceptContext,data_len);
				
				IssueTcpReceiveOp(pTcpAcceptContext->accept_socket,2);//在新连接的socket上发送接受数据操作到OP,初始发送2等待接受数据的等待操作
				IssueTcpAcceptOp(pTcpAcceptContext);//投递新的连接操作到队列,保持队列中连接操作的平衡 
				break;
			}
		case IO_TCP_RECEIVE: //接受数据完成
			{
				TCP_RECEIVE_CONTEXT* pTcpReceiveContext = (TCP_RECEIVE_CONTEXT*)pOverlapped;
				if(data_len>0)
				{
				    CompleteTcpReceiveOp(pTcpReceiveContext,data_len);

					//下面的函数测试用，向client发送收到的数据
					//this->IssueTcpSendOp(pTcpReceiveContext->socket,pTcpReceiveContext->receive_buffer,data_len);	

					this->IssueTcpReceiveOp(pTcpReceiveContext); //发送新的接收操作到完成端口队列
				}
				else //client 退出
				{
					
					if( IsValidSocket(pTcpReceiveContext->socket))
					{	
						this->TcpCloseComplete(pTcpReceiveContext->socket) ;	
						this->CloseSocket(pTcpReceiveContext->socket) ;
						this->ReleaseContext(pOverlapped);									
					}					
				}
				break;
			}
		case IO_TCP_SEND:  //发送数据完成
			{
				TCP_SEND_CONTEXT* pTcpSendContext = (TCP_SEND_CONTEXT*)pOverlapped;
				CompleteTcpSendOp(pTcpSendContext,data_len);

				this->ReleaseContext(pOverlapped);
				break;
			}
		case IO_UDP_RECEIVE:
			{
				UDP_CONTEXT* pUdpContext = (UDP_CONTEXT*)pOverlapped;

				this->CompleteUdpReceiveOp(pUdpContext,data_len); //接收数据完成例程 

				//下面的函数作为测试用
				this->IssueUdpSendOp(pUdpContext->remote_address,pUdpContext->data_buffer,data_len,pUdpContext->socket); //向client发送收到的数据

				this->IssueUdpReceiveOp(pUdpContext);//投递另外一个接收操作，保持完成端口队列中的接收操作平衡   
				break;
			}
		case IO_UDP_SEND:
			{
				UDP_CONTEXT* pUdpContext = (UDP_CONTEXT*)pOverlapped;

				this->CompleteUdpSendOp(pUdpContext,data_len); //发送数据完成例程
				this->ReleaseContext(pOverlapped);
				break;
			}
		case IO_EXIT:
			{
				break;
			}	
		default:
			break;
		}
	}
	catch(CSrvException& e)
	{
		PrintDebugMessage(e);
		//if(m_bExceptionStop)
		//	throw e;
	}
	catch(...)
	{
		string message("CompleteIoProcess 发生未知异常.");
		PrintDebugMessage(message.c_str());
		//if(m_bExceptionStop)
		//	throw message;
	}
}

//释放context到内存池

void CIocpServer::ReleaseContext(IO_PER_HANDLE_STRUCT* pOverlapped)
{
	try
	{
		if(pOverlapped==NULL)
		{
			PrintDebugMessage("ReleaseContext(),指针pOverlapped非法");
		}
		
        switch(pOverlapped->operate_type)
		{
		case IO_TCP_ACCEPT:
			{
				TCP_ACCEPT_CONTEXT* pTcpAcceptContext = (TCP_ACCEPT_CONTEXT*)pOverlapped;
				m_pTcpAcceptContextManager->ReleaseContext(pTcpAcceptContext);
				break;
			}
		case IO_TCP_RECEIVE:
			{
				TCP_RECEIVE_CONTEXT* pTcpReceiveContext = (TCP_RECEIVE_CONTEXT*)pOverlapped;
				m_pTcpReceiveContextManager->ReleaseContext(pTcpReceiveContext);
				break;
			}
		case IO_TCP_SEND:
			{
				TCP_SEND_CONTEXT* pTcpSendContext = (TCP_SEND_CONTEXT*)pOverlapped;
				m_pTcpSendContextManager->ReleaseContext(pTcpSendContext);
				break;
			}
		case IO_UDP_RECEIVE:
			{
				UDP_CONTEXT* pUdpContext = (UDP_CONTEXT*)pOverlapped;
				m_pUdpContextManager->ReleaseContext(pUdpContext);
				break;
			}
		case IO_UDP_SEND:
			{
				UDP_CONTEXT* pUdpContext = (UDP_CONTEXT*)pOverlapped;
				m_pUdpContextManager->ReleaseContext(pUdpContext);
				break;
			}
		case IO_EXIT:
			{
				break;
			}	
		default:
			break;
		}
	}
	catch(CSrvException& e)
	{
		this->PrintDebugMessage(e);
		
	}
	catch(...)
	{
		string message("ReleaseContext 发生未知异常.");
		PrintDebugMessage(message.c_str());
		//if(m_bExceptionStop)
			//throw message;
	}
}
int CIocpServer::PostUdpReceiveOp(SOCKET socket,unsigned short numbers)
{
	int counter=0;
	for(int i=0;i<numbers;i++)
       counter += IssueUdpReceiveOp(socket);
	
	return counter;
}
int CIocpServer::IssueUdpReceiveOp(SOCKET socket)
{
	try
	{
		if(m_pUdpContextManager==NULL)
            PrintDebugMessage("指针m_pUdpContextManager非法");

		int errorCode = 1;
		unsigned long dwPostReceiveNumbs = 0;
		
		UDP_CONTEXT* pUdpContext = NULL;
		m_pUdpContextManager->GetContext(pUdpContext);
		
		if(pUdpContext==NULL)
			PrintDebugMessage("pUdpContext指针非法.");

		pUdpContext->socket = socket;  //设置套接字 
		
		return IssueUdpReceiveOp(pUdpContext);
	}
	catch(CSrvException& e)
	{
		this->PrintDebugMessage(e);
		
	}
	catch(...)
	{
		string message("IssueUdpReceiveOp 发生未知异常.");
		PrintDebugMessage(message.c_str());
		//if(m_bExceptionStop)
			//throw message;
	}
	return 0;
}
//内部测试函数
int CIocpServer::IssueUdpReceiveOp(UDP_CONTEXT* pUdpContext)
{
	try
	{
		if(m_bStopServer)
		{
			ReleaseContext((IO_PER_HANDLE_STRUCT*)pUdpContext);
			return FALSE;
		}
		
		int errorCode = 1;
		unsigned long dwPostReceiveNumbs = 0;
		
		if(pUdpContext==NULL)
			throw CSrvException("pUdpContext指针非法.",-1);
		
		unsigned long dwFlag = 0;
		unsigned long dwBytes = 0;

	    ZeroMemory(&pUdpContext->operate_overlapped,sizeof(WSAOVERLAPPED));
		pUdpContext->operate_type = IO_UDP_RECEIVE;
        //pUdpContext->listen_socket = m_hUdpServerSocket;
		pUdpContext->operate_buffer.buf = pUdpContext->data_buffer;
		pUdpContext->operate_buffer.len = DEFAULT_UDP_BUFFER_SIZE;
        ZeroMemory(&pUdpContext->remote_address,sizeof(struct sockaddr_in));
		pUdpContext->remote_address_len = sizeof(struct sockaddr);


		//发送接收operation到完成端口队列中,此处一定是异步操作
		errorCode = WSARecvFrom(
			pUdpContext->socket,
			&pUdpContext->operate_buffer,
			1,
			&dwBytes,
			&dwFlag,
			(struct sockaddr*)&pUdpContext->remote_address,
			&pUdpContext->remote_address_len,
			&pUdpContext->operate_overlapped,
			NULL);
	  
		if (SOCKET_ERROR == errorCode && ERROR_IO_PENDING != WSAGetLastError())		
		{
			//if(WSAECONNRESET !=WSAGetLastError())//当远方关闭后，因为ICMP协议关系，会出现此错误

			PrintDebugMessage("WSARecvFrom()函数发生错误.");
		}
		return TRUE;
	}
	catch(CSrvException& e)
	{
		ReleaseContext((IO_PER_HANDLE_STRUCT*)pUdpContext);
		this->PrintDebugMessage(e);
		if(m_bExceptionStop)
			throw e;
	}
	catch(...)
	{
		string message("IssueUdpReceiveOp 发生未知异常.");
		PrintDebugMessage(message.c_str());
		//if(m_bExceptionStop)
			//throw message;
	}
	return FALSE;
}
//UDP接收完成函数
//但会进行帧头及帧尾的检测,如果数据传输有误，将会检测出来(如果真实数据发生错误，将由高层检测出来)
void CIocpServer::CompleteUdpReceiveOp(UDP_CONTEXT* pUdpContext,unsigned long data_len)
{	
	//此处发送给高层的数据已经去掉了帧头
	if(m_pUdpRevCallbackFuc) //执行回调函数
	{
		m_pUdpRevCallbackFuc
		(inet_ntoa(pUdpContext->remote_address.sin_addr),
		ntohs(pUdpContext->remote_address.sin_port),
		pUdpContext->data_buffer,
		data_len,
		pUdpContext->socket
		);
	}
	
	this->UdpReceiveComplete
		(
		pUdpContext->remote_address,
		pUdpContext->data_buffer,
		data_len,
		pUdpContext->socket
		);
}
//数据异步发送函数
//对数据长度大于DEFAULT_UDP_BUFFER_SIZE - 6的数据会进行分包处理
int CIocpServer::IssueUdpSendOp(struct sockaddr_in& remoteAddr,char* buf,unsigned long data_len,SOCKET socket)
{
	try
	{
		if(m_bStopServer) 
		{
			return FALSE;
		}

		if(m_pUdpContextManager==NULL)
			PrintDebugMessage("IssueUdpSendOp m_pUdpContextManager指针非法.");

			//下面的循环是把数据块分包
			unsigned long leave_bytes = data_len;
			unsigned long trans_bytes = data_len;
			unsigned short counter = 0;
			unsigned short data_start_index = 0;  //数据真实的起始位

		
			do
			{
				
				UDP_CONTEXT* pUdpContext = NULL;
				m_pUdpContextManager->GetContext(pUdpContext);
				
				if(pUdpContext==NULL)
					PrintDebugMessage("pUdpContext指针非法.");

				if(leave_bytes <=DEFAULT_UDP_BUFFER_SIZE) //小于一次发送量
					trans_bytes = leave_bytes;
				else
					trans_bytes = DEFAULT_UDP_BUFFER_SIZE;
				
				leave_bytes = leave_bytes - trans_bytes; //一次发送后剩余的字节数
				
				memcpy(pUdpContext->data_buffer,buf+counter*DEFAULT_UDP_BUFFER_SIZE,trans_bytes);  //拷贝字节数
				counter++;

				ZeroMemory(&pUdpContext->operate_overlapped,sizeof(WSAOVERLAPPED));
				pUdpContext->operate_type = IO_UDP_SEND;
				pUdpContext->socket = socket;
				pUdpContext->operate_buffer.buf = pUdpContext->data_buffer;
				pUdpContext->operate_buffer.len = trans_bytes;//注意此处的6
				pUdpContext->remote_address = remoteAddr;
				pUdpContext->remote_address_len = sizeof(struct sockaddr);
				
				DWORD nubmerBytes = 0;
				int err = WSASendTo(
					pUdpContext->socket,
					&pUdpContext->operate_buffer,
					1,
					&nubmerBytes,
					0,
					(struct sockaddr*)&pUdpContext->remote_address,
					sizeof(pUdpContext->remote_address),
					&pUdpContext->operate_overlapped,
					NULL);
				if(SOCKET_ERROR == err && WSA_IO_PENDING != WSAGetLastError())
				{
					ReleaseContext((IO_PER_HANDLE_STRUCT*)pUdpContext);
					PrintDebugMessage("IssueUdpSendOp->WSASend发生错误.");
				}
				else if(0 == err)
				{
					//还要考虑立即完成的情况
				}
			}while(leave_bytes>0);

		return TRUE;
	}
	catch(CSrvException& e)
	{
		this->PrintDebugMessage(e);
		//if(m_bExceptionStop)
			//throw e;
	}
	catch(...)
	{
		string message("IssueUdpSendOp 发生未知异常.");
		PrintDebugMessage(message.c_str());
		if(m_bExceptionStop)
			throw message;
	}
	return FALSE;
}
//下面的两个函数应该放在高层
/*
unsigned short CIocpServer::AddHead(char* data,unsigned short data_len,unsigned short pack_index)   //在udp数据包中加上帧头
{

	    WORD index = 0;
		//封包格式,封包共占6个字节
		data[index++] =  (BYTE)PACKET_START1;   //0xAB
		data[index++] = (BYTE)PACKET_START2;   //0xCD
		//*PWORD(&data[index]) = (WORD)pack_index;	//数据包index
		//index += 2;
		*PWORD(&data[index]) = (WORD)data_len;	//数据长度,2个字节
		index += 2;
		index += data_len;                    //跳过真实数据区域 
		data[index++] = (BYTE)PACKET_END1;	  //0xDC
		data[index++] = (BYTE)PACKET_END2;	  //0xBA

		return index ;
}
int CIocpServer::CheckHead(char* data,unsigned short data_len,PPACKET_HEAD& pPacketHead) //检udp数据包,看数据是否合法
{
	if(data_len <= PACKET_HEAD_SIZE)	//表明是空数据,肯定有错误
		return FALSE;

	PPACKET_HEAD _pPackeHead = (PPACKET_HEAD)data;	//得到封包的头

	if( _pPackeHead->fram_head != ((DWORD)PACKET_START2<<8) + PACKET_START1 )//帧头判断
		return FALSE;

	if( _pPackeHead->data_len+PACKET_HEAD_SIZE != data_len)	   //pChenkSum->Length是真实数据的数据长度, +6是服务器收到数据包长度
		return FALSE;

	if( *PWORD(&data[_pPackeHead->data_len+PACKET_HEAD_SIZE-2]) != (((DWORD)PACKET_END2<<8) + PACKET_END1) ) //最后两字节是否是帧尾
		return FALSE;

	pPacketHead = _pPackeHead;

	return TRUE;
}
*/
//发送一个udp数据报完成例程
void CIocpServer::CompleteUdpSendOp(UDP_CONTEXT* pUdpContext,unsigned long data_len)
{
	if(m_pUdpSendCallbackFuc)  //如果设置了回调函数,则执行毁掉函数
	{
		m_pUdpSendCallbackFuc
			(
			inet_ntoa(pUdpContext->remote_address.sin_addr),
			ntohs(pUdpContext->remote_address.sin_port),
			pUdpContext->data_buffer,
			data_len,
			pUdpContext->socket
			);
	}

		this->UdpSendComplete
			(
			inet_ntoa(pUdpContext->remote_address.sin_addr),
			ntohs(pUdpContext->remote_address.sin_port),
			pUdpContext->data_buffer,
			data_len,
			pUdpContext->socket
			);
}

//向完成端口中投递新的accept操作
int CIocpServer::PostTcpAcceptOp(unsigned short accept_numbers)
{
	try
	{ 
		unsigned short counter = 0;
		for(int i=0;i<accept_numbers;i++)
			counter+=IssueTcpAcceptOp();
		if(counter != accept_numbers)
			PrintDebugMessage("投递accept有错误发生,");

		return TRUE;
	}
	catch(CSrvException& e)
	{
		this->PrintDebugMessage(e);
		
	}
	catch(...)
	{
		string message("PostTcpAcceptOp 发生未知异常.");
		PrintDebugMessage(message.c_str());
		//if(m_bExceptionStop)
			//throw message;
	}
	return FALSE;
}
int CIocpServer::IssueTcpAcceptOp()
{
	try
	{
		if(m_pTcpAcceptContextManager==NULL)
		    PrintDebugMessage("指针m_pTcpAcceptContextManager非法.");

		TCP_ACCEPT_CONTEXT* pAccContext = NULL;
		m_pTcpAcceptContextManager->GetContext(pAccContext);
		if (NULL == pAccContext)
		{
		    PrintDebugMessage("指针pAccContext");
		}

		ZeroMemory(&pAccContext->operate_overlapped,sizeof(WSAOVERLAPPED));
		pAccContext->operate_type = IO_TCP_ACCEPT;
		pAccContext->listen_socket = this->m_hTcpServerSocket;
        pAccContext->accept_socket = NULL;
		ZeroMemory(pAccContext->remoteAddressBuffer,ADDRESS_LENGTH*2);

        return this->IssueTcpAcceptOp(pAccContext);
	}
	catch(CSrvException& e)
	{
		this->PrintDebugMessage(e);
		
	}
	catch(...)
	{
		string message("IssueTcpAcceptOp 发生未知异常.");
		PrintDebugMessage(message.c_str());
		//if(m_bExceptionStop)
			//throw message;
	}
	return FALSE;
}
int CIocpServer::IssueTcpAcceptOp(TCP_ACCEPT_CONTEXT* pContext)
{
	SOCKET hClientSocket = NULL;
	try
	{
		if(pContext==NULL)
			PrintDebugMessage("IssueTcpAcceptOp(),指针pContext非法.");


		int errorCode = 1;
		hClientSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
		if (INVALID_SOCKET == hClientSocket)
		{
			PrintDebugMessage("IssueTcpAcceptOp()->WSASocket(),Invalid socket handle.");
		}

		//设置为异步模式
		ULONG ul = 1;
		errorCode =ioctlsocket(hClientSocket, FIONBIO, &ul);
		if(SOCKET_ERROR == errorCode)
		{
			PrintDebugMessage("IssueTcpAcceptOp()->ioctlsocket(),incorrect ioctlsocket operation.");
		}

		long accTime = -1; //此标志是区别于连接的socket还是非连接的socket关键
		errorCode = setsockopt(hClientSocket, SOL_SOCKET, SO_GROUP_PRIORITY, (char *)&accTime, sizeof(accTime));
		if(SOCKET_ERROR == errorCode)
		{
			PrintDebugMessage("IssueTcpAcceptOp()->setsockopt().");
		}

		ZeroMemory(&pContext->operate_overlapped,sizeof(WSAOVERLAPPED));
		pContext->operate_type = IO_TCP_ACCEPT;
		pContext->listen_socket = this->m_hTcpServerSocket;
		pContext->accept_socket = hClientSocket;
		ZeroMemory(pContext->remoteAddressBuffer,ADDRESS_LENGTH*2);


		unsigned long dwBytes = 0;
		unsigned long dwAcceptNumbs = 0;

		//向完成端口投递accept操作
		errorCode = m_pfAcceptEx(
			pContext->listen_socket,
			pContext->accept_socket, 
			pContext->remoteAddressBuffer,
			0,
			ADDRESS_LENGTH, 
			ADDRESS_LENGTH,
			&dwBytes,
			&(pContext->operate_overlapped));
		if(FALSE == errorCode && ERROR_IO_PENDING != WSAGetLastError())
		{
			ReleaseContext((IO_PER_HANDLE_STRUCT*)pContext);
			PrintDebugMessage("IssueTcpAcceptOp()->AcceptEx,incorrect AcceptEx operation.");
		}

		
		this->SaveConnectSocket(hClientSocket); //把socket放入链表进行管理

		return TRUE;
	}
	catch(CSrvException& e)
	{
		if(hClientSocket != INVALID_SOCKET)
		{
			CloseSocket(hClientSocket);
		}
		this->PrintDebugMessage(e);
		//if(m_bExceptionStop)
			//throw e;
	}
	catch(...)
	{
		string message("IssueTcpAcceptOp 发生未知异常.");
		PrintDebugMessage(message.c_str());
		//if(m_bExceptionStop)
			//throw message;
	}
	return FALSE;
}

//新到连接处理函数
void CIocpServer::CompleteTcpAcceptOp(TCP_ACCEPT_CONTEXT* pContext,unsigned long data_len)
{
	try
	{
		if(pContext == NULL)
			PrintDebugMessage("CompleteTcpAcceptOp error,指针pContext非法.");

		this->GetNewConnectInf(pContext);   //得到新连接socket的信息

		if(m_pTcpAcceptCallbackFuc)
			m_pTcpAcceptCallbackFuc(pContext->accept_socket,pContext->remoteAddressBuffer,ADDRESS_LENGTH*2);
		//下面是修改相应socket的参数
		this->SetNewSocketParameters(pContext->accept_socket);

		
	}
	catch(CSrvException& e)
	{
		this->ReleaseContext((IO_PER_HANDLE_STRUCT*)pContext);
		this->PrintDebugMessage(e);
		if(m_bExceptionStop)
			throw e;
	}
	catch(...)
	{
		string message("CompleteTcpAcceptOp 发生未知异常.");
		PrintDebugMessage(message.c_str());
		if(m_bExceptionStop)
			throw message;
	}
}
//功能
//1 设置新联入socket的参数,只针对tcp
//2 把设置完参数的socket放入到一个map中进行集中管理
void CIocpServer::SetNewSocketParameters(SOCKET socket)
{
	try
	{
		int errorCode = 0;
		int nZero=0;
		errorCode = setsockopt(socket,SOL_SOCKET,SO_SNDBUF,(char *)&nZero,sizeof(nZero));
		if(errorCode == SOCKET_ERROR)
		{
			PrintDebugMessage("CompleteTcpAcceptOp->setsockopt() error..");
		}

		nZero = DEFAULT_TCP_RECEIVE_BUFFER_SIZE;
		errorCode = setsockopt(socket,SOL_SOCKET,SO_RCVBUF,(char *)&nZero,sizeof(nZero));
		if(errorCode == SOCKET_ERROR)
		{
			PrintDebugMessage("CompleteTcpAcceptOp->setsockopt() error..");
		}
		int nOpt = 1;//socket重用,在出现TIME_WAIT状态的时候重用该socket
		errorCode = setsockopt(socket,SOL_SOCKET ,SO_REUSEADDR,(const char*)&nOpt,sizeof(nOpt));
		if(errorCode == SOCKET_ERROR)
		{
			PrintDebugMessage("CompleteTcpAcceptOp->setsockopt() error..");
		}
		int dontLinget = 1;//服务器关闭socket的时候,不执行正常的四步握手关闭，而是执行RESET
		errorCode = setsockopt(socket,SOL_SOCKET,SO_DONTLINGER,(char *)&dontLinget,sizeof(dontLinget)); 
		if(errorCode == SOCKET_ERROR)
		{
			PrintDebugMessage("CompleteTcpAcceptOp->setsockopt() error..");
		}
		char chOpt=1; //发送的时候关闭Nagle算法,关闭nagel算法有可能会引起断流(压缩)
		errorCode = setsockopt(socket,IPPROTO_TCP,TCP_NODELAY,&chOpt,sizeof(char));   
		if(errorCode == SOCKET_ERROR)
		{
			PrintDebugMessage("CompleteTcpAcceptOp->setsockopt() error..");
		}

		//更新当前socket活动的时间,用于超时检测
		long accTime = ::InterlockedExchangeAdd(&m_iCurrentTime,0);
		errorCode = setsockopt(socket, SOL_SOCKET, SO_GROUP_PRIORITY, (char *)&accTime, sizeof(accTime));
		if(errorCode == SOCKET_ERROR)
		{
			PrintDebugMessage("CompleteTcpAcceptOp->setsockopt() error..");
		}
		if(!this->SetHeartBeatCheck(socket))//设置心跳包参数
		{
			PrintDebugMessage("CompleteTcpAcceptOp->setsockopt() error..");
		}
		//把新的socket与完成端口绑定
		if(NULL == CreateIoCompletionPort((HANDLE)socket,this->m_hCompletionPort,(ULONG_PTR)socket,0))
		{
			PrintDebugMessage("CompleteTcpAcceptOp->setsockopt() error..");
		}
	}
	catch(CSrvException& e)
	{
		/**
		 *	防止目标节点不在线仍发送数据
		 */
	
		this->PrintDebugMessage(e);
		if(m_bExceptionStop)
			throw e.GetExpCode();
	}
	catch(...)
	{
		string message("SetNewSocketParameters 发生未知异常.");
		PrintDebugMessage(message.c_str());
		//if(m_bExceptionStop)
			//throw message;
	}
}
//显示新连接client信息
void CIocpServer::GetNewConnectInf(TCP_ACCEPT_CONTEXT* pContext)
{
	//在客户端发起高速连接时候,如果开启该功能，会出现后面的IssueReceiveOperation中报10057错误,而此函数输出可看到没有正确解析出客户端信息
	sockaddr_in* pClientAddr = NULL;
	sockaddr_in* pLocalAddr = NULL;
	int nClientAddrLen = 0;
	int nLocalAddrLen = 0;

	m_pfGetAddrs(pContext->remoteAddressBuffer, 0,ADDRESS_LENGTH,ADDRESS_LENGTH,
		(LPSOCKADDR*)&pLocalAddr, &nLocalAddrLen, (LPSOCKADDR*)&pClientAddr, &nClientAddrLen);

	//printf("客户端: %s : %u 连接服务器成功.\n",inet_ntoa(pClientAddr->sin_addr),ntohs(pClientAddr->sin_port));

	this->TcpAcceptComplete(pContext->accept_socket,inet_ntoa(pClientAddr->sin_addr),ntohs(pClientAddr->sin_port));
								  
}
int CIocpServer::IssueTcpReceiveOp(SOCKET socket,unsigned short times/*=1*/)
{  
	TCP_RECEIVE_CONTEXT* pContext = NULL;
	try
	{
		if(m_pTcpReceiveContextManager==NULL)
			PrintDebugMessage("IssueTcpReceiveOp(),m_pTcpReceiveContextManager指针非法.");

		for(int i=0;i<times;i++)
		{
			m_pTcpReceiveContextManager->GetContext(pContext);
			if(pContext==NULL)
			{
				PrintDebugMessage("IssueTcpReceiveOp,取对象pContext失败.");
			}

			ZeroMemory(&pContext->operate_overlapped,sizeof(WSAOVERLAPPED));
			pContext->operate_type = IO_TCP_RECEIVE;
			pContext->socket = socket;
			pContext->operate_buffer.buf = pContext->receive_buffer;
			pContext->operate_buffer.len = DEFAULT_TCP_RECEIVE_BUFFER_SIZE;

			if(!this->IssueTcpReceiveOp(pContext))
				 PrintDebugMessage("IssueTcpReceiveOp,投递receive 操作失败.");

		}
		return TRUE;
	}
	catch(CSrvException& e)
	{
		this->ReleaseContext((IO_PER_HANDLE_STRUCT*)pContext);
		this->PrintDebugMessage(e);
		if(m_bExceptionStop)
			throw e;
	}
	catch(...)
	{
		string message("IssueTcpReceiveOp 发生未知异常.");
		PrintDebugMessage(message.c_str());
		//if(m_bExceptionStop)
			//throw message;
	}
	return FALSE;
}
int CIocpServer::IssueTcpReceiveOp(TCP_RECEIVE_CONTEXT* pContext)
{
	try
	{
		unsigned long bytes = 0;
		unsigned long flag = 0;
		int err = WSARecv(pContext->socket,&pContext->operate_buffer,1,&bytes,&flag, &(pContext->operate_overlapped), NULL);
		if (SOCKET_ERROR == err && WSA_IO_PENDING != WSAGetLastError())
		{
			PrintDebugMessage("IssueTcpReceiveOp->WSARecv() error.");
		}
		return TRUE;
	}
	catch(CSrvException& e)
	{
		this->PrintDebugMessage(e);
		//if(m_bExceptionStop)
			//throw e;
	}
	catch(...)
	{
		string message("IssueTcpReceiveOp 发生未知异常.");
		PrintDebugMessage(message.c_str());
		//if(m_bExceptionStop)
			//throw message;
	}
	return FALSE;
}
void CIocpServer::CompleteTcpReceiveOp(TCP_RECEIVE_CONTEXT* pContext,unsigned long data_len)
{
	long accTime = ::InterlockedExchangeAdd(&m_iCurrentTime,0);
	setsockopt(pContext->socket, SOL_SOCKET, SO_GROUP_PRIORITY, (char *)&accTime, sizeof(accTime));
	
	if(m_pTcpRevCallbackFuc)
        m_pTcpRevCallbackFuc(pContext->socket,pContext->receive_buffer,data_len);

	this->TcpReceiveComplete(pContext->socket,pContext->receive_buffer,data_len);
}

int CIocpServer::IssueTcpSendOp(SOCKET client,char* buf,unsigned long data_len)
{
	try
	{//下面的循环是把数据块分包
		unsigned long leave_bytes = data_len;
		unsigned long trans_bytes = data_len;
		unsigned long counter = 0;
		do
		{

			TCP_SEND_CONTEXT* pContext = NULL;
			m_pTcpSendContextManager->GetContext(pContext);

			if(pContext==NULL)
			{
				return 0;
				//throw CSrvException("IssueTcpSendOp(),pContext指针非法.",-1);
			}

			//ZeroMemory(pUdpContext,sizeof(UDP_CONTEXT));


			if(leave_bytes <=DEFAULT_TCP_SEND_BUFFER_SIZE) //小于一次发送量
				trans_bytes = leave_bytes;
			else
				trans_bytes = DEFAULT_TCP_SEND_BUFFER_SIZE;

			leave_bytes = leave_bytes - trans_bytes; //一次发送后剩余的字节数

			memcpy(pContext->send_buffer,buf+counter*DEFAULT_TCP_SEND_BUFFER_SIZE,trans_bytes);  //拷贝字节数
			counter++;

			ZeroMemory(&pContext->operate_overlapped,sizeof(WSAOVERLAPPED));
			pContext->operate_type = IO_TCP_SEND;
			pContext->socket = client;
			pContext->operate_buffer.buf = pContext->send_buffer;
			pContext->operate_buffer.len = trans_bytes;

			DWORD nubmerBytes = 0;
			int err = WSASend(pContext->socket,&pContext->operate_buffer,1,&nubmerBytes,0,&pContext->operate_overlapped,NULL);
			if(SOCKET_ERROR == err && WSA_IO_PENDING != WSAGetLastError())
			{
				/**
				 *	异常掉线，发送会出异常
				 */
				this->ReleaseContext((IO_PER_HANDLE_STRUCT*)pContext);
				PrintDebugMessage("IssueTcpSendOp->WSASend occured an error.1111");
			}
			else if(0 == err)
			{
				;//还要考虑立即完成的情况,立即完成如果完成端口上的线程
			}
		}
		while(leave_bytes>0);

		return TRUE;
	}
	catch(CSrvException& e)
	{
		//PrintDebugMessage(e);
		throw e; 
		
		//if(m_bExceptionStop)
			//throw e;
	}
	catch(...)
	{
		string message("IssueTcpSendOp 发生未知异常.");
		PrintDebugMessage(message.c_str());
	}
	return FALSE;
}

void CIocpServer::CompleteTcpSendOp(TCP_SEND_CONTEXT* pContext,unsigned long data_len)
{
	long accTime = ::InterlockedExchangeAdd(&m_iCurrentTime,0);
	setsockopt(pContext->socket, SOL_SOCKET, SO_GROUP_PRIORITY, (char *)&accTime, sizeof(accTime));

	if(m_pTcpSendCallbackFuc)
		m_pTcpSendCallbackFuc(pContext->socket,pContext->send_buffer,data_len);

	this->TcpSendComplete(pContext->socket,pContext->send_buffer,data_len);
}

//外部数据异步发送函数
//如果socket == NULL,则表示是服务器端向client发送数据
//如果socket是关联的socket,则可能是客户端,也可能是服务器端
int CIocpServer::UdpSend(char* remote_ip,unsigned short remote_port,char* buf,unsigned long data_len,SOCKET socket/*=NULL*/)
{
	try
	{
		if(remote_ip==NULL)
			PrintDebugMessage("客户端ip地址错误.");
		if(data_len==0)
			PrintDebugMessage("发送字节数为0.");

		struct sockaddr_in remote_address;
		remote_address.sin_family = AF_INET;
		remote_address.sin_addr.s_addr = inet_addr(remote_ip);
		remote_address.sin_port = htons(remote_port);

		SOCKET temp_socket = socket;
		if(socket == NULL)   //默认是通过udp服务器的socket发送数据
			temp_socket = this->m_hUdpServerSocket;
		if(temp_socket == NULL)
			PrintDebugMessage("在发送数据时需要指定socket句柄.");

        if(IssueUdpSendOp(remote_address,buf,data_len,temp_socket)==FALSE) //发送数据
			PrintDebugMessage("发送数据失败.");

		return TRUE;
	}
	catch(CSrvException& e)
	{
		this->PrintDebugMessage(e);
		if(m_bExceptionStop)
			throw e.GetExpCode();
	}
	catch(...)
	{
		string message("UdpSend 发生未知异常.");
		PrintDebugMessage(message.c_str());
		//if(m_bExceptionStop)
			//throw message;
	}
	return FALSE;
}
//供外部使用的tcp数据发送函数
int CIocpServer::TcpSend(SOCKET client,char* buf,unsigned long data_len)
{
try
	{
		if(client==NULL)
			PrintDebugMessage("客户端socket错误.");
		if(data_len==0)
			PrintDebugMessage("发送字节数为0.");

        if(IssueTcpSendOp(client,buf,data_len)==FALSE) //发送数据
			PrintDebugMessage("发送数据失败.");

		return TRUE;
	}
	catch(CSrvException& e)
	{
		if( IsValidSocket( client ) )
		{
			TcpCloseComplete(client) ;	
			CloseSocket(client);
		}	
		this->PrintDebugMessage(e);
	}
	catch(...)
	{
		string message("TcpSend 发生未知异常.");
		PrintDebugMessage(message.c_str());
		//if(m_bExceptionStop)
			//throw message;
	}
	return FALSE;
}
int CIocpServer::Run()
{
	try
	{
		if(m_bServerIsRunning)
			PrintDebugMessage("服务器已经运行.");

		CreateThread(); //创建相关的工作线程
        
		m_bServerIsRunning = true;
		return TRUE;
	}
	catch(CSrvException& e)
	{
		this->PrintDebugMessage(e,false);
		//if(m_bExceptionStop)
		//	throw e;
	}
	catch(...)
	{
		string message("Run 发生未知异常.");
		PrintDebugMessage(message.c_str());
		//if(m_bExceptionStop)
			//throw message;
	}
	return FALSE;
}
int CIocpServer::Stop()
{
	try
	{
		if(!m_bInitOk)
            PrintDebugMessage("服务器初始化失败,无法启动服务器.");
		if(!m_bServerIsRunning)
			PrintDebugMessage("服务器已经停止.");
		
		this->ReleaseResource();

		m_bInitOk = true;
		m_bStopServer = false;
		m_bServerIsRunning = false;

		return TRUE;
	}
	catch(CSrvException& e)
	{
		this->PrintDebugMessage(e);
		//if(m_bExceptionStop)
		//	throw e;
	}
	catch(...)
	{
		string message("Stop 发生未知异常.");
		PrintDebugMessage(message.c_str());
		//if(m_bExceptionStop)
		//	throw message;
	}
	return FALSE;
}
void PASCAL CIocpServer::TimerProc(UINT wTimerID,UINT msg,DWORD dwUser,DWORD dw1,DWORD dw2)
{
	
 	CIocpServer* p = (CIocpServer*)dwUser;
 
 	p->m_iCurrentTime = (long)time(NULL); 
// 
 	p->ConnectTimeOutCheck(); //联接超时检测
 	p->TimerSecond() ; 
//    
// 	long contextSize=0;
// 	long busyContext =0;
// 
// 	//printf("IO op counter:%ld\n",p->m_iOpCounter);
// 
// 	if(p->m_pUdpContextManager)
// 	{
// 		p->m_pUdpContextManager->GetSize(contextSize);
// 		p->m_pUdpContextManager->GetBusyCounter(busyContext);
// 		//printf("udp context:%ld,udp busy context:%ld\n",contextSize,busyContext);
// 	}
// 
// 	if(p->m_pTcpAcceptContextManager)
// 	{
// 		p->m_pTcpAcceptContextManager->GetSize(contextSize);
// 		p->m_pTcpAcceptContextManager->GetBusyCounter(busyContext);
// 		//printf("tcp accept context:%ld,tcp accept busy context:%ld\n",contextSize,busyContext);
// 	}
// 	if(p->m_pTcpReceiveContextManager)
// 	{
// 		p->m_pTcpReceiveContextManager->GetSize(contextSize);
// 		p->m_pTcpReceiveContextManager->GetBusyCounter(busyContext);
// 		//printf("tcp receive context:%ld,tcp receive busy context:%ld\n",contextSize,busyContext);
// 	}
// 
// 	if(p->m_pTcpSendContextManager)
// 	{
// 		p->m_pTcpSendContextManager->GetSize(contextSize);
// 		p->m_pTcpSendContextManager->GetBusyCounter(busyContext);
// 		//printf("tcp send context:%ld,tcp send busy context:%ld\n\n",contextSize,busyContext);
// 	}

}
//释放分配了的资源
void CIocpServer::ReleaseResource()
{
	try
	{
	m_bStopServer = true;
	timeKillEvent(m_iTimerId);
	m_iTimerId = 0;

	CloseAllSocket(); //关闭所有socket,包括活动和非活动的,会使完成端口中的所有排队的操作返回

	if(m_hUdpServerSocket)
	{
		CancelIo((HANDLE)m_hUdpServerSocket);
		shutdown(m_hUdpServerSocket,SD_BOTH); 
		closesocket(m_hUdpServerSocket);
		m_hUdpServerSocket = NULL;
	}
	if(m_hTcpServerSocket)
	{
		CancelIo((HANDLE)m_hTcpServerSocket);
		shutdown(m_hTcpServerSocket,SD_BOTH); 
		closesocket(m_hTcpServerSocket);
		m_hTcpServerSocket = NULL;
	}
	if(m_hTcpClientSocket)
	{
		CancelIo((HANDLE)m_hTcpClientSocket);
		shutdown(m_hTcpClientSocket,SD_BOTH); 
		closesocket(m_hTcpClientSocket);
		m_hTcpClientSocket = NULL;
	}
	if(m_hUdpClientSocket)
	{
		CancelIo((HANDLE)m_hUdpClientSocket);
		shutdown(m_hUdpClientSocket,SD_BOTH); 
		closesocket(m_hUdpClientSocket);
		m_hUdpClientSocket = NULL;
	}

    Sleep(1000);

	for(int i=0;i<m_iWorkerThreadNumbers;i++)
		PostQueuedCompletionStatus(m_hCompletionPort,0,IO_EXIT,NULL);

	WaitForMultipleObjects(m_iWorkerThreadNumbers,m_hWorkThread,TRUE,INFINITE);
	for(int i=0;i<m_iWorkerThreadNumbers;i++)
	{
		CloseHandle(m_hWorkThread[i]);
		m_hWorkThread[i] = NULL;
	}

	if(m_hListenThread)
	{
		SetEvent(m_hPostAcceptEvent);
		WaitForSingleObject(m_hListenThread,INFINITE);
		CloseHandle(m_hListenThread);
		m_hListenThread = NULL;
	}

	long busyCounter = 0;
	if(m_pUdpContextManager)
	{	
		m_pUdpContextManager->GetBusyCounter(busyCounter);
		if(busyCounter != 0)
		   PrintDebugMessage("m_pUdpContextManager 对象有内存泄漏.",busyCounter);

		delete m_pUdpContextManager;
		m_pUdpContextManager = NULL;
	}
	if(m_pTcpAcceptContextManager)
	{
		m_pTcpAcceptContextManager->GetBusyCounter(busyCounter);
		if(busyCounter != 0)
		   PrintDebugMessage("m_pTcpAcceptContextManager 对象有内存泄漏.",busyCounter);

		delete m_pTcpAcceptContextManager;
		m_pTcpAcceptContextManager = NULL;
	}
	if(m_pTcpReceiveContextManager)
	{
		m_pTcpReceiveContextManager->GetBusyCounter(busyCounter);
		if(busyCounter != 0)
		   PrintDebugMessage("m_pTcpReceiveContextManager 对象有内存泄漏.",busyCounter);

		delete m_pTcpReceiveContextManager;
		m_pTcpReceiveContextManager = NULL;
	}
	if(m_pTcpSendContextManager)
	{
		m_pTcpSendContextManager->GetBusyCounter(busyCounter);
		if(busyCounter != 0)
		   PrintDebugMessage("m_pTcpSendContextManager 对象有内存泄漏.",busyCounter);

		delete m_pTcpSendContextManager;
		m_pTcpSendContextManager = NULL;
	}
	}
	catch(CSrvException& e)
	{
		PrintDebugMessage(e);
		if(m_bExceptionStop)
			throw e;
	}
	catch(...)
	{
		string message("ReleaseResource 发生未知异常.");
		PrintDebugMessage(message.c_str());
		//if(m_bExceptionStop)
			//throw message;
	}
}
int CIocpServer::SaveConnectSocket(SOCKET socket)
{
	int rc = 0;
	::EnterCriticalSection(&m_struCriSec);

	rc = (int)m_mapAcceptSockQueue.size();
	if(m_mapAcceptSockQueue.insert(std::make_pair(socket,rc++)).second)
		rc = 1;
	else
		rc = 0;
	LeaveCriticalSection(&m_struCriSec);
	return rc;
}
bool CIocpServer::IsValidSocket(SOCKET socket)
{
	bool validSocket = false;
	::EnterCriticalSection(&this->m_struCriSec);
	SOCKET sock = NULL;
	map<SOCKET,long>::iterator pos;
	pos = m_mapAcceptSockQueue.find(socket);
	if(pos!=m_mapAcceptSockQueue.end())
	{
		validSocket = true;
	}
	::LeaveCriticalSection(&this->m_struCriSec);
	return validSocket;
}
//关闭socket
void CIocpServer::CloseSocket(SOCKET socket)
{
	EnterCriticalSection(&this->m_struCriSec);
	SOCKET sock = NULL;
	map<SOCKET,long>::iterator pos;
	pos = m_mapAcceptSockQueue.find(socket);
	if(pos!=m_mapAcceptSockQueue.end())
	{
		sock = pos->first;
		m_mapAcceptSockQueue.erase(pos);
	}

	if(sock)
	{
	    CancelIo((HANDLE)sock);
		shutdown(sock,SD_BOTH); 
		closesocket(sock);//SO_UPDATE_ACCEPT_CONTEXT 参数会导致出现TIME_WAIT,即使设置了DONT
	} 

	::LeaveCriticalSection(&this->m_struCriSec);
}
void CIocpServer::CloseAllSocket()//关闭在完成端口上进行操作的所有socket
{
   EnterCriticalSection(&m_struCriSec);

   map<SOCKET,long>::iterator pos;
   for(pos = m_mapAcceptSockQueue.begin();pos!=m_mapAcceptSockQueue.end();)
   {
	   CancelIo((HANDLE)pos->first);
	   shutdown(pos->first,SD_BOTH); 
	   closesocket(pos->first);
	   m_mapAcceptSockQueue.erase(pos++);
   }

   LeaveCriticalSection(&m_struCriSec);
}
//检测突然掉线的socket超时时间
void CIocpServer::ConnectTimeOutCheck()
{
	if(TryEnterCriticalSection(&m_struCriSec))
	{

		map<SOCKET,long>::iterator pos;
		int iIdleTime = 0;
		int nTimeLen = 0;
		for(pos = m_mapAcceptSockQueue.begin();pos!=m_mapAcceptSockQueue.end();)
		{
			nTimeLen = sizeof(iIdleTime);
			//getsockopt(pos->first,SOL_SOCKET,SO_CONNECT_TIME,(char *)&iConnectTime, &nTimeLen);//此只能检测从开始链接到现在的时间，如果此socket在中间有数据接收或发送，仍然会断开连接
			getsockopt(pos->first,SOL_SOCKET, SO_GROUP_PRIORITY, (char *)&iIdleTime, &nTimeLen);
			if(iIdleTime == -1)	//非连接socket
			{
				++pos;
				continue;
			}
			if((int)time(NULL) - iIdleTime > MAX_CONNECT_TIMEOUT) //timeout value is 2 minutes.
			{
				printf("客户端: %d 超时,系统将关闭此连接.\n",pos->first);
			    CancelIo((HANDLE)pos->first);
				shutdown(pos->first,SD_BOTH); 
				closesocket(pos->first);
				m_mapAcceptSockQueue.erase(pos++);
			}
			else
			{
				++pos;
			}
		}
		LeaveCriticalSection(&m_struCriSec);
	}
}
//断线检测,通过心跳包来检测
int CIocpServer::SetHeartBeatCheck(SOCKET socket)
{
	DWORD dwError = 0L,dwBytes = 0;
	TCP_KEEPALIVE1 sKA_Settings = {0},sReturned = {0};
	sKA_Settings.onoff = 1 ;
	sKA_Settings.keepalivetime = 3000 ; // Keep Alive in 5.5 sec.
	sKA_Settings.keepaliveinterval = 1000 ; // Resend if No-Reply

	dwError = WSAIoctl(socket, SIO_KEEPALIVE_VALS, &sKA_Settings,sizeof(sKA_Settings), &sReturned, sizeof(sReturned),&dwBytes,NULL, NULL);
	if(dwError == SOCKET_ERROR )
	{
		dwError = WSAGetLastError();
		//m_pLogFile->LogEx("SetHeartBeatCheck->WSAIoctl()发生错误,错误代码: %ld",dwError);	
		return FALSE;
	}
	return TRUE;
}
void CIocpServer::PrintDebugMessage(CSrvException& e,bool log/*=true*/)
{
    ostringstream message;
	message<<e.GetExpDescription()<<endl;
	printf(message.str().c_str());
	if(log)
	{
		if(m_pLogFile)
			m_pLogFile->Log(message.str().c_str());
	}
	else
	{
		//MessageBox(NULL,message.str().c_str(),"程序发生异常",MB_OK|MB_ICONERROR); 
	}
}
void CIocpServer::PrintDebugMessage(const char* str,int errorCode/*=-1*/,bool log/* = true*/)
{
	ostringstream message;
	message<<str<< ":" <<errorCode << endl;


	if(log)
	{
		if(m_pLogFile)
			m_pLogFile->Log(message.str().c_str());
	}
	else
	{
		//MessageBox(NULL,message.str().c_str(),"程序发生异常",MB_OK|MB_ICONERROR);
	}

}