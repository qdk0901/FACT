#ifndef USBMANAGER_H
#define USBMANAGER_H
#include "dataManager.h"
#include "lusb0_usb.h"
#include <set>
using namespace std;

#define MAX_PATH 260
class UsbManager;

class UsbbootThread
  : public wxThread
{
	friend class UsbManager;
protected:
	virtual ExitCode Entry();
private:
	UsbbootThread(UsbManager* um,DataManager* dm,int stage,usb_dev_handle *dev);
	UsbManager* m_um;
	DataManager* m_dm;
	TaskManager* m_tm;
	unsigned short check_sum(char* buffer,int size);
	int usbDownload(usb_dev_handle* dev,char* filePath,int addr);
	int bootDevice();
	int m_stage;
	usb_dev_handle* m_dev;
};
class UsbManager : public wxThread{
public:
	static const int DEV_NULL	= 0;
	static const int DEV_USBBOOT_S1 = 1;
	static const int DEV_USBBOOT_S2 = 2;
	static const int DEV_FASTBOOT = 3;

	static int init(DataManager* dm);
	int detect();
	usb_dev_handle* openDevice(struct usb_device *ud);
	usb_dev_handle* getDevice(int d);
	int releaseDevice(usb_dev_handle* dev);
protected:
	virtual ExitCode Entry();
private:
	static UsbManager* m_um;
	UsbManager(DataManager* dm);
	DataManager* m_dm;

	//vid pid for usb boot
	int m_vid1s1;
	int m_pid1s1;
	int m_vid1s2;
	int m_pid1s2;
	//vid pid for fastboot
	int m_vid2;
	int m_pid2;
	set<string> m_opened;
	void lockDevice(char* dev);
	void unlockDevice(char* dev);
	int isLock(char* dev);
};
#endif