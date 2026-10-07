#ifndef _IRSDK_H
#define _IRSDK_H

#ifdef _WIN32
#include <stdio.h>
#ifdef IRSDK_EXPORTS
#define IR_SDK_API extern "C" __declspec(dllexport)
#else
#define IR_SDK_API extern "C" __declspec(dllimport)
#endif
#else
#define IR_SDK_API extern "C" __attribute__((visibility("default")))
#endif

#define DEVICE_MAX				(32)							// Maximum 32 device instance handles supported
#define OBJ_MAX					(32)							// Maximum 32 measurement objects, upper layer cannot exceed this limit

typedef int (*CBF_IR)(void * lData, void * lParam);				// Callback function prototype used in SDK

// Temperature conversion macro
#define CALTEMP(x,y)		((x-10000)/(float)y)				// Conversion formula between Y16 raw data and actual temperature

// File operation flags
#define OPEN_FILE				(1)
#define CLOSE_FILE				(2)
#define WR_FRAME 				(3)

#define  MAX_W					(2048)
#define  MAX_H					(1536)

// Frame format, 32-byte header placed at the start of data buffer
typedef struct tagFrame
{
	unsigned short width;				// Image width	
	unsigned short height;				// Image height
	unsigned short u16FpaTemp;			// FPA focal plane temperature
	unsigned short u16EnvTemp;			// Ambient environment temperature
	unsigned char  u8TempDiv;			// Divisor for temperature conversion, refer to document for details
	unsigned char  u8DeviceType;		// Unused
	unsigned char  u8SensorType;		// Unused
	unsigned char  u8MeasureSel;		// Temperature measurement range gear
	unsigned char  u8Lens;				// Lens parameter
	unsigned char  u8Fps;				// Frame rate
	unsigned char  u8TriggerFrame;		// Trigger frame flag
	unsigned char  u8Reversed2;			// Reserved 
	unsigned int   u32FrameIndex;		// Frame sequence index
	unsigned short u16MeasureDis;		// Measuring distance
	unsigned char  Reversed[8];		    // Reserved space
	unsigned char  u8Handle;		    // Device handle ID
	unsigned char  u8ObjTempFilterSw;	// Object temperature temporal filter switch
	unsigned short buffer[MAX_W*MAX_H];		// Row-major image raw data, max resolution 2048x1536, each pixel stored as unsigned short
} Frame;
 

// 2D point coordinate (x,y)
typedef struct t_point
{
	unsigned short x;
	unsigned short y;
}T_POINT;

// Line segment defined by two endpoints P1 and P2
typedef struct t_line
{
	T_POINT P1;
	T_POINT P2;
}T_LINE;

// Circle / Ellipse structure
// Pc: center coordinate; a: horizontal semi-axis; b: vertical semi-axis
// For standard circle, set a equal to b as radius
typedef struct t_circle
{
	T_POINT Pc;
	unsigned short a;
	unsigned short b;
}T_CIRCLE;

// Rectangle region
// P1: top-left vertex; P2: bottom-right vertex
typedef struct t_rect
{
	T_POINT P1;
	T_POINT P2;
}T_RECT;

// Arbitrary polygon shape
// Supports up to 16 vertices; Pt_num: vertex count; Pt: array of vertex coordinates
typedef struct t_polygon
{
	unsigned int Pt_num;
	T_POINT Pt[16];
}T_POLYGON;

// Radar temperature distribution graph
// Supports up to 64 sampling points; Pt_num: sampling point count
typedef struct t_radar
{
	unsigned int Pt_num;
	unsigned char circle_num;
	unsigned short max_radiu;
	T_POINT Pt[64];
}T_RADAR;

// Statistics of max/min/average temperature and their corresponding pixel coordinates
typedef struct stat_temper
{
	float maxTemper;
	float minTemper;
	float avgTemper;

	T_POINT maxTemperPT;
	T_POINT minTemperPT;
}STAT_TEMPER;

// RGBA color definition
typedef struct t_color
{
	unsigned char r;
	unsigned char g;
	unsigned char b;
	unsigned char a;
}T_COLOR;

// Alarm threshold type enumeration
enum T_ALARMTYPE
{
	OverHigh	= 0,			// Temperature exceeds high threshold
	UnderLow	= 1,			// Temperature below low threshold
	BetweenHL	= 2,			// Temperature falls between low and high threshold
	DeBetweenHL = 3,			// Temperature outside low-high threshold range
};

