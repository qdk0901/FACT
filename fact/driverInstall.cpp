#include "driverInstall.h"
#include <setupapi.h>
#include <cfgmgr32.h>
#define INSTALLFLAG_FORCE 0x00000001
#define CR_SUCCESS                        0x00000000
#define CR_NO_SUCH_DEVNODE                0x0000000D
#define CONFIGFLAG_REINSTALL              0x00000020



#define LOG(fmt,...) wxLogMessage(wxString::Format(fmt, ##__VA_ARGS__))
/* setupapi.dll exports */
typedef BOOL (WINAPI * setup_copy_oem_inf_t)(PCSTR, PCSTR, DWORD, DWORD,
        PSTR, DWORD, PDWORD, PSTR*);
typedef BOOL (WINAPI * update_driver_for_plug_and_play_devices_t)(HWND,
        LPCSTR,
        LPCSTR,
        DWORD,
        PBOOL);

typedef BOOL (WINAPI * rollback_driver_t)(HDEVINFO,
        PSP_DEVINFO_DATA,
        HWND,
        DWORD,
        PBOOL);

typedef BOOL (WINAPI * uninstall_device_t)(HWND,
        HDEVINFO,
        PSP_DEVINFO_DATA,
        DWORD,
        PBOOL);

BOOL usb_install_find_model_section(HINF inf_handle, PINFCONTEXT inf_context)
{
	// find the .inf file's model-section. This is normally a [Devices]
	// section, but could be anything.

	char tmp[MAX_PATH];
	char* model[8];
	BOOL success = FALSE;
	DWORD field_index;

	// first find [Manufacturer].  The models are listeed in this section.
	if (SetupFindFirstLineA(inf_handle, "Manufacturer", NULL, inf_context))
	{
		memset(model, 0, sizeof(model));
		for (field_index = 1; field_index < ( sizeof(model) / sizeof(model[0]) ); field_index++)
		{
			success = SetupGetStringFieldA(inf_context, field_index, tmp, sizeof(tmp), NULL);
			if (!success) break;

			model[(field_index-1)] = _strdup(tmp);
			switch(field_index)
			{
			case 1:	// The first field is the base model-section-name, "Devices" for example.
				LOG("model-section-name=%s\n", tmp);
				break;
			default: // These are the target OS field(s), "NT" or "NTX86" for example.
				LOG("target-os-version=%s\n", tmp);
			}
		}

		// if the base model-section-name was found..
		if (field_index > 1)
		{
			// find the model-section
			field_index = 0;
			strcpy(tmp, model[0]);
			while (!(success = SetupFindFirstLineA(inf_handle, tmp, NULL, inf_context)))
			{
				field_index++;
				if (!model[field_index]) break;
				sprintf(tmp, "%s.%s", model[0], model[field_index]);
			}
		}

		// these were allocated with _strdup above.
		for (field_index = 0; model[field_index] != NULL; field_index++)
			free(model[field_index]);

		// model-section-name or model-section not found
		if (!success)
		{
			LOG(".inf file does not contain a valid model-section-name\n");
		}
	}
	else
	{
		LOG(".inf file does not contain a valid Manufacturer section\n");
	}

	return success;
}

