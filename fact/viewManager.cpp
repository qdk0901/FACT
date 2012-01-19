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

class viewManagerApp: public wxApp
{
public:
    virtual bool OnInit();
};


class viewManagerFrame : public wxFrame
{
public:
    viewManagerFrame(wxFrame *frame, const wxString &title, int x, int y, int w, int h);
    ~viewManagerFrame();
	void onTimer(wxTimerEvent& f_event);

	;

private:
	wxNotebook* m_noteBook;
	wxTextCtrl* m_log;
    wxLog *m_logOld;
	DataManager* m_dm;
	wxDataViewListCtrl* m_actCtrl;
	wxDataViewListCtrl* m_tskCtrl;
	wxTimer* m_timer;

	DECLARE_EVENT_TABLE()
};
BEGIN_EVENT_TABLE(viewManagerFrame, wxFrame)
	EVT_TIMER(0, viewManagerFrame::onTimer)
END_EVENT_TABLE() 

IMPLEMENT_APP(viewManagerApp)

bool viewManagerApp::OnInit()
{
    if ( !wxApp::OnInit() )
        return false;

    viewManagerFrame *frame =
        new viewManagerFrame(NULL, "Factory Flash Utility", 40, 40, 1000, 540);

    frame->Show(true);
    return true;
}



viewManagerFrame::viewManagerFrame(wxFrame *frame, const wxString &title, int x, int y, int w, int h):
  wxFrame(frame, wxID_ANY, title, wxPoint(x, y), wxSize(w, h))
{
	m_timer = new wxTimer(this,0);
	m_timer->Start(100);


	m_dm = new DataManager(this);
	dataModelInit(m_dm);

	m_noteBook = new wxNotebook( this, wxID_ANY );

	//add image panel
	wxPanel *act_panel = new wxPanel( m_noteBook, wxID_ANY );
	m_actCtrl = new wxDataViewListCtrl(act_panel,wxID_ANY,wxDefaultPosition,wxDefaultSize,0);
	m_dm->setDataView(m_actCtrl,DVC_ACT_CTRL);
	m_actCtrl->SetMinSize(wxSize(-1, 400));

	wxSizer *act_sizer = new wxBoxSizer( wxVERTICAL );
	act_sizer->Add(m_actCtrl, 1, wxGROW|wxALL, 5);
	act_panel->SetSizerAndFit(act_sizer);

	//add task panel
	wxPanel *tsk_panel = new wxPanel( m_noteBook, wxID_ANY );
	m_tskCtrl = new wxDataViewListCtrl(tsk_panel,wxID_ANY,wxDefaultPosition,wxDefaultSize,0);
	m_dm->setDataView(m_tskCtrl,DVC_TSK_CTRL);
	m_tskCtrl->SetMinSize(wxSize(-1, 400));

	wxSizer *tsk_sizer = new wxBoxSizer( wxVERTICAL );
	tsk_sizer->Add(m_tskCtrl, 1, wxGROW|wxALL, 5);
	tsk_panel->SetSizerAndFit(tsk_sizer);


	m_noteBook->AddPage(act_panel, "Actions List");
    m_noteBook->AddPage(tsk_panel, "Tasks List");

	wxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    

	// redirect logs from our event handlers to text control
    m_log = new wxTextCtrl( this, wxID_ANY, wxString(), wxDefaultPosition,
                            wxDefaultSize, wxTE_MULTILINE );
    m_log->SetMinSize(wxSize(800, 100));
    m_logOld = wxLog::SetActiveTarget(new wxLogTextCtrl(m_log));
    wxLogMessage( "This is the log window" );

	mainSizer->Add( m_noteBook, 1, wxGROW );
	mainSizer->Add( m_log, 0, wxGROW );
	SetSizerAndFit(mainSizer);

#if 1 //do some test
	addActItem("E:\\system.img","system");

	//m_imgCtrl->DeleteItem(0);
	wxDataViewListStore* p= (wxDataViewListStore*)m_actCtrl->GetModel();
	wxLogMessage( wxString::Format("row %d", p->GetCount()) );

	addTskItem("12345678","flashing system",90);
#endif
}

void viewManagerFrame::onTimer(wxTimerEvent& f_event)
{
		static int i=0;
		i = (i+10)%100;
		wxLogMessage( wxString::Format("row %d", i) );
		modifyTskItem("12345678","Flashing System",i);
}

viewManagerFrame::~viewManagerFrame()
{
}