// Alarm rule configuration
typedef struct t_alarm
{
	unsigned char alarmType;	// Alarm trigger type
	unsigned char isDraw;		// Whether to render alarm overlay on screen
	unsigned char isVioce;		// Whether to enable voice alarm prompt
	unsigned char isVideo;		// Whether to auto record video on alarm
	float		  HighThresh;	// High temperature threshold value
	float		  LowThresh;	// Low temperature threshold value
	T_COLOR		  colorAlarm; 	// Overlay color for alarm region	
}T_ALARM;

// Global full-image temperature measurement parameters and statistics
typedef struct stat_global
{
	stat_global()
	{
		inputEmiss = 0.98;
		inputReflect = 20.0;
		inputDis = 2.0;
		inputOffset = 0;
	}
	STAT_TEMPER sTemp;
	unsigned int  LableEx[32];		// Extended object label text
	unsigned char Lable[32];		// Short object label text
	float		  inputEmiss;		// Object surface emissivity
	float		  inputReflect;		// Reflected ambient temperature
	float		  inputDis;			// Measuring distance to target
	float         inputOffset;      // Global temperature correction offset
	float		  Area;				// Calculated statistical area size
	unsigned char pal;				// Palette lookup index
	unsigned char reserved2;
	unsigned char reserved3;
	unsigned char reserved4;
	T_COLOR	 color;						// Drawing color of this measurement object
	T_ALARM  sAlarm;
}STAT_GLOBAL;

// Single temperature point measurement data
typedef struct stat_point
{
	stat_point()
	{
		inputEmiss = 0.98;
		inputReflect = 20.0;
		inputDis = 2.0;
		inputOffset = 0;
	}
	T_POINT sPoint;
	STAT_TEMPER sTemp;
	unsigned int  LableEx[32];		// Extended object label text
	unsigned char Lable[32];		// Short object label text
	float		  inputEmiss;		// Object surface emissivity
	float		  inputReflect;		// Reflected ambient temperature
	float		  inputDis;			// Measuring distance to target
	float         inputOffset;      // Temperature correction offset
	float		  Area;				// Statistical area (point area = 1 pixel)
	unsigned char reserved1;
	unsigned char reserved2;
	unsigned char reserved3;
	unsigned char reserved4;
	T_COLOR	 color;						// Drawing color of this measurement object
	T_ALARM  sAlarm;
}STAT_POINT;

// Line segment temperature measurement statistics
typedef struct stat_line
{
	stat_line()
	{
		inputEmiss = 0.98;
		inputReflect = 20.0;
		inputDis = 2.0;
		inputOffset = 0;
	}
	T_LINE sLine;
	STAT_TEMPER sTemp;
	unsigned int  LableEx[32];		// Extended object label text
	unsigned char Lable[32];		// Short object label text
	float		  inputEmiss;		// Object surface emissivity
	float		  inputReflect;		// Reflected ambient temperature
	float		  inputDis;			// Measuring distance to target
	float         inputOffset;      // Temperature correction offset
	float		  Area;				// Statistical pixel count of line
	unsigned char reserved1;
	unsigned char reserved2;
	unsigned char reserved3;
	unsigned char reserved4;
	T_COLOR	 color;					// Drawing color of this measurement object	
	T_ALARM  sAlarm;
}STAT_LINE;

// Circle / Ellipse region temperature statistics
typedef struct stat_circle
{
	stat_circle()
	{
		inputEmiss = 0.98;
		inputReflect = 20.0;
		inputDis = 2.0;
		inputOffset = 0;
	}
	T_CIRCLE sCircle;
	STAT_TEMPER sTemp;
	unsigned int  LableEx[32];		// Extended object label text
	unsigned char Lable[32];		// Short object label text
	float		  inputEmiss;		// Object surface emissivity
	float		  inputReflect;		// Reflected ambient temperature
	float		  inputDis;			// Measuring distance to target
	float         inputOffset;      // Temperature correction offset
	float		  Area;				// Pixel area inside circle
	unsigned char reserved1;
	unsigned char reserved2;
	unsigned char reserved3;
	unsigned char reserved4;
	T_COLOR		 color;					// Drawing color of this measurement object	
	T_ALARM  sAlarm;
}STAT_CIRCLE;

