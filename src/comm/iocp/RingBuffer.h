
#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

static const int _size = 256*10240;

// 用于字节流的环形缓冲区
template <class T>
class CRingBuffer
{
public:
	CRingBuffer(unsigned long size = 256);
	~CRingBuffer();
public:
	unsigned long size();
	unsigned long space();
	bool put(T *data, int len);
	bool get(T *data, int len, bool takeAway);
	bool peek(T *data, int len);
    bool seek(int len); //
private:
	T* _rb;
	unsigned long _first;			// 指向第一个数据位置
	unsigned long _last;			// 指向第一个写入位置
	unsigned long _size;
};

template <class T>
CRingBuffer<T>::CRingBuffer(unsigned long size/* = 256*/)
{
	_first = 0;
	_last = 0;
	_size = size;
	_rb = new T[_size];
    ZeroMemory(_rb,_size*sizeof(T));
}

template <class T>
CRingBuffer<T>::~CRingBuffer()
{
  delete []_rb;
  _rb = 0;
}

// 数据长度
template <class T>
unsigned long CRingBuffer<T>::size()
{
	if((0 <= _first && _first < _size) && (0 <= _last && _last < _size))
	{

		if(_last >= _first)
			return _last - _first;
		else
			return _size - _first + _last;
	}
	else
		return 0;
}

// 剩余空间
template <class T>
unsigned long CRingBuffer<T>::space()//环形队列中剩余空间
{
	return _size - 1 - size();
}

// 往缓冲区写数据
template <class T>
bool CRingBuffer<T>::put(T *data, int len)
{

	if(data == NULL || len <=0)
		return false;
	
	if(len > (int)space()) // 保证有足够的空间
		return false;
	
	if(_size - _last >= (unsigned long)len) // 缓冲区末尾有足够空间
	{
		memcpy(_rb + _last, data, len*sizeof(T));
		_last = (_last + len) % _size;
	}
	else // 缓冲区末尾空间不够，分两次复制
	{
		memcpy(_rb + _last, data, (_size - _last)*sizeof(T));
		memcpy(_rb, data + _size - _last, (len - (_size - _last))*sizeof(T));
		_last = len - (_size - _last);
	}
	return true;
}

// 从缓冲区读数据
template <class T>
bool CRingBuffer<T>::get(T *data, int len, bool takeAway = true)
{
	if(data == NULL || len <=0)
		return false;
	
	if(len > (int)size()) // 保证有足够的数据
		return false;
	
	if(_last >= _first || _size - _first >= (unsigned long)len) // 数据连续，或不连续但末尾有足够数据
	{
		memcpy(data, _rb + _first, len*sizeof(T));
		if(takeAway)
			_first = (_first + len) % _size;
	}
	else // 数据不连续，且末尾没有足够数据，分两次复制
	{
		memcpy(data, _rb + _first, (_size - _first)*sizeof(T));
		memcpy(data + _size - _first, _rb, (len - (_size - _first))*sizeof(T));
		if(takeAway)
			_first = len - (_size - _first);
	}
	return true;
}

// 查看缓冲区，不取出
template <class T>
bool CRingBuffer<T>::peek(T *data, int len)
{
	return get(data, len, false);
}

template <class T>
bool CRingBuffer<T>::seek(int len)
{
	if(len <= 0)
		return false;
	
	if(len > (int)size()) // 保证有足够的数据
		return false;
	
	if(_last >= _first || _size - _first >= (unsigned long)len) // 数据连续，或不连续但末尾有足够数据
		_first = (_first + len) % _size;
	else // 数据不连续，且末尾没有足够数据，分两次复制
		_first = len - (_size - _first);
	
	return true;
}

#endif // #ifndef RING_BUFFER_H