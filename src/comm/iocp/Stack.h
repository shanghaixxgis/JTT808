#pragma once

//自己实现此类的目标是高性能

const unsigned long MAX_STACK_CAPACITY = 32768 * 4;  //栈的最大容量

template <class T>
class CStack
{

public:
	struct STACK_ARRAY{T* pContext;};
	CStack(long stackCapacity = MAX_STACK_CAPACITY);
	~CStack(void);
public:
	bool Push(T* pContext);
	bool Pop(T*& pContext);
	T* Pop();
	bool IsEmpty();
	bool IsFull();
	long Size();
public:
   long max_stack_capacity;//栈最大容量
   long max_stack_size;//当前的栈大小;
   struct STACK_ARRAY* stack_unit;
};

template <class T>
CStack<T>::CStack(long stackCapacity)
{
	if(stackCapacity > MAX_STACK_CAPACITY)
		max_stack_capacity = MAX_STACK_CAPACITY; 
	else
		max_stack_capacity = stackCapacity;
   max_stack_size = -1;
   stack_unit = new STACK_ARRAY[max_stack_capacity];
}

template <class T>
CStack<T>::~CStack(void)
{
	delete []stack_unit;
    stack_unit = NULL;
	max_stack_size = -1;
	max_stack_capacity = 0;
}
template <class T>
bool CStack<T>::Push(T* pContext)
{
	if(pContext==NULL || stack_unit==NULL)
		return false;

	if(max_stack_size+1>=max_stack_capacity)//栈满
		return false;

	stack_unit[++max_stack_size].pContext = pContext;
	return true;
}
template <class T>
bool CStack<T>::Pop(T*& pContext)
{
	if(max_stack_size>=0 && stack_unit)//栈非空
	{  
		pContext = stack_unit[max_stack_size--].pContext;
		stack_unit[max_stack_size+1].pContext=NULL;
		return true;
	}
	return false;
}
template <class T>
T* CStack<T>::Pop()
{
	T* rc=NULL;
	if(max_stack_size>=0 && stack_unit)//栈非空
	{  
		rc = stack_unit[max_stack_size--].pContext;
		stack_unit[max_stack_size+1].pContext=NULL;
	}
	return rc;
}
template <class T>
bool CStack<T>::IsEmpty()
{
	return max_stack_size<0?true:false;
}
template <class T>
bool CStack<T>::IsFull()
{
	if(max_stack_size+1==max_stack_capacity)
		 return true;
	else
		 return false;
}
template <class T>
long CStack<T>::Size()
{
    return max_stack_size+1;
}