// Rectangular region temperature statistics
typedef struct stat_rect
{
	stat_rect()
	{
		inputEmiss = 0.98;
		inputReflect = 20.0;
		inputDis = 2.0;
		inputOffset = 0;
	}
	T_RECT sRect;
	STAT_TEMPER sTemp;
	unsigned int  LableEx[32];		// Extended object label text
	unsigned char Lable[32];		// Short object label text
	float		  inputEmiss;		// Object surface emissivity
	float		  inputReflect;		// Reflected ambient temperature
	float		  inputDis;			// Measuring distance to target
	float         inputOffset;      // Temperature correction offset
	float		  Area;				// Total pixels inside rectangle
	unsigned char reserved1;
	unsigned char reserved2;
	unsigned char reserved3;
	unsigned char reserved4;
	T_COLOR	 color;					// Drawing color of this measurement object	
	T_ALARM  sAlarm;
}STAT_RECT;

// Arbitrary polygon region temperature statistics
typedef struct stat_polygon
{
	stat_polygon()
	{
		inputEmiss = 0.98;
		inputReflect = 20.0;
		inputDis = 2.0;
		inputOffset = 0;
	}
	T_POLYGON sPolygon;
	STAT_TEMPER sTemp;

	unsigned int  LableEx[32];		// Extended object label text
	unsigned char Lable[32];		// Short object label text
	float		  inputEmiss;		// Object surface emissivity
	float		  inputReflect;		// Reflected ambient temperature
	float		  inputDis;			// Measuring distance to target
	float         inputOffset;      // Temperature correction offset
	float		  Area;				// Pixel area inside polygon
	unsigned char reserved1;
	unsigned char reserved2;
	unsigned char reserved3;
	unsigned char reserved4;
	T_COLOR	 color;
	T_ALARM  sAlarm;
}STAT_POLYGON;

// Radar multi-point temperature distribution statistics
typedef struct stat_radar
{
	stat_radar()
	{
		inputEmiss = 0.98;
		inputReflect = 20.0;
		inputDis = 2.0;
		inputOffset = 0;
	}
	T_RADAR sRadar;
	STAT_TEMPER sTemp[64];

	unsigned int  LableEx[32];		// Extended object label text
	unsigned char Lable[32];		// Short object label text
	float		  inputEmiss;		// Object surface emissivity
	float		  inputReflect;		// Reflected ambient temperature
	float		  inputDis;			// Measuring distance to target
	float         inputOffset;      // Temperature correction offset
	float		  Area;				// Total radar coverage pixel area
	unsigned char reserved1;
	unsigned char reserved2;
	unsigned char reserved3;
	unsigned char reserved4;
	T_COLOR	 color;
	T_ALARM  sAlarm;
}STAT_RADAR;

// Container structure storing all measurement objects
// num fields represent count of each shape type
// All object buffers must be pre-allocated before use
typedef struct stat_obj
{
	unsigned char numPt;
	unsigned char numLine;
	unsigned char numCircle;
	unsigned char numRect;
	unsigned char numPolygon;
	unsigned char numRadar;		// Maximum 1 radar graph allowed
	unsigned char Reserved2;
	unsigned char Reserved3;

	STAT_GLOBAL sGlobal;
	STAT_POINT	sPt[OBJ_MAX];
	STAT_LINE	sLine[OBJ_MAX];
	STAT_CIRCLE sCircle[OBJ_MAX];
	STAT_RECT	sRect[OBJ_MAX];
	STAT_POLYGON sPolygon[OBJ_MAX];
	STAT_RADAR sRadar[1];
}STAT_OBJ;


// 128-byte file header saved into raw video files; parsed during playback
typedef struct tagSAVEHead
{
	unsigned char  Head[32];
	unsigned short width;
	unsigned short height;
	unsigned int   totalFrames;			// Total frame count; set to 1 for single photo capture
	unsigned short Freq;				// Video frame rate
	unsigned char  Lens;				// Lens focal parameter
	unsigned char  version;				// SDK version tag
	unsigned int   timelen;				// Total video recording duration
	unsigned char  timestamp[32];		// Capture / recording time string
	unsigned short measuredis;			// Target measuring distance during capture
	unsigned char  devicetype[16];		// Thermal camera model name
	unsigned char  serialnum[24];		// Device serial number
	unsigned char  Reserved1;
	unsigned char  Reserved2;

	unsigned short BackgroundTemp; // Background temperature for medical image capture, stored as temp * 10
	signed   char  BackgroundCorrTemp; // Background correction temperature for medical image capture, stored as temp * 10
	unsigned char  Reserved3;
}T_SAVE_HEAD;


