#ifndef USBMANAGER_H
#define USBMANAGER_H
#include "dataManager.h"
#include <windows.h>
#include <usb100.h>
#include "adb_api.h"
#include "adb_winusb_api.h"
#include "driverInstall.h"


#include <set>
#include <map>
using namespace std;

struct usb_handle {
    ADBAPIHANDLE  adb_interface;
    ADBAPIHANDLE  adb_read_pipe;
    ADBAPIHANDLE  adb_write_pipe;
    wchar_t*         interface_name;
};
static const GUID usb_class_id = ANDROID_USB_CLASS_ID;

class UsbManager;

class UsbbootThread
  : public wxThread
{
	friend class UsbManager;
protected:
	virtual ExitCode Entry();
private:
	UsbbootThread(UsbManager* um,DataManager* dm,int stage,usb_handle *dev);
	UsbManager* m_um;
	DataManager* m_dm;
	TaskManager* m_tm;
	unsigned short check_sum(char* buffer,int size);
	int usbDownload(usb_handle* dev,char* data,int size,int addr);
	int bootDevice();
	int m_stage;
	usb_handle* m_dev;
	int	m_bulkIn;
	int m_bulkOut;
};
class FastbootThread
	: public wxThread
{
	friend class UsbManager;
protected:
	virtual ExitCode Entry();	
private:
	FastbootThread(UsbManager* um,DataManager* dm,usb_handle *dev);
	UsbManager* m_um;
	DataManager* m_dm;
	TaskManager* m_tm;
	usb_handle* m_dev;
	int	m_bulkIn;
	int m_bulkOut;
	int checkResponse(unsigned size,unsigned dataOk,char* response);
	int sendCommandFull(const char* cmd,const void* data,unsigned size,char* response);
	int sendCommand(const char* cmd);
	int sendCommand(const char* cmd,char* response);
	int downloadData(const void* data,unsigned size);
	int flashPart(char* part);
	int erasePart(char* part);
	int repart();
	int resetMacAddr();
	int reboot();
};
class UsbManager : public wxThread{
public:
	static const int DEV_USBBOOT_S1 = 1;
	static const int DEV_USBBOOT_S2 = 2;
	static const int DEV_FASTBOOT = 3;
	static const int DEV_NULL = 4;

	static void init();
	static void start(DataManager* dm);
	int UsbManager::detect(wchar_t* ifname_out);
	usb_handle* usbOpen(const wchar_t* if_name);
	int usbClean(usb_handle* handle);
	int usbWrite(usb_handle* handle, const char* data, int len);
	int usbRead(usb_handle *handle, void* data, int len);

	static int installDriver();

	void setError();
protected:
	virtual ExitCode Entry();
private:
	static UsbManager* m_um;
	UsbManager(DataManager* dm);
	DataManager* m_dm;



	int idToDevice(int vid,int pid);

	set<wstring> m_lockIf;
	void lockIf(wstring if_name);
	void unlockIf(wstring if_name);
	int isLock(wstring if_name);
	int m_isError;
	static map<int,int> m_idMap;
};
#endif