

#include "dataManager.h"

DataManager* DataManager::m_dm = NULL;

DataManager::DataManager(wxFrame* pFrame)
{
	m_pFrame = pFrame;
	m_actList = new wxDataViewListStore();
	m_tskList = new wxDataViewListStore();
}
int DataManager::bindView(wxDataViewListCtrl* ctrl,int type)
{
	wxSize sz = ctrl->GetClientSize();
	switch(type){
		case DVC_ACT_CTRL:
			{
				ctrl->AssociateModel(m_actList);
				ctrl->AppendTextColumn("Action");
				ctrl->AppendTextColumn("Type");
				break;
			}
		case DVC_TSK_CTRL:
			{
				
				ctrl->AssociateModel(m_tskList);
				
				ctrl->AppendTextColumn("Device",wxDATAVIEW_CELL_INERT,sz.GetWidth()/4);
				ctrl->AppendTextColumn("Boot Type",wxDATAVIEW_CELL_INERT,sz.GetWidth()/4);
				ctrl->AppendTextColumn("Status",wxDATAVIEW_CELL_INERT,sz.GetWidth()/4);
				ctrl->AppendProgressColumn( "Progress",wxDATAVIEW_CELL_INERT,sz.GetWidth()/10);
				break;
			}
		default:
			break;
	}

	
	return 0;
}
int DataManager::addActItem(char* path,char* type)
{
	wxVector<wxVariant> data;
    data.push_back( wxString::Format("%s", path) );
    data.push_back( wxString::Format("%s", type) );

	m_actList->AppendItem(data);
	return 0;
}
void DataManager::init(wxFrame* pFrame)
{
	if(m_dm == NULL){
		m_dm = new DataManager(pFrame);
	}
}
DataManager* DataManager::getManager()
{
	return m_dm;
}

TaskManager* DataManager::newTask()
{
	TaskManager* tm = new TaskManager(m_tskList);
	return tm;
}
void DataManager::getVidPid(int* vid,int* pid,int type)
{
	if(type == VID_PID_1S1){
		*vid = 0x04e8;
		*pid = 0x1234;
	}else if(type == VID_PID_1S2){
		*vid = 0x04e8;
		*pid = 0x1235;
	}else if(type == VID_PID_2){
		*vid = 0x18d1;
		*pid = 0x0005;
	}
}
void DataManager::getBl2(char* out)
{
	strcpy(out,"F:\\usbboot\\bin\\V210_USB.BL2.bin");
}
void DataManager::getUboot(char* out)
{
	strcpy(out,"F:\\usbboot\\bin\\u-boot.bin");
}
void DataManager::getAddress(int* bl2,int* uboot)
{
	*bl2 = 0xd0020010;
	*uboot = 0x23e00000;
}

TaskManager::TaskManager(wxDataViewListStore* p)
{
	wxVector<wxVariant> data;
	
	m_tskList = p;
	m_tskRow = m_tskList->GetCount();
	data.push_back( ""); //Device
    data.push_back( ""); //Boot Type
    data.push_back( ""); //Status
	data.push_back( 0 ); //progress

	m_tskList->AppendItem(data);
}
void TaskManager::update(char* device,char* bootType)
{
	m_tskList->SetValueByRow(device,m_tskRow,0);
	m_tskList->RowValueChanged(m_tskRow,0);

	m_tskList->SetValueByRow(bootType,m_tskRow,1);
	m_tskList->RowValueChanged(m_tskRow,1);
}
void TaskManager::update(char* status)
{
	m_tskList->SetValueByRow(status,m_tskRow,2);
	m_tskList->RowValueChanged(m_tskRow,2);
}
void TaskManager::update(long progress)
{
	m_tskList->SetValueByRow(progress,m_tskRow,3);
	m_tskList->RowValueChanged(m_tskRow,3);
}