// 512-byte fixed device information structure, all fields stored as string
// Buffer space pre-allocated, no extra memory allocation required
typedef struct tagDeviceID
{
	unsigned char  Name[32];			// Custom device name
	unsigned char  Model[32];		// Thermal camera hardware model
	unsigned char  SerialNum[32];	// Unique device serial number
	unsigned char  Lens[32];			// Lens specification text
	unsigned char  FactoryTime[32];	// Factory production date
	unsigned char  WorkTime[32];		// Cumulative working runtime
	unsigned char  Mac[32];			// Network MAC address
	unsigned char  IP[32];			// Network IP address
	unsigned char  TempRange[32];   // Supported temperature measurement range
	unsigned char  Reserved2[32];
	unsigned char  Reserved3[32];
	unsigned char  Reserved4[32];
	unsigned char  Reserved5[32];
	unsigned char  Reserved6[32];
	unsigned char  Reserved7[32];
	unsigned char  Reserved8[32];
}T_DEVICE_INFO;

/*============================================================================
function:	IRSDK_GetVersion: Get SDK version string formatted as vX.X.X.X
parameter:	char *version: Pre-allocated string buffer (>=16 bytes) to receive version text
return:		int:-1: Execution error
			0: Success
history:	null
==============================================================================*/
IR_SDK_API int IRSDK_GetVersion(char *version);

/*============================================================================
function:	IRSDK_GetPaletteJpeg: Export palette color bar as JPEG image
parameter:	unsigned char* pPaletteJpeg: Output JPEG buffer, pre-allocate ~256*3 bytes
			unsigned int *pJpegLen: Output valid byte length of JPEG data
			unsigned char Method: Fixed value set to 0
			int Pal: Palette index range (0~18)
return:		int:0
history:	null
==============================================================================*/
IR_SDK_API int IRSDK_GetPaletteJpeg(unsigned char* pPaletteJpeg, unsigned int *pJpegLen, unsigned char Method, int Pal);

/*============================================================================
function:	IRSDK_GetPaletteBmp: Export palette color bar as BMP image
parameter:	unsigned char* pPaletteBmp: Output BMP buffer, pre-allocate at least (54+4*256) bytes
			unsigned int *pBmpLen: Output valid byte length of BMP data
			unsigned char Method: Fixed value set to 0
			int Pal: Palette index range (0~18)
return:		int:0
history:	null
==============================================================================*/
IR_SDK_API int IRSDK_GetPaletteBmp(unsigned char* pPaletteBmp, unsigned int *pBmpLen, unsigned char Method, int Pal);

/*============================================================================
function:	IRSDK_GetAnyPointTemp: Calculate temperature of arbitrary pixel coordinate (for mouse hover temperature reading)
parameter:	Frame *pFrame: Input raw frame data pointer
STAT_POINT *pPointStat: Input target point coordinate and measurement parameters
return:		int:0
history:	null
==============================================================================*/
IR_SDK_API int IRSDK_GetAnyPointTemp(Frame *pFrame, STAT_POINT *pPointStat);

/*============================================================================
function:	IRSDK_GetGlobalTemp: Calculate full-image maximum, minimum and average temperature
parameter:	Frame *pFrame: Input raw frame data pointer
			STAT_GLOBAL* pGlobalStat: Input global measurement parameters & output statistics
return:		int:-1: Null pointer error
			0: Success
history:	null
==============================================================================*/
IR_SDK_API int IRSDK_GetGlobalTemp(Frame *pFrame, STAT_GLOBAL* pGlobalStat);

/*============================================================================
function:	IRSDK_GetPointTemp: Calculate temperature statistics for single point object
parameter:	Frame *pFrame: Input raw frame data pointer
			STAT_POINT *pPointStat: Input point coordinate & measurement parameters
			unsigned char statIndex: Temporal filter buffer index; unused if temporal filter disabled (filter not recommended by default)
return:		int:-1: Null pointer error
                0: Success
history:	null
==============================================================================*/
IR_SDK_API int IRSDK_GetPointTemp(Frame *pFrame, STAT_POINT *pPointStat, unsigned char index);

