#ifndef DATA_MANAGER_H
#define DATA_MANAGER_H

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "wx/dataview.h"
#include <map>
using namespace std;

#define MAX_PATH 260

#define STR_FLASH  "Flash Partition"
#define STR_ERASE  "Erase Partition"
#define STR_SETMAC "Reset Mac Address"
#define STR_REPART "Repartition"

#define PART_UBOOT "bootloader"
#define PART_KERNEL	"kernel"
#define PART_RAMDISK "ramdisk"
#define PART_SYSTEM "system"
#define PART_RECOVERY "recovery"
#define PART_DATA "userdata"
#define PART_CACHE "cache"
#define PART_FAT "fat"

class DataManager;

class TaskManager
{
public:
	friend class DataManager;
	~TaskManager();
	void update(char* device,char* bootType);
	void update(char* status);
	void update(long progress);
private:
	DataManager* m_dm;
	TaskManager(DataManager* dm,wxDataViewListStore* p);
	int m_tskRow;
	wxDataViewListStore* m_tskList;
};

struct fi
{
	char* data;
	int size;

	fi():data(0),size(0){}
	fi(char* x,int y):data(x),size(y){}
};
class DataManager
{
public:
	static const int DVC_ACT_CTRL = 101;
	static const int DVC_TSK_CTRL = 102;
	static const int VID_PID_1S1 = 103;
	static const int VID_PID_1S2 = 104;
	static const int VID_PID_2 = 105;
	
	static const int FILE_BL2 = 106;
	static const int FILE_UBOOT = 107;
	static const int FILE_KERNEL = 108;
	static const int FILE_RAMDISK = 109;
	static const int FILE_SYSTEM = 110;
	static const int FILE_RECOVERY = 111;

	static const int OP_FLASH = 112;
	static const int OP_ERASE = 113;
	static const int OP_SETMAC = 114;
	static const int OP_REPART = 115;
	static const int EVT_SET_ERROR = wxID_HIGHEST + 100;
	static const int EVT_UPDATE_CTRL = wxID_HIGHEST + 101;
    ~DataManager();

	void setError(int i);
	int bindView(wxDataViewListCtrl* ctrl,int type);
	int addActItem(int operation,char* value1,char* value2);
	int idenImageFile(char* path);
	char* fileToPart(int file);

	void getVidPid(int* vid,int* pid,int type);

	char* loadFile(char* path,int* size);
	char* getFileData(int file,int* size);
	void getAddress(int* bl2,int* uboot);

	TaskManager* newTask();
	
	static void init(wxFrame* pFrame);
	static DataManager* getManager();
private:
	
	static DataManager* m_dm;

    DataManager(wxFrame* pFrame);
	wxDataViewListStore* m_actList;
	wxDataViewListStore* m_tskList;
	wxFrame* m_pFrame;

	char* opToStr(int op);
	int strToOp(char* str);
	map<int,struct fi> m_fileLoaded;
};
#endif