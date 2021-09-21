#include <winsock2.h>
#include <stdio.h>
#include "Session.h"

#pragma comment (lib, "ws2_32.lib")

int port = 5000;
char musicDirectory[1024];
SOCKET listenSocket = INVALID_SOCKET;
HANDLE mutex = NULL; 
Session* root = NULL;
Session* closedRoot = NULL;

// start session if socket is connected.
void StartSession(Session* session)
{
	DWORD dwWaitResult = WaitForSingleObject(mutex, INFINITE);  // no time-out interval     

	if(root == NULL) root = session; 
	else
	{
		session->_child = root;
		root = session; 
	}

	ReleaseMutex(mutex); 
}

// close session if socket is disconnected.
void CloseSession(Session* session)
{
	DWORD dwWaitResult = WaitForSingleObject(mutex, INFINITE);  // no time-out interval   

	if(session->_parent == NULL) root = session->_child;
	else session->_parent->_child = session->_child;

	session->_parent = NULL;
	session->_child = closedRoot;
	closedRoot = session; 
	printf("[%d] Ended session.\n", (DWORD)closedRoot);

	ReleaseMutex(mutex); 
}

// remove closed sessions from session list. 
void RemoveSessions()
{
	DWORD dwWaitResult = WaitForSingleObject(mutex, INFINITE);  // no time-out interval    

	while(closedRoot != NULL)
	{
		Session* child = closedRoot->_child;
		delete closedRoot;
		closedRoot = child; 
	}

	ReleaseMutex(mutex); 
}

void ListenProc(void* param)
{
	while(true)
	{
	    SOCKET clientSocket = accept(listenSocket, NULL, NULL);
		if (clientSocket == INVALID_SOCKET) 
		{
			printf("[Server] accept failed with error: %d\n", WSAGetLastError());
			continue;
		}
		StartSession(new Session(clientSocket));
		RemoveSessions();
	}
}

void Close()
{
	if(listenSocket != INVALID_SOCKET)
	{
		closesocket(listenSocket);
		listenSocket = INVALID_SOCKET;
	}
	if(mutex != NULL)
	{
		CloseHandle(mutex);
		mutex = NULL;
	}
	WSACleanup();	
}

bool Initialize()
{
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) 
	{
        printf("WSAStartup failed with error: %d\n", iResult);
        return false;
    }

	// create mutex to synthro
	mutex = CreateMutex(NULL, FALSE, NULL);    
    if (mutex == NULL) 
    {
        printf("CreateMutex error: %d\n", GetLastError());
        return false;
    }

	// create socket for listening.
	listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(listenSocket == INVALID_SOCKET)
	{
        printf("socket failed with error: %d\n", WSAGetLastError());
		return false;
	}

	// bind listen socket.
	SOCKADDR_IN sa;
	memset(&sa, 0, sizeof(SOCKADDR_IN));
	sa.sin_family		= AF_INET;
	sa.sin_port			= htons(port);
	sa.sin_addr.s_addr	= htonl(INADDR_ANY);
	if(bind(listenSocket, (LPSOCKADDR)&sa, sizeof(SOCKADDR_IN)) == SOCKET_ERROR)
	{
        printf("bind failed with error: %d\n", WSAGetLastError());
		Close();
		return false;
	}

	// start listen
    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        printf("listen failed with error: %d\n", WSAGetLastError());
		Close();
        return false;
    }
    printf("[Server] Listening at port %d...\n", port);

	return true;
}

int main()
{
	GetCurrentDirectoryA(1024, musicDirectory);
	strcat(musicDirectory, "\\music\\");
	if(!Initialize()) return -1;

	// create thread
	HANDLE hListenThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ListenProc, NULL, 0, NULL);
	if(hListenThread == NULL)
	{
		printf("Failed to create thread.");
		Close();
		return -1;
	}
		
	// wait until thread ends.
	WaitForSingleObject(hListenThread, INFINITE);
			
	Close();
	return 0;
}

