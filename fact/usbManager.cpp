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
		USBDBG( "Cannot create usb thread");
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
	int i = 0;
	while(1){
		int stage = detect();
		if(stage == DEV_USBBOOT_S1 || stage == DEV_USBBOOT_S2){
			usb_dev_handle *dev = getDevice(stage);
			if(dev != NULL)
				new UsbbootThread(m_um,m_dm,stage,dev);
		}else if(stage == DEV_FASTBOOT){
			usb_dev_handle *dev = getDevice(stage);
			if(dev != NULL)
				new FastbootThread(m_um,m_dm,dev);		
		}
		wxThread::Sleep(1000);
	}
	return 0;
}

UsbbootThread::UsbbootThread(UsbManager* um,DataManager* dm,int stage,usb_dev_handle *dev)
{
	m_bulkIn = 0x81;
	m_bulkOut = 0x02;
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

int UsbbootThread::usbDownload(usb_dev_handle* dev,char* data,int size,int addr)
{
	char* buffer;
	int n;

	buffer = (char*)malloc(size+10);
	if(!buffer){
		USBDBG("allocate buffer failed");
		return -1;
	}

	*((int*)buffer + 0) = addr;
	*((int*)buffer + 1) = size + 10;
	memcpy(&buffer[8],data,size);
	*((short*)(buffer + 8 + size)) = check_sum(&buffer[8],size);

	n = usb_bulk_write(dev,m_bulkOut,buffer,size + 10,USB_BULK_TIMEOUT);

	USBDBG("download file:%d bytes written",n);
	free(buffer);
	return n;
}

int UsbbootThread::bootDevice()
{
	char* bl2;
	char* uboot;
	int size_bl2,size_uboot;
	int add_bl2,add_uboot;
	int ret = -1;

	struct usb_device* ud = usb_device(m_dev);
	m_dm->getAddress(&add_bl2,&add_uboot);

	if(m_stage == UsbManager::DEV_USBBOOT_S1){
		bl2 = m_dm->getFileData(DataManager::FILE_BL2,&size_bl2);
		m_tm->update(ud->filename,"usbboot stage1");
		if(bl2){
			ret = usbDownload(m_dev,bl2,size_bl2,add_bl2);
		}
	}else if(m_stage == UsbManager::DEV_USBBOOT_S2){
		uboot = m_dm->getFileData(DataManager::FILE_UBOOT,&size_uboot);
		m_tm->update(ud->filename,"usbboot stage2");
		if(uboot){
			ret = usbDownload(m_dev,uboot,size_uboot,add_uboot);
		}
	}
	return ret;
}
wxThread::ExitCode UsbbootThread::Entry()
{
	int ret;
	m_tm = m_dm->newTask();
	ret = bootDevice();
	if(ret > 0 ){
		m_tm->update(long(100));
		m_um->releaseDevice(m_dev);
	}
	else
		m_tm->update(long(0));
	
	return 0;
}

FastbootThread::FastbootThread(UsbManager* um,DataManager* dm,usb_dev_handle *dev)
{
	m_bulkIn = 0x81;
	m_bulkOut = 0x02;
	m_um = um;
	m_dm = dm;
	m_dev = dev;
	if(Create() == wxTHREAD_NO_ERROR){
		Run();
	}
}

int FastbootThread::checkResponse(unsigned size,unsigned dataOk,char* response)
{
	unsigned char status[65];
    int r;

    for(;;) {
        r = usb_bulk_read(m_dev, m_bulkIn, (char*)status, 64,USB_BULK_TIMEOUT);
        if(r < 0) {
            USBDBG("status read failed (%s)", strerror(errno));
            return -1;
        }
        status[r] = 0;

        if(r < 4) {
            USBDBG("status malformed (%d bytes)", r);
            return -1;
        }

        if(!memcmp(status, "INFO", 4)) {
            USBDBG("(bootloader) %s\n", status + 4);
            continue;
        }

        if(!memcmp(status, "OKAY", 4)) {
            if(response) {
                strcpy(response, (char*) status + 4);
            }
            return 0;
        }

        if(!memcmp(status, "FAIL", 4)) {
            if(r > 4) {
                USBDBG("remote: %s", status + 4);
            } else {
                USBDBG("remote failure");
            }
            return -1;
        }

        if(!memcmp(status, "DATA", 4) && dataOk){
            unsigned dsize = strtoul((char*) status + 4, 0, 16);
            if(dsize > size) {
                USBDBG("data size too large");
                return -1;
            }
            return dsize;
        }

        USBDBG("unknown status code");
        break;
    }
    return -1;
}
int FastbootThread::sendCommandFull(const char* cmd,const void* data,unsigned size,char* response)
{
	int cmdsize = strlen(cmd);
    int r;
    
    if(response) {
        response[0] = 0;
    }

    if(cmdsize > 64) {
        USBDBG("command too large");
        return -1;
    }

    if(usb_bulk_write(m_dev, m_bulkOut,(char*)cmd, cmdsize,USB_BULK_TIMEOUT) != cmdsize) {
        USBDBG("command write failed (%s)", strerror(errno));
        return -1;
    }

    if(data == 0) {
        return checkResponse(size, 0, response);
    }

    r = checkResponse(size, 1, 0);
    if(r < 0) {
        return -1;
    }
    size = r;

    if(size) {
        r = usb_bulk_write(m_dev, m_bulkOut, (char*)data, size,USB_BULK_TIMEOUT);
        if(r < 0) {
            USBDBG("data transfer failure (%s)", strerror(errno));
            return -1;
        }
        if(r != ((int) size)) {
            USBDBG("data transfer failure (short transfer)");
            return -1;
        }
    }
    
    r = checkResponse(0, 0, 0);
    if(r < 0) {
        return -1;
    } else {
        return size;
    }
}
int FastbootThread::sendCommand(const char* cmd)
{
	return sendCommandFull(cmd, 0, 0, 0);
}
int FastbootThread::sendCommand(const char* cmd,char* response)
{
	return sendCommandFull(cmd, 0, 0, response);
}
int FastbootThread::downloadData(const void* data,unsigned size)
{
    char cmd[64];
    int r;
    
    sprintf(cmd, "download:%08x", size);
    r = sendCommandFull(cmd, data, size, 0);
    
    if(r < 0) {
        return -1;
    } else {
        return 0;
    }	
}

wxThread::ExitCode FastbootThread::Entry()
{
	char response[65];
	char cmd[65] = "oem setmac 11:22:33:44:55:66";
	m_tm = m_dm->newTask();

	struct usb_device* ud = usb_device(m_dev);
	m_tm->update(ud->filename,"fastboot");

	sendCommand(cmd,response);
	USBDBG("%s",response);

	m_um->releaseDevice(m_dev);

	return 0;
}