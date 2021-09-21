#include <stdio.h>
#include "Session.h"
#include <mmreg.h>
#include "DataQueue.h"

DataQueue _dataQueue;

void gSendProc(void* param)
{
	Session* session = (Session*)param;
	session->SendProc();
}

void gRecvProc(void* param)
{
	Session* session = (Session*)param;
	session->RecvProc();
}

void gKeyProc(void* param)
{
	Session* session = (Session*)param;
	session->KeyProc();
}

Session::Session(SOCKET clientSocket)
{
	_alive = 1; // 1:alive, 0:delete, -1:exit
	_clientSocket = clientSocket;
	_musicNames = NULL;
	_musicIndex = 0;
	_musicCount = 0;
	_musicOffset = 0;
	_keyStatus = 1; // MainMenu
	_recvOffset = 0;
	_recvSize = 0;
	_fileSize = 0;
	_fileOffset = 0;
	_sendStop = false;

	_request.starBites = 0x12345678;
	_request.type = 0;
	_request.size = sizeof(PacketInfo);
	_request.offset = 0;
	_request.count = 0;
	_request.serialnumber = 0;
	_request.stopBits = 0x87654321;

	// create mutex to synthro
	_mutex = CreateMutex(NULL, FALSE, NULL);    
    if (_mutex == NULL) 
    {
        printf("CreateMutex error: %d\n", GetLastError());
		_alive = 0;
        return;
    }

	// create threads for keyboard, sending and receiving.
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)gSendProc, this, 0, NULL);
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)gRecvProc, this, 0, NULL);
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)gKeyProc, this, 0, NULL);
}

Session::~Session(void)
{
	if(_clientSocket != INVALID_SOCKET)
	{
		closesocket(_clientSocket);
	}
	if(_musicNames != NULL) 
	{
		for(int i = 0; i < _musicCount; i++) 
		{
			if(_musicNames[i] != NULL) delete [] _musicNames[i];
		}
	}
}

void Session::KeyProc()
{
	char cmd[32];
	while(true)
	{
		if(_keyStatus == 0) Sleep(1000); // None
		else if(_keyStatus == 1) // MainMenu
		{
			printf("\n[L] get music list from server\n");
			printf("[V] view music list\n");
			printf("[number] play the music with number\n");
			printf("[Q] exit\n");
			printf("Your choice: ");
			scanf("%s", &cmd);

			if(cmd[0] == 'l' || cmd[0] == 'L') 
			{
				SetSendBuf(1); // ListInfo
				_keyStatus = 0;
				printf("\nDownloading the music list from server...\n\n");
			}
			else if(cmd[0] == 'v' || cmd[0] == 'V') 
			{
				if(_musicCount == 0)
				{
					SetSendBuf(1); // ListInfo
					_keyStatus = 0;
					printf("\nDownloading the music list from server...\n\n");
				}
				else
				{
					printf("\n");
					for(int i = 0; i < _musicCount; i++)
					{
						printf("[%d] %s\n", i + 1, _musicNames[i]);
					}
				}
			}
			else if(cmd[0] == 'q' || cmd[0] == 'Q') 
			{
				_alive = -1;
				break;
			}
			else
			{
				int number = atoi(cmd);
				if(number > 0 && number <= _musicCount)
				{
					_musicIndex = number - 1;
					SetSendBuf(3); // FileInfo
					_keyStatus = 0;
					printf("\nInitializing...\n\n");
				}
			}
		}
		else if(_keyStatus == 2) // PlayMenu
		{
			printf("\n[P] play\n");
			printf("[S] stop\n");
			printf("[A] pause\n");
			printf("[X] main menu\n");
			while(true)
			{
				printf("Your choice: ");
				scanf("%s", &cmd);
				if(cmd[0] == 'p' || cmd[0] == 'P') 
				{
					if(!_audio.IsAvailable())
					{
						SetSendBuf(3); // FileInfo
						_keyStatus = 0;
						while(!_audio.IsAvailable()) Sleep(500);
					}
					_audio.Play();
				}
				else if(cmd[0] == 's' || cmd[0] == 'S') 
				{
					_audio.Stop();
					SetSendBuf(3); // FileInfo
					_keyStatus = 0;
					break;
				}
				else if(cmd[0] == 'a' || cmd[0] == 'A') 
				{
					_audio.Pause();
				}
				else if(cmd[0] == 'x' || cmd[0] == 'X') 
				{
					_audio.Stop();
					_keyStatus = 1;
					break;
				}
			}
		}
	}
}

void Session::SendData()
{
	DWORD dwWaitResult = WaitForSingleObject(_mutex, INFINITE);  // no time-out interval         

	int len = send(_clientSocket, _sendbuf, sizeof(PacketInfo), 0 );
	if (len == SOCKET_ERROR) _alive = 0;

	ReleaseMutex(_mutex); 
}

void Session::SetSendBuf(int type, int serialnameber)
{
	_request.type = type;
	if(serialnameber != -1) _request.serialnumber = serialnameber;

	if(_request.type == 2) _request.offset = _musicOffset;
	else if(_request.type == 3) _request.offset = _musicIndex;
	else if(_request.type == 4) _request.offset = _fileOffset;

	DWORD dwWaitResult = WaitForSingleObject(_mutex, INFINITE);  // no time-out interval         
	
	memcpy(_sendbuf, &_request, sizeof(PacketInfo));
	
	ReleaseMutex(_mutex); 
}

