#include "Audio.h"
#include "DataQueue.h"

#pragma comment(lib, "dsound.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "winmm.lib")

extern DataQueue _dataQueue;

void gPlayProc(void* param)
{
	Audio* audio = (Audio*)param;
	audio->PlayProc();
}

Audio::Audio(void)
{
	directSound = NULL;
	primaryBuffer = NULL;
	secondaryBuffer = NULL;
	m_hThread = NULL;
	m_dwOffset = 0;
	m_dwSize = 0;
	m_bAvailable = false;

	Initialize();
}

Audio::~Audio(void)
{
	Stop();
	Shutdown();
}

bool Audio::Initialize()
{
	HRESULT result;
	DSBUFFERDESC bufferDesc;
	WAVEFORMATEX waveFormat;

	// Initialize the direct sound interface pointer for the default sound device.
	result = DirectSoundCreate8(NULL, &directSound, NULL);
	if (FAILED(result))
	{
		return false;
	}

	// Set the cooperative level to priority so the format of the primary sound buffer can be modified.
	// We use the handle of the desktop window since we are a console application.  If you do write a 
	// graphical application, you should use the HWnd of the graphical application. 
	result = directSound->SetCooperativeLevel(GetDesktopWindow(), DSSCL_PRIORITY);
	if (FAILED(result))
	{
		return false;
	}

	// Setup the primary buffer description.
	bufferDesc.dwSize = sizeof(DSBUFFERDESC);
	bufferDesc.dwFlags = DSBCAPS_PRIMARYBUFFER | DSBCAPS_CTRLVOLUME;
	bufferDesc.dwBufferBytes = 0;
	bufferDesc.dwReserved = 0;
	bufferDesc.lpwfxFormat = NULL;
	bufferDesc.guid3DAlgorithm = GUID_NULL;

	// Get control of the primary sound buffer on the default sound device.
	result = directSound->CreateSoundBuffer(&bufferDesc, &primaryBuffer, NULL);
	if (FAILED(result))
	{
		return false;
	}

	// Setup the format of the primary sound bufffer.
	// In this case it is a .WAV file recorded at 44,100 samples per second in 16-bit stereo (cd audio format).
	// Really, we should set this up from the wave file format loaded from the file.
	waveFormat.wFormatTag = WAVE_FORMAT_PCM;
	waveFormat.nSamplesPerSec = 44100;
	waveFormat.wBitsPerSample = 16;
	waveFormat.nChannels = 2;
	waveFormat.nBlockAlign = (waveFormat.wBitsPerSample / 8) * waveFormat.nChannels;
	waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
	waveFormat.cbSize = 0;

	// Set the primary buffer to be the wave format specified.
	result = primaryBuffer->SetFormat(&waveFormat);
	if (FAILED(result))
	{
		return false;
	}
	return true;
}

DWORD Audio::ReadData(void* data, DWORD size)
{
	int offset = 0;
	while(offset < size)
	{
		int len = min(m_dwSize - m_dwOffset, size - offset);
		if(len > 0)
		{
			memcpy((char*)data + offset, m_Data + m_dwOffset, len);
			m_dwOffset += len;
			offset += len;
		}
		else
		{
			while(true) 
			{
				if(_dataQueue.DelQueue(m_Data, m_dwSize))
				{
					m_dwOffset = 0;
					break;
				}
				else if(m_bDone)
				{
					m_bEmpty = true; 
					memset((char*)data + offset, 0, size - offset);
					m_dwSize = 0;
					m_dwOffset = 0;
					return size;
				}
				Sleep(2);
			}
		}
	}
	return size;
}

void Audio::MoveData(DWORD offset, DWORD start)
{
	if(start == FILE_BEGIN) m_dwOffset = offset;
	else if(start == FILE_CURRENT) m_dwOffset += offset;
}

