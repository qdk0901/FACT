#include "wx/wxprec.h"

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "wx/dataview.h"
#include "wx/notebook.h"
#include <wx/timer.h> 
#include "dataManager.h"
#include "usbManager.h"

class viewManagerApp: public wxApp
{
public:
    virtual bool OnInit();
};


class viewManagerFrame : public wxFrame
{
public:
    viewManagerFrame(wxFrame *frame, const wxString &title);
	
    ~viewManagerFrame();
private:
	wxNotebook* m_noteBook;
	wxTextCtrl* m_log;
    wxLog *m_logOld;
	DataManager* m_dm;
	wxDataViewListCtrl* m_actCtrl;
	wxDataViewListCtrl* m_tskCtrl;
	DECLARE_EVENT_TABLE()
};
BEGIN_EVENT_TABLE(viewManagerFrame, wxFrame)
END_EVENT_TABLE() 

IMPLEMENT_APP(viewManagerApp)

bool viewManagerApp::OnInit()
{
    if ( !wxApp::OnInit() )
        return false;

    viewManagerFrame *frame =
        new viewManagerFrame(NULL, "Factory Flash Utility");
	
    frame->Show(true);
    return true;
}


viewManagerFrame::viewManagerFrame(wxFrame *frame, const wxString &title):
	wxFrame(frame, wxID_ANY, title, wxPoint(-1, -1), wxSize(-1, -1))
{
	//FIXME:when task item added,the UI not update simultaneously,
	//add a timer can fix this problem temporary
	(new wxTimer(this,0))->Start(100);

	this->Maximize(true);
	DataManager::init(this);
	wxSize sz = this->GetClientSize();

	m_dm = DataManager::getManager();

	m_noteBook = new wxNotebook( this, wxID_ANY );

	wxSize ps = wxSize(sz.GetWidth(),2*sz.GetHeight()/3);
	//add image panel
	wxPanel *act_panel = new wxPanel( m_noteBook, wxID_ANY );
	m_actCtrl = new wxDataViewListCtrl(act_panel,wxID_ANY,wxDefaultPosition,ps,0);
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


	m_noteBook->AddPage(act_panel, "Actions List");
    m_noteBook->AddPage(tsk_panel, "Tasks List");

	wxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    
	wxSize ls = wxSize(sz.GetWidth(),sz.GetHeight()/3);
	// redirect logs from our event handlers to text control
    m_log = new wxTextCtrl( this, wxID_ANY, wxString(), wxDefaultPosition,
                            ls, wxTE_MULTILINE );
    m_log->SetMinSize(ls);
    m_logOld = wxLog::SetActiveTarget(new wxLogTextCtrl(m_log));
    wxLogMessage( "This is the log window" );

	mainSizer->Add( m_noteBook, 1, wxGROW );
	mainSizer->Add( m_log, 0, wxGROW );
	SetSizerAndFit(mainSizer);
	UsbManager::init(m_dm);
}
viewManagerFrame::~viewManagerFrame()
{
}