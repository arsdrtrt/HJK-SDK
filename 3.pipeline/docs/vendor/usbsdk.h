#pragma once

#ifndef _USBIRSDK_H
#define _USBIRSDK_H

#ifdef _WIN32
#include <stdio.h>
#define USB_SDK_API  extern "C" __declspec(dllexport) 
#else
#define USB_SDK_API  extern "C"
#endif

#define MAX_SERIAL_NUMBER_LEN 48

typedef void (*USBCBF_IR)(void* lData, void* lParam);

typedef unsigned int        DWORD;
typedef int                 BOOL;
typedef unsigned char       BYTE;
typedef unsigned short      WORD;
typedef float               FLOAT;

typedef struct tagUSB_Camera_Info
{
    DWORD   dwSize;   // Structure size
    DWORD   dwIndex; // Device index 
    DWORD   dwVID;   // Device VID
    DWORD   dwPID;   // Device PID
    char    szSerialNumber[MAX_SERIAL_NUMBER_LEN/*48*/];// Device serial number
    BYTE    byRes[248];
} USB_Camera_Info;

typedef struct tagUSB_SYSTEM_INFO
{
    BYTE   byFirmwareVersion[64]; // Main firmware version
    BYTE   byHardwareVersion[64]; // Core hardware version
    BYTE   byDeviceType[64];      // Device model
    BYTE   byProtocolVersion[4];  // Protocol version: "1.0"
    BYTE   bySerialNumber[64];    // Serial number
    BYTE   byModuleID[32];  // Core module ID
    BYTE   byDeviceID[64];  // Device ID
}USB_SYSTEM_INFO;

typedef struct tagUSB_THERMOMETRY_PARAM
{
    BYTE       byTemperatureRange;// Temperature measurement range: [1,2] two gears -20~150℃, 0~550℃
    DWORD       dwEmissivity;// Emissivity: 0.01~1 (2 decimal places), multiply actual value by 100 for transmission
    DWORD       dwDistance;// Distance: 0.3-2m, unit cm in protocol, 1 decimal precision
    BYTE       byReflectiveEnable;// Reflected temperature enable: 0 - disable; 1 - enable
    DWORD       dwReflectiveTemperature;// Reflected temperature: -100.0~1000.0℃ (1 decimal precision), transmit as (actual + 100) * 10
    DWORD       dwTemperatureRangeUpperLimit; // Upper limit of temperature gear (read-only), transmit as (actual + 100) * 10
    DWORD       dwTemperatureRangeLowerLimit; // Lower limit of temperature gear (read-only), transmit as (actual + 100) * 10
}USB_THERMOMETRY_PARAM;

typedef struct tagUSB_THERM_CORRECTION_PARAM
{
    DWORD   dwDistance; // Distance: 0.3-3m, unit cm in protocol transmission
    DWORD   dwEnviroTemperature; // Ambient temperature: -273.0-1000.0℃, transmit as (actual + 300) * 10
    DWORD   dwEmissivity; // Emissivity: 0.01-1.00, transmit as actual value * 100
    DWORD   dwPresetTemperature; // Blackbody reference temperature: -40.0-650.0℃, transmit as (actual + 100) * 10
    DWORD   dwPointX; // X coordinate normalized to 0-1000
    DWORD   dwPointY; // Y coordinate normalized to 0-1000
}USB_THERM_CORRECTION_PARAM;

typedef struct tagUSB_CAMERA_PARAM
{
    DWORD dwInterFrameNoiseReduceLevel;  // Temporal noise reduction level 0-100
    DWORD dwFrameNoiseReduceLevel; // Spatial noise reduction level 0-100
    BYTE  byLSEDetailEnabled;  // Image detail enhancement enable: 0-disable 1-enable
    DWORD dwLSEDetailLevel;  // Image detail enhancement strength: 0-100
}USB_CAMERA_PARAM;

enum UPGRADE_STATE
{
    GET_STATE_FAILED = -1,			// Failed to get upgrade status
    UPGRADE_FAILED = 1,			// Firmware upgrade failed
    UPGRADE_SUCCESS = 2,	    // Firmware upgrade completed successfully
    UPGRADE_TRANS = 3,			// Firmware data transferring
    UPGRADE_TYPE_UNMATCH = 4,       // Upgrade package type mismatch
    UNKNOWN = 5,        // Unknown error
};

enum DLL_TYPE
{
    DLL_SSL = 1,
    DLL_CRYPTO = 2,
    DLL_SYSTEMTRANSFORM = 3,
    DLL_LIBUSB = 4,
    DLL_PLAYCTRL = 5,
    DLL_FORMATCONVERSION = 6,
    DLL_LIBUVC = 8
};

/*============================================================================
function:	USBSDK_dlopen
brief:		Specify the custom path for the SDK's internal dynamic libraries
parameter:	DLL_TYPE dlltype: Type of the library to load
			const char *dllpath: File system path to the library directory.
return:		int: 0 on success, -1 on failure
history:	null
==============================================================================*/
USB_SDK_API int USBSDK_dlopen(DLL_TYPE dlltype, const char *dllpath);


