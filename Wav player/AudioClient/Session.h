#pragma once

#include "Audio.h"

struct PacketInfo
{
	int starBites;		// 0x12345678 
	int type;			// 0-status_info, 1-list_info, 2-list_block, 3-file_info, 4-file_block
	int size;
	int offset;
	int count;
	int serialnumber; 
	int stopBits;		//0x87654321 
};

struct ListInfo
{
	int offset;		
	int size;
};

class Session
{
	public:
		Session(SOCKET clientSocket);
		~Session(void);

		int _alive;
		void SendProc();
		void RecvProc();
		void KeyProc();
		void InputFromUser();

	private:
		HANDLE _mutex; 
		SOCKET _clientSocket;
		char _recvbuf[BLOCK_SIZE];
		char _recvData[BLOCK_SIZE];
		int _recvOffset;
		int _recvSize;
		char _sendbuf[BLOCK_SIZE];
		PacketInfo _request;
		char** _musicNames;
		int _musicIndex;
		int _musicCount;
		int _musicOffset;
		char _fileBlock[BLOCK_SIZE];
		int _blockSize;
		int _fileSize;
		int _fileOffset;
		int _keyStatus; // 0:None, 1:MainMenu, 2:PlayMenu
		bool _sendStop;
		Audio _audio;

		void SendData();
		void SetSendBuf(int type, int serialnameber = -1);
		void ParseData();
};

