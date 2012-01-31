rundll32 syssetup,SetupInfObjectInstallAction DefaultInstall 128 adb_driver\android_winusb.inf
rundll32 syssetup,SetupInfObjectInstallAction DefaultInstall 128 sec_driver\secusb2.inf
cd x86
install-filter install -ac
pause