/*============================================================================
function:	USBSDK_GetLastError
brief:		Get the latest error message string
parameter:	void
return:		char *: Constant error message string
history:	null
==============================================================================*/
USB_SDK_API char *USBSDK_GetLastError();


/*============================================================================
function:	USBSDK_Init: Initialize SDK, call only once globally
parameter:	void
return:		int: 1: SDK initialized successfully
history:	null
==============================================================================*/
USB_SDK_API int USBSDK_Init();


/*============================================================================
function:	USBSDK_EnumDevice: Enumerate connected thermal camera devices
parameter:	USB_Camera_Info: Output structure to receive device info
return:		int: 0: Enumeration completed
                -1: Enumeration failed
                -2: No camera detected
history:	null
==============================================================================*/
USB_SDK_API int USBSDK_EnumDevice(USB_Camera_Info* camera_info);


/*============================================================================
function:	USBSDK_LoginDevice: Log in to target thermal camera
parameter:	USB_Camera_Info: Target device information
return:		int: -1: Login failed
                Other positive value: Valid device session handle
history:	null
==============================================================================*/
USB_SDK_API int USBSDK_LoginDevice(USB_Camera_Info* camera_info);


/*============================================================================
function:	USBSDK_Get_SysInfo: Read device system information
parameter:	m_lUserID: Valid login session handle
            USB_SYSTEM_INFO: Output structure for system info
return:		int: -1: Read failed
                0: Operation success
history:	null
==============================================================================*/
USB_SDK_API int USBSDK_Get_SysInfo(int m_lUserID, USB_SYSTEM_INFO* sys_info);


/*============================================================================
function:	USBSDK_Logout: Log out camera session and release handle
parameter:	m_lUserID: Valid login session handle
return:		int: Always return 0
history:	null
==============================================================================*/
USB_SDK_API int USBSDK_Logout(int m_lUserID);


/*============================================================================
function:	USBSDK_CreateCallBack: Register frame data callback function
parameter:	m_lUserID: Valid login session handle
            callbackFun: Frame receive callback function pointer
            pUser: Custom user parameter passed to callback
return:		int: 0 Register success
            -1 Callback registration failed
history:	null
==============================================================================*/
USB_SDK_API int USBSDK_CreateCallBack(int m_lUserID, USBCBF_IR callbackFun, void* pUser);


/*============================================================================
function:	USBSDK_Get_ThermalParam: Read current temperature measurement parameters
parameter:	m_lUserID: Valid login session handle
            USB_THERMOMETRY_PARAM: Output parameter structure
return:		int: 0 Success
            -1 Operation failed
history:	null
==============================================================================*/
USB_SDK_API int USBSDK_Get_ThermalParam(int m_lUserID, USB_THERMOMETRY_PARAM* param);


/*============================================================================
function:	USBSDK_Set_ThermalParam: Update temperature measurement parameters
parameter:	m_lUserID: Valid login session handle
            USB_THERMOMETRY_PARAM: Input target parameter structure
return:		int: 0 Success
            -1 Operation failed
history:	null
==============================================================================*/
USB_SDK_API int USBSDK_Set_ThermalParam(int m_lUserID, USB_THERMOMETRY_PARAM* param);


/*============================================================================
function:	USBSDK_Manual_Correct: Trigger manual shutter flat field correction
parameter:	m_lUserID: Valid login session handle
return:		int: 0 Success
            -1 Operation failed
history:	null
==============================================================================*/
USB_SDK_API int USBSDK_Manual_Correct(int m_lUserID);


/*============================================================================
function:	USBSDK_Temp_Calibration: Secondary temperature calibration with blackbody data, interval >=5s between two calls recommended
parameter:	m_lUserID: Valid login session handle
            USB_THERM_CORRECTION_PARAM: Calibration configuration parameters
return:		int: 0 Success
            -1 Operation failed
history:	null
==============================================================================*/
USB_SDK_API int USBSDK_Temp_Calibration(int m_lUserID, USB_THERM_CORRECTION_PARAM* param);


/*============================================================================
function:	USBSDK_Start_Calibration: Start auto temperature calibration flow
parameter:	m_lUserID: Valid login session handle
return:		int: 0 Success
            -1 Operation failed
history:	null
==============================================================================*/
USB_SDK_API int USBSDK_Start_Calibration(int m_lUserID);


/*============================================================================
function:	USBSDK_Get_CameraFlip: Query current image mirror mode
parameter:	m_lUserID: Valid login session handle
return:		int: Mirror mode: 0-Disable 1-Center flip 2-Horizontal flip 3-Vertical flip
            -1 Query failed
history:	null
==============================================================================*/
USB_SDK_API int USBSDK_Get_CameraFlip(int m_lUserID);


