#define MAX_DATAQUEUE	400
#define BLOCK_SIZE 1024

class Data
{
	public:
		Data(char* pData, int count);
		~Data();
		void Copy(char* pData, int& count);

		char*	m_pData;
		int		m_iSize;

		Data*	m_pNext;
};

class DataQueue
{
	public:
		DataQueue();
		~DataQueue();

		void	AddQueue(char* data, int count);
		bool	DelQueue(char* data, int& count);
		bool	IsExistFree();
		void	Clear();

	protected:
		Data*		m_pRoot;
		Data*		m_pLastNext;
		int			m_iCount;
};