int usb_install_inf_np(const char *inf_file, 
					   BOOL remove_mode, 
					   BOOL copy_oem_inf_mode)
{
	HDEVINFO dev_info;
	SP_DEVINFO_DATA dev_info_data;
	INFCONTEXT inf_context;
	HINF inf_handle;
	DWORD config_flags, problem, status;
	BOOL reboot;
	char inf_path[MAX_PATH];
	char id[MAX_PATH];
	char tmp_id[MAX_PATH];
	char *p;
	int dev_index;
	HINSTANCE newdev_dll = NULL;
	HMODULE setupapi_dll = NULL;
	CONFIGRET cr;
	update_driver_for_plug_and_play_devices_t UpdateDriverForPlugAndPlayDevices;
	rollback_driver_t RollBackDriver;
	uninstall_device_t UninstallDevice;
	int field_index;
	setup_copy_oem_inf_t SetupCopyOEMInf;
	BOOL usb_reset_required;

	newdev_dll = LoadLibraryA("newdev.dll");
	if (!newdev_dll)
	{
		LOG("loading newdev.dll failed\n");
		return -1;
	}

	UpdateDriverForPlugAndPlayDevices = (update_driver_for_plug_and_play_devices_t)GetProcAddress(newdev_dll, "UpdateDriverForPlugAndPlayDevicesA");
	if (!UpdateDriverForPlugAndPlayDevices)
	{
		LOG("loading newdev.dll failed\n");
		return -1;
	}
	UninstallDevice = (uninstall_device_t)GetProcAddress(newdev_dll, "DiUninstallDevice");
	RollBackDriver = (rollback_driver_t)GetProcAddress(newdev_dll, "DiRollbackDriver");

	setupapi_dll = GetModuleHandleA("setupapi.dll");
	if (!setupapi_dll)
	{
		LOG("loading setupapi.dll failed\n");
		return -1;
	}
	SetupCopyOEMInf = (setup_copy_oem_inf_t)GetProcAddress(setupapi_dll, "SetupCopyOEMInfA");
	if (!SetupCopyOEMInf)
	{
		LOG("loading setupapi.dll failed\n");
		return -1;
	}

	dev_info_data.cbSize = sizeof(SP_DEVINFO_DATA);


	/* retrieve the full .inf file path */
	if (!GetFullPathNameA(inf_file, MAX_PATH, inf_path, NULL))
	{
		LOG(".inf file %s not found\n",
		       inf_file);
		return -1;
	}

	/* open the .inf file */
	inf_handle = SetupOpenInfFileA(inf_path, NULL, INF_STYLE_WIN4, NULL);

	if (inf_handle == INVALID_HANDLE_VALUE)
	{
		LOG("unable to open .inf file %s\n",
		       inf_file);
		return -1;
	}

	if (!usb_install_find_model_section(inf_handle, &inf_context))
	{
		SetupCloseInfFile(inf_handle);
		return -1;
	}

	do
	{
		/* get the device ID from the .inf file */

		field_index = 2;
		while(SetupGetStringFieldA(&inf_context, field_index++, id, sizeof(id), NULL))
		{
			/* convert the string to lowercase */
			strlwr(id);

			if (strncmp(id, "usb\\", 4) != 0)
			{
				LOG("invalid hardware id %s\n", id);
				SetupCloseInfFile(inf_handle);
				return -1;
			}

			LOG("%s device %s..\n",
			       remove_mode ? "removing" : "installing", id + 4);

			reboot = FALSE;

			if (!remove_mode)
			{
				if (copy_oem_inf_mode)
				{
					/* copy the .inf file to the system directory so that is will be found */
					/* when new devices are plugged in */
					if (!SetupCopyOEMInf(inf_path, NULL, SPOST_PATH, SP_COPY_NEWER_OR_SAME, NULL, 0, NULL, NULL))
					{
						LOG("SetupCopyOEMInf failed for %s", inf_path);
					}
					else
					{
						LOG(".inf file %s copied to system directory", inf_path);
						copy_oem_inf_mode = FALSE;
					}
				}

				/* force an update of all connected devices matching this ID */
				UpdateDriverForPlugAndPlayDevices(NULL, id, inf_path, INSTALLFLAG_FORCE, &reboot);
			}

			/* now search the registry for device nodes representing currently  */
			/* unattached devices or remove devices depending on the mode */

			/* get all USB device nodes from the registry, present and non-present */
			/* devices */
			dev_info = SetupDiGetClassDevsA(NULL, "USB", NULL, DIGCF_ALLCLASSES);

			if (dev_info == INVALID_HANDLE_VALUE)
			{
				SetupCloseInfFile(inf_handle);
				return -1;
			}

			dev_index = 0;

			/* enumerate the device list to find all attached and unattached */
			/* devices */
			while (SetupDiEnumDeviceInfo(dev_info, dev_index, &dev_info_data))
			{
				/* get the harware ID from the registry, this is a multi-zero string */
				if (SetupDiGetDeviceRegistryProperty(dev_info, &dev_info_data,
				                                     SPDRP_HARDWAREID, NULL,
				                                     (BYTE *)tmp_id,
				                                     sizeof(tmp_id), NULL))
				{
					/* check all possible IDs contained in that multi-zero string */
					for (p = tmp_id; *p; p += (strlen(p) + 1))
					{
						/* convert the string to lowercase */
						strlwr(p);

						/* found a match? */
						if (strstr(p, id))
						{
							if (remove_mode)
							{
								if (RollBackDriver)
								{
									if (RollBackDriver(dev_info, &dev_info_data, NULL, 0, &reboot))
									{
										break;
									}
									else
									{
										LOG("failed RollBackDriver for device %s\n", p);
									}
								}
								if (UninstallDevice)
								{
									if (UninstallDevice(NULL, dev_info, &dev_info_data, 0, &reboot))
									{
										usb_reset_required = TRUE;
										break;
									}
									else
									{
										LOG("failed UninstallDevice for device %s\n", p);
									}
								}

								if (SetupDiRemoveDevice(dev_info, &dev_info_data))
								{
									usb_reset_required = TRUE;
									break;
								}
								else
								{
									LOG("failed RemoveDevice for device %s\n", p);
								}

							}
							else
							{
								cr = CM_Get_DevNode_Status(&status,
								                           &problem,
								                           dev_info_data.DevInst,
								                           0);

								/* is this device disconnected? */
								if (cr == CR_NO_SUCH_DEVINST)
								{
									/* found a device node that represents an unattached */
									/* device */
									if (SetupDiGetDeviceRegistryProperty(dev_info,
									                                     &dev_info_data,
									                                     SPDRP_CONFIGFLAGS,
									                                     NULL,
									                                     (BYTE *)&config_flags,
									                                     sizeof(config_flags),
									                                     NULL))
									{
										/* mark the device to be reinstalled the next time it is */
										/* plugged in */
										config_flags |= CONFIGFLAG_REINSTALL;

										/* write the property back to the registry */
										SetupDiSetDeviceRegistryProperty(dev_info,
										                                 &dev_info_data,
										                                 SPDRP_CONFIGFLAGS,
										                                 (BYTE *)&config_flags,
										                                 sizeof(config_flags));
									}
								}
							}
							/* a match was found, skip the rest */
							break;
						}
					}
				}
				/* check the next device node */
				dev_index++;
			}

			SetupDiDestroyDeviceInfoList(dev_info);

			/* get the next device ID from the .inf file */
		}
	}
	while (SetupFindNextLine(&inf_context, &inf_context));

	/* we are done, close the .inf file */
	SetupCloseInfFile(inf_handle);

	return 0;
}