/*============================================================================
function:	USBSDK_Get_CameraBright: Query current image brightness value
parameter:	m_lUserID: Valid login session handle
return:		int: Brightness value
            -1 Query failed
history:	null
==============================================================================*/
USB_SDK_API int USBSDK_Get_CameraBright(int m_lUserID);


/*============================================================================
function:	USBSDK_Get_CameraContrast: Query current image contrast value
parameter:	m_lUserID: Valid login session handle
return:		int: Contrast value
            -1 Query failed
history:	null
==============================================================================*/
USB_SDK_API int USBSDK_Get_CameraContrast(int m_lUserID);


/*============================================================================
function:	USBSDK_Get_CameraNoiseReduce: Read noise reduction & detail enhancement parameters
parameter:	m_lUserID: Valid login session handle
            param: Output structure for image processing config
return:		int: 0 Success
            -1 Operation failed
history:	null
==============================================================================*/
USB_SDK_API int USBSDK_Get_CameraNoiseReduce(int m_lUserID, USB_CAMERA_PARAM* param);


/*============================================================================
function:	USBSDK_Set_CameraFlip: Set image mirror flip mode
parameter:	m_lUserID: Valid login session handle
            type: Mirror mode 0-Disable 1-Center flip 2-Horizontal flip 3-Vertical flip
return:		int: 0 Success
            -1 Operation failed
history:	null
==============================================================================*/
USB_SDK_API int USBSDK_Set_CameraFlip(int m_lUserID, int type);


/*============================================================================
function:	USBSDK_Set_CameraBright: Adjust image brightness
parameter:	m_lUserID: Valid login session handle
            bright: Target brightness value
return:		int: 0 Success
            -1 Operation failed
history:	null
==============================================================================*/
USB_SDK_API int USBSDK_Set_CameraBright(int m_lUserID, int bright);


/*============================================================================
function:	USBSDK_Set_CameraContrast: Adjust image contrast
parameter:	m_lUserID: Valid login session handle
            contrast: Target contrast value
return:		int: 0 Success
            -1 Operation failed
history:	null
==============================================================================*/
USB_SDK_API int USBSDK_Set_CameraContrast(int m_lUserID, int contrast);


/*============================================================================
function:	USBSDK_Set_CameraNoiseReduce: Configure noise reduction and detail enhancement
parameter:	m_lUserID: Valid login session handle
            param: Target image processing configuration
return:		int: 0 Success
            -1 Operation failed
history:	null
==============================================================================*/
USB_SDK_API int USBSDK_Set_CameraNoiseReduce(int m_lUserID, USB_CAMERA_PARAM* param);


/*============================================================================
function:	USBSDK_Import_TempFile: Import calibration file, replug camera after import takes effect
parameter:	m_lUserID: Valid login session handle
            pFile: Absolute path of calibration file
return:		int: 0 Success
            -1 Operation failed
history:	null
==============================================================================*/
USB_SDK_API int USBSDK_Import_TempFile(int m_lUserID, char* pFile);


/*============================================================================
function:	USBSDK_Export_TempFile: Export current calibration file to specified path
parameter:	m_lUserID: Valid login session handle
            pPath: Target output file path
return:		int: 0 Success
            -1 Operation failed
history:	null
==============================================================================*/
USB_SDK_API int USBSDK_Export_TempFile(int m_lUserID, char* pPath);


/*============================================================================
function:	USBSDK_Reset: Restore all camera factory default parameters
parameter:	m_lUserID: Valid login session handle
return:		int: 0 Success
            -1 Operation failed
history:	null
==============================================================================*/
USB_SDK_API int USBSDK_Reset(int m_lUserID);


/*============================================================================
function:	USBSDK_Upgrade: Start firmware upgrade, query progress via USBSDK_Get_Upgrade_State, replug camera after finish
parameter:	m_lUserID: Valid login session handle
            pFile: Firmware bin file path
return:		int: 0 Success
            -1 Operation failed
history:	null
==============================================================================*/
USB_SDK_API int USBSDK_Upgrade(int m_lUserID, char* pFile);


/*============================================================================
function:	USBSDK_Get_Upgrade_State: Query current firmware upgrade progress status
parameter:	m_lUserID: Valid login session handle
return:		See enum UPGRADE_STATE for definition
history:	null
==============================================================================*/
USB_SDK_API int USBSDK_Get_Upgrade_State(int m_lUserID);


/*============================================================================
function:	USBSDK_Close_AutoShutter: Enable or disable automatic shutter correction
parameter:	m_lUserID: Session token
            close: true-disable auto shutter, false-enable auto shutter
return:		int: 0 Success
            -1 Failure
history:	null
==============================================================================*/
USB_SDK_API int USBSDK_Close_AutoShutter(int m_lUserID, bool close);


/*============================================================================
function:	USBSDK_Vignetting_Correction: Execute vignetting correction (disable auto shutter before calling this function)
parameter:	m_lUserID: Session token
return:		int: 0 Success
            -1 Failure
history:	null
==============================================================================*/
USB_SDK_API int USBSDK_Vignetting_Correction(int m_lUserID);

#endif
