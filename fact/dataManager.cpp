
#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "wx/dataview.h"
#include "dataManager.h"


DataManager::DataManager(wxFrame* pFrame)
{
	m_pFrame = pFrame;
	m_actList = new wxDataViewListStore();
	m_tskList = new wxDataViewListStore();
}
int DataManager::setDataView(wxDataViewListCtrl* ctrl,int type)
{
	switch(type){
		case DVC_ACT_CTRL:
			{
				ctrl->AssociateModel(m_actList);
				ctrl->AppendTextColumn("action");
				ctrl->AppendTextColumn("type");
				break;
			}
		case DVC_TSK_CTRL:
			{
				
				ctrl->AssociateModel(m_tskList);

				ctrl->AppendTextColumn("device");
				ctrl->AppendTextColumn("status");
				ctrl->AppendProgressColumn( "Progress" );
				break;
			}
		default:
			break;
	}

	
	return 0;
}
int DataManager::addActItem(wxVector<wxVariant> data)
{
	m_actList->AppendItem(data);
	return 0;
}
int DataManager::addTskItem(wxVector<wxVariant> data)
{
	m_tskList->AppendItem(data);
	return 0;
}
int DataManager::modifyTskItem(char* device,char* status,long progress)
{
	int c = m_tskList->GetCount();
	for(int i = 0 ; i < c ; i++){
		wxVariant data;
		m_tskList->GetValueByRow(data,i,0);
		if(!strcmp(data.GetString(),device)){
			m_tskList->SetValueByRow(status,i,1);
			m_tskList->RowValueChanged(i,1);
			m_tskList->SetValueByRow(progress,i,2);
			m_tskList->RowValueChanged(i,2);
		}
	}
	return 0;
}

DataManager* g_dm;

int dataModelInit(DataManager* dm)
{
	g_dm = dm;
	return 0;
}
int addActItem(char* path,char* type)
{
	wxVector<wxVariant> data;
    data.push_back( wxString::Format("%s", path) );
    data.push_back( wxString::Format("%s", type) );

    g_dm->addActItem( data );
	return 0;
}
int addTskItem(char* device,char* status,long progress)
{
	wxVector<wxVariant> data;
    data.push_back( wxString::Format("%s", device) );
    data.push_back( wxString::Format("%s", status) );
	data.push_back( progress );
    g_dm->addTskItem( data );
	return 0;
}
int modifyTskItem(char* device,char* status,long progress)
{
	g_dm->modifyTskItem(device,status,progress);
	return 0;
}