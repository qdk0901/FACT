#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "usbManager.h"

UsbManager* UsbManager::m_um = NULL;
map<int,int> UsbManager::m_idMap = map<int,int>();

void UsbManager::init()
{
	//set id to device map
	m_idMap[0x04e81234] = DEV_USBBOOT_S1;
	m_idMap[0x04e81235] = DEV_USBBOOT_S2;
	m_idMap[0x18d10002] = DEV_FASTBOOT;
	
}
void UsbManager::start(DataManager* dm)
{
	if(m_um == NULL){
		m_um = new UsbManager(dm);
	}
}
UsbManager::UsbManager(DataManager* dm)
	:wxThread(wxTHREAD_DETACHED)
{


	m_dm = dm;
	if(Create() == wxTHREAD_NO_ERROR){
		Run();
	}else{
		LOG( "Cannot create usb thread");
	}
}
void UsbManager::lockIf(wstring if_name)
{
	m_lockIf.insert(if_name);
}
void UsbManager::unlockIf(wstring if_name)
{
	set<wstring>::iterator it = m_lockIf.find(if_name);
	if(it != m_lockIf.end()){
		m_lockIf.erase(it);
	}	
}
int UsbManager::isLock(wstring if_name)
{
	return !(m_lockIf.find(if_name) == m_lockIf.end());
}
int UsbManager::idToDevice(int vid,int pid)
{
	map<int, int>::iterator it = m_idMap.find((vid<<16)|pid);
	if(it != m_idMap.end()){
		return it->second;
	}
	return DEV_NULL;
}

int UsbManager::detect(wchar_t* ifname_out)
{
	int ret = DEV_NULL;
	usb_handle* handle = NULL;
	char entry_buffer[2048];
	unsigned long entry_buffer_size = sizeof(entry_buffer);

    AdbInterfaceInfo* next_interface = (AdbInterfaceInfo*)(&entry_buffer[0]);

    ADBAPIHANDLE enum_handle =
        AdbEnumInterfaces(usb_class_id, true, true, true);

    if (NULL == enum_handle)
        return DEV_NULL;

    while (AdbNextInterface(enum_handle, next_interface, &entry_buffer_size)) {

		wchar_t* ifname = next_interface->device_name;

		if(isLock(ifname) == TRUE)
			continue;


		ADBAPIHANDLE h = AdbCreateInterfaceByName(ifname);
		if(NULL == h){
			break;
		}
		
		USB_DEVICE_DESCRIPTOR desc;

		if (!AdbGetUsbDeviceDescriptor(h,&desc)) {
			AdbCloseHandle(h);
			break;
		}
		ret = idToDevice(desc.idVendor,desc.idProduct);

		if(ret == DEV_NULL){
			AdbCloseHandle(h);
			continue;
		}
        wcscpy(ifname_out,ifname);

		AdbCloseHandle(h);
		break;
    }
    AdbCloseHandle(enum_handle);
	return ret;
}
int UsbManager::usbClean(usb_handle* handle)
{
	if (NULL != handle) {
		if (NULL != handle->interface_name){
			unlockIf(handle->interface_name);
            free(handle->interface_name);
		}
        if (NULL != handle->adb_write_pipe)
            AdbCloseHandle(handle->adb_write_pipe);
        if (NULL != handle->adb_read_pipe)
            AdbCloseHandle(handle->adb_read_pipe);
        if (NULL != handle->adb_interface)
            AdbCloseHandle(handle->adb_interface);

        handle->interface_name = NULL;
        handle->adb_write_pipe = NULL;
        handle->adb_read_pipe = NULL;
        handle->adb_interface = NULL;
    }
	return 0;
}
usb_handle* UsbManager::usbOpen(const wchar_t* if_name)
{
    usb_handle* ret = (usb_handle*)malloc(sizeof(usb_handle));
    if (NULL == ret)
        return NULL;

    ret->adb_interface = AdbCreateInterfaceByName(if_name);

    if (NULL == ret->adb_interface) {
        free(ret);
        return NULL;
    }

    ret->adb_read_pipe =
        AdbOpenDefaultBulkReadEndpoint(ret->adb_interface,
                                   AdbOpenAccessTypeReadWrite,
                                   AdbOpenSharingModeReadWrite);
    if (NULL != ret->adb_read_pipe) {
        ret->adb_write_pipe =
            AdbOpenDefaultBulkWriteEndpoint(ret->adb_interface,
                                      AdbOpenAccessTypeReadWrite,
                                      AdbOpenSharingModeReadWrite);
        if (NULL != ret->adb_write_pipe) {
            unsigned long name_len = 0;

            AdbGetInterfaceName(ret->adb_interface,
                          NULL,
                          &name_len,
                          false);
            if (0 != name_len) {
                ret->interface_name = (wchar_t*)malloc((name_len + 1)*2);

                if (NULL != ret->interface_name) {
                    if (AdbGetInterfaceName(ret->adb_interface,
                                  ret->interface_name,
                                  &name_len,
                                  false)) {

					    lockIf(if_name);
                        return ret;
                    }
                } else {
                    LOG("Out of memory!");
                }
            }
        }
    }
    usbClean(ret);
    free(ret);
    return NULL;
}
void UsbManager::setError()
{
	m_isError = TRUE;
}
int UsbManager::usbWrite(usb_handle* handle, const char* data, int len) {
    unsigned long time_out = -1;
    unsigned long written = 0;
    unsigned count = 0;
    int ret;

    if (NULL != handle) {
        while(len > 0) {
            int xfer = (len > 4096) ? 4096 : len;
            ret = AdbWriteEndpointSync(handle->adb_write_pipe,
                                   (void*)data,
                                   (unsigned long)xfer,
                                   &written,
                                   time_out);
            errno = GetLastError();
            if (ret == 0) {
                if (errno == ERROR_INVALID_HANDLE)
					LOG("usbWrite error:Invalid handle");

				LOG("%d",errno);
                return -1;
            }

            count += written;
            len -= written;
            data += written;

            if (len == 0)
                return count;
        }
    }

    LOG("usbWrite failed: %d\n", errno);
    return -1;
}
int UsbManager::usbRead(usb_handle *handle, void* data, int len) {
    unsigned long time_out = 500 + len * 8;
    unsigned long read = 0;
    int ret;

    if (NULL != handle) {
        while (1) {
            int xfer = (len > 4096) ? 4096 : len;

	        ret = AdbReadEndpointSync(handle->adb_read_pipe,
	                              (void*)data,
	                              (unsigned long)xfer,
	                              &read,
	                              time_out);
            errno = GetLastError();
            if (ret) {
                return read;
            } else if (errno != ERROR_SEM_TIMEOUT) {
                if (errno == ERROR_INVALID_HANDLE)
                    LOG("usbRead error:Invalid handle");
                break;
            }
            // else we timed out - try again
        }
    }
    LOG("usb_read failed: %d\n", errno);

    return -1;
}

