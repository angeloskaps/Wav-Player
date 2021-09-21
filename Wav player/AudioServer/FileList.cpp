#include <windows.h>
#include "FileList.h"

FileList::FileList(void)
{
	_fileName = NULL;
	_next = NULL;
}


FileList::~FileList(void)
{
	if(_fileName != NULL) 
	{
		delete [] _fileName;
		_fileName = NULL;
	}
}
