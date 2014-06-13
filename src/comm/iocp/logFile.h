

#pragma once

#include <assert.h>
#include <time.h>
#include <stdio.h>
#include <windows.h>
#include<stdlib.h>
#pragma warning(disable : 4996)
class CLogFile
{
protected:
	
	CRITICAL_SECTION _csLock;
	char * _szFileName;
	HANDLE _hFile;
	
	bool OpenFile(char* filePath)//打开文件， 指针到文件尾
	{
		if(IsOpen())
			return true;
		
		if(!filePath)
			return false;
		
		_hFile =  CreateFile(
			filePath, 
			GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			NULL 
			);
		
		if(!IsOpen() && GetLastError() == 2)//打开不成功， 且因为文件不存在， 创建文件
			_hFile =  CreateFile(
			filePath, 
			GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			NULL 
			);  
		
		if(IsOpen())
			SetFilePointer(_hFile, 0, NULL, FILE_END);
		
		return IsOpen();
	}
	
	DWORD Write(LPCVOID lpBuffer, DWORD dwLength)
	{
		DWORD dwWriteLength = 0;
		
		if(IsOpen())
			WriteFile(_hFile, lpBuffer, dwLength, &dwWriteLength, NULL);
		
		return dwWriteLength;
	}
	
	virtual void WriteLog( LPCVOID lpBuffer, DWORD dwLength)//写日志, 可以扩展修改
	{
		time_t now;
		char temp[22];
		DWORD dwWriteLength;
		
		if(IsOpen())
		{
			time(&now);
			strftime(temp, 20, "%Y.%m.%d %H:%M:%S", localtime(&now));
			
			WriteFile(_hFile, temp, 19, &dwWriteLength, NULL);
			WriteFile(_hFile, " : ", 3, &dwWriteLength, NULL);
			WriteFile(_hFile, lpBuffer, dwLength, &dwWriteLength, NULL);
			WriteFile(_hFile, "\r\n", 2, &dwWriteLength, NULL);
			
			//FlushFileBuffers(_hFile);	//此句加在这里极度影响速度
		}
	}
	
	void Lock()  { ::EnterCriticalSection(&_csLock); }
	void Unlock() { ::LeaveCriticalSection(&_csLock); }
	
public:
	
	CLogFile(const char *szFileName = "Log.log")//设定日志文件名
	{
		_szFileName = NULL;
		_hFile = INVALID_HANDLE_VALUE;
		::InitializeCriticalSection(&_csLock);
		//InitializeCriticalSectionAndSpinCount(&_csLock,4000);  
		
		SetFileName(szFileName);
	}
	
	virtual ~CLogFile()
	{
		::DeleteCriticalSection(&_csLock);
		Close();
		
		if(_szFileName)
		{
			delete []_szFileName;
			_szFileName = NULL;
		}

	}
	
	const char * GetFileName()
	{
		return _szFileName;
	}
	
	void SetFileName(const char *szName)//修改文件名， 同时关闭上一个日志文件
	{
		assert(szName);
		
		if(_szFileName)
		{
			delete []_szFileName;
			_szFileName = NULL;
		}
		
		Close();
		
		_szFileName = new char[strlen(szName)+1];
		assert(_szFileName);
		strcpy(_szFileName, szName);
	}
	
	bool IsOpen()
	{
		return _hFile != INVALID_HANDLE_VALUE;
	}
	
	void Close()
	{
		if(IsOpen())
		{
			FlushFileBuffers(_hFile);
			CloseHandle(_hFile);
			_hFile = INVALID_HANDLE_VALUE;
		}
	}
	
	void Log(LPCVOID lpBuffer, DWORD dwLength)//追加日志内容
	{
		assert(lpBuffer);
		__try
		{
			Lock();
			
			if(!OpenFile(_szFileName))
				return;
			
			WriteLog(lpBuffer, dwLength);
		}
		__finally
		{
			Unlock();
		} 
	}
	
	void Log(const char *szText)
	{
		Log(szText,(DWORD)strlen(szText));
	}
	
private://屏蔽函数
	
	CLogFile(const CLogFile&);
	CLogFile&operator = (const CLogFile&);
};

class CLogFileEx : public CLogFile
{
protected:
	
	char *_szPath;
	char _szLastDate[9];
	int _iType;
	
	void SetPath(const char *szPath)
	{
		assert(szPath);
		
		WIN32_FIND_DATA wfd;
		char temp[MAX_PATH + 1] = {0};
		
		if(FindFirstFile(szPath, &wfd) == INVALID_HANDLE_VALUE && CreateDirectory(szPath, NULL) == 0)
		{
			strcat(strcpy(temp, szPath), " Create Fail. Exit Now! Error ID :");
			ltoa(GetLastError(), temp + strlen(temp), 10);
			MessageBox(NULL, temp, "Class CLogFileEx", MB_OK);
			exit(1);
		}
		else
		{
			GetFullPathName(szPath, MAX_PATH, temp, NULL);
			_szPath = new char[strlen(temp) + 256];
			assert(_szPath);
			strcpy(_szPath, temp);
		}
	}
	
public:
	
	enum LOG_TYPE{YEAR = 0, MONTH = 1, DAY = 2};
	
	CLogFileEx(const char *szPath = ".", LOG_TYPE iType = MONTH)
	{
		_szPath = NULL;
		SetPath(szPath);
		_iType = iType;
		memset(_szLastDate, 0, 9);
	}
	
	~CLogFileEx()
	{
		if(_szPath)
			delete []_szPath;
	}
	
	const char * GetPath()
	{
		return _szPath;
	}
	
	virtual void Log(LPCVOID lpBuffer, DWORD dwLength)
	{
		assert(lpBuffer);
		
		char temp[10];
		static const char format[3][10] = {"%Y", "%Y-%m", "%Y%m%d"};
		
		__try
		{
			Lock();
			
			time_t now = time(NULL);
			
			strftime(temp, 9, format[_iType], localtime(&now));
			
			if(strcmp(_szLastDate, temp) != 0)//更换文件名
			{
				strcat(_szPath,"\\");
				strcat(_szPath, temp);
				strcat(_szPath,".log");
				strcpy(_szLastDate, temp);
				Close();
			}
			
			if(!OpenFile(_szPath))
				return;
			
			WriteLog(lpBuffer, dwLength);
		}
		__finally
		{
			Unlock();
		}
	}
	
	virtual void Log(const char *szText)
	{
		Log(szText, (DWORD)strlen(szText));
	}
	void LogEx(char *fmt, ...)
	{
		char s[512];
		va_list argptr;
		
		va_start(argptr, fmt);
		vsprintf(s, fmt, argptr);
		va_end(argptr);

        Log(s, (DWORD)strlen(s));
	}
	
private://屏蔽函数
	
	CLogFileEx(const CLogFileEx&);
	CLogFileEx&operator = (const CLogFileEx&);
	
};


