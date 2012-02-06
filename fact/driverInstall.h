#ifndef DRIVER_INSTALL_H
#define DRIVER_INSTALL_H

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include <windows.h>
int usb_install_inf_np(const char *inf_file, 
					   BOOL remove_mode, 
					   BOOL copy_oem_inf_mode);
#endif