// Search the file for the chunk we want  
// Returns the size of the chunk and its location in the file
bool Audio::FindChunk(FOURCC fourcc, DWORD& chunkSize, DWORD& chunkDataPosition)
{
	DWORD chunkType;
	DWORD chunkDataSize;
	DWORD riffDataSize = 0;
	DWORD fileType;
	DWORD bytesRead = 0;
	DWORD offset = 0;

	MoveData(0, FILE_BEGIN);
	while (true)
	{
		bytesRead = ReadData(&chunkType, sizeof(DWORD));
		bytesRead = ReadData(&chunkDataSize, sizeof(DWORD));
		switch (chunkType)
		{
			case fourccRIFF:
				riffDataSize = chunkDataSize;
				chunkDataSize = 4;
				bytesRead = ReadData(&fileType, sizeof(DWORD));
				break;
			case fourccDATA:
				break;

			default:
				MoveData(chunkDataSize, FILE_CURRENT);
		}

		offset += sizeof(DWORD) * 2;
		if (chunkType == fourcc)
		{
			chunkSize = chunkDataSize;
			chunkDataPosition = offset;
			return true;
		}

		offset += chunkDataSize;
		if (bytesRead >= riffDataSize)
		{
			return false;
		}
	}
	return true;
}

// Read a chunk of data of the specified size from the file at the specifed location into the supplied buffer
bool Audio::ReadChunkData(void* buffer, DWORD buffersize, DWORD bufferoffset)
{
	MoveData(bufferoffset, FILE_BEGIN);
	ReadData(buffer, buffersize);
	return true;
}

void Audio::PlayProc()
{
	WAVEFORMATEXTENSIBLE wfx = { 0 };
	WAVEFORMATEX waveFormat;
	DWORD chunkSize;
	DWORD chunkPosition;
	DWORD filetype;
	DSBUFFERDESC bufferDesc;
	IDirectSoundBuffer * tempBuffer;

	// Make sure we have a RIFF wave file
	FindChunk(fourccRIFF, chunkSize, chunkPosition);
	ReadChunkData(&filetype, sizeof(DWORD), chunkPosition);
	if (filetype != fourccWAVE) return;

	// Locate the 'fmt ' chunk, and copy its contents into a WAVEFORMATEXTENSIBLE structure. 
	FindChunk(fourccFMT, chunkSize, chunkPosition);
	ReadChunkData(&wfx, chunkSize, chunkPosition);

	// Find the audio data chunk
	FindChunk(fourccDATA, chunkSize, chunkPosition);

	// Set the wave format of the secondary buffer that this wave file will be loaded onto.
	// The value of wfx.Format.nAvgBytesPerSec will be very useful to you since it gives you
	// an approximate value for how many bytes it takes to hold one second of audio data.
	waveFormat.wFormatTag =  wfx.Format.wFormatTag;
	waveFormat.nSamplesPerSec = wfx.Format.nSamplesPerSec;
	waveFormat.wBitsPerSample = wfx.Format.wBitsPerSample;
	waveFormat.nChannels = wfx.Format.nChannels;
	waveFormat.nBlockAlign = wfx.Format.nBlockAlign;
	waveFormat.nAvgBytesPerSec = wfx.Format.nAvgBytesPerSec;
	waveFormat.cbSize = 0;

	// Set the buffer description of the secondary sound buffer that the wave file will be loaded onto. In
	// this example, we setup a buffer the same size as that of the audio data.  For the assignment, your
	// secondary buffer should only be large enough to hold approximately four seconds of data. 
	bufferDesc.dwSize = sizeof(DSBUFFERDESC);
	bufferDesc.dwFlags = DSBCAPS_CTRLVOLUME | DSBCAPS_GLOBALFOCUS | DSBCAPS_CTRLPOSITIONNOTIFY;
	bufferDesc.dwBufferBytes = 4 * waveFormat.nAvgBytesPerSec; // 4 seconds
	bufferDesc.dwReserved = 0;
	bufferDesc.lpwfxFormat = &waveFormat;
	bufferDesc.guid3DAlgorithm = GUID_NULL;

	// Create a temporary sound buffer with the specific buffer settings.
	HRESULT result = directSound->CreateSoundBuffer(&bufferDesc, &tempBuffer, NULL);
	if (FAILED(result)) return;

	// Test the buffer format against the direct sound 8 interface and create the secondary buffer.
	result = tempBuffer->QueryInterface(IID_IDirectSoundBuffer8, (void**)&secondaryBuffer);
	if (FAILED(result)) return;

	// Release the temporary buffer.
	tempBuffer->Release();

	PlayWave(waveFormat);
}

