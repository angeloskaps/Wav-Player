#pragma once

class FileList
{
	public:
		FileList(void);
		~FileList(void);

		char* _fileName;
		FileList* _next;
};

