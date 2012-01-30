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
		LOG( "Cannot create usb thread");
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
void UsbManager::setError()
{
	m_isError = TRUE;
}
wxThread::ExitCode UsbManager::Entry()
{
	int i = 0;
	m_isError = FALSE;
	while(1){
		if(m_isError == FALSE){
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
		}else{
			m_dm->setError((i++)&1); //set error blind
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
		LOG("allocate buffer failed");
		return -1;
	}

	*((int*)buffer + 0) = addr;
	*((int*)buffer + 1) = size + 10;
	memcpy(&buffer[8],data,size);
	*((short*)(buffer + 8 + size)) = check_sum(&buffer[8],size);

	n = usb_bulk_write(dev,m_bulkOut,buffer,size + 10,USB_BULK_TIMEOUT);

	LOG("download file:%d bytes written",n);
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
		bl2 = m_dm->getFileData(PART_BL2,&size_bl2);
		m_tm->update(ud->filename,"usbboot stage1");
		if(bl2){
			ret = usbDownload(m_dev,bl2,size_bl2,add_bl2);
		}
	}else if(m_stage == UsbManager::DEV_USBBOOT_S2){
		uboot = m_dm->getFileData(PART_UBOOT,&size_uboot);
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
	char status[65];
    int r;

    for(;;) {
        r = usb_bulk_read(m_dev, m_bulkIn, (char*)status, 64,USB_BULK_TIMEOUT);
        if(r < 0) {
            LOG("status read failed (%s)", strerror(errno));
            return -1;
        }
        status[r] = 0;

        if(r < 4) {
            LOG("status malformed (%d bytes)", r);
            return -1;
        }

        if(!memcmp(status, "INFO", 4)) {
            int p = 0;
			sscanf(status + strlen("INFO PROGRESS:"),"%d",&p);
			m_tm->update(long(p));
            continue;
        }

        if(!memcmp(status, "OKAY", 4)) {
            if(response) {
                strcpy(response, status + 4);
            }
            return 0;
        }

        if(!memcmp(status, "FAIL", 4)) {
            if(r > 4) {
                LOG("remote: %s", status + 4);
            } else {
                LOG("remote failure");
            }
            return -1;
        }

        if(!memcmp(status, "DATA", 4) && dataOk){
            unsigned dsize = strtoul((char*) status + 4, 0, 16);
            if(dsize > size) {
                LOG("data size too large");
                return -1;
            }
            return dsize;
        }

        LOG("unknown status code");
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
        LOG("command too large");
        return -1;
    }

    if(usb_bulk_write(m_dev, m_bulkOut,(char*)cmd, cmdsize,USB_BULK_TIMEOUT) != cmdsize) {
        LOG("command write failed (%s)", strerror(errno));
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

		long long frame = 0x40000;
		long long rest = size;
		do{
			if(frame > rest)
				frame = rest;
			r = usb_bulk_write(m_dev, m_bulkOut, 
					(char*)((unsigned)data + size - rest), frame,USB_BULK_TIMEOUT);
			if(r != (int)frame) {
				LOG("data transfer failure (%s)", strerror(errno));
				return -1;
			}
			rest -= frame;
			int p = 100*(size - rest)/size;
			m_tm->update(long(p));
		}while(rest > 0);
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
int FastbootThread::flashPart(char* part)
{
	char status[64];
	LOG("flash part:%s",part);
	char* data;
	int size;
	data = m_dm->getFileData(part,&size);
	if(!data){
		LOG("cannot get image %s",part);
		return -1;
	}

	sprintf(status,"downloading %s ...",part);
	m_tm->update(status);
	if(downloadData(data,size) < 0){
		LOG("download %s failed",part);
		return -1;
	}

	sprintf(status,"flashing %s ...",part);
	m_tm->update(status);

	char cmd[64];
	char response[64];
	sprintf(cmd, "flash:%s", part);
	if(sendCommand(cmd,response) < 0){
		LOG("flash %s failed",part);
		return -1;
	}
	LOG(".............done!");
	return 0;
}
int FastbootThread::erasePart(char* part)
{
	char status[64];
	LOG("erase part:%s",part);
	sprintf(status,"erasing %s ...",part);
	m_tm->update(status);
	m_tm->update(long(0));
	char cmd[64];
	sprintf(cmd, "erase:%s", part);
	if(sendCommand(cmd) < 0){
		LOG("erase %s failed",part);
		return -1;
	}
	m_tm->update(100);
	LOG(".............done!");
	return 0;
}
int FastbootThread::repart()
{
	LOG("repart");
	m_tm->update("repart");
	LOG(".............done!");
	return 0;
}
int FastbootThread::resetMacAddr()
{
	LOG("reset mac address");
	char cmd[64];
	char mac[20];
	char response[64];
	m_dm->macGen(mac);

	sprintf(cmd, "oem setmac %s", mac);

	m_tm->update(cmd);
	if(sendCommand(cmd,response) < 0){
		LOG("reset mac address failed");
		return -1;
	}
	LOG(".............done!");
	return 0;
}
int FastbootThread::reboot()
{
	LOG("reboot the device");
	m_tm->update("rebooting...");
	char cmd[64];
	char response[64];
	sprintf(cmd, "reboot");
	if(sendCommand(cmd,response) < 0){
		LOG("reboot failed");
		return -1;
	}
	LOG(".............done!");
	return 0;
}

wxThread::ExitCode FastbootThread::Entry()
{
	int i = 0;
	struct tk t;
	struct usb_device* ud = usb_device(m_dev);
	m_tm = m_dm->newTask();
	m_tm->update(ud->filename,"fastboot");
	while(TRUE == m_dm->walkActionList(i++,&t))
	{
		int ret = 0;
		switch(t.op)
		{
			case DataManager::OP_FLASH:
				ret = flashPart(t.v1);
			break;
			case DataManager::OP_ERASE:
				ret = erasePart(t.v1);
			break;
			case DataManager::OP_SETMAC:
				ret = resetMacAddr();
			break;
			case DataManager::OP_REPART:
				ret = repart();
			break;
			case DataManager::OP_REBOOT_TO_SYS:
				ret = reboot();
			break;
		}
		if(ret < 0){
			m_um->setError();
			m_tm->update("ERROR!!!");
			break;
		}
	}
	//m_um->releaseDevice(m_dev);

	return 0;
}