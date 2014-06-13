
#pragma once 

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000


#include "Stack.h"
#include "base_define.h"

#include <math.h> //for floor()


template <class T>
class CContextManager  
{
public:
	CContextManager(unsigned short size);
	virtual ~CContextManager();
public:
	void GetContext(T*& pContext);
	void ReleaseContext(T* pContext);
	void GetSize(long& size);
	void GetIdleCounter(long& idle_counter);
	void GetBusyCounter(long& busy_counter);
private:
	bool delete_flag;
	unsigned short delete_counter;
	unsigned short delete_stop_val;
	unsigned short delete_factor_counter;
	long delete_total_counter;
	long delete_busy_counter;
	double avl_memory_factor;
private:
	unsigned long m_iTimerId;
	long m_iSize;
	long m_iBusyCounter;

	CStack<T>* m_pStack;
    CRITICAL_SECTION critical_section;
private:
	static void PASCAL TimerProc(UINT wTimerID,UINT msg,DWORD dwUser,DWORD dw1,DWORD dw2);

};
template <class T>
CContextManager<T>::CContextManager(unsigned short size)
{
	delete_flag = false;
	delete_counter = 0;
	delete_stop_val = 64;
    delete_factor_counter = 0;
	delete_total_counter = 0;
    delete_busy_counter = 0;
	avl_memory_factor = 0.0;

	m_iTimerId = 0;
	m_iSize = size;
	m_iBusyCounter = 0;

	InitializeCriticalSectionAndSpinCount(&critical_section,4000);

    m_pStack = NULL;
	m_pStack = new CStack<T>();

	for(int i=0;i<m_iSize;i++)
	{
        T* pContext = new T();
        m_pStack->Push(pContext);
	}

	m_iTimerId = timeSetEvent(50,1000,TimerProc,(DWORD)this,TIME_PERIODIC);//1秒钟定时器
}

template <class T>
CContextManager<T>::~CContextManager()
{
	timeKillEvent(m_iTimerId);
	m_iTimerId = 0;

	for(int i=0;i<m_iSize;i++)
	{
        T* pContext = m_pStack->Pop();
        if(pContext)
			delete pContext;
	}
	delete_flag = false;
    m_iSize = 0;
	::DeleteCriticalSection(&critical_section);
	delete m_pStack;
	m_pStack = NULL;
}

template <class T>
void PASCAL CContextManager<T>::TimerProc(UINT wTimerID,UINT msg,DWORD dwUser,DWORD dw1,DWORD dw2)
{
	CContextManager<T>* p = (CContextManager<T>*)dwUser;
	static const int circle_counter = 100;

	if(TryEnterCriticalSection(&p->critical_section))
	{
		//printf("size%ld,busy:%ld\n",p->m_iSize,p->m_iBusyCounter);
		if(p->delete_factor_counter < circle_counter)
		{

		    p->delete_total_counter += (p->m_iSize-p->m_iBusyCounter);
            p->delete_busy_counter += p->m_iBusyCounter;
			if(p->m_iBusyCounter>2)
				p->delete_factor_counter++;
		}
		else
		{
			p->delete_factor_counter = 0;
			
            p->delete_total_counter /=circle_counter;
            p->delete_busy_counter /=circle_counter;
			p->avl_memory_factor = (double)p->delete_total_counter/(double)p->delete_busy_counter;
			if(p->avl_memory_factor > 2.5)
			{
				p->delete_stop_val = (int)(2.5*5*p->avl_memory_factor);
				p->avl_memory_factor = 0.0;
				p->delete_flag = true;	
				p->delete_total_counter = 0;
				p->delete_busy_counter = 0;

			}
			else
			{
				p->delete_flag = false;
			}
		}
  
		LeaveCriticalSection(&p->critical_section);
	}
}
template <class T>
void CContextManager<T>::GetContext(T*& pContext) //没有考虑出栈失败因素
{
	EnterCriticalSection(&critical_section);
	
	if(m_pStack)
	{
		if(m_pStack->IsEmpty())
		{
			pContext = new T();
			m_iSize++;
		}
		else
		{
			m_pStack->Pop(pContext);
		}	
		m_iBusyCounter++;
	}

	LeaveCriticalSection(&critical_section);
}
template <class T>
void CContextManager<T>::ReleaseContext(T* pContext) //没有考虑入栈失败因素
{
	EnterCriticalSection(&critical_section);

	if(m_pStack)
	{
		if(delete_flag)
		{
			delete pContext;
			m_iSize--;
			delete_counter++;
			if(delete_counter>=delete_stop_val)
			{
				delete_flag = false;
				delete_counter = 0;
			}
		}
		else
		{
			m_pStack->Push(pContext); //入栈失败
		}
		m_iBusyCounter--;
	}
	LeaveCriticalSection(&critical_section);
}
//得到pool的size
template <class T>
void CContextManager<T>::GetSize(long& size)
{
    InterlockedExchange(&size,m_iSize);
}
//得到空闲池的size
template <class T>
void CContextManager<T>::GetIdleCounter(long& idle_counter)
{
	InterlockedExchange(&idle_counter,m_iSize - m_iBusyCounter);
}
//得到busy池的size
template <class T>
void CContextManager<T>::GetBusyCounter(long& busy_counter)
{
	InterlockedExchange(&busy_counter,m_iBusyCounter);
}