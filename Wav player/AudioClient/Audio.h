#pragma once

#include <Windows.h>
#include <mmsystem.h>
#include <mmreg.h>
#include <dsound.h>

#define fourccRIFF MAKEFOURCC('R', 'I', 'F', 'F')
#define fourccDATA MAKEFOURCC('d', 'a', 't', 'a')
#define fourccFMT  MAKEFOURCC('f', 'm', 't', ' ')
#define fourccWAVE MAKEFOURCC('W', 'A', 'V', 'E')
#define fourccXWMA MAKEFOURCC('X', 'W', 'M', 'A')

#define BLOCK_SIZE 1024

class Audio
{
	public:
		Audio(void);
		~Audio(void);
		void Start();
		void Play();
		void Stop();
		void Done();
		void Pause();
		void PlayProc();
		bool IsAvailable();

	private:
		IDirectSound8*			directSound;
		IDirectSoundBuffer*		primaryBuffer;
		IDirectSoundBuffer8*	secondaryBuffer;
		DWORD				    averageBytesPerSecond;
		HANDLE					m_hThread;
		char					m_Data[BLOCK_SIZE];
		int						m_dwOffset;
		int						m_dwSize;
		bool					m_bAvailable;
		bool					m_bDone;
		bool					m_bEmpty;

		bool Initialize();
		bool FindChunk(FOURCC fourcc, DWORD& chunkSize, DWORD& chunkDataPosition);
		bool ReadChunkData(void* buffer, DWORD buffersize, DWORD bufferoffset);
		DWORD ReadData(void* data, DWORD size);
		void MoveData(DWORD offset, DWORD start);
		void Shutdown();
		void PlayWave(WAVEFORMATEX waveFormat);
};

