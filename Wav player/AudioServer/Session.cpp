#include <windows.h>
#include <stdio.h>
#include "Session.h"
#include "FileList.h"

extern void CloseSession(Session* session);
extern char musicDirectory[1024];

void gTransferProc(void* param)
{
	Session* session = (Session*)param;
	session->TransferProc();
}

Session::Session(SOCKET clientSocket)
{
	_parent = NULL;
	_child = NULL;
	_clientSocket = clientSocket;
	_musicNames = NULL;
	_musicIndex = 0;
	_musicCount = 0;
	_fileSize = 0;

	_response.starBites = 0x12345678;
	_response.type = 0;
	_response.size = sizeof(PacketInfo);
	_response.offset = 0;
	_response.count = 0;
	_response.serialnumber = 0;
	_response.stopBits = 0x87654321;

	// create thread
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)gTransferProc, this, 0, NULL);

	printf("[%d] Started session.\n", (DWORD)this);
}

Session::~Session(void)
{
	if(_clientSocket != INVALID_SOCKET)
	{
		closesocket(_clientSocket);
	}
}
		
void Session::SetSendBuf(int type, int size)
{
	_response.type = type;
	if(size == 0) _response.size = sizeof(PacketInfo);
	else _response.size = size;

	if(type == 1) _response.count = _musicCount;
	else if(type == 3 || type == 4) _response.count = _fileSize;
	_response.serialnumber++;
	memcpy(_sendbuf, &_response, sizeof(PacketInfo));
}
		
void Session::TransferProc()
{
	int lastType = 0;
	int lastSerialnumber = 0;

	while(true)
	{
		try
		{
			int len = recv(_clientSocket, _recvbuf, BLOCK_SIZE, 0);
			if (len <= 0) 
			{
				printf("\n[%d] recv failed with error: %d\n", (int)this, WSAGetLastError());
				CloseSession(this);
				break;
			}

			if (len < sizeof(PacketInfo)) continue; 

			PacketInfo* request = (PacketInfo*)_recvbuf; 
			if(request->starBites != 0x12345678) continue;
			if(request->stopBits != 0x87654321) continue;

			if(request->type == 1) printf("\n[%d] recv music list info.\n", (DWORD)this);
			else if(request->type == 2) printf("\n[%d] recv music offset : %d\n", (DWORD)this, request->offset);
			else if(request->type == 3) printf("\n[%d] recv file info : %d\n", (DWORD)this, request->offset);
			else if(request->type == 4) printf("\r[%d] recv file offset : %d", (DWORD)this, request->offset);

			int offset = sizeof(PacketInfo);
			if(request->type != 0 && request->serialnumber >= lastSerialnumber)
			{
				if(request->type == 1) // ListInfo
				{
					GetFiles();
					SetSendBuf(1); // ListInfo
				}
				else if(request->type == 2) // ListBlock
				{
					if(_musicNames == NULL) GetFiles();

					ListInfo listInfo;
					listInfo.offset = request->offset;
					while(listInfo.offset < _musicCount)
					{
						listInfo.size = strlen(_musicNames[listInfo.offset]) + 1;
						if(offset + sizeof(ListInfo) + listInfo.size >=  BLOCK_SIZE) break;

						memcpy(_sendbuf + offset, &listInfo, sizeof(ListInfo));
						offset += sizeof(ListInfo);

						memcpy(_sendbuf + offset, _musicNames[listInfo.offset], listInfo.size);
						offset += listInfo.size;

						listInfo.offset++;
					}
					SetSendBuf(2, offset); // ListBlock
				}
				else if(request->type == 3) // FileInfo
				{
					_musicIndex = request->offset;
					_fileSize = GetSize();
					SetSendBuf(3); // FileInfo
				}
				else // FileBlock
				{
					int blockSize = min(BLOCK_SIZE - sizeof(PacketInfo), _fileSize - request->offset);
					_response.offset = request->offset;
					
					char filename[1024];
					strcpy(filename, musicDirectory);
					strcat(filename, _musicNames[_musicIndex]);

					HANDLE handle = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
					if(handle == INVALID_HANDLE_VALUE) 
					{
						_fileSize = -1;
						SetSendBuf(4, sizeof(PacketInfo)); // FileBlock
					}
					else
					{
						SetFilePointer(handle, request->offset, NULL, FILE_BEGIN);

						DWORD dwRead;
						ReadFile(handle, _sendbuf + offset, blockSize, &dwRead, NULL);
						CloseHandle(handle);

						SetSendBuf(4, sizeof(PacketInfo) + blockSize); // FileBlock
						offset += blockSize;
					}
				}
				lastType = request->type;
				lastSerialnumber = request->serialnumber;
			}
			else SetSendBuf(0); // Status

			// Echo the buffer back to the sender
			int iResult = send(_clientSocket, _sendbuf, offset, 0 );
			if (iResult == SOCKET_ERROR) 
			{
				CloseSession(this);
				break;
			}
		}
		catch(...)
		{
			printf("\n[%d] recv failed with error: %d\n", (int)this, WSAGetLastError());
			CloseSession(this);
			break;
		}
	}
}

void Session::GetFiles()
{
	if(_musicNames != NULL) 
	{
		for(int i = 0; i < _musicCount; i++) delete [] _musicNames[i];
	}

	char filter[1024];
	strcpy(filter, musicDirectory);
	strcat(filter, "*.wav");

	FileList* root = NULL;
	_musicCount = 0;

	WIN32_FIND_DATAA FindFileData;
	HANDLE hFind = FindFirstFileA(filter, &FindFileData);
	if (hFind != INVALID_HANDLE_VALUE) 
	{
		FileList* file = new FileList();
		if(root == NULL) root = file;
		else 
		{
			file->_next = root;
			root = file;
		}

		int len = strlen(FindFileData.cFileName);
		root->_fileName = new char[len + 1];
		strcpy(root->_fileName, FindFileData.cFileName);
		_musicCount++;

		while(FindNextFileA(hFind, &FindFileData))
		{
			FileList* file = new FileList();
			if(root == NULL) root = file;
			else 
			{
				file->_next = root;
				root = file;
			}

			len = strlen(FindFileData.cFileName);
			root->_fileName = new char[len + 1];
			strcpy(root->_fileName, FindFileData.cFileName);
			_musicCount++;
		}
	}
	FindClose(hFind);

	if(_musicCount > 0)
	{
		_musicNames = (char**)(new char*[_musicCount]);
		int i = _musicCount - 1;
		while(root != NULL)
		{
			int len = strlen(root->_fileName);
			_musicNames[i] = new char[len + 1];
			strcpy(_musicNames[i], root->_fileName);

			FileList* next = root->_next;
			delete root;
			root = next;
			i--;
		}
	}
}

DWORD Session::GetSize()
{
	if(_musicIndex >= _musicCount) return -1;

	char filename[1024];
	strcpy(filename, musicDirectory);
	strcat(filename, _musicNames[_musicIndex]);

	HANDLE handle = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if(handle == INVALID_HANDLE_VALUE) return -1;

	DWORD size = GetFileSize(handle, NULL);
	CloseHandle(handle);
	if(size == INVALID_FILE_SIZE) return -1;
	return size;
}

