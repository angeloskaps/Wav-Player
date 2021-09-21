#pragma once

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

#define BLOCK_SIZE 1024

class Session
{
	public:
		Session(SOCKET clientSocket);
		~Session(void);

		Session* _parent;
		Session* _child;

		void TransferProc();
	
	private:
		SOCKET _clientSocket;
		char _recvbuf[BLOCK_SIZE];
		char _sendbuf[BLOCK_SIZE];
		PacketInfo _response; 
		char** _musicNames;
		int _musicCount;
		int _musicIndex;
		int _fileSize;

		void SetSendBuf(int type, int size = 0);
		void GetFiles();
		DWORD GetSize();
};