void Session::SendProc()
{
	while(_alive == 1)
	{
		if(_request.type == 0)
		{
			Sleep(500); // status
		}

		Sleep(2); 
		if(_sendStop) continue;
		if(_request.type == 4 && !_dataQueue.IsExistFree()) continue;

		SendData();
	}
}

void Session::RecvProc()
{
	while(_alive == 1)
	{
		try
		{
			_sendStop = false;
			int len = recv(_clientSocket, _recvbuf, BLOCK_SIZE, 0);
			_sendStop = true;

			if(len <= 0)
			{
				printf("\nrecv failed with error: %d\n", WSAGetLastError());
				_alive = 0;
				break;
			}

			int offset = 0;
			if(_recvSize == 0)
			{
				if(len < sizeof(PacketInfo)) continue;

				PacketInfo* response = (PacketInfo*)_recvbuf;
				if(response->starBites != 0x12345678) continue;
				if(response->stopBits != 0x87654321) continue;

				_recvSize = response->size;

				memcpy(_recvData, _recvbuf, sizeof(PacketInfo));
				_recvOffset = sizeof(PacketInfo);
				offset = sizeof(PacketInfo);
			}

			int remain = min(len - offset, _recvSize - _recvOffset);
			if(remain > 0)
			{
				memcpy(_recvData + _recvOffset, _recvbuf + offset, remain);
				_recvOffset += remain;
				offset += remain;
			}

			if(_recvOffset >= _recvSize)
			{
				ParseData();
				_recvOffset = 0;
				_recvSize = 0;
			}
		}
		catch(...)
		{
			printf("\nrecv failed with error: %d\n", WSAGetLastError());
			_alive = 0;
			break;
		}
	}
}

void Session::ParseData()
{
	PacketInfo* response = (PacketInfo*)_recvData;
	if(response->type == 1) // ListInfo
	{
		if(_musicNames != NULL) 
		{
			for(int i = 0; i < _musicCount; i++) 
			{
				if(_musicNames[i] != NULL) delete [] _musicNames[i];
			}
		}
		_musicCount = response->count;
		_musicNames = (char**)(new char*[_musicCount]);
		for(int i = 0; i < _musicCount; i++) _musicNames[i] = NULL;
		_musicOffset = 0;

		SetSendBuf(2, response->serialnumber); // ListBlock
	}
	else if(response->type == 2) // ListBlock
	{
		if(_musicNames == NULL)
		{
			SetSendBuf(1, response->serialnumber); // ListInfo
			return;
		}

		if(_musicOffset != _request.offset) 
		{
			SetSendBuf(2, response->serialnumber); // ListBlock
			return;
		}

		int offset = sizeof(PacketInfo);
		while(offset < _recvSize && _musicOffset < _musicCount)
		{
			ListInfo* listInfo = (ListInfo*)(_recvData + offset);
			if(listInfo->offset != _musicOffset) break;
			offset += sizeof(ListInfo);

			if(_recvSize - offset < listInfo->size) break;

			_musicNames[_musicOffset] = new char[listInfo->size];
			memcpy(_musicNames[_musicOffset], _recvData + offset, listInfo->size);
			printf("[%d] %s\n", _musicOffset + 1, _musicNames[_musicOffset]);
			_musicOffset++;
			offset += listInfo->size;
		}

		if(_musicOffset < _musicCount)
		{
			SetSendBuf(2, response->serialnumber); // ListBlock
			return;
		}

		_keyStatus = 1;
		SetSendBuf(0, response->serialnumber); // Satatus
	}
	else if(response->type == 3) // FileInfo
	{
		_fileSize = response->count;
		if(_fileSize == -1) 
		{
			printf("[%s couldn't be read\n", _musicNames[_musicIndex]);
			_keyStatus = 1;
			SetSendBuf(0, response->serialnumber); // Satatus
			return;
		}
		else if(_fileSize < 1024) printf("\n%s filesize: %d bytes\n", _musicNames[_musicIndex], _fileSize);
		else printf("\n%s filesize: %d KB\n", _musicNames[_musicIndex], _fileSize / 1024);

		_audio.Start();
		_fileOffset = 0;
		_keyStatus = 2;
		SetSendBuf(4, response->serialnumber); // FileBlock
	}
	else if(response->type == 4) // FileBlock
	{
		if(_audio.IsAvailable()) _keyStatus = 2;

		_blockSize = _recvSize - sizeof(PacketInfo);
		if(_fileOffset != response->offset) 
		{
			SetSendBuf(4, response->serialnumber); // FileBlock
			return;
		}

		_dataQueue.AddQueue(_recvData + sizeof(PacketInfo), _blockSize);
		_fileOffset += _blockSize;

		if(_fileOffset < _fileSize)
		{
			SetSendBuf(4, response->serialnumber); // ListBlock
			return;
		}

		_audio.Done();
		SetSendBuf(0, response->serialnumber); // Satatus
	}
}