/*============================================================================
function:	IRSDK_GetLineTemp: Calculate temperature statistics along line segment
parameter:	Frame *pFrame: Input raw frame data pointer
			STAT_LINE *pLineStat: Input line endpoints & measurement parameters
			unsigned char statIndex: Temporal filter buffer index; unused if temporal filter disabled (filter not recommended by default)
return:		int:-1: Null pointer error
				0: Success
history:	null
==============================================================================*/
IR_SDK_API int IRSDK_GetLineTemp(Frame *pFrame, STAT_LINE *pLineStat, unsigned char index);

/*============================================================================
function:	IRSDK_GetCircleTemp: Calculate temperature statistics inside circle/ellipse region
parameter:	Frame *pFrame: Input raw frame data pointer
			STAT_CIRCLE *pCircleStat: Input circle geometry & measurement parameters
			unsigned char statIndex: Temporal filter buffer index; unused if temporal filter disabled (filter not recommended by default)
return:		int:-1: Null pointer error
				0: Success
history:	null
==============================================================================*/
IR_SDK_API int IRSDK_GetCircleTemp(Frame *pFrame, STAT_CIRCLE *pCircleStat, unsigned char index);

/*============================================================================
function:	IRSDK_GetRectTemp: Calculate temperature statistics inside rectangular region
parameter:	Frame *pFrame: Input raw frame data pointer
			STAT_RECT *pRectStat: Input rectangle geometry & measurement parameters
			unsigned char statIndex: Temporal filter buffer index; unused if temporal filter disabled (filter not recommended by default)
return:		int:-1: Null pointer error
				0: Success
history:	null
==============================================================================*/
IR_SDK_API int IRSDK_GetRectTemp(Frame *pFrame, STAT_RECT *pRectStat, unsigned char index);

/*============================================================================
function:	IRSDK_GetPolygonTemp: Calculate temperature statistics inside arbitrary polygon region
parameter:	Frame *pFrame: Input raw frame data pointer
			STAT_POLYGON *pPolygonStat: Input polygon vertex data & measurement parameters
			unsigned char statIndex: Temporal filter buffer index; unused if temporal filter disabled (filter not recommended by default)
return:		int:-1: Null pointer error
				0: Success
history:	null
==============================================================================*/
IR_SDK_API int IRSDK_GetPolygonTemp(Frame *pFrame, STAT_POLYGON *pPolygonStat, unsigned char index);

/*============================================================================
function:	IRSDK_GetObjTemp: Batch calculate temperature statistics for all measurement objects
parameter:	Frame *pFrame: Input raw frame data pointer
			STAT_OBJ *pObjStat: Container storing all shape objects and output statistics
return:		int:-1: Null pointer error
			0: Success
history:	null
==============================================================================*/
IR_SDK_API int IRSDK_GetObjTemp(Frame *pFrame, STAT_OBJ *pObjStat);

/*============================================================================
function:	IRSDK_Rgb2Bmp: Convert RGB pixel buffer to standard BMP image
parameter:	unsigned char * pBmpData: Pre-allocated output BMP buffer (minimum size: 54 + width*height*3 bytes)
			unsigned int *pLen: Output total valid byte length of generated BMP
			unsigned char* pRgb: Input interleaved RGB pixel data buffer
			unsigned short Width: Source image pixel width
			unsigned short Height: Source image pixel height
return:		int:0
history:	null
==============================================================================*/
IR_SDK_API int IRSDK_Rgb2Bmp(unsigned char * pBmpData, unsigned int *pLen, unsigned char* pRgb, unsigned short Width, unsigned short Height);

/*============================================================================
function:	IRSDK_Rgb2Jpeg: Compress RGB pixel buffer into JPEG image
parameter:	unsigned char * pJpegout: Pre-allocated output JPEG buffer (minimum size: width*height*3 bytes)
			unsigned int *pLen: Output valid byte length of compressed JPEG
			int quality: Compression quality factor (10~100, recommended value >=80)
			unsigned char* pRgb: Input interleaved RGB pixel data buffer
			unsigned short Width: Source image pixel width
			unsigned short Height: Source image pixel height
return:		int:0
history:	null
==============================================================================*/
IR_SDK_API int IRSDK_Rgb2Jpeg(unsigned char * pJpegout, unsigned int *pLen, int quality, unsigned char * pRgb, unsigned short Width, unsigned short Height);

