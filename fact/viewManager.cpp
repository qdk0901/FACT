#include "wx/wxprec.h"

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "wx/dataview.h"
#include "wx/notebook.h"
#include "wx/aboutdlg.h"
#include <wx/timer.h> 
#include "dataManager.h"
#include "usbManager.h"

enum
{
    ID_LOAD_CONFIG = wxID_HIGHEST+1,
    ID_START,
	ID_INSTALL_DRV,
    ID_ADD_IMAGE,
    ID_ERASE_DATA,
	ID_ERASE_CACHE,
	ID_ERASE_FAT,
	ID_REPART,
	ID_SETMAC,
	ID_REBOOT,
	ID_DELETE,
    ID_EXIT,
	ID_ABOUT,
	ID_ACTION_LIST,
};
class viewManagerApp: public wxApp
{
public:
    virtual bool OnInit();
};


class viewManagerFrame : public wxFrame
{
public:
    viewManagerFrame(wxFrame *frame, const wxString &title);
	void setError();
    ~viewManagerFrame();
private:
	void OnLoadConfig(wxCommandEvent& event);
	void OnStart(wxCommandEvent& event);
	void OnInstallDriver(wxCommandEvent& event);
	void OnAddImage(wxCommandEvent& event);
	void OnMisc(wxCommandEvent& event);
	void OnDelete(wxCommandEvent& event);
	void OnAbout(wxCommandEvent& event);
	void OnContextMenu( wxDataViewEvent &event );
	void OnSetError(wxCommandEvent& WXUNUSED(event));
	void OnUpdateData(wxCommandEvent& WXUNUSED(event));

	void updateTitle(wxString mode,wxString status);

	wxString m_mode;
	wxString m_status;

	wxNotebook* m_noteBook;
	wxTextCtrl* m_log;
    wxLog *m_logOld;
	DataManager* m_dm;
	wxDataViewListCtrl* m_actCtrl;
	wxDataViewListCtrl* m_tskCtrl;
	DECLARE_EVENT_TABLE()
};
BEGIN_EVENT_TABLE(viewManagerFrame, wxFrame)
	EVT_MENU( ID_LOAD_CONFIG, viewManagerFrame::OnLoadConfig)
	EVT_MENU( ID_START, viewManagerFrame::OnStart)
	EVT_MENU( ID_INSTALL_DRV, viewManagerFrame::OnInstallDriver)
	EVT_MENU( ID_ADD_IMAGE, viewManagerFrame::OnAddImage)
	EVT_MENU( ID_ERASE_DATA, viewManagerFrame::OnMisc)
	EVT_MENU( ID_ERASE_CACHE, viewManagerFrame::OnMisc)
	EVT_MENU( ID_ERASE_FAT, viewManagerFrame::OnMisc)
	EVT_MENU( ID_SETMAC, viewManagerFrame::OnMisc)
	EVT_MENU( ID_REPART, viewManagerFrame::OnMisc)
	EVT_MENU( ID_REBOOT, viewManagerFrame::OnMisc)
	EVT_MENU( ID_DELETE, viewManagerFrame::OnDelete)
	EVT_MENU( ID_ABOUT, viewManagerFrame::OnAbout)
	EVT_DATAVIEW_ITEM_CONTEXT_MENU(ID_ACTION_LIST, viewManagerFrame::OnContextMenu)
	EVT_COMMAND( wxID_ANY,DataManager::EVT_SET_ERROR,viewManagerFrame::OnSetError)
	EVT_COMMAND( wxID_ANY,DataManager::EVT_UPDATE_DATA,viewManagerFrame::OnUpdateData)
	
END_EVENT_TABLE() 

IMPLEMENT_APP(viewManagerApp)

bool viewManagerApp::OnInit()
{
    if ( !wxApp::OnInit() )
        return false;

    viewManagerFrame *frame =
        new viewManagerFrame(NULL, "");
	
    frame->Show(true);
    return true;
}


