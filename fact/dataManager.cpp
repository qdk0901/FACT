

#include "dataManager.h"
#include <time.h>


DataManager* DataManager::m_dm = NULL;

DataManager::DataManager(wxFrame* pFrame)
{
	macGenInit((unsigned) time(NULL));
	m_pFrame = pFrame;
	m_actList = new wxDataViewListStore();
	m_tskList = new wxDataViewListStore();
	m_partPath[PART_BL2]=getDefaultPath(PART_BL2);
	m_partPath[PART_UBOOT]=getDefaultPath(PART_UBOOT);
}
string DataManager::getDefaultPath(char* part)
{
	string s = wxGetCwd().ToStdString();
	if(!strcmp(part,PART_BL2)){
		s += "\\V210_USB.BL2.bin";
		return s;
	}
	else if(!strcmp(part,PART_UBOOT)){
		s += "\\u-boot.bin";
		return s;
	}
	
	return 0;
}
// i== 0, normal
// i== 1, exception
void DataManager::setError(int i)
{
	wxCommandEvent evt(EVT_SET_ERROR);
	evt.SetInt(i);
	wxPostEvent(m_pFrame->GetEventHandler(),evt);
}
void DataManager::updateData()
{
	wxCommandEvent evt(EVT_UPDATE_DATA);
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
		case OP_ERASE:
			return STR_ERASE;
		case OP_SETMAC:
			return STR_SETMAC;
		case OP_REPART:
			return STR_REPART;
		case OP_REBOOT_TO_SYS:
			return STR_REBOOT;
	}
	return NULL;
}
int DataManager::strToOp(const char* str)
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
	}else if(!strcmp(str,STR_REBOOT)){
		return OP_REBOOT_TO_SYS;
	}
	return -1;
}
int DataManager::addActItem(int operation,const char* value1,const char* value2)
{
	if(operation == OP_FLASH){
		m_partPath[value1] = value2;
	}
	wxVector<wxVariant> data;
	data.push_back( opToStr(operation) );
    data.push_back( wxString::Format("%s", value1) );
    data.push_back( wxString::Format("%s", value2) );

	m_actList->AppendItem(data);
	updateData();
	return 0;
}
int DataManager::delActItem(int n)
{
	string op;
	string part;
	wxVariant v;
	m_actList->GetValueByRow(v,n,0);
	op = v.GetString().ToStdString();
	
	if(op == STR_FLASH){
		m_actList->GetValueByRow(v,n,1);
		part = v.GetString().ToStdString();
		m_partPath.erase(part);	
	}
	m_actList->DeleteItem(n);
	updateData();
	return 0;
}
int DataManager::clearActItems()
{
	m_actList->DeleteAllItems();
	updateData();
	return 0;
}
int DataManager::walkActionList(int n,struct tk* t)
{
	if(n >= m_actList->GetCount())
		return FALSE;

	wxVariant v;
	m_actList->GetValueByRow(v,n,0);
	t->op = strToOp((char*)v.GetString().ToStdString().c_str());

	m_actList->GetValueByRow(v,n,1);
	strcpy(t->v1,v.GetString().mb_str());

	m_actList->GetValueByRow(v,n,2);
	strcpy(t->v2,v.GetString().mb_str());

	return TRUE;
}

#define IDEN_BUFFER 512
char* DataManager::idenImageFile(const char* path)
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
			return PART_UBOOT;
		}
	}else if(sz < 2*1024*1024){
		if(!strncmp(data+0x20,"ramdisk",strlen("ramdisk"))){
			return PART_RAMDISK;
		}
	}else if(sz < 4*1024*1024){
		int* p = (int*)data;
		if(p[0] == 0xe1a00000 && p[1] == 0xe1a00000 && p[2] == 0xe1a00000 && p[3] == 0xe1a00000){
			return PART_KERNEL;
		}
	}else if(sz > 100*1024*1024){
		for(int i=0;i<IDEN_BUFFER;i++){
			if(data[i] != 0)
				return 0;
		}
		return PART_SYSTEM;
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
char* DataManager::getFileData(char* file,int* size)
{
	map<string, struct fi>::iterator it = m_fileLoaded.find(file);
	if(it != m_fileLoaded.end()){
		*size = it->second.size;
		return it->second.data;
	}
	map<string, string>::iterator it2 = m_partPath.find(file);
	if(it2 == m_partPath.end()){
		return NULL;
	}
	char* filePath = (char*)it2->second.c_str();
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
int DataManager::loadBash(char* dir,char* bash)
{
	ifstream ifs(bash);
	string s,op,v1,v2;
	stringstream ss(s);
	if(!ifs){
		LOG("open file %s error",bash);
		return -1;
	}
	int ret = 0;
	clearActItems();
	while(getline(ifs,s))
	{
		ss.clear();
		ss.str(s);
		ss >> op;
		if(op == STR_FLASH){
			ss >> v1;
			ss >> v2;
			if(v1 == PART_UBOOT || v1 == PART_KERNEL || v1 == PART_RAMDISK || v1 == PART_SYSTEM || v1 == PART_RECOVERY){
				string p = dir;
				p = p + "\\" + v2;
				char* iden = idenImageFile(p.c_str());
				if(iden && v1 == iden){
					addActItem(strToOp(op.c_str()),v1.c_str(),p.c_str());
				}else{
					LOG("%s is not a valid image file",p);
					ret = -1;
					break;
				}
			}else{
					LOG("%s is not a valid partition",v1);
					ret = -1;
					break;
			}
		}else if(op == STR_ERASE){
			ss >> v1;
			if(v1 == PART_DATA || v1 == PART_CACHE || v1 == PART_FAT){
				addActItem(strToOp(op.c_str()),v1.c_str(),"");
			}else{
					LOG("%s is not a valid partition",v1);
					ret = -1;
					break;
			}
		}else if(op == STR_SETMAC || op == STR_REPART || op == STR_REBOOT){
			addActItem(strToOp(op.c_str()),"","");
		}else{
					LOG("%s is not a valid operation",op);
					ret = -1;
					break;
		}
	}
	if(ret < 0)
		clearActItems();
	return ret;
}

int DataManager::macGenInit(int seed)
{
	srand(seed);
	return 0;
}
int DataManager::macGen(char* out)
{
	sprintf(out,"00:03:%02X:%02X:%02X:%02X\0",rand()&0xFF,rand()&0xFF,rand()&0xFF,rand()&0xFF);
	return 0;
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
	m_dm->updateData();
}
void TaskManager::update(char* device,char* bootType)
{
	m_tskList->SetValueByRow(device,m_tskRow,0);
	m_tskList->SetValueByRow(bootType,m_tskRow,1);
	m_dm->updateData();
}
void TaskManager::update(char* status)
{
	m_tskList->SetValueByRow(status,m_tskRow,2);
	m_dm->updateData();
}
void TaskManager::update(long progress)
{
	m_tskList->SetValueByRow(progress,m_tskRow,3);
	m_dm->updateData();
}