/*============================================================================
function:	IRSDK_SaveFrame2Row: Export raw Y16 thermal frame data to file
parameter:	char *pFile: Full output file path string
			Frame *pFrame: Input raw thermal frame structure pointer
return:		int:-2: Null pointer parameter error
			-1: File I/O operation failed
			0: Success
history:	null
==============================================================================*/
IR_SDK_API int IRSDK_SaveFrame2Row(char* pFile, Frame* pFrame);

/*============================================================================
function:	IRSDK_SaveFrame2Jpeg_ext_v2: Extended JPEG capture function with device metadata and custom parameter text
parameter:	char *pFile: Output image file path
			Frame *pFrame: Input raw thermal frame data
			unsigned char* pRgb: Rendered color RGB image buffer
			unsigned char isSaveObj: Flag to embed measurement object overlays into JPEG
			STAT_OBJ *pObj: All measurement object data to be drawn
			T_DEVICE_INFO *pDev: Device hardware information to embed
			unsigned char* paramline: Custom user parameter text string
return:		int:-2: Null pointer parameter error
			-1: File I/O operation failed
			0: Success
history:	null
==============================================================================*/
IR_SDK_API int IRSDK_SaveFrame2Jpeg_ext_v2(char* pFile, Frame* pFrame, unsigned char* pRgb, unsigned char isSaveObj, STAT_OBJ* pObj, T_DEVICE_INFO* pDev, unsigned char* paramline);

/*============================================================================
function:	IRSDK_ReadJpeg2Frame_ext_v2: Parse extended JPEG capture file and restore frame data, measurement objects and metadata
parameter:	char *pFile: Input JPEG file path
			Frame *pFrame: Output restored raw thermal frame data
			unsigned char isLoadObj: Flag to load embedded measurement object data
			STAT_OBJ *pObj: Output restored measurement object container
			T_SAVE_HEAD *pImageHead: Output embedded image metadata header
			unsigned char* paramline: Pre-allocated buffer to read custom parameter text
return:		int:-2: Null pointer parameter error
				-1: File parsing / I/O failed
				0: Success
history:	null
==============================================================================*/
IR_SDK_API int IRSDK_ReadJpeg2Frame_ext_v2(char* pFile, Frame* pFrame, unsigned char isLoadObj, STAT_OBJ* pObj, T_SAVE_HEAD* pImageHead, unsigned char* paramline);

/*============================================================================
function:	IRSDK_SaveFrame2Video_ext_v2: Write thermal video stream in GCV format, support metadata and frame skip recording
parameter:	char *pFile: Output video file path
			Frame *pFrame: Input thermal frame data to write
			unsigned char Op: File operation flag (OPEN / WRITE / CLOSE)
			unsigned short recordinter: Frame interval for recording; set to 1 to record every single frame
			unsigned char isSaveObj: Flag to embed measurement object overlays into video
			STAT_OBJ *pObj: Measurement object data to render
			unsigned char* paramline: Custom user parameter text embedded in video header
			unsigned char * pThreadBuf: Thread persistent buffer (min 1024 bytes, keep address unchanged during recording session)
			T_DEVICE_INFO *pDev: Device hardware information to embed
return:		int:-2: File open failed
			-1: Invalid video format error
			0: Success
history:	null
==============================================================================*/
IR_SDK_API int IRSDK_SaveFrame2Video_ext_v2(char* pFile, Frame* pFrame, unsigned char Op, unsigned short recordinter, unsigned char isSaveObj, STAT_OBJ* pObj, unsigned char* paramline, unsigned char* pThreadBuf, T_DEVICE_INFO* pDev);

/*============================================================================
function:	IRSDK_ReadVideo2Frame_v2: Decode GCV thermal video file with custom parameter text support
parameter:	char *pFile: Input GCV video file path
			Frame *pFrame: Output decoded thermal frame data
			unsigned int Index: Target frame index to seek and decode
			unsigned char Op: File operation flag (OPEN / READ / CLOSE)
			T_SAVE_HEAD *pVideoHead: Output embedded video metadata header
			STAT_OBJ *pObj: Output restored measurement object data
			unsigned char* pParamline: Pre-allocated 2KB buffer to read custom parameter text
			unsigned char * pThreadBuf: Thread persistent buffer (min 1024 bytes)
return :
			-2: File open failed
			-1: Invalid video format error
			0: Success
history:	null
==============================================================================*/
IR_SDK_API int IRSDK_ReadVideo2Frame_v2(char* pFile, Frame* pFrame, unsigned int Index, unsigned char Op, T_SAVE_HEAD* pVideoHead, STAT_OBJ* pObj, unsigned char* pParamline, unsigned char* pThreadBuf);