wxThread::ExitCode UsbManager::Entry()
{
	int i = 0;
	m_isError = FALSE;
	while(1){
		if(m_isError == FALSE){
			wchar_t ifname[2048];
			int stage = detect(ifname);
			if(stage == DEV_USBBOOT_S1 || stage == DEV_USBBOOT_S2){
				usb_handle *dev = usbOpen(ifname);
				if(dev != NULL)
					new UsbbootThread(m_um,m_dm,stage,dev);
			}else if(stage == DEV_FASTBOOT){
				usb_handle *dev = usbOpen(ifname);
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


int UsbManager::installDriver()
{
	string d = wxGetCwd().ToStdString();
	string p = d + "\\adb_driver\\android_winusb.inf";

	int ret = usb_install_inf_np(p.c_str(),FALSE,TRUE);
	if(ret == 0){
		LOG("安装成功!");
	}
	return 0;
}

UsbbootThread::UsbbootThread(UsbManager* um,DataManager* dm,int stage,usb_handle *dev)
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

int UsbbootThread::usbDownload(usb_handle* dev,char* data,int size,int addr)
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

	n = m_um->usbWrite(dev,buffer,size + 10);

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

	m_dm->getAddress(&add_bl2,&add_uboot);

	if(m_stage == UsbManager::DEV_USBBOOT_S1){
		bl2 = m_dm->getFileData(PART_BL2,&size_bl2);
		m_tm->update(m_dev->interface_name,"usbboot stage1");
		if(bl2){
			ret = usbDownload(m_dev,bl2,size_bl2,add_bl2);
		}
	}else if(m_stage == UsbManager::DEV_USBBOOT_S2){
		uboot = m_dm->getFileData(PART_UBOOT,&size_uboot);
		m_tm->update(m_dev->interface_name,"usbboot stage2");
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
		m_um->usbClean(m_dev);
	}
	else
		m_tm->update(long(0));
	
	return 0;
}

FastbootThread::FastbootThread(UsbManager* um,DataManager* dm,usb_handle *dev)
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
        r = m_um->usbRead(m_dev, (char*)status, 64);
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

    if(m_um->usbWrite(m_dev, (char*)cmd, cmdsize) != cmdsize) {
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
			r = m_um->usbWrite(m_dev,(char*)((unsigned)data + size - rest), frame);
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
	int i = 0,ret = 0;
	struct tk t;
	m_tm = m_dm->newTask();
	m_tm->update(m_dev->interface_name,"fastboot");
	while(TRUE == m_dm->walkActionList(i++,&t))
	{
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
	if(ret == 0){
		m_tm->update("OK!");
	}
	m_um->usbClean(m_dev);

	return 0;
}