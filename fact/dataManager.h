#ifndef DATA_MANAGER_H
#define DATA_MANAGER_H

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "wx/dataview.h"
#include <map>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
using namespace std;

#define MAX_PATH 260

#define STR_FLASH  "FLASH_PART"
#define STR_ERASE  "ERASE_PART"
#define STR_SETMAC "SET_MAC"
#define STR_REPART "REPART"
#define STR_REBOOT "REBOOT"

#define PART_BL2 "bl2"
#define PART_UBOOT "bootloader"
#define PART_KERNEL	"kernel"
#define PART_RAMDISK "ramdisk"
#define PART_SYSTEM "system"
#define PART_RECOVERY "recovery"
#define PART_DATA "userdata"
#define PART_CACHE "cache"
#define PART_FAT "fat"

#define LOG(fmt,...) wxLogMessage(wxString::Format(fmt, ##__VA_ARGS__))

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
struct tk
{
	int op;
	char v1[MAX_PATH];
	char v2[MAX_PATH];
};
class DataManager
{
public:
	static const int DVC_ACT_CTRL = 101;
	static const int DVC_TSK_CTRL = 102;
	static const int VID_PID_1S1 = 103;
	static const int VID_PID_1S2 = 104;
	static const int VID_PID_2 = 105;
	
	static const int OP_FLASH = 112;
	static const int OP_ERASE = 113;
	static const int OP_SETMAC = 114;
	static const int OP_REPART = 115;
	static const int OP_REBOOT_TO_SYS = 116;
	static const int EVT_SET_ERROR = wxID_HIGHEST + 100;
	static const int EVT_UPDATE_DATA = wxID_HIGHEST + 101;
    ~DataManager();

	void setError(int i);
	int bindView(wxDataViewListCtrl* ctrl,int type);
	int addActItem(int operation,const char* value1,const char* value2);
	int delActItem(int row);
	int clearActItems();
	char* idenImageFile(const char* path);

	void getVidPid(int* vid,int* pid,int type);

	char* loadFile(char* path,int* size);
	char* getFileData(char* file,int* size);
	void getAddress(int* bl2,int* uboot);
	int walkActionList(int current,struct tk* t);
	int loadBash(const char* dir,const char* bash);
	int macGenInit(int seed);
	int macGen(char* out);
	void updateData();
	int checkAutoLoad();

	TaskManager* newTask();
	
	
	static void init(wxFrame* pFrame);
	static DataManager* getManager();
private:
	string getDefaultPath(char* part);
	static DataManager* m_dm;

    DataManager(wxFrame* pFrame);
	wxDataViewListStore* m_actList;
	wxDataViewListStore* m_tskList;
	wxFrame* m_pFrame;

	char* opToStr(int op);
	int strToOp(const char* str);

	map<string,struct fi> m_fileLoaded;
	map<string,string> m_partPath;
};
#endif