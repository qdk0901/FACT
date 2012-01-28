

#include "dataManager.h"

DataManager* DataManager::m_dm = NULL;

DataManager::DataManager(wxFrame* pFrame)
{
	m_pFrame = pFrame;
	m_actList = new wxDataViewListStore();
	m_tskList = new wxDataViewListStore();
}
// i== 0, normal
// i== 1, exception
void DataManager::setError(int i)
{
	wxCommandEvent evt(EVT_SET_ERROR);
	evt.SetInt(i);
	wxPostEvent(m_pFrame->GetEventHandler(),evt);
}
int DataManager::bindView(wxDataViewListCtrl* ctrl,int type)
{
	wxSize sz = ctrl->GetClientSize();
	switch(type){
		case DVC_ACT_CTRL:
			{
				ctrl->AssociateModel(m_actList);
				ctrl->AppendTextColumn("Action",wxDATAVIEW_CELL_INERT,sz.GetWidth()/3);
				ctrl->AppendTextColumn("Value1",wxDATAVIEW_CELL_INERT,sz.GetWidth()/10);
				ctrl->AppendTextColumn("Value2",wxDATAVIEW_CELL_INERT,sz.GetWidth()/3);
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
char* DataManager::opToStr(int op)
{
	switch(op)
	{
		case OP_FLASH:
			return STR_FLASH;
		break;
		case OP_ERASE:
			return STR_ERASE;
		break;
		case OP_SETMAC:
			return STR_SETMAC;
		case OP_REPART:
			return STR_REPART;
	}
	return NULL;
}
int DataManager::strToOp(char* str)
{
	if(!str){
		return -1;
	}else if(!strcmp(str,STR_FLASH)){
		return OP_FLASH;
	}else if(!strcmp(str,STR_ERASE)){
		return OP_ERASE;
	}else if(!strcmp(str,STR_SETMAC)){
		return OP_SETMAC;
	}else if(!strcmp(str,STR_REPART)){
		return OP_REPART;
	}
	return -1;
}
char* DataManager::fileToPart(int file)
{
	switch(file)
	{
		case FILE_UBOOT:
			return PART_UBOOT;
		break;
		case FILE_KERNEL:
			return PART_KERNEL;
		break;
		case FILE_RAMDISK:
			return PART_RAMDISK;
		case FILE_SYSTEM:
			return PART_SYSTEM;
		case FILE_RECOVERY:
			return PART_RECOVERY;
	}
	return NULL;	
}

int DataManager::addActItem(int operation,char* value1,char* value2)
{
	wxVector<wxVariant> data;
	data.push_back( opToStr(operation) );
    data.push_back( wxString::Format("%s", value1) );
    data.push_back( wxString::Format("%s", value2) );

	m_actList->AppendItem(data);
	return 0;
}

#define IDEN_BUFFER 512
int DataManager::idenImageFile(char* path)
{
	int sz;
    FILE* f;
	char data[IDEN_BUFFER];

    f = fopen(path, "rb");
    if(!f) return 0;

    fseek(f , 0 , SEEK_END);
	sz = ftell(f);
	rewind( f );
	if(sz < IDEN_BUFFER)
		return 0;

	if(fread(data,1,IDEN_BUFFER,f) != IDEN_BUFFER)
		return 0;
	fclose(f);

	if(sz < 1024*1024){
		int* p = (int*)data;
		if(p[0] == 0x2000 && p[1] == 0 && p[2] == 0 && p[3] == 0){
			return FILE_UBOOT;
		}
	}else if(sz < 2*1024*1024){
		if(!strncmp(data+0x20,"ramdisk",strlen("ramdisk"))){
			return FILE_RAMDISK;
		}
	}else if(sz < 4*1024*1024){
		int* p = (int*)data;
		if(p[0] == 0xe1a00000 && p[1] == 0xe1a00000 && p[2] == 0xe1a00000 && p[3] == 0xe1a00000){
			return FILE_KERNEL;
		}
	}else if(sz > 100*1024*1024){
		for(int i=0;i<IDEN_BUFFER;i++){
			if(data[i] != 0)
				return 0;
		}
		return FILE_SYSTEM;
	}
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
	TaskManager* tm = new TaskManager(m_dm,m_tskList);
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
		*pid = 0x0002;
	}
}
char* DataManager::loadFile(char* fn,int* _sz)
{
    char *data;
    int sz;
    FILE* f;

    data = 0;
    f = fopen(fn, "rb");
    if(!f) return 0;

    fseek(f , 0 , SEEK_END);
	sz = ftell(f);
	rewind( f );
    if(sz < 0) goto oops;

    if(fseek(f, 0, SEEK_SET) != 0) goto oops;

    data = (char*) malloc(sz);
    if(data == 0) goto oops;

    int n =fread(data,1, sz,f);
	if(n != sz) goto oops;
    fclose(f);

    if(_sz) *_sz = sz;
    return data;

oops:
    fclose(f);
    if(data != 0) free(data);
    return 0;	
}
char* DataManager::getFileData(int file,int* size)
{
	map<int, struct fi>::iterator it = m_fileLoaded.find(file);
	if(it != m_fileLoaded.end()){
		*size = it->second.size;
		return it->second.data;
	}
	char filePath[MAX_PATH] = {0};
	switch(file)
	{
		case FILE_BL2:
			strcpy(filePath,"F:\\usbboot\\bin\\V210_USB.BL2.bin");
		break;
		case FILE_UBOOT:
			strcpy(filePath,"F:\\usbboot\\bin\\u-boot.bin");
		break;
	}
	char* data;
	data = loadFile(filePath,size);
	if(data){
		m_fileLoaded.insert(std::make_pair(file,fi(data,*size)));
	}
	return data;
}
void DataManager::getAddress(int* bl2,int* uboot)
{
	*bl2 = 0xd0020010;
	*uboot = 0x23e00000;
}

TaskManager::TaskManager(DataManager* dm,wxDataViewListStore* p)
{
	wxVector<wxVariant> data;
	
	m_dm = dm;
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
	m_tskList->SetValueByRow(bootType,m_tskRow,1);
}
void TaskManager::update(char* status)
{
	m_tskList->SetValueByRow(status,m_tskRow,2);
}
void TaskManager::update(long progress)
{
	m_tskList->SetValueByRow(progress,m_tskRow,3);
}