viewManagerFrame::viewManagerFrame(wxFrame *frame, const wxString &title):
	wxFrame(frame, wxID_ANY, title, wxPoint(-1, -1), wxSize(-1, -1)),
	m_mode(wxT("人工")),m_status(wxT("等待"))
{
	this->Maximize(true);


	SetIcon(wxICON(MAIN_ICON));

	wxMenu *op_menu = new wxMenu;
    op_menu->Append(ID_LOAD_CONFIG, wxT("批量加载"));
	op_menu->Append(ID_INSTALL_DRV, wxT("安装驱动"));
    op_menu->Append(ID_START, wxT("开始"));
    op_menu->AppendSeparator();
    op_menu->Append(ID_EXIT, wxT("退出"));

	wxMenu *about_menu = new wxMenu;
    about_menu->Append(ID_ABOUT, wxT("关于"));

    wxMenuBar *menu_bar = new wxMenuBar;
    menu_bar->Append(op_menu, wxT("操作"));
    menu_bar->Append(about_menu, wxT("关于"));
	SetMenuBar(menu_bar);


	DataManager::init(this);
	wxSize sz = this->GetClientSize();

	m_dm = DataManager::getManager();

	m_noteBook = new wxNotebook( this, wxID_ANY );

	wxSize ps = wxSize(sz.GetWidth(),2*sz.GetHeight()/3);
	//add image panel
	wxPanel *act_panel = new wxPanel( m_noteBook, wxID_ANY );
	m_actCtrl = new wxDataViewListCtrl(act_panel,ID_ACTION_LIST,wxDefaultPosition,ps,0);
	m_dm->bindView(m_actCtrl,DataManager::DVC_ACT_CTRL);
	m_actCtrl->SetMinSize(ps);

	wxSizer *act_sizer = new wxBoxSizer( wxVERTICAL );
	act_sizer->Add(m_actCtrl, 1, wxGROW|wxALL, 5);
	act_panel->SetSizerAndFit(act_sizer);

	//add task panel
	wxPanel *tsk_panel = new wxPanel( m_noteBook, wxID_ANY );
	m_tskCtrl = new wxDataViewListCtrl(tsk_panel,wxID_ANY,wxDefaultPosition,ps,0);
	m_dm->bindView(m_tskCtrl,DataManager::DVC_TSK_CTRL);
	m_tskCtrl->SetMinSize(ps);

	wxSizer *tsk_sizer = new wxBoxSizer( wxVERTICAL );
	tsk_sizer->Add(m_tskCtrl, 1, wxGROW|wxALL, 5);
	tsk_panel->SetSizerAndFit(tsk_sizer);


	m_noteBook->AddPage(act_panel, "任务列表");
    m_noteBook->AddPage(tsk_panel, "状态列表");

	wxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    
	wxSize ls = wxSize(sz.GetWidth(),sz.GetHeight()/3);
	// redirect logs from our event handlers to text control
    m_log = new wxTextCtrl( this, wxID_ANY, wxString(), wxDefaultPosition,
		ls, wxTE_MULTILINE|wxTE_READONLY);
    m_log->SetMinSize(ls);
	

    m_logOld = wxLog::SetActiveTarget(new wxLogTextCtrl(m_log));
    wxLogMessage( "日志窗口" );
	m_log->SetForegroundColour(*wxGREEN);
    m_log->SetBackgroundColour(*wxBLACK);

	mainSizer->Add( m_noteBook, 1, wxGROW );
	mainSizer->Add( m_log, 0, wxGROW );
	SetSizerAndFit(mainSizer);
	
	updateTitle(wxT(""),wxT(""));
	if(m_dm->checkAutoLoad() == 0){
		UsbManager::start(m_dm);
		m_noteBook->SetSelection(1);
		m_noteBook->Enable(false);
		updateTitle(wxT("自动"),wxT(""));
	}

	UsbManager::init();
	
}
void viewManagerFrame::OnInstallDriver(wxCommandEvent& event)
{
	UsbManager::installDriver();
}
void viewManagerFrame::updateTitle(wxString mode,wxString status)
{
	wxString t = wxT("刷机工具 1.01 -");
	
	if(mode != wxT(""))
		m_mode = mode;
	if(status != wxT(""))
		m_status = status;


	t = t + wxString::Format(wxT("模式[%s] 状态[%s]"),m_mode,m_status);

	SetTitle(t);
}
void viewManagerFrame::OnSetError(wxCommandEvent& event)
{
	int i = event.GetInt();
	if(i == 0){
		m_log->SetForegroundColour(*wxGREEN);
		m_log->SetBackgroundColour(*wxBLACK);
	}else{
		m_log->SetForegroundColour(*wxBLACK);
		m_log->SetBackgroundColour(*wxRED);	
	}
	m_log->Refresh();
	updateTitle(wxT(""),wxT("出错"));
}
void viewManagerFrame::OnUpdateData(wxCommandEvent& event)
{
	m_tskCtrl->Refresh();
	m_actCtrl->Refresh();
}
viewManagerFrame::~viewManagerFrame()
{
}
void viewManagerFrame::OnLoadConfig( wxCommandEvent& WXUNUSED(event) )
{
    wxFileDialog dialog
                 (
                    this,
                    wxT("选择sld文件"),
                    wxEmptyString,
                    wxEmptyString,
                    wxT("sld文件 (*.sld)|*.sld")
					);

    dialog.CentreOnParent();

    if (dialog.ShowModal() == wxID_OK)
    {
		char dir[MAX_PATH];
		char path[MAX_PATH];
		strcpy(path,dialog.GetPath().mb_str());
		strcpy(dir,dialog.GetDirectory().mb_str());
        int ret = m_dm->loadBash(dir,path);
		if(ret != 0){
			wxString info;
			info.Printf(wxT("无效的文件: %s\n"),
						dialog.GetPath().c_str()
						);
			wxMessageDialog dialog2(this, info, wxT("选择sld文件"));
			dialog2.ShowModal();
		}
    }	
}
void viewManagerFrame::OnStart( wxCommandEvent& WXUNUSED(event) )
{
	UsbManager::start(m_dm);
	m_noteBook->SetSelection(1);
	m_noteBook->Enable(false);
	updateTitle(wxT(""),wxT("运行中"));
}
void viewManagerFrame::OnAddImage( wxCommandEvent& WXUNUSED(event) )
{
    wxFileDialog dialog
                 (
                    this,
                    wxT("选择镜像文件"),
                    wxEmptyString,
                    wxEmptyString,
                    wxT("镜像文件 (*.*)|*.*")
					);

    dialog.CentreOnParent();

    if (dialog.ShowModal() == wxID_OK)
    {
		char path[MAX_PATH]={0};
		strcpy(path,dialog.GetPath().mb_str());
        char* part = m_dm->idenImageFile(path);
		if(part != NULL){
			m_dm->addActItem(DataManager::OP_FLASH,part,path);
		}else{
			wxString info;
			info.Printf(wxT("无效的镜像文件: %s\n"),
						dialog.GetPath().c_str()
						);
			wxMessageDialog dialog2(this, info, wxT("选择镜像文件"));
			dialog2.ShowModal();
		}
    }
}
void viewManagerFrame::OnMisc( wxCommandEvent& event)
{
	int id = event.GetId();
	event.Skip();

	int action = 0;
	char* value1="";
	switch(id){
		case ID_ERASE_DATA:
			action = DataManager::OP_ERASE;
			value1 = PART_DATA;
		break;
		case ID_ERASE_CACHE:
			action = DataManager::OP_ERASE;
			value1 = PART_CACHE;
		break;
		case ID_ERASE_FAT:
			action = DataManager::OP_ERASE;
			value1 = PART_FAT;
		break;
		case ID_SETMAC:
			action = DataManager::OP_SETMAC;
			value1 = "";
		break;
		case ID_REPART:
			action = DataManager::OP_REPART;
			value1 = "";
		break;
		case ID_REBOOT:
			action = DataManager::OP_REBOOT_TO_SYS;
			value1 = "";
		break;
	}
	if(action != 0)
		m_dm->addActItem(action,value1,"");

	
}
void viewManagerFrame::OnDelete( wxCommandEvent& WXUNUSED(event) )
{
	int n = m_actCtrl->GetSelectedRow();
	if(n >= 0)
		m_dm->delActItem(n);
}

void viewManagerFrame::OnAbout( wxCommandEvent& WXUNUSED(event) )
{
    wxAboutDialogInfo info;
    info.SetName(_("刷机工具v1.01"));
    info.SetDescription(_("Used for Mass Production"));
    info.SetCopyright(wxT("(C) 2012- SangGu Technology"));
    info.AddDeveloper("Derek Quan");

    wxAboutBox(info);
}
void viewManagerFrame::OnContextMenu( wxDataViewEvent& event)
{
    wxMenu menu;
	menu.Append(ID_ADD_IMAGE, "添加镜像");
    menu.Append(ID_ERASE_DATA, "格式化data分区");
	menu.Append(ID_ERASE_CACHE, "格式化cache分区");
	menu.Append(ID_ERASE_FAT, "格式化fat分区");
	menu.Append(ID_REPART, "重新分区");
	menu.Append(ID_SETMAC, "设置网卡地址");
	menu.Append(ID_REBOOT, "重启设备");
	menu.Append(ID_DELETE, "删除");
    m_actCtrl->PopupMenu(&menu);
}