/*============================================================================
function:	IRSDK_SaveRgb2AVI: Encode RGB image sequence into standard AVI video
parameter:	char* pFile: Output file path (max length 512 bytes)
			unsigned char *pRgb: Interleaved RGB pixel buffer of single frame
			unsigned short Width: Image pixel width
			unsigned short Height: Image pixel height
			unsigned char Op: File operation flag (OPEN / WRITE / CLOSE)
			int quality: JPEG compression quality factor
			unsigned char * pThreadBuf: Persistent thread buffer (pre-allocate 1024 bytes, fixed address during recording)
return:		int:0
history:	null
==============================================================================*/
IR_SDK_API int IRSDK_SaveRgb2AVI(char* pFile, unsigned char *pRgb, unsigned short Width, unsigned short Height, unsigned char Op, int quality, unsigned char * pThreadBuf);

/*============================================================================
function:	IRSDK_SaveObj2CSV_e: Export all measurement object temperature data to CSV file (multi-screen playback version with custom timestamp support)
parameter:	char *pFile: Output CSV file path
			unsigned char Op: File operation flag (OPEN / WRITE / CLOSE)
			STAT_OBJ *pObj: All measurement object temperature statistics to export
			char *timestamp: Custom capture timestamp string; pass NULL for real-time mode
			unsigned char * pThreadBuf: Thread persistent buffer (min 1024 bytes)

return:		-2: Null pointer parameter error
			-1: File I/O operation failed
			0: Success
history:	null
==============================================================================*/
IR_SDK_API int IRSDK_SaveObj2CSV_e(char* pFile, unsigned char Op, STAT_OBJ* pObj, char* timestamp, unsigned char* pThreadBuf);

/*============================================================================
function:	IRSDK_SaveFrame2CSV: Export single full-frame temperature statistics to CSV file
parameter:	char *pFile: Output CSV file path
			Frame *pFrame: Input raw thermal frame data
			float emit: Default object emissivity (0.98 recommended)
			float dis: Default measuring distance (2 meters recommended)
			float reflect: Default reflected ambient temperature (20.0℃ recommended)
			float tempoffset: Global temperature correction offset
return:		-2: Null pointer parameter error
			-1: File I/O operation failed
			0: Success
history:	null
==============================================================================*/
IR_SDK_API int IRSDK_SaveFrame2CSV(char* pFile, Frame *pFrame, float emit, float dis, float reflect, float tempoffset);

/*============================================================================
function:	IRSDK_SaveMultiFrame2CSV: Batch export continuous multi-frame temperature data into single CSV log
parameter:	char *pFile: Output CSV log file path
			unsigned char Op: File operation flag (OPEN / WRITE / CLOSE)
			Frame *pFrame: Current thermal frame data to write
			float emit: Default object emissivity
			float dis: Default measuring distance
			float reflect: Default reflected ambient temperature
			float tempoffset: Global temperature correction offset
			unsigned char * pThreadBuf: Thread persistent buffer (min 1024 bytes)
return:		-2: Null pointer parameter error
			-1: File I/O operation failed
			0: Success
history:	null
==============================================================================*/
IR_SDK_API int IRSDK_SaveMultiFrame2CSV(char* pFile, unsigned char Op, Frame *pFrame, float emit, float dis, float reflect, float tempoffset, unsigned char * pThreadBuf);

/*============================================================================
function:	IRSDK_SaveLine2CSV: Export temperature distribution data of line measurement object to CSV
parameter:	char *pFile: Output CSV file path
			Frame *pFrame: Input thermal frame raw data
			STAT_LINE sLine: Target line segment measurement data
			unsigned char u8Format: Output CSV format selector
return:		-2: Null pointer parameter error
			-1: File I/O operation failed
			0: Success
history:	null
==============================================================================*/
IR_SDK_API int IRSDK_SaveLine2CSV(char* pFile, Frame *pFrame, STAT_LINE sLine, unsigned char u8Format);