void Audio::PlayWave(WAVEFORMATEX waveFormat)
{
	HRESULT result;
	unsigned char*	bufferPtr1;
	unsigned long   bufferSize1;
	unsigned char*	bufferPtr2;
	unsigned long   bufferSize2;

	DWORD soundBytesOutput = 0;
	bool fillFirstHalf = true;
	LPDIRECTSOUNDNOTIFY8 directSoundNotify;
	DSBPOSITIONNOTIFY positionNotify[2];

	// Set position of playback at the beginning of the sound buffer.
	result = secondaryBuffer->SetCurrentPosition(0);
	if (FAILED(result)) return;

	// Set volume of the buffer to 100%.
	result = secondaryBuffer->SetVolume(DSBVOLUME_MAX);
	if (FAILED(result)) return;

	// Create an event for notification that playing has stopped.  This is only useful
	// when your audio file fits in the entire secondary buffer (as in this example).  
	// For the assignment, you are going to need notifications when the playback has reached the 
	// first quarter of the buffer or the third quarter of the buffer so that you know when 
	// you should copy more data into the secondary buffer. 
	HANDLE playEventHandles[2];
	playEventHandles[0] = CreateEvent(NULL, FALSE, FALSE, NULL);
	playEventHandles[1] = CreateEvent(NULL, FALSE, FALSE, NULL);

	result = secondaryBuffer->QueryInterface(IID_IDirectSoundNotify8, (LPVOID*)&directSoundNotify);
	if (FAILED(result)) return;

	// This notification is used to indicate that we have finished playing the buffer of audio. In
	// the assignment, you will need two different notifications as mentioned above. 
	positionNotify[0].dwOffset = waveFormat.nAvgBytesPerSec / 2;
	positionNotify[0].hEventNotify = playEventHandles[0];
	positionNotify[1].dwOffset = waveFormat.nAvgBytesPerSec * 3 / 2;
	positionNotify[1].hEventNotify = playEventHandles[1];
	directSoundNotify->SetNotificationPositions(2, positionNotify);

	// Now we can fill our secondary buffer and play it.  In the assignment, you will not be able to fill
	// the buffer all at once since the secondary buffer will not be large enough.  Instead, you will need to
	// loop through the data that you have retrieved from the server, filling different sections of the 
	// secondary buffer as you receive notifications.

	// Lock the first part of the secondary buffer to write wave data into it. In this case, we lock the entire
	// buffer, but for the assignment, you will only want to lock the half of the buffer that is not being played.
	// You will definately want to look up the methods for the IDIRECTSOUNDBUFFER8 interface to see what these
	// methods do and what the parameters are used for. 
	result = secondaryBuffer->Lock(0, waveFormat.nAvgBytesPerSec * 2, (void**)&bufferPtr1, (DWORD*)&bufferSize1, (void**)&bufferPtr2, (DWORD*)&bufferSize2, 0);
	if (FAILED(result)) return;

	// Copy the wave data into the buffer. If you need to insert some silence into the buffer, insert values of 0.
	ReadData(bufferPtr1, bufferSize1);
	if (bufferPtr2 != NULL) ReadData(bufferPtr2, bufferSize2);

	// Unlock the secondary buffer after the data has been written to it.
	result = secondaryBuffer->Unlock((void*)bufferPtr1, bufferSize1, (void*)bufferPtr2, bufferSize2);
	if (FAILED(result)) return;

	// Play the contents of the secondary sound buffer. If you want play to go back to the start of the buffer
	// again, set the last parameter to DSBPLAY_LOOPING instead of 0.  If play is already in progress, then 
	// play will just continue. 
    m_bAvailable = true;

	DWORD dwRetSamples = 0, dwRetBytes = 0;

	while(true)
	{
		// Wait for notifications.  In this case, we only have one notification so we could use WaitForSingleObject,
		// but for the assignment you will need more than one notification, so you will need WaitForMultipleObjects
		result = WaitForMultipleObjects(2, playEventHandles, FALSE, INFINITE);

		// In this case, we have been notified that playback has finished so we can just finish. In the assignment,
		// you should use the appropriate notification to determine which part of the secondary buffer needs to be
		// filled and handle it accordingly.

		if(WAIT_OBJECT_0 == result) 
		{
			//Lock DirectSoundBuffer Second Part
			HRESULT hr = secondaryBuffer->Lock(2 * waveFormat.nAvgBytesPerSec, 4 * waveFormat.nAvgBytesPerSec, 
				(void**)&bufferPtr1, (DWORD*)&bufferSize1, (void**)&bufferPtr2, (DWORD*)&bufferSize2, 0);
			if ( FAILED(hr) ) return;
		}
		else if (WAIT_OBJECT_0 + 1 == result) 
		{		
			//Lock DirectSoundBuffer First Part
			HRESULT hr = secondaryBuffer->Lock(0, 2 * waveFormat.nAvgBytesPerSec, (void**)&bufferPtr1, (DWORD*)&bufferSize1, 
				(void**)&bufferPtr2, (DWORD*)&bufferSize2, 0);
			if ( FAILED(hr) ) return;
		}
		else return;

		dwRetBytes = dwRetSamples * waveFormat.nBlockAlign;
	
		//If near the end of the audio data
		if (dwRetSamples < waveFormat.nSamplesPerSec) 
		{
			memset(bufferPtr1 + dwRetBytes, 0, waveFormat.nAvgBytesPerSec - dwRetBytes);				
		}
	
		//Copy AudioBuffer to DirectSoundBuffer
		ReadData(bufferPtr1, bufferSize1);
		if (NULL != bufferPtr2)
		{
			ReadData(bufferPtr2, bufferSize2);
		}
	
		//Unlock DirectSoundBuffer
		secondaryBuffer->Unlock(bufferPtr1, bufferSize1, bufferPtr2, bufferSize2);
		if(m_bEmpty) 
		{
			Pause();
			break;
		}
	}

	directSoundNotify->Release();
	CloseHandle(playEventHandles[0]);
	CloseHandle(playEventHandles[1]);
	Stop();
}

