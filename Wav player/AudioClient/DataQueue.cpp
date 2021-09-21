#include <windows.h>
#include "DataQueue.h"

CRITICAL_SECTION*	g_pcsQueue = NULL;

VOID InitCS()
{
	if(g_pcsQueue != NULL)
	{
		DeleteCriticalSection(g_pcsQueue);
		delete g_pcsQueue;
		g_pcsQueue = NULL;
	}
	g_pcsQueue = new CRITICAL_SECTION;
	InitializeCriticalSection(g_pcsQueue);
}

VOID EnterCS()
{
	if(g_pcsQueue != NULL)
		EnterCriticalSection(g_pcsQueue);
}

VOID LeaveCS()
{
	if(g_pcsQueue != NULL)
		LeaveCriticalSection(g_pcsQueue);
}

VOID DeleteCS()
{
	if(g_pcsQueue != NULL)
	{
		DeleteCriticalSection(g_pcsQueue);
		delete g_pcsQueue;
		g_pcsQueue = NULL;
	}
}

Data::Data(char* pData, int count)
{
	m_pData = new char[count];
	memcpy(m_pData, pData, count);
	m_iSize = count;
	m_pNext = NULL;
}

Data::~Data()
{
	delete [] m_pData;
}

void Data::Copy(char* pData, int& count)
{
	count = m_iSize;
	memcpy(pData, m_pData, count);
}

DataQueue::DataQueue()
{
	InitCS();
	
	m_pRoot = NULL;
	m_pLastNext = NULL;
	m_iCount = 0;
}

DataQueue::~DataQueue()
{
	Clear();
	DeleteCS();
}
		
void DataQueue::AddQueue(char* data, int count)
{
	EnterCS();

	Data* pData = new Data(data, count);
	if(m_pLastNext == NULL)
	{
		m_pRoot = pData;
		m_pLastNext = pData;
	}
	else
	{
		m_pLastNext->m_pNext = pData;
		m_pLastNext = m_pLastNext->m_pNext;
	}
	m_iCount++;

	LeaveCS();
}

bool DataQueue::DelQueue(char* data, int& count)
{
	bool re = false;
	count = 0;

	EnterCS();

	if(m_pRoot != NULL)
	{
		Data* pData = m_pRoot;
		m_pRoot = m_pRoot->m_pNext;

		if(m_pRoot == NULL) m_pLastNext = NULL;
		m_iCount--;

		pData->Copy(data, count);
		delete pData;
		re = true;
	}

	LeaveCS();

	return re;
}

bool DataQueue::IsExistFree()
{
	bool re = false;

	EnterCS();
	
	if(m_iCount < MAX_DATAQUEUE) re = true;

	LeaveCS();

	return re;
}


void DataQueue::Clear()
{
	EnterCS();
	
	while(m_pRoot != NULL)
	{
		Data* pData = m_pRoot;
		m_pRoot = m_pRoot->m_pNext;

		if(m_pRoot == NULL) m_pLastNext = NULL;
		m_iCount--;

		delete pData;
	}

	LeaveCS();
}


