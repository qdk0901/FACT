enum{
	DVC_ACT_CTRL = 101,
	DVC_TSK_CTRL = 102
};

class DataManager
{
public:
    DataManager(wxFrame* pFrame);
    ~DataManager();

	int setDataView(wxDataViewListCtrl* ctrl,int type);
	int addActItem(wxVector<wxVariant> data);
	int addTskItem(wxVector<wxVariant> data);
	int modifyTskItem(char* device,char* status,long progress);

private:
	wxDataViewListStore* m_actList;
	wxDataViewListStore* m_tskList;
	wxFrame* m_pFrame;
};


int dataModelInit(DataManager* dm);
int addActItem(char* path,char* type);
int addTskItem(char* device,char* status,long progress);
int modifyTskItem(char* device,char* status,long progress);