bool Audio::IsAvailable()
{
	return m_bAvailable;
}
	
void Audio::Start()
{
	_dataQueue.Clear();
	m_dwOffset = 0;
	m_dwSize = 0;
	m_bDone = false;
	m_bEmpty = false;

	m_hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)gPlayProc, this, 0, NULL);
	if(m_hThread == NULL) return;
}

void Audio::Play()
{
	if(secondaryBuffer != NULL) secondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);
}

void Audio::Pause()
{
	if(secondaryBuffer != NULL) secondaryBuffer->Stop();
}

void Audio::Done()
{
	m_bDone = true;
}

void Audio::Stop()
{
    m_bAvailable = false;

	Pause();

	if(m_hThread == NULL) return;
	if(::WaitForSingleObject(m_hThread, 1000) != WAIT_OBJECT_0)
	{
		m_hThread = NULL;
	}

	if (secondaryBuffer != NULL)
	{
		secondaryBuffer->Release();
		secondaryBuffer = NULL;
	}
}

void Audio::Shutdown()
{
	// Release the primary sound buffer pointer.
	if (primaryBuffer != NULL)
	{
		primaryBuffer->Release();
		primaryBuffer = NULL;
	}

	// Release the direct sound interface pointer.
	if (directSound != NULL)
	{
		directSound->Release();
		directSound = NULL;
	}
}