/*============================================================================
function:	IRSDK_SaveRect2CSV: Export rectangular region temperature statistics to CSV
parameter:	char *pFile: Output CSV file path
			Frame *pFrame: Input thermal frame raw data
			STAT_RECT sRect: Target rectangle measurement data
return:		-2: Null pointer parameter error
			-1: File I/O operation failed
			0: Success
history:	null
==============================================================================*/
IR_SDK_API int IRSDK_SaveRect2CSV(char* pFile, Frame *pFrame, STAT_RECT sRect);

/*============================================================================
function:	IRSDK_SaveCircle2CSV: Export circular region temperature statistics to CSV
parameter:	char *pFile: Output CSV file path
			Frame *pFrame: Input thermal frame raw data
			STAT_CIRCLE sCircle: Target circle measurement data
return:		-2: Null pointer parameter error
			-1: File I/O operation failed
			0: Success
history:	null
==============================================================================*/
IR_SDK_API int IRSDK_SaveCircle2CSV(char* pFile, Frame *pFrame, STAT_CIRCLE sCircle);

/*============================================================================
function:	IRSDK_SavePolygon2CSV: Export arbitrary polygon region temperature statistics to CSV
parameter:	char *pFile: Output CSV file path
			Frame *pFrame: Input thermal frame raw data
			STAT_POLYGON sPolygon: Target polygon measurement data
return:		-2: Null pointer parameter error
			-1: File I/O operation failed
			0: Success
history:	null
==============================================================================*/
IR_SDK_API int IRSDK_SavePolygon2CSV(char* pFile, Frame *pFrame, STAT_POLYGON sPolygon);

/*============================================================================
function:    IRSDK_TempPntCorrect: Correct a temperature value with the most flexible correction method
parameter:   float f32Emiss: emissivity (0.01~1.00)
             float f32Reflect: reflected temperature
             float f32Dis: distance
             float offset: temperature offset correction
             float temp: temperature value to be corrected
return:      float: corrected temperature value
history:     null
==============================================================================*/
IR_SDK_API float IRSDK_TempPntCorrect(float f32Emiss, float f32Reflect, float f32Dis, float offset, float temp);

/*============================================================================
function:    IRSDK_TempPntRevCorrect: Reverse-correct a temperature value to obtain the original uncorrected value
parameter:   float f32Emiss: emissivity (0.01~1.00)
             float f32Reflect: reflected temperature
             float f32Dis: distance
             float offset: temperature offset correction
             float temp: corrected temperature value
return:      float: temperature value before correction
history:     null
==============================================================================*/
IR_SDK_API float IRSDK_TempPntRevCorrect(float f32Emiss, float f32Reflect, float f32Dis, float offset, float temp);

/*============================================================================
function:	IRSDK_Frame2Gray_DDE_m_v3: Latest image rendering pipeline with DDE detail enhancement
parameter:	Frame *pFrame: Input raw thermal frame pointer
			unsigned char *pGray: Output 8-bit grayscale image buffer (0~255 pixel range)
			unsigned char *pRgba: Output RGBA color mapped image buffer (0~255 per channel)
			float f32Constrast: Global contrast adjustment factor
			float f32Bright: Global brightness offset factor
			float f32MinT : Lower temperature limit for auto-ranging;
			float f32MaxT : Upper temperature limit for auto-ranging; Set MinT == MaxT to enable fully automatic temperature range adjustment;
			unsigned char u8Method: Render pipeline mode, fixed value set to 0;
			unsigned short u16TFilterCoef: Temporal noise filter strength (0~100, 0=disable, 1=strongest smoothing, 100=weakest smoothing)
			unsigned char u8DDEcoef: DDE local detail enhancement strength (0~100, 0=disable enhancement)
			unsigned char u8Gamma: Gamma correction factor (0~10, 0=disable gamma adjustment)
			unsigned char u8Pal: Color palette lookup index (0~18) If (u8Pal & 0xE0) is non-zero, output RGB; otherwise output RGBA.
			unsigned char *pThreadbuf: Pre-allocated thread exclusive buffer (w*h*4 bytes), buffer address must stay unchanged during rendering loop
return:		int:0
history:	null
==============================================================================*/
IR_SDK_API int IRSDK_Frame2Gray_DDE_m_v3(Frame* pFrame, unsigned char* pGray, unsigned char* pRgba, float f32Constrast, float f32Bright, float f32MinT, float f32MaxT, unsigned char u8Method, unsigned short u16TFilterCoef, unsigned char u8DDEcoef, unsigned char u8Gamma, unsigned char u8Pal, unsigned char* pThreadbuf);

#endif