#ifndef DATA_MANAGER_H
#define DATA_MANAGER_H

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "wx/dataview.h"

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
	TaskManager(wxDataViewListStore* p);
	int m_tskRow;
	wxDataViewListStore* m_tskList;
};

class DataManager
{
public:
	static const int DVC_ACT_CTRL = 101;
	static const int DVC_TSK_CTRL = 102;
	static const int VID_PID_1S1 = 103;
	static const int VID_PID_1S2 = 104;
	static const int VID_PID_2 = 105;
    ~DataManager();

	int bindView(wxDataViewListCtrl* ctrl,int type);
	int addActItem(char* path,char* type);
	void getVidPid(int* vid,int* pid,int type);
	void getBl2(char* out);
	void getUboot(char* out);
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
};
#endif