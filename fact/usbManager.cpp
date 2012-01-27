#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "usbManager.h"

UsbManager* UsbManager::m_um = NULL;

int UsbManager::init(DataManager* dm)
{
	if(m_um == NULL){
		usb_init(); /* initialize the library */
		m_um = new UsbManager(dm);
	}
	return 0;
}
UsbManager::UsbManager(DataManager* dm)
	:wxThread(wxTHREAD_DETACHED)
{
	dm->getVidPid(&m_vid1s1,&m_pid1s1,DataManager::VID_PID_1S1);
	dm->getVidPid(&m_vid1s2,&m_pid1s2,DataManager::VID_PID_1S2);

	dm->getVidPid(&m_vid2,&m_pid2,DataManager::VID_PID_2);
	m_dm = dm;
	if(Create() == wxTHREAD_NO_ERROR){
		Run();
	}else{
		wxLogMessage( "Cannot create usb thread\n");
	}
}
void UsbManager::lockDevice(char* dev)
{
	m_opened.insert(dev);
}
void UsbManager::unlockDevice(char* dev)
{
	set<string>::iterator it = m_opened.find(dev);
	if(it != m_opened.end()){
		m_opened.erase(it);
	}	
}
int UsbManager::isLock(char* dev)
{
	set<string>::iterator it = m_opened.find(dev);
	if(it != m_opened.end()){
		return TRUE;
	}
	return FALSE;
}
int UsbManager::detect()
{
	struct usb_bus* bus;
	struct usb_device *dev;
	usb_find_busses(); /* find all busses */
	usb_find_devices(); /* find all connected devices */

	int dt = DEV_NULL;

	for(bus = usb_get_busses(); bus; bus = bus->next){
		for(dev = bus->devices; dev; dev = dev->next){
			int vid = dev->descriptor.idVendor;
			int pid = dev->descriptor.idProduct;

			dt = DEV_NULL;
			if( vid == m_vid1s1 && pid == m_pid1s1){
				dt = DEV_USBBOOT_S1;
			}else if( vid == m_vid1s2 && pid == m_pid1s2){
				dt = DEV_USBBOOT_S2;
			}else if(vid == m_vid2 && pid == m_pid2){
				dt = DEV_FASTBOOT;
			}
			if(dt != DEV_NULL && isLock(dev->filename) == FALSE){
				break;
			}
		}
	}
	return dt;
}
usb_dev_handle* UsbManager::openDevice(struct usb_device* ud)
{
	usb_dev_handle *dev = NULL;
	if (!(dev = usb_open(ud))){
        return NULL;
    }
	if (usb_set_configuration(dev, 1) < 0){
        usb_close(dev);
        return NULL;
    }
	
	if (usb_claim_interface(dev, 0) < 0) {
		usb_close(dev);
		return NULL;
	}
	lockDevice(ud->filename);
	return dev;
}
usb_dev_handle* UsbManager::getDevice(int d)
{
	struct usb_bus* bus;
	struct usb_device *dev;
	usb_find_busses(); /* find all busses */
	usb_find_devices(); /* find all connected devices */
	for(bus = usb_get_busses(); bus; bus = bus->next){
		for(dev = bus->devices; dev; dev = dev->next){
			int vid = dev->descriptor.idVendor;
			int pid = dev->descriptor.idProduct;

			if((d == DEV_USBBOOT_S1 && vid == m_vid1s1 && pid == m_pid1s1) ||
				( d == DEV_USBBOOT_S2 && vid == m_vid1s2 && pid == m_pid1s2) ||
				( d == DEV_FASTBOOT && vid == m_vid2 && pid == m_pid2)
				)
			{
				return openDevice(dev);
			}
		}
	}
	return NULL;
}
int UsbManager::releaseDevice(usb_dev_handle* dev)
{
	struct usb_device* ud;
	ud = usb_device(dev);
	usb_release_interface(dev,0);
	usb_close(dev);
	unlockDevice(ud->filename);
	return 0;
}

wxThread::ExitCode UsbManager::Entry()
{
	while(1){
		int stage = detect();
		if(stage == DEV_USBBOOT_S1 || stage == DEV_USBBOOT_S2){
			usb_dev_handle *dev = getDevice(stage);
			if(dev != NULL)
				new UsbbootThread(m_um,m_dm,stage,dev);
		}
		wxThread::Sleep(1000);
	}
	return 0;
}

UsbbootThread::UsbbootThread(UsbManager* um,DataManager* dm,int stage,usb_dev_handle *dev)
{
	m_um = um;
	m_dm = dm;
	m_stage = stage;
	m_dev = dev;
	if(Create() == wxTHREAD_NO_ERROR){
		Run();
	}
}
unsigned short UsbbootThread::check_sum(char* buffer,int size)
{
	unsigned short cs=0;
	for(int i=0;i<size;i++){
		cs += buffer[i];
	}
	return cs;
}

int UsbbootThread::usbDownload(usb_dev_handle* dev,char* filePath,int addr)
{
	FILE* f;
	int size;
	char* buffer;
	int n;

	f = fopen(filePath,"rb");
	if(!f){
		wxLogMessage(wxString::Format("%s open error",filePath));
		return FALSE; 
	}
	fseek(f , 0 , SEEK_END);
	size = ftell(f);
	rewind( f );

	buffer = (char*)malloc(size+10);
	if(!buffer){
		wxLogMessage("allocate buffer failed");
		fclose(f);
		return FALSE;
	}

	*((int*)buffer + 0) = addr;
	*((int*)buffer + 1) = size + 10;
	fread(&buffer[8],1,size,f);
	*((short*)(buffer + 8 + size)) = check_sum(&buffer[8],size);

	n = usb_bulk_write(dev,2,buffer,size + 10,5000);

	wxLogMessage(wxString::Format("download file:%s,%d bytes written",filePath,n));
	free(buffer);
	return TRUE;
}

int UsbbootThread::bootDevice()
{
	char bl2[MAX_PATH];
	char uboot[MAX_PATH];
	int add_bl2,add_uboot;
	int ret;

	struct usb_device* ud = usb_device(m_dev);
	m_dm->getBl2(bl2);
	m_dm->getUboot(uboot);
	m_dm->getAddress(&add_bl2,&add_uboot);

	if(m_stage == UsbManager::DEV_USBBOOT_S1){
		m_tm->update(ud->filename,"usbboot stage1");
		ret = usbDownload(m_dev,bl2,add_bl2);
	}else if(m_stage == UsbManager::DEV_USBBOOT_S2){
		m_tm->update(ud->filename,"usbboot stage2");
		ret = usbDownload(m_dev,uboot,add_uboot);
	}
	return ret;
}
wxThread::ExitCode UsbbootThread::Entry()
{
	int ret;
	m_tm = m_dm->newTask();
	ret = bootDevice();
	if(ret == TRUE)
		m_tm->update(long(100));
	else
		m_tm->update(long(0));
	m_um->releaseDevice(m_dev);
	return 0;
}