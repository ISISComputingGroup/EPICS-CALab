// This software is copyrighted by the HELMHOLTZ-ZENTRUM BERLIN FUER MATERIALIEN UND ENERGIE G.M.B.H., BERLIN, GERMANY (HZB).
// The following terms apply to all files associated with the software. HZB hereby grants permission to use, copy, and modify
// this software and its documentation for non-commercial educational or research purposes, provided that existing copyright
// notices are retained in all copies. The receiver of the software provides HZB with all enhancements, including complete
// translations, made by the receiver.
// IN NO EVENT SHALL HZB BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING
// OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY DERIVATIVES THEREOF, EVEN IF HZB HAS BEEN ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE. HZB SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS,
// AND HZB HAS NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.

//==================================================================================================
// Name        : caLab.cpp
// Author      : Carsten Winkler, (Tobias Höft)
// Version     : 1.5.0.5
// Copyright   : HZB
// Description : library for reading, writing and handle events of EPICS variables (PVs) in LabVIEW
//==================================================================================================

// Definitions
#include <alarmString.h>
#include <cadef.h>
#include <envDefs.h>
#include <epicsMath.h>
#include <epicsStdio.h>
#include <extcode.h>
#include <limits.h>
#include <malloc.h>
#include <map>
#include <queue>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <io.h>
#endif /* _WIN32 */

//#define STOPWATCH
#define DLL_PROCESS_ATTACH  1
#define DLL_PROCESS_DETACH  0
#define DLL_THREAD_ATTACH   2
#define DLL_THREAD_DETACH   3
#define MAX_ERROR_SIZE		255
#define MUTEX_TIMEOUT		6000
#define NO_ALARM            0
#define TASK_DELAY			.1

#if  IsOpSystem64Bit
#define uPtr uQ
#else
#define uPtr uL
#endif

#if MSWin && (ProcessorType == kX86)
/* Windows x86 targets use 1-byte structure packing. */
#pragma pack(push,1)
#pragma warning (disable : 4103)
#endif

// Typedefs
typedef struct {
	size_t dimSize;
	LStrHandle elt[1];
} sStringArray;
typedef sStringArray **sStringArrayHdl;

typedef struct {
	uInt32 dimSizes[2];
	LStrHandle elt[1];
} sStringArray2D;
typedef sStringArray2D **sStringArray2DHdl;

typedef struct {
	size_t dimSize;
	double elt[1];
} sDoubleArray;
typedef sDoubleArray **sDoubleArrayHdl;

typedef struct {
	uInt32 dimSizes[2];
	double elt[1];
} sDoubleArray2D;
typedef sDoubleArray2D **sDoubleArray2DHdl;

typedef struct {
	size_t dimSize;
	uInt32 elt[1];
} sIntArray;
typedef sIntArray **sIntArrayHdl;

typedef struct {
	uInt32 dimSizes[2];
	int64_t elt[1];
} sLongArray2D;
typedef sLongArray2D **sLongArray2DHdl;

typedef struct {
	LVBoolean status;                  // error status
	uInt32 code;                        // error code
	LStrHandle source;                 // error message
} sError;
typedef sError **sErrorHdl;

typedef struct {
	size_t dimSize;
	sError result[1];
} sErrorArray;
typedef sErrorArray **sErrorArrayHdl;

typedef struct {
	LStrHandle PVName;                 // names of PV as string array
	uInt32 Elements;                   // number of PVs
	sStringArrayHdl StringValueArray;  // values as string array
	sDoubleArrayHdl ValueNumberArray;  // values as double array
	LStrHandle StatusString;           // status of PV as string
	int16_t StatusNumber;              // status of PV as short
	LStrHandle SeverityString;         // severity of PV as string
	int16_t SeverityNumber;            // severity of PV as short
	LStrHandle TimeStampString;        // timestamp of PV as string
	uInt32 TimeStampNumber;             // severity of PV as integer
	sStringArrayHdl FieldNameArray;    // optional field names as string array
	sStringArrayHdl FieldValueArray;   // field values as string array
	sError ErrorIO;                    // error structure
} sResult;
typedef sResult *sResultPtr;
typedef sResult **sResultHdl;

typedef struct {
	size_t dimSize;
	sResult result[1];
} sResultArray;
typedef sResultArray **sResultArrayHdl;

typedef struct {
	char szVarName[MAX_STRING_SIZE];
	uInt32 lElems;
	char *szValueArray;
	double *dValueArray;
	char szStatus[MAX_STRING_SIZE];
	short nStatus;
	char szSeverity[MAX_STRING_SIZE];
	short nSeverity;
	char szTimeStamp[MAX_STRING_SIZE];
	long lTimeStamp;
	char *szFieldNameArray;
	char *szFieldValueArray;
	size_t iFieldCount;
	uInt32 lError;
	char szError[MAX_ERROR_SIZE];
} PVData;

typedef struct {
	PVData data;
	chid id[2];    // [0]=value; [1]=enum-strings
	evid sEvid[2]; // [0]=value; [1]=enum-strings
	dbr_gr_enum sEnum;
	double dTimeout;
	double dQueueTimeout;
	char isFirstRun;
	unsigned long lRefNum;
	sResult *pCaLabCluster;
	char hasValue;
	char nPutStatus; // 0=ready; 1=waiting for callback
	chtype nativeType;
} PV;

#if MSWin && (ProcessorType == kX86)
#pragma pack(pop)
#endif

enum epicsAlarmSeverity {
	epicsSevNone = NO_ALARM,
	epicsSevMinor,
	epicsSevMajor,
	epicsSevInvalid,
	ALARM_NSEV
};

enum epicsAlarmCondition {
	epicsAlarmNone = NO_ALARM,
	epicsAlarmRead,
	epicsAlarmWrite,
	epicsAlarmHiHi,
	epicsAlarmHigh,
	epicsAlarmLoLo,
	epicsAlarmLow,
	epicsAlarmState,
	epicsAlarmCos,
	epicsAlarmComm,
	epicsAlarmTimeout,
	epicsAlarmHwLimit,
	epicsAlarmCalc,
	epicsAlarmScan,
	epicsAlarmLink,
	epicsAlarmSoft,
	epicsAlarmBadSub,
	epicsAlarmUDF,
	epicsAlarmDisable,
	epicsAlarmSimm,
	epicsAlarmReadAccess,
	epicsAlarmWriteAccess,
	ALARM_NSTATUS
};

struct cmp_str
{
	bool operator()(char const *a, char const *b)
	{
		return strcmp(a, b) < 0;
	}
};

TH_REENTRANT EXTERNC MgErr _FUNCC DbgPrintfv(const char *buf, va_list args);
static void postEvent(PV* pv);

static bool 											bCaLabPolling = false;
static bool 											bStopped = false;
std::map<const char*, chid, cmp_str> 					chidMap;
std::map<const char*, chid, cmp_str>::const_iterator 	chidMapIt;
static epicsMutexId 									connectQueueLock;
static epicsMutexId 									getLock;
const uInt32 											ERROR_OFFSET = 7000;
static uInt32 											globalCounter = 0;
static uInt32 											iConnectionChangedTaskCounter = 0;
static uInt32 											iConnectTaskCounter = 0;
static uInt32 											iGetCounter = 0;
static uInt32 											iInstances = 0;
static epicsMutexId 									instanceQueueLock = 0x0;
static uInt32 											iPutCounter = 0;
static uInt32 											iPutSyncCounter = 0;
static uInt32 											iPutTaskCounter = 0;
static uInt32 											iValueChangedTaskCounter = 0;
static unsigned long 									lPVCounter = 0;
static struct ca_client_context* 						pcac;
static chid* 											pChannels = NULL;
static FILE * 											pFile = 0x0;
static epicsMutexId 									putLock;
static PV** 											pvList = NULL;
static char** 											pPVNameList = NULL;
static epicsMutexId 									pollingLock;
static epicsMutexId 									pvLock;
std::queue<uInt32> 										sConnectQueue;
epicsTimeStamp 											StartTime;
static epicsMutexId 									syncQueueLock;
static char* 											szCaLabDbg = 0x0;

#ifdef WIN32
	#define EXPORT __declspec(dllexport)
#else
	#define EXPORT
	#define __stdcall
	static int memcpy_s(void *dest, size_t numberOfElements, const void *src, size_t count) {
		memcpy(dest, src, count);
		return 0;
	}
	static int strncpy_s(char *strDest, size_t numberOfElements, const char *strSource, size_t count) {
		strcpy(strDest, strSource);
		return 0;
	}
	static int strncat_s(char *strDest, size_t numberOfElements, const char *strSource, size_t count) {
		strcat(strDest, strSource);
		return 0;
	}
	void __attribute__ ((constructor)) caLabLoad(void);
	void __attribute__ ((destructor)) caLabUnload(void);
#endif

// DbgPrintf wrapper
static MgErr CaLabDbgPrintf(const char *buf, ...) {
	int done = 0;
	va_list listPointer;
	va_start(listPointer, buf);
	if(szCaLabDbg && *szCaLabDbg && pFile) {
		done = vfprintf(pFile, buf, listPointer);
		fprintf(pFile,"\n");
		fflush(pFile);
	} else {
		done = DbgPrintfv(buf, listPointer);
	}
	va_end(listPointer);
	return done;
}

// Write currend time stamp into debug window
static void DbgTime(void) {
	epicsTime current = epicsTime::getCurrent();
	char date[80];
	current.strftime (date, sizeof(date), "%a %b %d %Y %H:%M:%S.%f");
	CaLabDbgPrintf("");CaLabDbgPrintf("%s", date );
}

// mutex wrapper
static void CaLabMutexLock(epicsMutexId id, epicsMutexLockStatus *status, const char* name, double timeout = 1) {
	double itry;
	timeout *= 1000;
	for(itry=0; itry<timeout; itry++) {
		*status = epicsMutexTryLock(id);
		if(*status==epicsMutexLockOK) break;
		epicsThreadSleep(.001);
	}
	if(itry>=timeout) {
		if(name && *name) {
			DbgTime();CaLabDbgPrintf("Warning: timeout in %s", name);
		}
	}
}

// mutex wrapper
static void CaLabMutexUnlock(epicsMutexId id, epicsMutexLockStatus *status) {
	if(*status==epicsMutexLockOK) epicsMutexUnlock(id);
}

// Callback for Channel Access warnings and errors
static void exceptionCallback(struct exception_handler_args args) {
	// Don't enter if library terminates
	if(bStopped)
		return;
	chid					chid = args.chid;	// Channel ID
	long					stat = args.stat;	// Channel status
	unsigned long int		lIndex = 0;			// Current index for local PV list
	epicsMutexLockStatus	lockStatus;			// Status marker for mutex
	PV*						pv = 0x0;			// Pointer to current PV

	if(stat!=192) {
		DbgTime();CaLabDbgPrintf("Channel Access error: %s", ca_message(stat));
		CaLabDbgPrintf("%s", args.ctx);
	}
	if(stat==200) {
		DbgTime();CaLabDbgPrintf("%s","(Please check your configuration of \"EPICS_CA_ADDR_LIST\" and \"EPICS_CA_AUTO_ADDR_LIST\")");
	}
	if(chid) {
		lIndex = (unsigned long int)ca_puser(args.chid);
		CaLabMutexLock(pvLock, &lockStatus, "");
		if(!pvList || lIndex < 0 || lIndex >= lPVCounter || !pvList[lIndex]) {
			CaLabMutexUnlock(pvLock, &lockStatus);
			return;
		}
		pv = pvList[lIndex];	// Get current PV
		epicsSnprintf(pv->data.szError, MAX_ERROR_SIZE, "%s", ca_message(stat));
		pv->data.lError = stat;
		CaLabMutexUnlock(pvLock, &lockStatus);
	}
	return;
}

// Callback for changed EPICS values
//    args:   contains EPICS channel ID and local index of PV
static void valueChanged(evargs args) {
	// Don't enter if library terminates
	if(bStopped)
		return;
	unsigned long int	lIndex = 0;				// Current index for local PV list
	PV*					pv = 0x0;				// Pointer to current PV
	char				szName[MAX_STRING_SIZE];// Name of PV
	char* 				pIndicator;				// Indicator of '.' character
	bool				bDbrTime = 0;			// Marker for results with time stamp
	epicsMutexLockStatus lockStatus;			// Status marker for mutex
	double*				pValue = NULL;			// Value array for doubles
	char*				pszValue = NULL;		// Value array for strings
	unsigned long		lSize = 0;				// Size of value array
	dbr_ctrl_enum*		tmpEnum;				// Temporary enumeration helper

	iValueChangedTaskCounter++;
	lSize = ca_element_count(args.chid);
	lIndex = (unsigned long int)ca_puser(args.chid);
	pValue = (double*)realloc(pValue, lSize*sizeof(double));
	memset(pValue, 0xFF, lSize*sizeof(double));
	pszValue = (char*)realloc(pszValue, lSize*MAX_STRING_SIZE*sizeof(char));
	memset(pszValue, 0x0, lSize*MAX_STRING_SIZE*sizeof(char));
	if(args.status == ECA_NORMAL) {
		// Check type of PV and convert values to double and string array
		switch(args.type) {
		case DBR_TIME_STRING:
			for(uInt32 lCount=0; lCount<lSize; lCount++) {
				char* tmp;
				pValue[lCount] = strtod(((dbr_string_t*)dbr_value_ptr(args.dbr, args.type))[lCount], &tmp);
				epicsSnprintf(pszValue+(lCount*MAX_STRING_SIZE), MAX_STRING_SIZE, "%s", ((dbr_string_t*)dbr_value_ptr(args.dbr, args.type))[lCount]);
			}
			bDbrTime = 1;
			break;
		case DBR_TIME_SHORT:
			short shortValue;
			for(uInt32 lCount=0; lCount<lSize; lCount++) {
				shortValue = ((dbr_short_t*)dbr_value_ptr(args.dbr, args.type))[lCount];
				pValue[lCount] = shortValue;
				epicsSnprintf(pszValue+(lCount*MAX_STRING_SIZE), MAX_STRING_SIZE, "%d", (uInt32)shortValue);
			}
			bDbrTime = 1;
			break;
		case DBR_TIME_CHAR:
			for(uInt32 lCount=0; lCount<lSize; lCount++) {
				pValue[lCount] = ((dbr_char_t*)dbr_value_ptr(args.dbr, args.type))[lCount];
				epicsSnprintf(pszValue+(lCount*MAX_STRING_SIZE), MAX_STRING_SIZE, "%c", ((dbr_char_t*)dbr_value_ptr(args.dbr, args.type))[lCount]);
			}
			bDbrTime = 1;
			break;
		case DBR_TIME_LONG:
			for(uInt32 lCount=0; lCount<lSize; lCount++) {
				pValue[lCount] = ((dbr_long_t*)dbr_value_ptr(args.dbr, args.type))[lCount];
				epicsSnprintf(pszValue+(lCount*MAX_STRING_SIZE), MAX_STRING_SIZE, "%d", ((dbr_long_t*)dbr_value_ptr(args.dbr, args.type))[lCount]);
			}
			bDbrTime = 1;
			break;
		case DBR_TIME_FLOAT:
			for(uInt32 lCount=0; lCount<lSize; lCount++) {
				pValue[lCount] = ((dbr_float_t*)dbr_value_ptr(args.dbr, args.type))[lCount];
				epicsSnprintf(pszValue+(lCount*MAX_STRING_SIZE), MAX_STRING_SIZE, "%g", ((dbr_float_t*)dbr_value_ptr(args.dbr, args.type))[lCount]);
			}
			bDbrTime = 1;
			break;
		case DBR_TIME_DOUBLE:
			for(uInt32 lCount=0; lCount<lSize; lCount++) {
				pValue[lCount] = ((dbr_double_t*)dbr_value_ptr(args.dbr, args.type))[lCount];
				epicsSnprintf(pszValue+(lCount*MAX_STRING_SIZE), MAX_STRING_SIZE, "%g", ((dbr_double_t*)dbr_value_ptr(args.dbr, args.type))[lCount]);
			}
			bDbrTime = 1;
			break;
		case DBR_TIME_ENUM:
			CaLabMutexLock(pvLock, &lockStatus, "value changed DBR_TIME_ENUM");
			for(uInt32 lCount=0; lCount<lSize; lCount++) {
				pValue[lCount] = ((dbr_enum_t*)dbr_value_ptr(args.dbr, args.type))[lCount];
				if(pValue[lCount] < pvList[lIndex]->sEnum.no_str) {
					epicsSnprintf(pszValue+(lCount*MAX_STRING_SIZE), MAX_STRING_SIZE, "%s", pvList[lIndex]->sEnum.strs[((dbr_enum_t*)dbr_value_ptr(args.dbr, args.type))[lCount]]);
				} else {
					epicsSnprintf(pszValue+(lCount*MAX_STRING_SIZE), MAX_STRING_SIZE, "%d", ((dbr_enum_t*)dbr_value_ptr(args.dbr, args.type))[lCount]);
				}
			}
			bDbrTime = 1;
			CaLabMutexUnlock(pvLock, &lockStatus);
			break;
		case DBR_CTRL_ENUM:
			CaLabMutexLock(pvLock, &lockStatus, "value changed DBR_CTRL_ENUM");
			tmpEnum = (dbr_ctrl_enum*)args.dbr;
			pvList[lIndex]->sEnum.no_str = tmpEnum->no_str;
			for(uInt32 i=0; i<(uInt32)tmpEnum->no_str; i++) {
				epicsSnprintf(pvList[lIndex]->sEnum.strs[i], MAX_ENUM_STRING_SIZE, "%s", tmpEnum->strs[i]);
			}
			for(uInt32 lCount=0; lCount<(uInt32)lSize; lCount++) {
				epicsInt16 enumValue = (epicsInt16)pvList[lIndex]->data.dValueArray[lCount];
				if(enumValue < pvList[lIndex]->sEnum.no_str) {
					epicsSnprintf(pszValue+(lCount*MAX_STRING_SIZE), MAX_STRING_SIZE, "%s", pvList[lIndex]->sEnum.strs[enumValue]);
					pValue[lCount] = enumValue;
				} else {
					iValueChangedTaskCounter--;
					CaLabMutexUnlock(pvLock, &lockStatus);
					return;
				}
			}
			bDbrTime = 0;
			CaLabMutexUnlock(pvLock, &lockStatus);
			break;
			// Handle changes of field values
		case DBR_STRING:
			// Splice PV name into variable name and field name
			epicsSnprintf(szName, MAX_STRING_SIZE, "%s", ca_name(args.chid));
			pIndicator = strstr(szName, ".");
			if(!pIndicator) {
				break;
			}
			*pIndicator = 0;
			pIndicator++;
			CaLabMutexLock(pvLock, &lockStatus, "value changed DBR_STRING");
			// Search field and update value
			for(unsigned long int lElement=0; lElement<lPVCounter; lElement++) {
				if(strcmp(szName, pvList[lElement]->data.szVarName) == 0) {
					for(uInt32 iElement=0; (uInt32)iElement<((uInt32)pvList[lElement]->data.iFieldCount); iElement++) {
						if(strcmp(pIndicator, pvList[lElement]->data.szFieldNameArray+iElement*MAX_STRING_SIZE) == 0) {
							epicsSnprintf(pvList[lElement]->data.szFieldValueArray+iElement*MAX_STRING_SIZE, MAX_STRING_SIZE, "%s", (char*)args.dbr);
							pvList[lIndex]->hasValue = 1;
							lElement=lPVCounter;
							break;
						}
					}
				}
			}
			CaLabMutexUnlock(pvLock, &lockStatus);
			bDbrTime = 0;
			break;
		default:
			uInt32 result = ECA_BADTYPE;
			CaLabMutexLock(pvLock, &lockStatus, "value changed DEFAULT");
			epicsSnprintf(pvList[lIndex]->data.szError, MAX_ERROR_SIZE, "%s", ca_message(result));
			pvList[lIndex]->data.lError = result;
			CaLabMutexUnlock(pvLock, &lockStatus);
			break;
		}

	}

	// Get current PV
	if(lIndex < 0 || lIndex >= lPVCounter) {
		if(pValue) {
			free(pValue);
			pValue = NULL;
		}
		if(pszValue) {
			free(pszValue);
			pszValue = NULL;
		}
		iValueChangedTaskCounter--;
		return;
	}
	if(!pvList) {
		if(pValue) {
			free(pValue);
			pValue = NULL;
		}
		if(pszValue) {
			free(pszValue);
			pszValue = NULL;
		}
		iValueChangedTaskCounter--;
		return;
	}
	CaLabMutexLock(pvLock, &lockStatus, "value changed");
	pv = pvList[lIndex];
	if(!pv) {
		if(pValue) {
			free(pValue);
			pValue = NULL;
		}
		if(pszValue) {
			free(pszValue);
			pszValue = NULL;
		}
		iValueChangedTaskCounter--;
		CaLabMutexUnlock(pvLock, &lockStatus);
		return;
	}
	if(args.status == ECA_NORMAL) {
		memcpy_s(pv->data.dValueArray, pv->data.lElems*sizeof(double), pValue, lSize*sizeof(double));
		memcpy_s(pv->data.szValueArray, pv->data.lElems*MAX_STRING_SIZE*sizeof(char), pszValue, lSize*MAX_STRING_SIZE*sizeof(char));
		// Update time stamp, status, serverity and error messages
		if(bDbrTime) {
			epicsTimeToStrftime(pv->data.szTimeStamp, MAX_STRING_SIZE, "%Y-%m-%d %H:%M:%S.%06f", &((struct dbr_time_short*)args.dbr)->stamp);
			pv->data.lTimeStamp = ((struct dbr_time_short*)args.dbr)->stamp.secPastEpoch;
			epicsSnprintf(pv->data.szStatus, MAX_STRING_SIZE, "%s", alarmStatusString[((struct dbr_time_short*)args.dbr)->status]);
			pv->data.nStatus = ((struct dbr_time_short*)args.dbr)->status;
			epicsSnprintf(pv->data.szSeverity, MAX_STRING_SIZE, "%s", alarmSeverityString[((struct dbr_time_short*)args.dbr)->severity]);
			pv->data.nSeverity = ((struct dbr_time_short*)args.dbr)->severity;
			epicsSnprintf(pv->data.szError, MAX_ERROR_SIZE, "%s", ca_message(ECA_NORMAL));
			pv->data.lError = ECA_NORMAL;
			pv->hasValue = 1;
		}
		pv->dQueueTimeout = 0;
	} else {
		epicsSnprintf(pv->data.szError, MAX_ERROR_SIZE, "%s", ca_message(args.status));
		pv->data.lError = args.status;
	}
	// Trigger event
	if(pv->lRefNum)
		postEvent(pv);
	CaLabMutexUnlock(pvLock, &lockStatus);
	if(pValue) {
		free(pValue);
		pValue = NULL;
	}
	if(pszValue) {
		free(pszValue);
		pszValue = NULL;
	}
	iValueChangedTaskCounter--;
}

// Callback for state of new built PVs
//    args:   contains EPICS channel ID and local index of PV
static void connectionChanged(connection_handler_args args) {
	// Don't enter if library terminates
	if(bStopped)
		return;
	unsigned long	lIndex = 0;		// Current index for local PV list
	unsigned long	lElems = 0;		// Number of elements in value array
	PV* 			pv = 0x0;		// Pointer to current PV
	epicsMutexLockStatus lockStatus;// Status marker for mutex

	iConnectionChangedTaskCounter++;
	// Get current PV
	lIndex = (unsigned long)ca_puser(args.chid);
	if(lIndex < 0 || lIndex >= lPVCounter || !pvList) {
		iConnectionChangedTaskCounter--;
		return;
	}
	CaLabMutexLock(pvLock, &lockStatus, "connectionChanged");
	pv = pvList[lIndex];
	if(!pv) {
		CaLabMutexUnlock(pvLock, &lockStatus);
		iConnectionChangedTaskCounter--;
		return;
	}
	// Update status, severity and error messages
	if(args.op==CA_OP_CONN_DOWN) { // Connection is down
		epicsSnprintf(pv->data.szStatus, MAX_STRING_SIZE, "%s", "INVALID");
		pv->data.nStatus = epicsSevInvalid;
		epicsSnprintf(pv->data.szSeverity, MAX_STRING_SIZE, "%s", "DISCONNECTED");
		pv->data.nSeverity = epicsAlarmComm;
		pv->data.lError = ECA_DISCONN;
		epicsSnprintf(pv->data.szError, MAX_ERROR_SIZE, "%s", ca_message(ECA_DISCONN));
		// Trigger event
		if(pv->lRefNum)
			postEvent(pv);
		CaLabMutexUnlock(pvLock, &lockStatus);
		iConnectionChangedTaskCounter--;
		return;
	}
	if(args.op==CA_OP_CONN_UP) { // Connection is OK and event is not initialized
		// Get size of value array and build double and string arrays if necessary
		lElems = ca_element_count(args.chid);
		pv->nativeType = ca_field_type(args.chid);
		pv->data.lElems = lElems;
		if(!pv->data.szValueArray || !pv->data.dValueArray) {
			pv->data.dValueArray = (double*)realloc(pv->data.dValueArray, lElems * sizeof(double));
			if(!pv->data.dValueArray) {
				pv->data.lError = ECA_ALLOCMEM;
				epicsSnprintf(pv->data.szError, MAX_ERROR_SIZE, "%s", ca_message(ECA_ALLOCMEM));
				if(pv->lRefNum)
					postEvent(pv);
				CaLabMutexUnlock(pvLock, &lockStatus);
				iConnectionChangedTaskCounter--;
				return;
			}
			for(uInt32 lCount=0; pv->data.dValueArray && lCount<lElems; lCount++)
				pv->data.dValueArray[lCount] = epicsNAN;
			pv->data.szValueArray = (char*)realloc(pv->data.szValueArray, lElems * MAX_STRING_SIZE * sizeof(char));
			if(!pv->data.szValueArray) {
				pv->data.lError = ECA_ALLOCMEM;
				epicsSnprintf(pv->data.szError, MAX_ERROR_SIZE, "%s", ca_message(ECA_ALLOCMEM));
				if(pv->lRefNum)
					postEvent(pv);
				CaLabMutexUnlock(pvLock, &lockStatus);
				iConnectionChangedTaskCounter--;
				return;
			}
			memset(pv->data.szValueArray, 0, lElems * MAX_STRING_SIZE * sizeof(char));
		}
		if(!pv->id[0]) {
			// Trigger event in error case too
			if(pv->lRefNum)
				postEvent(pv);
			CaLabMutexUnlock(pvLock, &lockStatus);
			iConnectionChangedTaskCounter--;
			return;
		}
		// Create subscriptions for PV value, optional fields and enumerations
		if(!pv->sEvid[0]) {
			if(pv->data.iFieldCount > 0 && strchr(pv->data.szVarName, '.')) { // Field of a PV
				CaLabMutexUnlock(pvLock, &lockStatus);
				ca_create_subscription(DBR_STRING, lElems, pv->id[0], DBE_VALUE | DBE_ALARM, valueChanged, (void*)lIndex, &pv->sEvid[0]);
				CaLabMutexLock(pvLock, &lockStatus, "connectionChanged 1");
			} else {              // Normal PV name
				CaLabMutexUnlock(pvLock, &lockStatus);
				ca_create_subscription(dbf_type_to_DBR_TIME(pv->nativeType), lElems, pv->id[0], DBE_VALUE | DBE_ALARM, valueChanged, (void*)lIndex, &pv->sEvid[0]);
				CaLabMutexLock(pvLock, &lockStatus, "connectionChanged 2");
			}
		}
		if(!pvList || !pv) {
			CaLabMutexUnlock(pvLock, &lockStatus);
			iConnectionChangedTaskCounter--;
			return;
		}
		if(!pv->sEvid[1]) {
			pv->data.lElems = lElems;
			// Check for enumerations
			if(pv->nativeType == DBF_ENUM && !pv->sEnum.no_str) {
				CaLabMutexUnlock(pvLock, &lockStatus);
				ca_create_subscription(DBR_CTRL_ENUM, 1, pv->id[0], DBE_VALUE, valueChanged, (void*)lIndex, &pv->sEvid[1]);
				CaLabMutexLock(pvLock, &lockStatus, "connectionChanged 3");
			}
		}
	}
	// Trigger event
	if(pv && pv->lRefNum)
		postEvent(pv);
	CaLabMutexUnlock(pvLock, &lockStatus);
	iConnectionChangedTaskCounter--;
}

// Callback for finished put task
//    args:   contains EPICS channel ID and local index of PV
static void putState (evargs args ) {
	// Don't enter if library terminates
	if(bStopped)
		return;
	ca_set_puser(args.chid, (void*)args.status);
	iPutSyncCounter--;
}

// Task to connect variables to EPICS
static void connectTask(void) {
	// Don't enter if library terminates
	if(bStopped)
		return;

	uInt32				iResult = 0;	// Result of CA functions
	long int			lIndex = 0;		// Current index for local PV list
	epicsMutexLockStatus lockStatus1;	// Status marker for mutex
	epicsMutexLockStatus lockStatus2;	// Status marker for mutex

	iConnectTaskCounter++;
	ca_attach_context(pcac);
	// Monitoring loop
	while(!bStopped) {
		ca_pend_io(.001);	// Keep it running
		while(!bStopped) { // Check for unconnected PVs
			CaLabMutexLock(connectQueueLock, &lockStatus1, "connectTask");
			if(sConnectQueue.empty()) {
				CaLabMutexUnlock(connectQueueLock, &lockStatus1);
				break;
			}
			lIndex = sConnectQueue.front();
			sConnectQueue.pop();
			CaLabMutexUnlock(connectQueueLock, &lockStatus1);
			if(!pvList) {
				iConnectTaskCounter--;
				ca_detach_context();
				return;
			}
			// Create EPICS channel
			iResult = ca_create_channel(pvList[lIndex]->data.szVarName, connectionChanged, (void*)lIndex, 20, &pvList[lIndex]->id[0]);
			if(iResult != ECA_NORMAL) {
				CaLabMutexLock(pvLock, &lockStatus2, "connectTask 1");
				pvList[lIndex]->data.lError = iResult;
				epicsSnprintf(pvList[lIndex]->data.szError, MAX_ERROR_SIZE, "%s", ca_message(iResult));
				if(pvList[lIndex]->id[0]) {
					CaLabMutexUnlock(pvLock, &lockStatus2);
					ca_clear_channel(pvList[lIndex]->id[0]);
					CaLabMutexLock(pvLock, &lockStatus2, "connectTask 2");
					pvList[lIndex]->id[0] = 0;
				}
				if(pvList[lIndex]->sEvid[0]) {
					CaLabMutexUnlock(pvLock, &lockStatus2);
					ca_clear_subscription(pvList[lIndex]->sEvid[0]);
					CaLabMutexLock(pvLock, &lockStatus2, "connectTask 3");
					pvList[lIndex]->sEvid[0] = 0;
				}
				CaLabMutexUnlock(pvLock, &lockStatus2);
			}
			continue;
		}
		epicsThreadSleep(TASK_DELAY);
	}
	CaLabMutexLock(connectQueueLock, &lockStatus1, "connectTask");
	while(!sConnectQueue.empty())
		sConnectQueue.pop();
	CaLabMutexUnlock(connectQueueLock, &lockStatus1);
	ca_detach_context();
	iConnectTaskCounter--;
}

// Callback of LabVIEW when any caLab-VI is loaded
//		instanceState:	undocumented pointer
extern "C" EXPORT MgErr reserved(InstanceDataPtr *instanceState) {
	// Don't enter if library terminates
	if(bStopped)
		return 0;

	epicsMutexLockStatus lockStatus;	// Status marker for mutex

	if(!instanceQueueLock)
		instanceQueueLock = epicsMutexCreate();
	CaLabMutexLock(instanceQueueLock, &lockStatus, "reserved");
	iInstances++;
	if(iInstances > 1) {
		CaLabMutexUnlock(instanceQueueLock, &lockStatus);
		return 0;
	}
	epicsTimeGetCurrent(&StartTime);
	// Create EPICS context
	if(ca_context_create(ca_enable_preemptive_callback) != ECA_NORMAL) {
		CaLabMutexUnlock(instanceQueueLock, &lockStatus);
		return -1;
	}
	// Remember this channel access context
	pcac = ca_current_context();
	// Add exeption callback
	ca_add_exception_event(exceptionCallback, NULL);
	// Create mutexes
	pvLock = epicsMutexCreate();
	connectQueueLock = epicsMutexCreate();
	syncQueueLock = epicsMutexCreate();
	getLock = epicsMutexCreate();
	putLock  = epicsMutexCreate();
	pollingLock = epicsMutexCreate();
	// Create empty EPICS event (semaphore)
	//sEpicsEventId = epicsEventCreate(epicsEventEmpty);
	// Start init task
	epicsThreadCreate("connectTask",
		epicsThreadPriorityBaseMax,
		epicsThreadGetStackSize(epicsThreadStackBig),
		(EPICSTHREADFUNC)connectTask,0);
	CaLabMutexUnlock(instanceQueueLock, &lockStatus);
	return 0;
}

// Callback of LabVIEW when any caLab-VI is unloaded
//		instanceState:	undocumented pointer
extern "C" EXPORT MgErr unreserved(InstanceDataPtr *instanceState)
{
	return 0;
}

// Callback of LabVIEW when any caLab-VI is aborted
//		instanceState:	undocumented pointer
extern "C" EXPORT MgErr aborted(InstanceDataPtr *instanceState)
{
	return unreserved(instanceState);
}

// Clean up complete result array structure
//		args:	Array of result object
void disposeResultArray(sResultArrayHdl *ResultArray) {
	if(DSCheckHandle(*ResultArray) == noErr && (***ResultArray).dimSize > 0) {
		if(DSCheckHandle((((***ResultArray).result)[0]).PVName) == noErr) {
			DSDisposeHandle((((***ResultArray).result)[0]).PVName);
		}
		if(DSCheckHandle((((***ResultArray).result)[0]).StringValueArray) == noErr) {
			DSDisposeHandle((((***ResultArray).result)[0]).StringValueArray);
		}
		if(DSCheckHandle((((***ResultArray).result)[0]).ValueNumberArray) == noErr) {
			DSDisposeHandle((((***ResultArray).result)[0]).ValueNumberArray);
		}
		if(DSCheckHandle((((***ResultArray).result)[0]).StatusString) == noErr) {
			DSDisposeHandle((((***ResultArray).result)[0]).StatusString);
		}
		if(DSCheckHandle((((***ResultArray).result)[0]).SeverityString) == noErr) {
			DSDisposeHandle((((***ResultArray).result)[0]).SeverityString);
		}
		if(DSCheckHandle((((***ResultArray).result)[0]).TimeStampString) == noErr) {
			DSDisposeHandle((((***ResultArray).result)[0]).TimeStampString);
		}
		if(DSCheckHandle((((***ResultArray).result)[0]).FieldNameArray) == noErr) {
			DSDisposeHandle((((***ResultArray).result)[0]).FieldNameArray);
		}
		if(DSCheckHandle((((***ResultArray).result)[0]).FieldValueArray) == noErr) {
			DSDisposeHandle((((***ResultArray).result)[0]).FieldValueArray);
		}
		if(DSCheckHandle(((((***ResultArray).result)[0]).ErrorIO).source) == noErr) {
			DSDisposeHandle(((((***ResultArray).result)[0]).ErrorIO).source);
		}
		DSDisposeHandle(*ResultArray);
	} else {
		//CaLabDbgPrintf("%s","Bad ResultArray handle");
	}
	*ResultArray = 0x0;
}

// Initialize PV
//		pv:	Result object
//		szName:				New name for result object
//		fieldNameArray:		Array of optional field names for result object
//		bIsFistCall:		Indicator for first call (default: false)
uInt32 initPV(PV* pv, char szName[MAX_STRING_SIZE], sStringArrayHdl* fieldNameArray=0x0, bool bIsFistCall=false) {
	// Don't enter if library terminates
	if(bStopped)
		return 1;

	// Set result object to defaults and add PV name and field names
	pv->dTimeout = 3.0;
	epicsSnprintf(pv->data.szVarName, MAX_STRING_SIZE, "%s", szName);
	pv->data.szValueArray = 0;
	pv->data.dValueArray = 0;
	epicsSnprintf(pv->data.szStatus, MAX_STRING_SIZE, "%s", "INVALID");
	pv->data.nStatus	 = epicsSevInvalid;
	epicsSnprintf(pv->data.szSeverity, MAX_STRING_SIZE, "%s", "DISCONNECTED");
	pv->data.nSeverity = epicsAlarmComm;
	epicsSnprintf(pv->data.szTimeStamp, MAX_STRING_SIZE, "%s", "unknown");
	pv->data.lTimeStamp = 0;
	pv->sEnum.no_str = 0;
	pv->data.lError = ECA_DISCONN;
	epicsSnprintf(pv->data.szError, MAX_ERROR_SIZE, "%s", ca_message(ECA_DISCONN));
	pv->dQueueTimeout = 0;
	pv->isFirstRun = 1;
	pv->hasValue = 0;
	pv->nPutStatus = 1;
	pv->nativeType = DBF_NO_ACCESS;
	// Initialize following values in first call case only
	if(bIsFistCall) {
		pv->data.szFieldNameArray=0;
		pv->data.szFieldValueArray=0;
		pv->data.lElems = 0;
		if(fieldNameArray && *fieldNameArray && **fieldNameArray)
			pv->data.iFieldCount = (**fieldNameArray)->dimSize;
		else
			pv->data.iFieldCount = 0;
		pv->id[0] = 0;
		pv->id[1] = 0;
		pv->sEvid[0] = 0;
		pv->sEvid[1] = 0;
		pv->lRefNum = 0;
		if(fieldNameArray && *fieldNameArray && **fieldNameArray && (**fieldNameArray)->dimSize) {
			pv->data.szFieldNameArray = (char*)realloc(pv->data.szFieldNameArray, (**fieldNameArray)->dimSize*MAX_STRING_SIZE*sizeof(char));
			if(!pv->data.szFieldNameArray) {
				DbgTime();CaLabDbgPrintf("%s", ca_message(ECA_ALLOCMEM));
				return ECA_ALLOCMEM;
			}
			memset(pv->data.szFieldNameArray, 0, (**fieldNameArray)->dimSize*MAX_STRING_SIZE*sizeof(char));
			pv->data.szFieldValueArray = (char*)realloc(pv->data.szFieldValueArray, (**fieldNameArray)->dimSize*MAX_STRING_SIZE*sizeof(char));
			if(!pv->data.szFieldValueArray) {
				DbgTime();CaLabDbgPrintf("%s", ca_message(ECA_ALLOCMEM));
				return ECA_ALLOCMEM;
			}
			memset(pv->data.szFieldValueArray, 0, (**fieldNameArray)->dimSize*MAX_STRING_SIZE*sizeof(char));
			for(uInt32 lElement=0; lElement<((uInt32)(**fieldNameArray)->dimSize); lElement++) {
				uInt32 lCnt = (*(**fieldNameArray)->elt[lElement])->cnt;
				while(lCnt>0 && (*(**fieldNameArray)->elt[lElement])->str[lCnt-1] < 32)
					lCnt--;
				memset(pv->data.szFieldNameArray+lElement*MAX_STRING_SIZE, 0, MAX_STRING_SIZE);
				if(lCnt >= MAX_STRING_SIZE)
					lCnt = MAX_STRING_SIZE-1;
				memcpy_s(pv->data.szFieldNameArray+lElement*MAX_STRING_SIZE, MAX_STRING_SIZE, (const char*)(*(**fieldNameArray)->elt[lElement])->str, lCnt);
			}
		}
	}
	return 0;
}

// Add PV to array
//		szPVName:		Name of PV
//		fieldNameArray:	Array of optional field names
//		return:			Index number for added PV; Error case: -1
uInt32 addPV(char* szPVName, sStringArrayHdl* fieldNameArray) {
	// Don't enter if library terminates
	if(bStopped)
		return -1;

	epicsMutexLockStatus lockStatus1;	// Status marker for mutex
	epicsMutexLockStatus lockStatus2;	// Status marker for mutex

	CaLabMutexLock(pvLock, &lockStatus2, "addPV");
	// Resize memory for new PV
	pvList = (PV**)realloc(pvList, (lPVCounter+1)*sizeof(PV*));
	if(!pvList) {
		DbgTime();CaLabDbgPrintf("%s", ca_message(ECA_ALLOCMEM));
		CaLabMutexUnlock(pvLock, &lockStatus2);
		return -1;
	}
	pvList[lPVCounter] = (PV*)calloc(1, sizeof(PV));
	if(!pvList[lPVCounter]) {
		DbgTime();CaLabDbgPrintf("%s", ca_message(ECA_ALLOCMEM));
		CaLabMutexUnlock(pvLock, &lockStatus2);
		return -1;
	}
	// Set PV to default
	if(initPV(pvList[lPVCounter], szPVName, fieldNameArray, true)) {
		CaLabMutexUnlock(pvLock, &lockStatus2);
		return -1;
	}
	CaLabMutexUnlock(pvLock, &lockStatus2);
	CaLabMutexLock(connectQueueLock, &lockStatus1, "addPV 1");
	// Push PV into connect queue
	sConnectQueue.push(lPVCounter);
	CaLabMutexUnlock(connectQueueLock, &lockStatus1);
	lPVCounter++;
	return lPVCounter-1;
}

// Find PV in local list or add it
//	PvNameArray:	Handle of a array of PV names
//	FieldNameArray:	Handle of a array of optional field names
//	PvIndexArray:	Handle of a array of indexes
//	ValueArraySize:	Number of elements in value array
//	Timeout:		EPICS event timeout in seconds
void getPV(sStringArrayHdl *PvNameArray, sStringArrayHdl *FieldNameArray, sIntArrayHdl *PvIndexArray, uInt32 *ValueArraySize, double Timeout) {
	// Don't enter if library terminates
	if(bStopped)
		return;

	char				szField[MAX_STRING_SIZE];		// Current field name
	char				szName[MAX_STRING_SIZE];		// Current PV name
	char				szNameField[MAX_STRING_SIZE];	// Current PV name for field access
	uInt32				lFind;							// Current index for local PV list
	uInt32				lCounter;						// Simple counter
	epicsMutexLockStatus lockStatus;					// Status marker for mutex
	std::queue<uInt32>	sHasNoValueQueue;				// Waiting queue for unprocessed PVs
	double				timeout;						// Counter for time out

	// Process all given PV names
	for(uInt32 lIndex=0; lIndex<(uInt32)(**PvNameArray)->dimSize; lIndex++) {
		// What's the name?
		memset(szName, 0, MAX_STRING_SIZE);
		if((*(**PvNameArray)->elt[lIndex])->cnt < MAX_STRING_SIZE)
			memcpy_s(szName, MAX_STRING_SIZE, (*(**PvNameArray)->elt[lIndex])->str, (*(**PvNameArray)->elt[lIndex])->cnt);
		else
			memcpy_s(szName, MAX_STRING_SIZE, (*(**PvNameArray)->elt[lIndex])->str, MAX_STRING_SIZE-1);
		// Spaces in PV names are not allowed
		if(strchr(szName, ' ')) {
			DbgTime();CaLabDbgPrintf("white space in PV name \"%s\" detected", szName);
			*(strchr(szName, ' ')) = 0;
		}
		if(strchr(szName, '\t')) {
			DbgTime();CaLabDbgPrintf("tabulator in PV name \"%s\" detected", szName);
			*(strchr(szName, '\t')) = 0;
		}
		// Do I know you?
		lCounter = 0;
		lFind = lIndex;
		while(lCounter < lPVCounter) {
			if(lFind >= lPVCounter)
				lFind = 0;
			if(!pvList) {
				return;
			}
			CaLabMutexLock(pvLock, &lockStatus, "getPV");
			if(!strcmp(szName, pvList[lFind]->data.szVarName)) {
				CaLabMutexUnlock(pvLock, &lockStatus);
				break;
			}
			CaLabMutexUnlock(pvLock, &lockStatus);
			lFind++;
			lCounter++;
		}
		// If known in local PV list set PV index array (LV variable)
		if(lCounter < lPVCounter) {
			(**PvIndexArray)->elt[lIndex] = (uInt32)lFind;
		} else { // Else add PV to local list and than set index
			lFind = addPV(szName, FieldNameArray);
			if(lFind >= 0) {
				(**PvIndexArray)->elt[lIndex] = lFind;
				sHasNoValueQueue.push(lFind);
			}
		}
		// Process all given field variables
		for(uInt32 iFieldIndex=0; FieldNameArray && *FieldNameArray && **FieldNameArray && iFieldIndex<((uInt32)(**FieldNameArray)->dimSize); iFieldIndex++) {
			if(!(**FieldNameArray)->elt[iFieldIndex])
				continue;
			// What's the name?
			memset(szField, 0, MAX_STRING_SIZE);
			memset(szNameField, 0, MAX_STRING_SIZE);
			if((*(**FieldNameArray)->elt[iFieldIndex])->cnt < MAX_STRING_SIZE)
				memcpy_s(szField, MAX_STRING_SIZE, (*(**FieldNameArray)->elt[iFieldIndex])->str, (*(**FieldNameArray)->elt[iFieldIndex])->cnt);
			else
				memcpy_s(szField, MAX_STRING_SIZE, (*(**FieldNameArray)->elt[iFieldIndex])->str, MAX_STRING_SIZE-1);
			epicsSnprintf(szNameField, MAX_STRING_SIZE, "%s.%s", szName, szField);
			// Spaces in field names are not allowed
			if(strchr(szNameField, ' ')) {
				DbgTime();CaLabDbgPrintf("white space in PV name \"%s\" detected", szNameField);
				*(strchr(szNameField, ' ')) = 0;
			}
			if(strchr(szNameField, '\t')) {
				DbgTime();CaLabDbgPrintf("tabulator in PV name \"%s\" detected", szNameField);
				*(strchr(szNameField, '\t')) = 0;
			}
			// Do I know you?
			lFind = 0;
			for(lFind=0; lFind<lPVCounter; lFind++) {
				// Yep, I found you
				if(!pvList) {
					return;
				}
				CaLabMutexLock(pvLock, &lockStatus, "getPV 1");
				if(!strcmp(szNameField, pvList[lFind]->data.szVarName)) {
					CaLabMutexUnlock(pvLock, &lockStatus);
					break;
				}
				CaLabMutexUnlock(pvLock, &lockStatus);
			}
			// Not found. Add this field to local list
			if(lFind >= lPVCounter) {
				uInt32 lResult = addPV(szNameField, FieldNameArray);
				if(lResult >= 0) {
					sHasNoValueQueue.push(lResult);
				}
			}
		}
	}
	// Wait for values
	timeout = Timeout * 1000;
	while(!sHasNoValueQueue.empty() && !bStopped && timeout > 0) {
		unsigned long lIndex = sHasNoValueQueue.front();
		sHasNoValueQueue.pop();
		if(lIndex < 0 || lIndex >= lPVCounter) {
			continue;
		}
		if(!pvList) {
			return;
		}
		CaLabMutexLock(pvLock, &lockStatus, "getPV 2");
		if(pvList[lIndex]->id[0] && pvList[lIndex]->hasValue) {
			CaLabMutexUnlock(pvLock, &lockStatus);
			continue;
		} else {
			CaLabMutexUnlock(pvLock, &lockStatus);
			sHasNoValueQueue.push(lIndex);
			epicsThreadSleep(.001);
			timeout--;
		}
	}
	// Clean waiting queue
	while(!sHasNoValueQueue.empty())
		sHasNoValueQueue.pop();
	*ValueArraySize = 0;
	// Find biggest array size and set it to ValueArraySize
	if(!pvList) {
		return;
	}
	CaLabMutexLock(pvLock, &lockStatus, "getPV 3");
	for(uInt32 lIndex=0; lIndex < (uInt32)(**PvIndexArray)->dimSize; lIndex++) {
		if(pvList[(**PvIndexArray)->elt[lIndex]]->data.lElems > *ValueArraySize) {
			*ValueArraySize = pvList[(**PvIndexArray)->elt[lIndex]]->data.lElems;
		}
	}
	CaLabMutexUnlock(pvLock, &lockStatus);
}

// Read EPICS PVs
//	PvNameArray:			Handle of a array of PV names
//	FieldNameArray:			Handle of a array of optional field names
//	PvIndexArray:			Handle of a array of indexes
//	Timeout:				EPICS event timeout in seconds
//	ResultArray:			Handle of a result-cluster (result object)
//	FirstStringValue:		Handle of a array of first values converted to a string
//	FirstDoubleValue:		Handle of a array of first values converted to a double value
//	DoubleValueArray:		Handle of a 2d array of double values
//	DoubleValueArraySize:	Array size of DoubleValueArray
//	CommunicationStatus:	Status of Channel Access communication; 0 = no problem; 1 = any problem occurred
//	FirstCall:				Indicator for first call
extern "C" EXPORT void getValue(sStringArrayHdl *PvNameArray, sStringArrayHdl *FieldNameArray, sIntArrayHdl *PvIndexArray, double Timeout, sResultArrayHdl *ResultArray, sStringArrayHdl *FirstStringValue, sDoubleArrayHdl *FirstDoubleValue, sDoubleArray2DHdl *DoubleValueArray, uInt32* DoubleValueArraySize, LVBoolean *CommunicationStatus, LVBoolean *FirstCall) {
	// Don't enter if library terminates
	if(bStopped && !getLock)
		return;

	double					dValue;					// Value of double
	uInt32					fieldSize = 0;			// Size of filed name array
	uInt32					iResult = 0;			// Result of LabVIEW function
	uInt32					lIndex=0;				// Current index for local PV list
	epicsMutexLockStatus	lockStatus1;			// Status marker for mutex "getlock"
	epicsMutexLockStatus	lockStatus2;			// Status marker for mutex "pvLock"
	epicsMutexLockStatus	lockStatus3;			// Status marker for mutex "pollingLock"
	size_t					lResultArraySize = 0;	// Size of result array
	size_t					lSize = 0;				// Size of a string
	const char*				pValue = 0x0;			// Pointer to current value
	uInt32					valueArraySize = 0;		// Size of value array

#ifdef STOPWATCH
	epicsTimeStamp time1;
	epicsTimeStamp time2;
	epicsTimeStamp time3;
	epicsTimeGetCurrent(&time1);
#endif
	if(bCaLabPolling) CaLabMutexLock(pollingLock, &lockStatus3, "polling mode", 2*Timeout);
	CaLabMutexLock(getLock, &lockStatus1, "getValue", 2*Timeout);
	iGetCounter++;
	// Check time out
	if(Timeout <= 0)
		Timeout = 3;
	// Clean up on first call
	if(*FirstCall) {
		if(DSCheckHandle(*PvIndexArray) == noErr) {
			DSDisposeHandle(*PvIndexArray);
			*PvIndexArray = 0x00;
		}
		if(DSCheckHandle(*ResultArray) == noErr) {
			disposeResultArray(ResultArray);
		}
		if(DSCheckHandle(*FirstStringValue) == noErr) {
			DSDisposeHandle(*FirstStringValue);
			*FirstStringValue = 0x00;
		}
		if(DSCheckHandle(*FirstDoubleValue) == noErr) {
			DSDisposeHandle(*FirstDoubleValue);
			*FirstDoubleValue = 0x00;
		}
		if(DSCheckHandle(*DoubleValueArray) == noErr) {
			DSDisposeHandle(*DoubleValueArray);
			*DoubleValueArray = 0x00;
		}
	}
	// Check handles and pointers
	if(CommunicationStatus == 0x0) {
		DbgTime();CaLabDbgPrintf("%s","ERROR: Invalid pointer for variable CommunicationStatus");
		disposeResultArray(ResultArray);
		iGetCounter--;
		CaLabMutexUnlock(getLock, &lockStatus1);
		if(bCaLabPolling) CaLabMutexUnlock(pollingLock, &lockStatus3);
		return;
	}
	*CommunicationStatus = 1;
	if(DoubleValueArraySize == 0x0) {
		DbgTime();CaLabDbgPrintf("%s","ERROR: Invalid pointer for value array size");
		disposeResultArray(ResultArray);
		iGetCounter--;
		CaLabMutexUnlock(getLock, &lockStatus1);
		if(bCaLabPolling) CaLabMutexUnlock(pollingLock, &lockStatus3);
		return;
	}
	if(*PvNameArray == 0x0 || (**PvNameArray)->dimSize <= 0 || !(**PvNameArray)->elt[0]) {
		DbgTime();CaLabDbgPrintf("%s","ERROR: Invalid PV name array");
		disposeResultArray(ResultArray);
		iGetCounter--;
		CaLabMutexUnlock(getLock, &lockStatus1);
		if(bCaLabPolling) CaLabMutexUnlock(pollingLock, &lockStatus3);
		return;
	}
	*CommunicationStatus = 0;
	// Get number of requested PVs
	lResultArraySize = (**PvNameArray)->dimSize;
	// Get number of optional fields
	if(*FieldNameArray) {
		fieldSize = (uInt32)(**FieldNameArray)->dimSize;
	}
	// Create index array or use previous one
	if(*FirstCall) {
		*PvIndexArray = (sIntArrayHdl)DSNewHClr(sizeof(size_t)+lResultArraySize*sizeof(uInt32[1]));
		(**PvIndexArray)->dimSize = lResultArraySize;
	} else if(**PvIndexArray == 0x0) {
		*PvIndexArray = (sIntArrayHdl)DSNewHClr(sizeof(size_t)+lResultArraySize*sizeof(uInt32[1]));
		(**PvIndexArray)->dimSize = lResultArraySize;
	} else if((**PvIndexArray)->dimSize != lResultArraySize) {
		DSDisposeHandle(*PvIndexArray);
		*PvIndexArray = (sIntArrayHdl)DSNewHClr(sizeof(size_t)+lResultArraySize*sizeof(uInt32[1]));
		(**PvIndexArray)->dimSize = lResultArraySize;
	}
#ifdef STOPWATCH
	epicsTimeGetCurrent(&time2);
#endif
	getPV(PvNameArray, FieldNameArray, PvIndexArray, DoubleValueArraySize, Timeout);
#ifdef STOPWATCH
	epicsTimeGetCurrent(&time3);
	double diff = epicsTimeDiffInSeconds(&time3, &time2);
	if(diff > 1)
		CaLabDbgPrintf("getPV: %.0fms",1000*diff);
#endif
	CaLabMutexLock(pvLock, &lockStatus2, "getValue 1");
	if(!pvList) {
		disposeResultArray(ResultArray);
		iGetCounter--;
		CaLabMutexUnlock(pvLock, &lockStatus2);
		CaLabMutexUnlock(getLock, &lockStatus1);
		if(bCaLabPolling) CaLabMutexUnlock(pollingLock, &lockStatus3);
		return;
	}
	// Create double value array or use previous one
	if(*FirstCall && DSCheckHandle(*FirstStringValue) != noErr) {
		*FirstStringValue = (sStringArrayHdl)DSNewHClr(sizeof(size_t)+lResultArraySize*sizeof(LStrHandle[1]));
		(**FirstStringValue)->dimSize = lResultArraySize;
	} else if(*FirstStringValue == 0x0) {
		*FirstStringValue = (sStringArrayHdl)DSNewHClr(sizeof(size_t)+lResultArraySize*sizeof(LStrHandle[1]));
		(**FirstStringValue)->dimSize = lResultArraySize;
	} else if((**FirstStringValue)->dimSize != lResultArraySize) {
		DSDisposeHandle(*FirstStringValue);
		*FirstStringValue = (sStringArrayHdl)DSNewHClr(sizeof(size_t)+lResultArraySize*sizeof(LStrHandle[1]));
		(**FirstStringValue)->dimSize = lResultArraySize;
	}
	if(*FirstCall && DSCheckHandle(*FirstDoubleValue) != noErr) {
		*FirstDoubleValue = (sDoubleArrayHdl)DSNewHClr(sizeof(size_t)+lResultArraySize*sizeof(double[1]));
		(**FirstDoubleValue)->dimSize = lResultArraySize;
	} else if(*FirstDoubleValue == 0x0) {
		*FirstDoubleValue = (sDoubleArrayHdl)DSNewHClr(sizeof(size_t)+lResultArraySize*sizeof(double[1]));
		(**FirstDoubleValue)->dimSize = lResultArraySize;
	} else if((**FirstDoubleValue)->dimSize != lResultArraySize) {
		DSDisposeHandle(*FirstDoubleValue);
		*FirstDoubleValue = (sDoubleArrayHdl)DSNewHClr(sizeof(size_t)+lResultArraySize*sizeof(double[1]));
		(**FirstDoubleValue)->dimSize = lResultArraySize;
	}
	if(*FirstCall && DSCheckHandle(*DoubleValueArray) != noErr) {
		*DoubleValueArray = (sDoubleArray2DHdl)DSNewHClr(sizeof(uInt32[2])+(*DoubleValueArraySize*lResultArraySize*sizeof(double[1])));
		(**DoubleValueArray)->dimSizes[0] = (uInt32)lResultArraySize;
		(**DoubleValueArray)->dimSizes[1] = *DoubleValueArraySize;
	} else if(*DoubleValueArray == 0x0) {
		*DoubleValueArray = (sDoubleArray2DHdl)DSNewHClr(sizeof(uInt32[2])+(*DoubleValueArraySize*lResultArraySize*sizeof(double[1])));
		(**DoubleValueArray)->dimSizes[0] = (uInt32)lResultArraySize;
		(**DoubleValueArray)->dimSizes[1] = *DoubleValueArraySize;
	} else if((**DoubleValueArray)->dimSizes[0]*(**DoubleValueArray)->dimSizes[1] != (uInt32)(*DoubleValueArraySize*lResultArraySize)) {
		DSDisposeHandle(*DoubleValueArray);
		*DoubleValueArray = (sDoubleArray2DHdl)DSNewHClr(sizeof(uInt32[2])+(*DoubleValueArraySize*lResultArraySize*sizeof(double[1])));
		(**DoubleValueArray)->dimSizes[0] = (uInt32)lResultArraySize;
		(**DoubleValueArray)->dimSizes[1] = *DoubleValueArraySize;
	}
	memset((***DoubleValueArray).elt, 0x0, *DoubleValueArraySize*lResultArraySize*sizeof(double[1]));

	// Create result array (cluster) or use previous one
	if(*FirstCall && DSCheckHandle(*ResultArray) != noErr) {
		*ResultArray = (sResultArrayHdl)DSNewHClr(sizeof(size_t)+lResultArraySize*sizeof(sResult[1]));
	} else if(*ResultArray == 0x0) {
		*ResultArray = (sResultArrayHdl)DSNewHClr(sizeof(size_t)+lResultArraySize*sizeof(sResult[1]));
	} else if((**ResultArray)->dimSize != lResultArraySize) {
		disposeResultArray(ResultArray);
		*ResultArray = (sResultArrayHdl)DSNewHClr(sizeof(size_t)+lResultArraySize*sizeof(sResult[1]));
	}
	// Build result array (write content)
	for(lIndex = 0; !bStopped && pvList && lIndex<(uInt32)lResultArraySize; lIndex++) {
		// Create PVName or use previous one
		pValue = pvList[(**PvIndexArray)->elt[lIndex]]->data.szVarName;
		lSize = strlen(pValue);
		if((**ResultArray)->result[lIndex].PVName == 0x0) {
			(**ResultArray)->result[lIndex].PVName = (LStrHandle)DSNewHClr(sizeof(size_t)+lSize*sizeof(uChar[1]));
		} else {
			if((*(**ResultArray)->result[lIndex].PVName)->cnt != (int32)lSize) {
				DSDisposeHandle((**ResultArray)->result[lIndex].PVName);
				(**ResultArray)->result[lIndex].PVName = (LStrHandle)DSNewHClr(sizeof(size_t)+lSize*sizeof(uChar[1]));
				if(DSCheckHandle((**ResultArray)->result[lIndex].PVName) != noErr) {
					DbgTime();CaLabDbgPrintf("%s","Error while creating LabVIEW array (1a)");
					*CommunicationStatus = 1;
					disposeResultArray(ResultArray);
					iGetCounter--;
					CaLabMutexUnlock(pvLock, &lockStatus2);
					CaLabMutexUnlock(getLock, &lockStatus1);
					if(bCaLabPolling) CaLabMutexUnlock(pollingLock, &lockStatus3);
					return;
				}
			}
		}
		memcpy_s((*(**ResultArray)->result[lIndex].PVName)->str, lSize, pValue, lSize);
		(*(**ResultArray)->result[lIndex].PVName)->cnt = (int32)lSize;
		// Get size of value array
		valueArraySize = pvList[(**PvIndexArray)->elt[lIndex]]->data.lElems;
		// Create StringValueArray2D or use previous one
		if(valueArraySize > 0) {
			if((**ResultArray)->result[lIndex].StringValueArray == 0x0) {
				(**ResultArray)->result[lIndex].StringValueArray = (sStringArrayHdl)DSNewHClr(sizeof(size_t)+valueArraySize*sizeof(LStrHandle[1]));
			} else {
				if((uInt32)((*(**ResultArray)->result[lIndex].StringValueArray)->dimSize) != valueArraySize) {
					// Remove values
					for(int32 ii=((int32)(*(**ResultArray)->result[lIndex].StringValueArray)->dimSize)-1; !bStopped && ii>=0; ii--) {
						if((*(**ResultArray)->result[lIndex].StringValueArray)->elt[ii] != 0x0) {
							(*(*(**ResultArray)->result[lIndex].StringValueArray)->elt[ii])->cnt = 0;
							iResult = DSDisposeHandle((*(**ResultArray)->result[lIndex].StringValueArray)->elt[ii]);
							(*(**ResultArray)->result[lIndex].StringValueArray)->elt[ii] = 0x0;
							if(iResult != noErr) {
								DbgTime();CaLabDbgPrintf("%s","Error while cleaning up LabVIEW array (2)");
								*CommunicationStatus = 1;
								disposeResultArray(ResultArray);
								iGetCounter--;
								CaLabMutexUnlock(pvLock, &lockStatus2);
								CaLabMutexUnlock(getLock, &lockStatus1);
								if(bCaLabPolling) CaLabMutexUnlock(pollingLock, &lockStatus3);
								return;
							}
						} else {
							DbgTime();CaLabDbgPrintf("%s","Error while cleaning up LabVIEW array (2a)");
							*CommunicationStatus = 1;
							disposeResultArray(ResultArray);
							iGetCounter--;
							CaLabMutexUnlock(pvLock, &lockStatus2);
							CaLabMutexUnlock(getLock, &lockStatus1);
							if(bCaLabPolling) CaLabMutexUnlock(pollingLock, &lockStatus3);
							return;
						}
					}
					if((*(**ResultArray)->result[lIndex].StringValueArray)->dimSize != (size_t)valueArraySize) {
						DSDisposeHandle((**ResultArray)->result[lIndex].StringValueArray);
						(**ResultArray)->result[lIndex].StringValueArray = (sStringArrayHdl)DSNewHClr(sizeof(size_t)+valueArraySize*sizeof(LStrHandle[1]));
						if(DSCheckHandle((**ResultArray)->result[lIndex].StringValueArray) != noErr) {
							DbgTime();CaLabDbgPrintf("%s","Error while creating LabVIEW array (2b)");
							*CommunicationStatus = 1;
							disposeResultArray(ResultArray);
							iGetCounter--;
							CaLabMutexUnlock(pvLock, &lockStatus2);
							CaLabMutexUnlock(getLock, &lockStatus1);
							if(bCaLabPolling) CaLabMutexUnlock(pollingLock, &lockStatus3);
							return;
						}
					}
				}
			}
			// Copy values from cache to LV array
			for(uInt32 lArrayIndex=0; !bStopped && lArrayIndex<(uInt32)valueArraySize; lArrayIndex++) {
				pValue = pvList[(**PvIndexArray)->elt[lIndex]]->data.szValueArray+lArrayIndex*MAX_STRING_SIZE;
				lSize = strlen(pValue);
				if(*ResultArray == 0x0 || &(**ResultArray)->result[lIndex] == 0x0) {
					DbgTime();CaLabDbgPrintf("%s","bad handle size for ResultArray");
					*CommunicationStatus = 1;
					disposeResultArray(ResultArray);
					iGetCounter--;
					CaLabMutexUnlock(pvLock, &lockStatus2);
					CaLabMutexUnlock(getLock, &lockStatus1);
					if(bCaLabPolling) CaLabMutexUnlock(pollingLock, &lockStatus3);
					return;
				}
				if((*(**ResultArray)->result[lIndex].StringValueArray)->elt[lArrayIndex] == 0x0) {
					(*(**ResultArray)->result[lIndex].StringValueArray)->elt[lArrayIndex] = (LStrHandle) DSNewHClr(sizeof(size_t)+lSize*sizeof(uChar[1]));
				} else {
					if((*(*(**ResultArray)->result[lIndex].StringValueArray)->elt[lArrayIndex])->cnt != (int32)lSize) {
						DSDisposeHandle((*(**ResultArray)->result[lIndex].StringValueArray)->elt[lArrayIndex]);
						(*(**ResultArray)->result[lIndex].StringValueArray)->elt[lArrayIndex] = (LStrHandle) DSNewHClr(sizeof(size_t)+lSize*sizeof(uChar[1]));
						if(DSCheckHandle((*(**ResultArray)->result[lIndex].StringValueArray)->elt[lArrayIndex]) != noErr) {
							DbgTime();CaLabDbgPrintf("%s","Error while creating LabVIEW array (2c)");
							*CommunicationStatus = 1;
							disposeResultArray(ResultArray);
							iGetCounter--;
							CaLabMutexUnlock(pvLock, &lockStatus2);
							CaLabMutexUnlock(getLock, &lockStatus1);
							if(bCaLabPolling) CaLabMutexUnlock(pollingLock, &lockStatus3);
							return;
						}
					}
				}
				memcpy_s((*(*(**ResultArray)->result[lIndex].StringValueArray)->elt[lArrayIndex])->str, lSize, pValue, lSize);
				(*(*(**ResultArray)->result[lIndex].StringValueArray)->elt[lArrayIndex])->cnt = (int32)lSize;
			}
			(*(**ResultArray)->result[lIndex].StringValueArray)->dimSize = valueArraySize;
			// Create ValueNumberArray or use previous one
			if((**ResultArray)->result[lIndex].ValueNumberArray == 0x0) {
				(**ResultArray)->result[lIndex].ValueNumberArray = (sDoubleArrayHdl)DSNewHClr(sizeof(size_t)+valueArraySize*sizeof(double[1]));
			} else {
				if((uInt32)((*(**ResultArray)->result[lIndex].ValueNumberArray)->dimSize) != (size_t)valueArraySize) {
					DSDisposeHandle((**ResultArray)->result[lIndex].ValueNumberArray);
					(**ResultArray)->result[lIndex].ValueNumberArray = (sDoubleArrayHdl)DSNewHClr(sizeof(size_t)+valueArraySize*sizeof(double[1]));
					if(DSCheckHandle((((**ResultArray)->result)[lIndex]).ValueNumberArray) != noErr) {
						DbgTime();CaLabDbgPrintf("%s","Error while creating LabVIEW array (3)");
						*CommunicationStatus = 1;
						disposeResultArray(ResultArray);
						iGetCounter--;
						CaLabMutexUnlock(pvLock, &lockStatus2);
						CaLabMutexUnlock(getLock, &lockStatus1);
						if(bCaLabPolling) CaLabMutexUnlock(pollingLock, &lockStatus3);
						return;
					}
				}
			}
			for(uInt32 lArrayIndex=0; !bStopped && lArrayIndex<valueArraySize && lArrayIndex<*DoubleValueArraySize; lArrayIndex++) {
				dValue = pvList[(**PvIndexArray)->elt[lIndex]]->data.dValueArray[lArrayIndex];
				((*(**ResultArray)->result[lIndex].ValueNumberArray)->elt[lArrayIndex]) = dValue;
				(*(**DoubleValueArray)).elt[lArrayIndex + lIndex*(*(**DoubleValueArray)).dimSizes[1]] = dValue;
			}
			(*(**ResultArray)->result[lIndex].ValueNumberArray)->dimSize = valueArraySize;
		}
		// Create StatusString or use previous one
		if(valueArraySize > 0)
			pValue = pvList[(**PvIndexArray)->elt[lIndex]]->data.szStatus;
		else
			pValue = "INVALID";
		lSize = strlen(pValue);
		if((**ResultArray)->result[lIndex].StatusString == 0x0) {
			(**ResultArray)->result[lIndex].StatusString = (LStrHandle)DSNewHClr(sizeof(size_t)+lSize*sizeof(uChar[1]));
		} else {
			if((*(**ResultArray)->result[lIndex].StatusString)->cnt != (int32)lSize) {
				DSDisposeHandle((**ResultArray)->result[lIndex].StatusString);
				(**ResultArray)->result[lIndex].StatusString = (LStrHandle)DSNewHClr(sizeof(size_t)+lSize*sizeof(uChar[1]));
				if(DSCheckHandle((**ResultArray)->result[lIndex].StatusString) != noErr) {
					DbgTime();CaLabDbgPrintf("%s","Error while creating LabVIEW array (4)");
					*CommunicationStatus = 1;
					disposeResultArray(ResultArray);
					iGetCounter--;
					CaLabMutexUnlock(pvLock, &lockStatus2);
					CaLabMutexUnlock(getLock, &lockStatus1);
					if(bCaLabPolling) CaLabMutexUnlock(pollingLock, &lockStatus3);
					return;
				}
			}
		}
		memcpy_s((*(**ResultArray)->result[lIndex].StatusString)->str, lSize, pValue, lSize);
		(*(**ResultArray)->result[lIndex].StatusString)->cnt = (int32)lSize;
		// Write StatusNumber
		if(valueArraySize > 0)
			(**ResultArray)->result[lIndex].StatusNumber = pvList[(**PvIndexArray)->elt[lIndex]]->data.nStatus;
		else
			(**ResultArray)->result[lIndex].StatusNumber = 3;
		// Create SeverityString or use previous one
		if(valueArraySize > 0)
			pValue = pvList[(**PvIndexArray)->elt[lIndex]]->data.szSeverity;
		else
			pValue = "DISCONNECTED";
		lSize = strlen(pValue);
		if((**ResultArray)->result[lIndex].SeverityString == 0x0) {
			(**ResultArray)->result[lIndex].SeverityString = (LStrHandle)DSNewHClr(sizeof(size_t)+lSize*sizeof(uChar[1]));
		} else {
			if((*(**ResultArray)->result[lIndex].SeverityString)->cnt != (int32)lSize) {
				DSDisposeHandle((**ResultArray)->result[lIndex].SeverityString);
				(**ResultArray)->result[lIndex].SeverityString = (LStrHandle)DSNewHClr(sizeof(size_t)+lSize*sizeof(uChar[1]));
				if(DSCheckHandle((**ResultArray)->result[lIndex].SeverityString) != noErr) {
					DbgTime();CaLabDbgPrintf("%s","Error while creating LabVIEW array (5)");
					*CommunicationStatus = 1;
					disposeResultArray(ResultArray);
					iGetCounter--;
					CaLabMutexUnlock(pvLock, &lockStatus2);
					CaLabMutexUnlock(getLock, &lockStatus1);
					if(bCaLabPolling) CaLabMutexUnlock(pollingLock, &lockStatus3);
					return;
				}
			}
		}
		memcpy_s((*(**ResultArray)->result[lIndex].SeverityString)->str, lSize, pValue, lSize);
		(*(**ResultArray)->result[lIndex].SeverityString)->cnt = (int32)lSize;
		// Write SeverityNumber
		if(valueArraySize > 0)
			(**ResultArray)->result[lIndex].SeverityNumber = pvList[(**PvIndexArray)->elt[lIndex]]->data.nSeverity;
		else
			(**ResultArray)->result[lIndex].SeverityNumber = 9;
		// Create TimeStampString or use previous one
		if(valueArraySize > 0)
			pValue = pvList[(**PvIndexArray)->elt[lIndex]]->data.szTimeStamp;
		else
			pValue = "unknown";
		lSize = strlen(pValue);
		if((**ResultArray)->result[lIndex].TimeStampString == 0x0) {
			(**ResultArray)->result[lIndex].TimeStampString = (LStrHandle)DSNewHClr(sizeof(size_t)+lSize*sizeof(uChar[1]));
		} else {
			if((*(**ResultArray)->result[lIndex].TimeStampString)->cnt != (int32)lSize) {
				DSDisposeHandle((**ResultArray)->result[lIndex].TimeStampString);
				(**ResultArray)->result[lIndex].TimeStampString = (LStrHandle)DSNewHClr(sizeof(size_t)+lSize*sizeof(uChar[1]));
				if(DSCheckHandle((**ResultArray)->result[lIndex].TimeStampString) != noErr) {
					DbgTime();CaLabDbgPrintf("%s","Error while creating LabVIEW array (6)");
					*CommunicationStatus = 1;
					disposeResultArray(ResultArray);
					iGetCounter--;
					CaLabMutexUnlock(pvLock, &lockStatus2);
					CaLabMutexUnlock(getLock, &lockStatus1);
					if(bCaLabPolling) CaLabMutexUnlock(pollingLock, &lockStatus3);
					return;
				}
			}
		}
		memcpy_s((*(**ResultArray)->result[lIndex].TimeStampString)->str, lSize, pValue, (int32)lSize);
		(*(**ResultArray)->result[lIndex].TimeStampString)->cnt = (int32)lSize;
		// Write TimeStampNumber
		if(valueArraySize > 0)
			(**ResultArray)->result[lIndex].TimeStampNumber = pvList[(**PvIndexArray)->elt[lIndex]]->data.lTimeStamp;
		else
			(**ResultArray)->result[lIndex].TimeStampNumber = 0;
		if(fieldSize > 0) {
			// Create FieldNameArray or use previous one
			if((**ResultArray)->result[lIndex].FieldNameArray == 0x0) {
				(**ResultArray)->result[lIndex].FieldNameArray = (sStringArrayHdl)DSNewHClr(sizeof(size_t)+fieldSize*sizeof(LStrHandle[1]));
			} else {
				if((*(**ResultArray)->result[lIndex].FieldNameArray)->dimSize != (size_t)fieldSize) {
					// Remove values
					for(int32 iArrayIndex=((int32)(*(**ResultArray)->result[lIndex].FieldNameArray)->dimSize)-1; !bStopped && iArrayIndex>=0; iArrayIndex--) {
						if((*(**ResultArray)->result[lIndex].FieldNameArray)->elt[iArrayIndex] != 0x0) {
							(*(*(**ResultArray)->result[lIndex].FieldNameArray)->elt[iArrayIndex])->cnt = 0;
							iResult = DSDisposeHandle((*(**ResultArray)->result[lIndex].FieldNameArray)->elt[iArrayIndex]);
							if(iResult != noErr) {
								DbgTime();CaLabDbgPrintf("%s","Error while cleaning up LabVIEW array (7)");
								*CommunicationStatus = 1;
								disposeResultArray(ResultArray);
								iGetCounter--;
								CaLabMutexUnlock(pvLock, &lockStatus2);
								CaLabMutexUnlock(getLock, &lockStatus1);
								if(bCaLabPolling) CaLabMutexUnlock(pollingLock, &lockStatus3);
								return;
							}
						} else {
							DbgTime();CaLabDbgPrintf("%s","Error while cleaning up LabVIEW array (7a)");
							*CommunicationStatus = 1;
							disposeResultArray(ResultArray);
							iGetCounter--;
							CaLabMutexUnlock(pvLock, &lockStatus2);
							CaLabMutexUnlock(getLock, &lockStatus1);
							if(bCaLabPolling) CaLabMutexUnlock(pollingLock, &lockStatus3);
							return;
						}
					}
					if((*(**ResultArray)->result[lIndex].FieldNameArray)->dimSize != (size_t)fieldSize) {
						DSDisposeHandle((**ResultArray)->result[lIndex].FieldNameArray);
						(**ResultArray)->result[lIndex].FieldNameArray = (sStringArrayHdl)DSNewHClr(sizeof(size_t)+fieldSize*sizeof(LStrHandle[1]));
						if(DSCheckHandle((**ResultArray)->result[lIndex].FieldNameArray) != noErr) {
							DbgTime();CaLabDbgPrintf("%s","Error while creating LabVIEW array (7b)");
							*CommunicationStatus = 1;
							disposeResultArray(ResultArray);
							iGetCounter--;
							CaLabMutexUnlock(pvLock, &lockStatus2);
							CaLabMutexUnlock(getLock, &lockStatus1);
							if(bCaLabPolling) CaLabMutexUnlock(pollingLock, &lockStatus3);
							return;
						}
					}
				}
			}
			// If fields are available create field objects and write all of them
			for(uInt32 iArrayIndex=0; !bStopped && iArrayIndex<fieldSize; iArrayIndex++) {
				// Create new FieldNameArray or use previous one
				if(pvList[(**PvIndexArray)->elt[lIndex]]->data.szFieldNameArray == 0x0) {
					DbgTime();CaLabDbgPrintf("Error: %s has already been configured with different optional fields", pvList[(**PvIndexArray)->elt[lIndex]]->data.szVarName);
					continue;
				}
				pValue = pvList[(**PvIndexArray)->elt[lIndex]]->data.szFieldNameArray+iArrayIndex*MAX_STRING_SIZE;
				lSize = strlen(pValue);
				if(((*(**ResultArray)->result[lIndex].FieldNameArray)->elt[iArrayIndex]) == 0x0) {
					((*(**ResultArray)->result[lIndex].FieldNameArray)->elt[iArrayIndex]) = (LStrHandle) DSNewHClr(sizeof(size_t)+lSize*sizeof(uChar[1]));
				} else {
					if((*(*(**ResultArray)->result[lIndex].FieldNameArray)->elt[iArrayIndex])->cnt != (int32)lSize) {
						DSDisposeHandle((*(**ResultArray)->result[lIndex].FieldNameArray)->elt[iArrayIndex]);
						((*(**ResultArray)->result[lIndex].FieldNameArray)->elt[iArrayIndex]) = (LStrHandle) DSNewHClr(sizeof(size_t)+lSize*sizeof(uChar[1]));
						if(DSCheckHandle((*(**ResultArray)->result[lIndex].FieldNameArray)->elt[iArrayIndex]) != noErr) {
							DbgTime();CaLabDbgPrintf("%s","Error while creating LabVIEW array (7c)");
							*CommunicationStatus = 1;
							disposeResultArray(ResultArray);
							iGetCounter--;
							CaLabMutexUnlock(pvLock, &lockStatus2);
							CaLabMutexUnlock(getLock, &lockStatus1);
							if(bCaLabPolling) CaLabMutexUnlock(pollingLock, &lockStatus3);
							return;
						}
					}
				}
				// Write names to FieldNameArray
				if((*(**ResultArray)->result[lIndex].FieldNameArray)->elt[iArrayIndex] != 0x0) {
					memcpy_s((*(*(**ResultArray)->result[lIndex].FieldNameArray)->elt[iArrayIndex])->str, lSize, pValue, lSize);
					(*(*(**ResultArray)->result[lIndex].FieldNameArray)->elt[iArrayIndex])->cnt = (int32)lSize;
				} else {
					DbgTime();CaLabDbgPrintf("%s","Error while creating LabVIEW array (7d)");
					*CommunicationStatus = 1;
					disposeResultArray(ResultArray);
					iGetCounter--;
					CaLabMutexUnlock(pvLock, &lockStatus2);
					CaLabMutexUnlock(getLock, &lockStatus1);
					if(bCaLabPolling) CaLabMutexUnlock(pollingLock, &lockStatus3);
					return;
				}
			}
			if(!(pvList[(**PvIndexArray)->elt[lIndex]]->data.szFieldNameArray == 0x0))
				(*(**ResultArray)->result[lIndex].FieldNameArray)->dimSize = (size_t)fieldSize;
			// Create new FieldValueArray or use previous one
			if((**ResultArray)->result[lIndex].FieldValueArray == 0x0) {
				(**ResultArray)->result[lIndex].FieldValueArray = (sStringArrayHdl)DSNewHClr(sizeof(size_t)+fieldSize*sizeof(LStrHandle[1]));
			} else {
				if((*(**ResultArray)->result[lIndex].FieldValueArray)->dimSize != (size_t)fieldSize) {
					// Remove values
					for(int32 iArrayIndex=((int32)(*(**ResultArray)->result[lIndex].FieldValueArray)->dimSize)-1; !bStopped && iArrayIndex>=0; iArrayIndex--) {
						if((*(**ResultArray)->result[lIndex].FieldValueArray)->elt[iArrayIndex] != 0x0) {
							(*(*(**ResultArray)->result[lIndex].FieldValueArray)->elt[iArrayIndex])->cnt = 0;
							iResult = DSDisposeHandle((*(**ResultArray)->result[lIndex].FieldValueArray)->elt[iArrayIndex]);
							if(iResult != noErr) {
								DbgTime();CaLabDbgPrintf("%s","Error while cleaning up LabVIEW array (8)");
								*CommunicationStatus = 1;
								disposeResultArray(ResultArray);
								iGetCounter--;
								CaLabMutexUnlock(pvLock, &lockStatus2);
								CaLabMutexUnlock(getLock, &lockStatus1);
								if(bCaLabPolling) CaLabMutexUnlock(pollingLock, &lockStatus3);
								return;
							}
						} else {
							DbgTime();CaLabDbgPrintf("%s","Error while cleaning up LabVIEW array (8a)");
							*CommunicationStatus = 1;
							disposeResultArray(ResultArray);
							iGetCounter--;
							CaLabMutexUnlock(pvLock, &lockStatus2);
							CaLabMutexUnlock(getLock, &lockStatus1);
							if(bCaLabPolling) CaLabMutexUnlock(pollingLock, &lockStatus3);
							return;
						}
					}
					if((*(**ResultArray)->result[lIndex].FieldValueArray)->dimSize != (size_t)fieldSize) {
						DSDisposeHandle((**ResultArray)->result[lIndex].FieldValueArray);
						(**ResultArray)->result[lIndex].FieldValueArray = (sStringArrayHdl)DSNewHClr(sizeof(size_t)+fieldSize*sizeof(LStrHandle[1]));
						if(DSCheckHandle((**ResultArray)->result[lIndex].FieldValueArray) != noErr) {
							DbgTime();CaLabDbgPrintf("%s","Error while creating LabVIEW array (8b)");
							*CommunicationStatus = 1;
							disposeResultArray(ResultArray);
							iGetCounter--;
							CaLabMutexUnlock(pvLock, &lockStatus2);
							CaLabMutexUnlock(getLock, &lockStatus1);
							if(bCaLabPolling) CaLabMutexUnlock(pollingLock, &lockStatus3);
							return;
						}
					}
				}
			}
			// Write values to FieldValueArray
			for(uInt32 iArrayIndex=0; !bStopped && iArrayIndex<fieldSize; iArrayIndex++) {
				if(pvList[(**PvIndexArray)->elt[lIndex]]->data.szFieldValueArray == 0x0) {
					continue;
				}
				pValue = pvList[(**PvIndexArray)->elt[lIndex]]->data.szFieldValueArray+iArrayIndex*MAX_STRING_SIZE;
				lSize = strlen(pValue);
				if(((*(**ResultArray)->result[lIndex].FieldValueArray)->elt[iArrayIndex]) == 0x0) {
					((*(**ResultArray)->result[lIndex].FieldValueArray)->elt[iArrayIndex]) = (LStrHandle) DSNewHClr(sizeof(size_t)+lSize*sizeof(uChar[1]));
				} else {
					if((*(*(**ResultArray)->result[lIndex].FieldValueArray)->elt[iArrayIndex])->cnt != (int32)lSize) {
						DSDisposeHandle((*(**ResultArray)->result[lIndex].FieldValueArray)->elt[iArrayIndex]);
						((*(**ResultArray)->result[lIndex].FieldValueArray)->elt[iArrayIndex]) = (LStrHandle) DSNewHClr(sizeof(size_t)+lSize*sizeof(uChar[1]));
						if(DSCheckHandle((*(**ResultArray)->result[lIndex].FieldValueArray)->elt[iArrayIndex]) != noErr) {
							DbgTime();CaLabDbgPrintf("%s","Error while creating LabVIEW array (8c)");
							*CommunicationStatus = 1;
							disposeResultArray(ResultArray);
							iGetCounter--;
							CaLabMutexUnlock(pvLock, &lockStatus2);
							CaLabMutexUnlock(getLock, &lockStatus1);
							if(bCaLabPolling) CaLabMutexUnlock(pollingLock, &lockStatus3);
							return;
						}
					}
				}
				if((*(**ResultArray)->result[lIndex].FieldValueArray)->elt[iArrayIndex] != 0x0) {
					memcpy_s((*(*(**ResultArray)->result[lIndex].FieldValueArray)->elt[iArrayIndex])->str, lSize, pValue, lSize);
					(*(*(**ResultArray)->result[lIndex].FieldValueArray)->elt[iArrayIndex])->cnt = (int32)lSize;
				} else {
					DbgTime();CaLabDbgPrintf("%s","Error while creating LabVIEW array (8d)");
					*CommunicationStatus = 1;
					disposeResultArray(ResultArray);
					iGetCounter--;
					CaLabMutexUnlock(pvLock, &lockStatus2);
					CaLabMutexUnlock(getLock, &lockStatus1);
					if(bCaLabPolling) CaLabMutexUnlock(pollingLock, &lockStatus3);
					return;
				}
			}
			if(!(pvList[(**PvIndexArray)->elt[lIndex]]->data.szFieldValueArray == 0x0))
				(*(**ResultArray)->result[lIndex].FieldValueArray)->dimSize = (size_t)fieldSize;
		}
		// Create ErrorIO and fill in structure
		pValue = pvList[(**PvIndexArray)->elt[lIndex]]->data.szError;
		lSize = strlen(pValue);
		if(pvList[(**PvIndexArray)->elt[lIndex]]->data.lError == ECA_NORMAL) {
			(**ResultArray)->result[lIndex].ErrorIO.code = 0;
		} else {
			(**ResultArray)->result[lIndex].ErrorIO.code = ERROR_OFFSET+pvList[(**PvIndexArray)->elt[lIndex]]->data.lError;
			*CommunicationStatus = 1;
		}
		(**ResultArray)->result[lIndex].ErrorIO.status = 0;
		if((**ResultArray)->result[lIndex].ErrorIO.source == 0x0) {
			(**ResultArray)->result[lIndex].ErrorIO.source = (LStrHandle)DSNewHClr(sizeof(size_t)+lSize*sizeof(uChar[1]));
		} else {
			if((*(**ResultArray)->result[lIndex].ErrorIO.source)->cnt != (int32)lSize) {
				DSDisposeHandle((**ResultArray)->result[lIndex].ErrorIO.source);
				(**ResultArray)->result[lIndex].ErrorIO.source = (LStrHandle)DSNewHClr(sizeof(size_t)+lSize*sizeof(uChar[1]));
				if(DSCheckHandle((**ResultArray)->result[lIndex].ErrorIO.source) != noErr) {
					DbgTime();CaLabDbgPrintf("%s","Error while creating LabVIEW array (9)");
					*CommunicationStatus = 1;
					disposeResultArray(ResultArray);
					iGetCounter--;
					CaLabMutexUnlock(pvLock, &lockStatus2);
					CaLabMutexUnlock(getLock, &lockStatus1);
					if(bCaLabPolling) CaLabMutexUnlock(pollingLock, &lockStatus3);
					return;
				}
			}
		}
		memcpy_s((*(**ResultArray)->result[lIndex].ErrorIO.source)->str, lSize, pValue, lSize);
		(*(**ResultArray)->result[lIndex].ErrorIO.source)->cnt = (int32)lSize;
		// Create new ResultArray or use previous one
		(**ResultArray)->result[lIndex].Elements = valueArraySize;
		if(valueArraySize > 0) {
			if((**FirstStringValue)->elt[lIndex] == 0x0) {
				(**FirstStringValue)->elt[lIndex] = (LStrHandle) DSNewHClr(sizeof(size_t)+(*(*(**ResultArray)->result[lIndex].StringValueArray)->elt[0])->cnt*sizeof(uChar[1]));
				(*((**FirstStringValue)->elt[lIndex]))->cnt = (*(*(**ResultArray)->result[lIndex].StringValueArray)->elt[0])->cnt;
			}
			if((*((**FirstStringValue)->elt[lIndex]))->cnt != (*(*(**ResultArray)->result[lIndex].StringValueArray)->elt[0])->cnt) {
				DSDisposeHandle((**FirstStringValue)->elt[lIndex]);
				(**FirstStringValue)->elt[lIndex] = (LStrHandle) DSNewHClr(sizeof(size_t)+(*(*(**ResultArray)->result[lIndex].StringValueArray)->elt[0])->cnt*sizeof(uChar[1]));
				if(DSCheckHandle((**FirstStringValue)->elt[lIndex]) != noErr) {
					DbgTime();CaLabDbgPrintf("%s","Error while resize first string value");
					*CommunicationStatus = 1;
					disposeResultArray(ResultArray);
					iGetCounter--;
					CaLabMutexUnlock(pvLock, &lockStatus2);
					CaLabMutexUnlock(getLock, &lockStatus1);
					if(bCaLabPolling) CaLabMutexUnlock(pollingLock, &lockStatus3);
					return;
				}
				(*((**FirstStringValue)->elt[lIndex]))->cnt = (*(*(**ResultArray)->result[lIndex].StringValueArray)->elt[0])->cnt;
			}
			memcpy_s((*(**FirstStringValue)->elt[lIndex])->str, (*((**FirstStringValue)->elt[lIndex]))->cnt, (*(*(**ResultArray)->result[lIndex].StringValueArray)->elt[0])->str, (*((**FirstStringValue)->elt[lIndex]))->cnt);
			(**FirstDoubleValue)->elt[lIndex] = (*(**ResultArray)->result[lIndex].ValueNumberArray)->elt[0];
		}
	}
	(**ResultArray)->dimSize = lResultArraySize;
	if(valueArraySize <= 0)
		*CommunicationStatus = 1;
	if(bStopped) {
		disposeResultArray(ResultArray);
	}
	if(bCaLabPolling) {
		// Destroy PV list
		for(unsigned long lPvs=0; lPvs<lPVCounter; lPvs++) {
			// Unsubscribe value events
			if(pvList[lPvs]->sEvid[0]) {
				CaLabMutexUnlock(pvLock, &lockStatus2);
				ca_clear_subscription(pvList[lPvs]->sEvid[0]);
				CaLabMutexLock(pvLock, &lockStatus2, "getValue 2");
				pvList[lPvs]->sEvid[0] = 0;
			}
			// Unsubscribe enum events
			if(pvList[lPvs]->sEvid[1]) {
				CaLabMutexUnlock(pvLock, &lockStatus2);
				ca_clear_subscription(pvList[lPvs]->sEvid[1]);
				CaLabMutexLock(pvLock, &lockStatus2, "getValue 3");
				pvList[lPvs]->sEvid[1] = 0;
			}
			// Remove channel for values
			if(pvList[lPvs]->id[0]) {
				CaLabMutexUnlock(pvLock, &lockStatus2);
				ca_change_connection_event(pvList[lPvs]->id[0], 0x0);
				ca_clear_channel(pvList[lPvs]->id[0]);
				CaLabMutexLock(pvLock, &lockStatus2, "getValue 4");
				pvList[lPvs]->id[0] = 0;
			}
			// Remove channel for enums
			if(pvList[lPvs]->id[1]) {
				CaLabMutexUnlock(pvLock, &lockStatus2);
				ca_change_connection_event(pvList[lPvs]->id[1], 0x0);
				ca_clear_channel(pvList[lPvs]->id[1]);
				CaLabMutexLock(pvLock, &lockStatus2, "getValue 5");
				pvList[lPvs]->id[1] = 0;
			}
		}
		if(!pvList) {
			disposeResultArray(ResultArray);
			iGetCounter--;
			CaLabMutexUnlock(pvLock, &lockStatus2);
			CaLabMutexUnlock(getLock, &lockStatus1);
			if(bCaLabPolling) CaLabMutexUnlock(pollingLock, &lockStatus3);
			return;
		}
		for(unsigned long lPvs=0; lPvs<lPVCounter; lPvs++) {
			// Delete double value array
			if(pvList[lPvs]->data.dValueArray) {
				free(pvList[lPvs]->data.dValueArray);
				pvList[lPvs]->data.dValueArray = 0;
			}
			// Delete string value array
			if(pvList[lPvs]->data.szValueArray) {
				free(pvList[lPvs]->data.szValueArray);
				pvList[lPvs]->data.szValueArray = 0;
			}
			// Delete field name array
			if(pvList[lPvs]->data.szFieldNameArray) {
				free(pvList[lPvs]->data.szFieldNameArray);
				pvList[lPvs]->data.szFieldNameArray = 0;
			}
			// Delete field value array
			if(pvList[lPvs]->data.szFieldValueArray) {
				free(pvList[lPvs]->data.szFieldValueArray);
				pvList[lPvs]->data.szFieldValueArray = 0;
			}
			// Delete PV structure
			free(pvList[lPvs]);
			pvList[lPvs] = 0;
		}
		// Delete PV array
		free(pvList);
		pvList = 0;
		lPVCounter = 0;
	}

#ifdef STOPWATCH
	epicsTimeGetCurrent(&time2);
	diff = epicsTimeDiffInSeconds(&time2, &time1);
	if(diff > 1)
		CaLabDbgPrintf("getValue: %.0fms",1000*diff);
#endif

	iGetCounter--;
	CaLabMutexUnlock(pvLock, &lockStatus2);
	CaLabMutexUnlock(getLock, &lockStatus1);
	if(bCaLabPolling) CaLabMutexUnlock(pollingLock, &lockStatus3);
	return;
}

// Write EPICS PV
//	PvNameArray:			Array of PV names
//	PvIndexArray:			Handle of a array of indexes
//	StringValueArray2D:		2D-array of string values
//	DoubleValueArray2D:		2D-array of double values
//	LongValueArray2D:		2D-array of long values
//	dataType:				Type of EPICS channel
//	Timeout:				EPICS event timeout in seconds
//	Synchronous:			true = callback will be used (no interrupt of motor records)
//	ValuesSetInColumns:		true = read values set vertically in array
//	ErrorArray:				Array of resulting errors
//	Status:					0 = no problem; 1 = any problem occurred
extern "C" EXPORT void putValue(sStringArrayHdl *PvNameArray, sIntArrayHdl *PvIndexArray, sStringArray2DHdl *StringValueArray2D, sDoubleArray2DHdl *DoubleValueArray2D, sLongArray2DHdl *LongValueArray2D, uInt32 DataType, double Timeout, LVBoolean *Synchronous, LVBoolean *ValuesSetInColumns, sErrorArrayHdl *ErrorArray, LVBoolean *Status) {
	// Don't enter if library terminates
	if(bStopped && !putLock)
		return;
	/*
	dataTypes
	===========
	# => LabVIEW => C++ => EPICS
	0 => String => char[] => dbr_string_t
	1 => Single-precision, floating-point => float => dbr_float_t
	2 => Double-precision, floating-point => double => dbr_double_t
	3 => Byte signed integer => char => dbr_char_t
	4 => Word signed integer => short => dbr_short_t
	5 => Long signed integer => int => dbr_long_t
	6 => Quad signed integer => long => dbr_long_t
	*/
	bool					bResult;						// Simple logical variable
	long					ca_status;						// Current channel status
	chid					currentChid = 0;				// Current channel ID
	uInt32					iResult = 0;					// Result of channel access function
	uInt32					iCounter = 0;					// Simple counter
	uInt32					iItemCounter = 0;				// Current item in set
	size_t					iNameArraySize;					// Size of name array
	uInt32					iNameCounter = 0;				// Current name in name array
	uInt32					iNumberOfValueSets = 0;			// Number of value arrays
	size_t					iSize = 0;						// Size of error message
	uInt32					iValuesPerSetIn = 0;			// Size of incoming value array
	uInt32					iValuesPerSetMax = 0;			// Maximum size of value array
	uInt32					iValuesPerSetOut = 0;			// Size of outgoing value array
	uInt32					iValueArrayCounter = 0;			// Index for transfer object (array to write)
	uInt32					iSourceIndex;					// Indicator for value array position
	epicsMutexLockStatus	lockStatus1;					// Status marker for mutex "putLock"
	epicsMutexLockStatus	lockStatus2;					// Status marker for mutex "pvLock"
	epicsMutexLockStatus	lockStatus3;					// Status marker for mutex "pollingLock"
	size_t					mapSize = 0;					// Size of channel ID map
	chtype					nativeType = DBF_NO_ACCESS;		// Retrieved native data type of EPICS variable
	char					szName[MAX_STRING_SIZE];		// Current EPICS name
	std::queue<chid>		sPutQueue1;						// Queue #1 which is to be written
	std::queue<chid>		sPutQueue2;						// Queue #2 which is to be written
	char					szTmp[MAX_STRING_SIZE];			// Temporary string
	double					timeout;						// Time out counter
	void*					valueArray = 0;					// Current value array

#ifdef STOPWATCH
	epicsTimeStamp time1;
	epicsTimeStamp time2;
	epicsTimeStamp time3;
	epicsTimeGetCurrent(&time1);
	epicsTimeGetCurrent(&time2);
#endif
	if(bCaLabPolling) CaLabMutexLock(pollingLock, &lockStatus3, "polling mode", 2*Timeout);
	CaLabMutexLock(putLock, &lockStatus1, "putValue", 2*Timeout);
	iPutCounter++;
	iPutSyncCounter = 0;
	// Check valid time out
	if(Timeout <= 0)
		Timeout = 3;
	// Check handles and pointers
	if(*PvNameArray == 0x0 || ((uInt32)(**PvNameArray)->dimSize) <= 0) {
		*Status = 1;
		DbgTime();CaLabDbgPrintf("%s","Missing or corrupt pv name array in caLabPut!");
		iPutCounter--;
		CaLabMutexUnlock(putLock, &lockStatus1);
		if(bCaLabPolling) CaLabMutexUnlock(pollingLock, &lockStatus3);
		return;
	}
	if(*ErrorArray == 0x0 || ((uInt32)(**ErrorArray)->dimSize) <= 0 || (**ErrorArray)->dimSize != (**PvNameArray)->dimSize) {
		*Status = 1;
		DbgTime();CaLabDbgPrintf("%s","Missing or corrupt error array in caLabPut!");
		iPutCounter--;
		CaLabMutexUnlock(putLock, &lockStatus1);
		if(bCaLabPolling) CaLabMutexUnlock(pollingLock, &lockStatus3);
		return;
	}
	*Status = 0;
	if(bCaLabPolling) *Synchronous = false;
	// Get number of requested PVs
	iNameArraySize = (**PvNameArray)->dimSize;

	/*
	// Create index #### July 2014: array not needed anymore ####
	if(*PvIndexArray == 0x0) {
	*PvIndexArray = (sIntArrayHdl)DSNewHClr(sizeof(size_t)+iNameArraySize*sizeof(uInt32[1]));
	} else if((**PvIndexArray)->dimSize != iNameArraySize) {
	DSDisposeHandle(*PvIndexArray);
	*PvIndexArray = (sIntArrayHdl)DSNewHClr(sizeof(size_t)+iNameArraySize*sizeof(uInt32[1]));
	}
	(**PvIndexArray)->dimSize = iNameArraySize;
	*/
	ca_attach_context(pcac);
	// Initialize all PVs and copy PVs to sPutQueue1 and sPutQueue2
	for(unsigned long lIndex=0; lIndex<(unsigned long)iNameArraySize; lIndex++) {
		// Copy valid PV name
		memset(szName, 0, MAX_STRING_SIZE);
		if(!(**PvNameArray)->elt[lIndex] || (*(**PvNameArray)->elt[lIndex])->cnt == 0) {
			*Status = 1;
			DbgTime();CaLabDbgPrintf("%s","Empty PV name in caLabPut! Remove corrupt PV name and restart runtime, please.");
			iPutCounter--;
			ca_detach_context();
			CaLabMutexUnlock(putLock, &lockStatus1);
			if(bCaLabPolling) CaLabMutexUnlock(pollingLock, &lockStatus3);
			return;
		}
		if((*(**PvNameArray)->elt[lIndex])->cnt < MAX_STRING_SIZE)
			memcpy_s(szName, MAX_STRING_SIZE, (*(**PvNameArray)->elt[lIndex])->str, (*(**PvNameArray)->elt[lIndex])->cnt);
		else
			memcpy_s(szName, MAX_STRING_SIZE, (*(**PvNameArray)->elt[lIndex])->str, MAX_STRING_SIZE-1);
		// White spaces in PV names are not allowed
		if(strchr(szName, ' ')) {
			DbgTime();CaLabDbgPrintf("white space in PV name \"%s\" detected", szName);
			*(strchr(szName, ' ')) = 0;
		}
		if(strchr(szName, '\t')) {
			DbgTime();CaLabDbgPrintf("tabulator in PV name \"%s\" detected", szName);
			*(strchr(szName, '\t')) = 0;
		}
		// Search for CHID
		CaLabMutexLock(syncQueueLock, &lockStatus2, "putValue 1");
		mapSize = chidMap.size();
		chidMapIt = chidMap.find(szName);
		if(!(bResult = (chidMapIt == chidMap.end()))) {
			currentChid = chidMapIt->second;
		}
		CaLabMutexUnlock(syncQueueLock, &lockStatus2);
		if (bResult) {
			// If local copy of CHID not found than ...
			// ... create new PV name entry
			pPVNameList = (char**)realloc(pPVNameList, (mapSize+1)*sizeof(char*));
			pPVNameList[mapSize] = (char*)malloc(MAX_STRING_SIZE*sizeof(char));
			memcpy_s(pPVNameList[mapSize], MAX_STRING_SIZE, szName, MAX_STRING_SIZE);
			pChannels = (chid*)realloc(pChannels, (mapSize+1)*sizeof(chid));
			iResult = ca_create_channel(szName, 0, 0, 20, &pChannels[mapSize]);
			if(iResult == ECA_NORMAL) {
				//size_t tester = (size_t)ca_puser(pChannels[mapSize]);
				//CaLabDbgPrintf("ID = %d", tester);
				currentChid = pChannels[mapSize];
				// ... publish it to a map
				CaLabMutexLock(syncQueueLock, &lockStatus2, "putValue 2");
				chidMap.insert(std::pair<const char*, chid>(pPVNameList[mapSize], pChannels[mapSize]));
				CaLabMutexUnlock(syncQueueLock, &lockStatus2);
			} else {
				DbgTime();CaLabDbgPrintf("put Value: Create PV \"%s\" failed (1)", szName);
			}
		} else {
			if(bCaLabPolling) {
				DbgTime();CaLabDbgPrintf("put Value: Create PV \"%s\" failed (2)", szName);
			}
		}
		sPutQueue1.push(currentChid);
		sPutQueue2.push(currentChid);
	}
	CaLabMutexLock(syncQueueLock, &lockStatus2, "putValue 3");
	mapSize = chidMap.size();
	CaLabMutexUnlock(syncQueueLock, &lockStatus2);
	// Waiting for the end of connecting and initialising of new PVs (sPutQueue1)
	timeout = Timeout*1000;
	while(!bStopped && !sPutQueue1.empty() && timeout > 0) {
		currentChid = sPutQueue1.front();
		sPutQueue1.pop();
		if(ca_state(currentChid) == cs_conn) {
			continue;
		}
		timeout--;
		epicsThreadSleep(.001);
		sPutQueue1.push(currentChid);
	}

#ifdef STOPWATCH
	epicsTimeGetCurrent(&time3);
	double diff = epicsTimeDiffInSeconds(&time3, &time2);
	if(diff > 1)
		CaLabDbgPrintf("putValue: connecting and initialising\t%.3fms",1000*diff);
	epicsTimeGetCurrent(&time2);
#endif

	// Clean waiting queue
	while(!sPutQueue1.empty()) {

#ifdef STOPWATCH
		if(!sPutQueue1.empty())
			CaLabDbgPrintf("put \"%s\" timed out", ca_name(sPutQueue1.front()));
		else
			CaLabDbgPrintf("%s", "put timed out");
#endif

		//lIndex = (unsigned long int)ca_puser(sPutQueue1.front());
		//if(lIndex < mapSize)
		//	pChannelState[lIndex] = ECA_DISCONN;
		//DbgPrintf("PutValue timeout for: %s (%d)",ca_name(sPutQueue1.front()), ca_state(sPutQueue1.front()));
		sPutQueue1.pop();
	}
	// Write all values of all arrays
	iValueArrayCounter = 0; // index for transfer object (array to write)
	iItemCounter = 0;		// current item in set
	// Get dimensions of array
	switch(DataType) {
	case 0:
		if(((*(*(*StringValueArray2D))).dimSizes)[0] == 1 && (**PvNameArray)->dimSize > 1)
			*ValuesSetInColumns = 1;
		if(*ValuesSetInColumns) {
			iNumberOfValueSets  = ((**StringValueArray2D)->dimSizes)[1];
			iValuesPerSetIn = ((**StringValueArray2D)->dimSizes)[0];
		} else {
			iNumberOfValueSets  = ((**StringValueArray2D)->dimSizes)[0];
			iValuesPerSetIn = ((**StringValueArray2D)->dimSizes)[1];
		}
		break;
	case 1:
	case 2:
		if(((*(*(*DoubleValueArray2D))).dimSizes)[0] == 1 && (**PvNameArray)->dimSize > 1)
			*ValuesSetInColumns = 1;
		if(*ValuesSetInColumns) {
			iNumberOfValueSets  = ((**DoubleValueArray2D)->dimSizes)[1];
			iValuesPerSetIn = ((**DoubleValueArray2D)->dimSizes)[0];
		} else {
			iNumberOfValueSets  = ((**DoubleValueArray2D)->dimSizes)[0];
			iValuesPerSetIn = ((**DoubleValueArray2D)->dimSizes)[1];
		}
		break;
	case 3:
	case 4:
	case 5:
	case 6:
		if(((*(*(*LongValueArray2D))).dimSizes)[0] == 1 && (**PvNameArray)->dimSize > 1)
			*ValuesSetInColumns = 1;
		if(*ValuesSetInColumns) {
			iNumberOfValueSets  = ((**LongValueArray2D)->dimSizes)[1];
			iValuesPerSetIn = ((**LongValueArray2D)->dimSizes)[0];
		} else {
			iNumberOfValueSets  = ((**LongValueArray2D)->dimSizes)[0];
			iValuesPerSetIn = ((**LongValueArray2D)->dimSizes)[1];
		}
		break;
	default:
		*Status = 1;
		DbgTime();CaLabDbgPrintf("%s","Unknown data type in value array of caLabPut!");
		iPutCounter--;
		ca_detach_context();
		CaLabMutexUnlock(putLock, &lockStatus1);
		if(bCaLabPolling) CaLabMutexUnlock(pollingLock, &lockStatus3);
		return;
	}
	// build value array and put in into channel (sPutQueue2 -> sPutQueue1)
	while(!bStopped && iCounter < iValuesPerSetIn*iNumberOfValueSets && iNameCounter < iNameArraySize) {
		// new set of values? -> initialize it
		if(iItemCounter++ == 0) {
			if(!sPutQueue2.empty()) {
				currentChid = sPutQueue2.front();
				sPutQueue1.push(currentChid);
				sPutQueue2.pop();
				if(ca_state(currentChid) != cs_conn) {
					//DbgTime();CaLabDbgPrintf("disconnected PV %s\n",szName);
					iItemCounter = 0;
					iValueArrayCounter = 0;
					iNameCounter++;
					continue;
				}
				iValuesPerSetMax = ca_element_count(currentChid);
				nativeType = ca_field_type(currentChid);
				if(iValuesPerSetIn <= iValuesPerSetMax)
					iValuesPerSetOut = iValuesPerSetIn;
				else
					iValuesPerSetOut = iValuesPerSetMax;
				if(!iValuesPerSetOut) {
					*Status = 1;
					if(valueArray) {
						free(valueArray);
						valueArray = 0;
					}
					iPutCounter--;
					ca_detach_context();
					CaLabMutexUnlock(putLock, &lockStatus1);
					if(bCaLabPolling) CaLabMutexUnlock(pollingLock, &lockStatus3);
					return;
				}
			} else {
				iItemCounter = 0;
				iValueArrayCounter = 0;
				iNameCounter++;
				continue;
			}
			// Create new transfer object (valueArray)
			switch(DataType) {
			case 0:
				valueArray = (char*)realloc(valueArray, iValuesPerSetOut*MAX_STRING_SIZE*sizeof(char));
				memset(valueArray, 0, iValuesPerSetOut*MAX_STRING_SIZE*sizeof(char));
				break;
			case 1:
				valueArray = (float*)realloc(valueArray, iValuesPerSetOut*sizeof(float));
				memset(valueArray, 0, iValuesPerSetOut*sizeof(float));
				break;
			case 2:
				valueArray = (double*)realloc(valueArray, iValuesPerSetOut*sizeof(double));
				memset(valueArray, 0, iValuesPerSetOut*sizeof(double));
				break;
			case 3:
				valueArray = (char*)realloc(valueArray, iValuesPerSetOut*sizeof(char));
				memset(valueArray, 0, iValuesPerSetOut*sizeof(char));
				break;
			case 4:
				valueArray = (short*)realloc(valueArray, iValuesPerSetOut*sizeof(short));
				memset(valueArray, 0, iValuesPerSetOut*sizeof(short));
				break;
			case 5:
				valueArray = (uInt32*)realloc(valueArray, iValuesPerSetOut*sizeof(uInt32));
				memset(valueArray, 0, iValuesPerSetOut*sizeof(uInt32));
				break;
			case 6:
				valueArray = (long*)realloc(valueArray, iValuesPerSetOut*sizeof(long));
				memset(valueArray, 0, iValuesPerSetOut*sizeof(long));
				break;
			default:
				// Handled in previous switch-case statement
				break;
			}
		}
		// Write values in transfer object
		if(iValueArrayCounter < iValuesPerSetOut) {
			// Get right memory position for next value
			if(*ValuesSetInColumns)
				iSourceIndex = iNameCounter+iValueArrayCounter*iNumberOfValueSets;
			else
				iSourceIndex = iValueArrayCounter+iNameCounter*iValuesPerSetIn;
			switch(DataType) {
			case 0:
				if((((**StringValueArray2D)->elt)[iSourceIndex]) && DSCheckHandle((((*(*(*StringValueArray2D))).elt)[iSourceIndex])) == noErr) {
					memset(szTmp, 0x0, MAX_STRING_SIZE);
					if((**(((**StringValueArray2D)->elt)[iSourceIndex])).cnt < MAX_STRING_SIZE-1)
						memcpy_s(szTmp, MAX_STRING_SIZE, (**(((**StringValueArray2D)->elt)[iSourceIndex])).str, (*(*(((*(*(*StringValueArray2D))).elt)[iSourceIndex]))).cnt);
					else
						memcpy_s(szTmp, MAX_STRING_SIZE, (**(((**StringValueArray2D)->elt)[iSourceIndex])).str, MAX_STRING_SIZE-1);
					if(strstr(szTmp, ","))
						*(strstr(szTmp, ",")) = '.';
					switch(nativeType) {
					case DBF_STRING:
					case DBF_ENUM:
						memcpy_s((char*)valueArray+iValueArrayCounter*MAX_STRING_SIZE, MAX_STRING_SIZE, szTmp, (*(*(((*(*(*StringValueArray2D))).elt)[iSourceIndex]))).cnt);
						break;
					case DBF_FLOAT:
						epicsSnprintf((char*)valueArray+iValueArrayCounter*MAX_STRING_SIZE, MAX_STRING_SIZE, "%f", (float)strtod(szTmp, 0x0));
						break;
					case DBF_DOUBLE:
						epicsSnprintf((char*)valueArray+iValueArrayCounter*MAX_STRING_SIZE, MAX_STRING_SIZE, "%f", (double)strtod(szTmp, 0x0));
						break;
					case DBF_CHAR:
						epicsSnprintf((char*)valueArray+iValueArrayCounter*MAX_STRING_SIZE, MAX_STRING_SIZE, "%d", (char)strtol(szTmp, 0x0, 10));
						break;
					case DBF_SHORT:
						epicsSnprintf((char*)valueArray+iValueArrayCounter*MAX_STRING_SIZE, MAX_STRING_SIZE, "%d", (dbr_short_t)strtol(szTmp, 0x0, 10));
						break;
					case DBF_LONG:
						epicsSnprintf((char*)valueArray+iValueArrayCounter*MAX_STRING_SIZE, MAX_STRING_SIZE, "%ld", (long int)strtol(szTmp, 0x0, 10));
						break;
					default:
						break;
					}
				}
				break;
			case 1:
				((float*)valueArray)[iValueArrayCounter] = (float)((**DoubleValueArray2D)->elt)[iSourceIndex];
				break;
			case 2:
				((double*)valueArray)[iValueArrayCounter] = (double)((**DoubleValueArray2D)->elt)[iSourceIndex];
				break;
			case 3:
				((char*)valueArray)[iValueArrayCounter] = (char)((**LongValueArray2D)->elt)[iSourceIndex];
				break;
			case 4:
				((short*)valueArray)[iValueArrayCounter] = (short)((**LongValueArray2D)->elt)[iSourceIndex];
				break;
			case 5:
			case 6:
				((long*)valueArray)[iValueArrayCounter] = (long)((**LongValueArray2D)->elt)[iSourceIndex];
				break;
			default:
				// Handled in previous switch-case statement
				break;
			}
			iValueArrayCounter++;
			iCounter++;
		} else {
			iCounter++;
		}
		// Check current transfer object (array) has all data
		if(!bStopped && !(iCounter%iValuesPerSetIn)) {
			// Call EPICS put-function to write data via Channel Access if connection of current PV has been established
			if((iResult=ca_state(currentChid)) == cs_conn) {
				if(iValueArrayCounter > ca_element_count(currentChid))
					iValueArrayCounter = ca_element_count(currentChid);
				// Case of synchronious writing (waiting for feedback of written PV)
				if(*Synchronous) {
					iPutSyncCounter++;
					ca_set_puser(currentChid, (void*)ECA_DISCONN);
					switch(DataType) {
					case 0:
						iResult = ca_array_put_callback(DBR_STRING, iValueArrayCounter, currentChid, valueArray, putState, 0);
						break;
					case 1:
						iResult = ca_array_put_callback(DBR_FLOAT, iValueArrayCounter, currentChid, valueArray, putState, 0);
						break;
					case 2:
						iResult = ca_array_put_callback(DBR_DOUBLE, iValueArrayCounter, currentChid, valueArray, putState, 0);
						break;
					case 3:
						iResult = ca_array_put_callback(DBR_CHAR, iValueArrayCounter, currentChid, valueArray, putState, 0);
						break;
					case 4:
						iResult = ca_array_put_callback(DBR_SHORT, iValueArrayCounter, currentChid, valueArray, putState, 0);
						break;
					case 5:
						iResult = ca_array_put_callback(DBR_LONG, iValueArrayCounter, currentChid, valueArray, putState, 0);
						break;
					case 6:
						iResult = ca_array_put_callback(DBR_LONG, iValueArrayCounter, currentChid, valueArray, putState, 0);
						break;
					default:
						// Handled in previous switch-case statement
						break;
					}
				} else {
					// Case of asynchronious writing (do NOT waiting for feedback of written PV)
					switch(DataType) {
					case 0:
						iResult = ca_array_put(DBR_STRING, iValueArrayCounter, currentChid, valueArray);
						break;
					case 1:
						iResult = ca_array_put(DBR_FLOAT, iValueArrayCounter, currentChid, valueArray);
						break;
					case 2:
						iResult = ca_array_put(DBR_DOUBLE, iValueArrayCounter, currentChid, valueArray);
						break;
					case 3:
						iResult = ca_array_put(DBR_CHAR, iValueArrayCounter, currentChid, valueArray);
						break;
					case 4:
						iResult = ca_array_put(DBR_SHORT, iValueArrayCounter, currentChid, valueArray);
						break;
					case 5:
						iResult = ca_array_put(DBR_LONG, iValueArrayCounter, currentChid, valueArray);
						break;
					case 6:
						iResult = ca_array_put(DBR_LONG, iValueArrayCounter, currentChid, valueArray);
						break;
					default:
						// Handled in previous switch-case statement
						break;
					}

#ifdef STOPWATCH
					epicsTimeGetCurrent(&time3);
					double diff = epicsTimeDiffInSeconds(&time3, &time2);
					if(diff > 1)
						CaLabDbgPrintf("putValue %s:\t%.3fms",ca_name(currentChid),1000*diff);
					epicsTimeGetCurrent(&time2);
#endif

					ca_flush_io();
				}
				if(ca_state(currentChid) == cs_conn) {
					ca_set_puser(currentChid, (void*)iResult);
				}
			}
			iItemCounter = 0;
			iValueArrayCounter = 0;
			iNameCounter++;

		} // end of if(!bStopped && !(iCounter%iValuesPerSetIn))
		if(!iCounter)
			break;
	}

#ifdef STOPWATCH
	epicsTimeGetCurrent(&time2);
	diff = epicsTimeDiffInSeconds(&time2, &time1);
	if(diff > 1)
		CaLabDbgPrintf("putValue all PVs:\t%.3fms",1000*diff);
	epicsTimeGetCurrent(&time2);
#endif

	// Release transfer object
	if(valueArray) {
		free(valueArray);
		valueArray = 0;
	}
	// In case of synchronous writing wait for feedback now
	if(!bStopped && *Synchronous) {
		timeout = Timeout*1000;
		while(!bStopped && iPutSyncCounter > 0 && timeout > 0) {
			epicsThreadSleep(.001);
			timeout--;
		}
	}
	for(unsigned long lIndex=0; lIndex<(unsigned long)iNameArraySize; lIndex++) {
		ca_status = ECA_DISCONN;
		if(!sPutQueue1.empty()) {
			currentChid = sPutQueue1.front();
			sPutQueue1.pop();
			if(ca_state(currentChid) == cs_conn) {
				ca_status = (int)ca_puser(currentChid);
			}
		}
		iSize = strlen(ca_message(ca_status));
		if(DSCheckHandle(((**ErrorArray)->result)[lIndex].source) != noErr) {
			((**ErrorArray)->result)[lIndex].source = (LStrHandle) DSNewHClr(sizeof(size_t)+iSize*sizeof(uChar));
		} else {
			if((*((**ErrorArray)->result)[lIndex].source)->cnt != (int32)iSize) {
				DSDisposeHandle(((**ErrorArray)->result)[lIndex].source);
				((**ErrorArray)->result)[lIndex].source = (LStrHandle) DSNewHClr(sizeof(size_t)+iSize*sizeof(uChar));
				if(DSCheckHandle(((**ErrorArray)->result)[lIndex].source) != noErr) {
					DbgTime();CaLabDbgPrintf("Serious problem with LabVIEW while writing error message for index %d!", lIndex);
					continue;
				}
			}
		}
		if(ca_status == ECA_NORMAL) {
			((**ErrorArray)->result)[lIndex].code = 0;
		} else {
			((**ErrorArray)->result)[lIndex].code = ERROR_OFFSET+ca_status;
			*Status = 1;
			//DbgPrintf("putValue error =%d",ca_status);
		}
		memcpy_s((*(*((((**ErrorArray)->result)[lIndex]).source))).str, iSize, ca_message(ca_status), iSize);
		(*(*((((**ErrorArray)->result)[lIndex]).source))).cnt = (int32)iSize;
	}

#ifdef STOPWATCH
	epicsTimeGetCurrent(&time3);
	diff = epicsTimeDiffInSeconds(&time3, &time2);
	if(diff > 1)
		CaLabDbgPrintf("putValue: get results took\t%.3fms",1000*diff);
	epicsTimeGetCurrent(&time2);
#endif

	if(bCaLabPolling) {
		CaLabMutexLock(syncQueueLock, &lockStatus2, "putValue 5");
		if(pPVNameList) {
			// dispose pPVNameList
			for(size_t i=0; i<chidMap.size(); i++) {
				if(pChannels[i]) {
					ca_clear_channel(pChannels[i]);
				}
				free(pPVNameList[i]);
				pPVNameList[i] = 0;
			}
			free(pPVNameList);
			pPVNameList = NULL;
		}
		if(pChannels) {
			free(pChannels);
			pChannels = NULL;
		}
		chidMap.clear();
		CaLabMutexUnlock(syncQueueLock, &lockStatus2);
	}

#ifdef STOPWATCH
	epicsTimeGetCurrent(&time3);
	diff = epicsTimeDiffInSeconds(&time3, &time2);
	if(diff > 1)
		CaLabDbgPrintf("putValue:\t%.3fms",1000*diff);

	epicsTimeGetCurrent(&time2);
	diff = epicsTimeDiffInSeconds(&time2, &time1);
	if(diff > 1)
		CaLabDbgPrintf("Full put:\t%.3fms",1000*diff);
#endif

	iPutCounter--;
	ca_detach_context();
	CaLabMutexUnlock(putLock, &lockStatus1);
	if(bCaLabPolling) CaLabMutexUnlock(pollingLock, &lockStatus3);
}

// Post PV values to LabVIEW
//    Pv:   item to post
static void postEvent(PV* pv) {
	// Don't enter if library terminates
	if(bStopped)
		return;

	size_t lSize = 0;	// Size of a string

	// Return if no values available
	if(!pv->data.lElems || pv->pCaLabCluster == 0x0 || DSCheckPtr(pv->pCaLabCluster) != noErr || DSCheckHandle(pv->pCaLabCluster->PVName) != noErr)
		return;
	if(pv->pCaLabCluster->StringValueArray == 0x0 || pv->pCaLabCluster->ValueNumberArray == 0) {
		pv->pCaLabCluster->StringValueArray = (sStringArrayHdl)DSNewHClr(sizeof(size_t)+pv->data.lElems*sizeof(LStrHandle[1]));
		if(DSCheckHandle(pv->pCaLabCluster->StringValueArray) != noErr) {
			DbgTime();CaLabDbgPrintf("%s","Error while creating Event cluster (1)");
			return;
		}
		pv->pCaLabCluster->ValueNumberArray = (sDoubleArrayHdl)DSNewHClr(sizeof(size_t)+pv->data.lElems*sizeof(double[1]));
		if(DSCheckHandle(pv->pCaLabCluster->ValueNumberArray) != noErr) {
			DbgTime();CaLabDbgPrintf("%s","Error while creating Event cluster (2)");
			return;
		}
		for(long int lArrayIndex=0; !bStopped && lArrayIndex<(long int)pv->data.lElems; lArrayIndex++) {
			(*pv->pCaLabCluster->StringValueArray)->elt[lArrayIndex] = (LStrHandle) DSNewHClr(sizeof(size_t)+1*sizeof(uChar[1]));
			if(DSCheckHandle((*pv->pCaLabCluster->StringValueArray)->elt[lArrayIndex]) != noErr) {
				DbgTime();CaLabDbgPrintf("%s","Error while creating Event cluster (3)");
				return;
			}
			(*((*pv->pCaLabCluster->StringValueArray)->elt[lArrayIndex]))->str[0] = 0x0;
			(*((*pv->pCaLabCluster->StringValueArray)->elt[lArrayIndex]))->cnt = 1;
			(*pv->pCaLabCluster->ValueNumberArray)->elt[lArrayIndex] = 0;
		}
		if(bStopped)
			return;
		(*pv->pCaLabCluster->StringValueArray)->dimSize = (size_t)pv->data.lElems;
		(*pv->pCaLabCluster->ValueNumberArray)->dimSize = (size_t)pv->data.lElems;
		if(DSCheckHandle(pv->pCaLabCluster->FieldNameArray) != noErr || DSCheckHandle(pv->pCaLabCluster->FieldValueArray) != noErr) {
			DbgTime();CaLabDbgPrintf("%s","Bad field name/value container (3a)");
			return;
		}
		for(long int lArrayIndex=0; !bStopped && lArrayIndex < (long int)pv->data.iFieldCount; lArrayIndex++) {
			lSize = strlen(pv->data.szFieldNameArray+lArrayIndex*MAX_STRING_SIZE);
			if(DSCheckHandle((*pv->pCaLabCluster->FieldNameArray)->elt[lArrayIndex]) != noErr) {
				DbgTime();CaLabDbgPrintf("%s","Bad field name container (4)");
				return;
			}
			if(DSCheckHandle((*pv->pCaLabCluster->FieldValueArray)->elt[lArrayIndex]) != noErr) {
				(*pv->pCaLabCluster->FieldValueArray)->elt[lArrayIndex] = (LStrHandle) DSNewHClr(sizeof(size_t)+1*sizeof(uChar[1]));
				if(DSCheckHandle((*pv->pCaLabCluster->FieldValueArray)->elt[lArrayIndex]) != noErr) {
					DbgTime();CaLabDbgPrintf("%s","Error while creating Event cluster (5)");
					return;
				}
			}
			(*((*pv->pCaLabCluster->FieldValueArray)->elt[lArrayIndex]))->str[0] = 0x0;
			(*((*pv->pCaLabCluster->FieldValueArray)->elt[lArrayIndex]))->cnt = 1;
		}
		if(bStopped)
			return;
		if(pv->data.iFieldCount > 0) {
			(*pv->pCaLabCluster->FieldValueArray)->dimSize = (size_t)pv->data.iFieldCount;
			(*pv->pCaLabCluster->FieldNameArray)->dimSize = (size_t)pv->data.iFieldCount;
		}
	}
	// Write value count
	pv->pCaLabCluster->Elements = pv->data.lElems;

	// Resize array
	if(!pv->pCaLabCluster->StringValueArray) {
		return;
	}
	if((*pv->pCaLabCluster->StringValueArray)->dimSize != (size_t)pv->data.lElems) {
		DSDisposeHandle(pv->pCaLabCluster->StringValueArray);
		pv->pCaLabCluster->StringValueArray = (sStringArrayHdl)DSNewHClr(sizeof(size_t)+pv->data.lElems*sizeof(LStrHandle[1]));
		if(DSCheckHandle(pv->pCaLabCluster->StringValueArray) != noErr) {
			//if(NumericArrayResize(uPtr, 1, (UHandle*)&pv->pCaLabCluster->StringValueArray, pv->data.lElems) != noErr) {
			DbgTime();CaLabDbgPrintf("%s","Serious problem with LabVIEW while resizing string value array!");
			(*pv->pCaLabCluster->StringValueArray)->dimSize = pv->data.lElems;
		}
	}
	if((*pv->pCaLabCluster->ValueNumberArray)->dimSize != (size_t)pv->data.lElems) {
		DSDisposeHandle(pv->pCaLabCluster->ValueNumberArray);
		pv->pCaLabCluster->ValueNumberArray = (sDoubleArrayHdl)DSNewHClr(sizeof(size_t)+pv->data.lElems*sizeof(double[1]));
		if(DSCheckHandle(pv->pCaLabCluster->ValueNumberArray) != noErr) {
			//if(NumericArrayResize(fD, 1, (UHandle*)&pv->pCaLabCluster->ValueNumberArray, pv->data.lElems) != noErr) {
			DbgTime();CaLabDbgPrintf("%s","Serious problem with LabVIEW while resizing value number array!");
			return;
		}
		(*pv->pCaLabCluster->ValueNumberArray)->dimSize = pv->data.lElems;
	}
	// Write cached values of PV to LabVIEW cluster
	for(uInt32 lElement=0; lElement<((uInt32)(*pv->pCaLabCluster->StringValueArray)->dimSize)/*lStringArraySize*/; lElement++) {
		// <-- Begin of fix by Phillip J. Wyss (Purdue University)
		// Solves problems with subscriptions to multi-element DBR_TIME_CHAR PV�s when requesting an event be posted
		(*pv->pCaLabCluster->ValueNumberArray)->elt[lElement] = pv->data.dValueArray[lElement];
		/*if((DSCheckHandle((*pv->pCaLabCluster->ValueStringArray)->elt[lElement])!=noErr)) {
		continue;
		}*/
		lSize = strlen(pv->data.szValueArray+lElement*MAX_STRING_SIZE);
		if( !lSize && (*pv->pCaLabCluster->StringValueArray)->elt[lElement] == NULL )
			continue; // If new and old are both NULL, leave well enough alone
		if( (*pv->pCaLabCluster->StringValueArray)->elt[lElement] == NULL && pv->pCaLabCluster->PVName != NULL) {
			if(DSCopyHandle(&(*pv->pCaLabCluster->StringValueArray)->elt[lElement],pv->pCaLabCluster->PVName) != noErr)
				continue; // Allocate new handle by copying PVname handle
		}
		if((DSCheckHandle((*pv->pCaLabCluster->StringValueArray)->elt[lElement]) != noErr)) {
			continue;
		}
		// End of fix -->

		if((*(*pv->pCaLabCluster->StringValueArray)->elt[lElement])->cnt != (int32)lSize) {
			DSDisposeHandle((*pv->pCaLabCluster->StringValueArray)->elt[lElement]);
			(*pv->pCaLabCluster->StringValueArray)->elt[lElement] = (LStrHandle) DSNewHClr(sizeof(size_t)+lSize*sizeof(uChar[1]));
			if((DSCheckHandle((*pv->pCaLabCluster->StringValueArray)->elt[lElement]) != noErr)) {
				DbgTime();CaLabDbgPrintf("%s","Serious problem with LabVIEW while resizing string value array!");
				continue;
			}
		}
		/*
		if(NumericArrayResize(uB, 1L, (UHandle*)&(*pv->pCaLabCluster->StringValueArray)->elt[lElement], (size_t)lSize) != noErr) {
		DbgTime();CaLabDbgPrintf("%s","Serious problem with LabVIEW while resizing string value array!");
		continue;
		}*/
		memcpy_s((*(*pv->pCaLabCluster->StringValueArray)->elt[lElement])->str, lSize, pv->data.szValueArray+lElement*MAX_STRING_SIZE, (size_t)lSize);
		(*(*pv->pCaLabCluster->StringValueArray)->elt[lElement])->cnt = (int32)lSize;
	}
	// Retrieve optional field values
	for(uInt32 lElement=0; lElement<((uInt32)pv->data.iFieldCount) && pv->pCaLabCluster->FieldValueArray && *pv->pCaLabCluster->FieldValueArray; lElement++) {
		if((*pv->pCaLabCluster->FieldValueArray)->elt[lElement] && (DSCheckHandle((*pv->pCaLabCluster->FieldValueArray)->elt[lElement])==noErr)) {
			lSize = strlen(pv->data.szFieldValueArray+lElement*MAX_STRING_SIZE);
			if((*(*pv->pCaLabCluster->FieldValueArray)->elt[lElement])->cnt != (int32)lSize) {
				DSDisposeHandle((*pv->pCaLabCluster->FieldValueArray)->elt[lElement]);
				(*pv->pCaLabCluster->FieldValueArray)->elt[lElement] = (LStrHandle) DSNewHClr(sizeof(size_t)+lSize*sizeof(uChar[1]));
				if((DSCheckHandle((*pv->pCaLabCluster->FieldValueArray)->elt[lElement]) != noErr)) {
					DbgTime();CaLabDbgPrintf("%s","Serious problem with LabVIEW while resizing field value array!");
					continue;
				}
			}
			/*
			if(NumericArrayResize(uB, 1L, (UHandle*)&(*pv->pCaLabCluster->FieldValueArray)->elt[lElement], (size_t)lSize) != noErr) {
			DbgTime();CaLabDbgPrintf("%s","Serious problem with LabVIEW while resizing field value array!");
			continue;
			}*/
			memcpy_s((*(*pv->pCaLabCluster->FieldValueArray)->elt[lElement])->str, lSize, pv->data.szFieldValueArray+lElement*MAX_STRING_SIZE, lSize);
			(*(*pv->pCaLabCluster->FieldValueArray)->elt[lElement])->cnt = (int32)lSize;
		}
	}
	// Retrieve severity, status and timestamp
	lSize = strlen(pv->data.szSeverity);
	if((*pv->pCaLabCluster->SeverityString)->cnt != (int32)lSize) {
		DSDisposeHandle(pv->pCaLabCluster->SeverityString);
		pv->pCaLabCluster->SeverityString = (LStrHandle)DSNewHClr(sizeof(size_t)+lSize*sizeof(uChar[1]));
		if(DSCheckHandle(pv->pCaLabCluster->SeverityString) != noErr) {
			//if(NumericArrayResize(uB, 1L, (UHandle*)&pv->pCaLabCluster->SeverityString, (size_t)lSize) != noErr) {
			DbgTime();CaLabDbgPrintf("%s","Serious problem with LabVIEW while resizing severity string!");
		} else {
			memcpy_s((*pv->pCaLabCluster->SeverityString)->str, lSize, pv->data.szSeverity, lSize);
			(*pv->pCaLabCluster->SeverityString)->cnt = (int32)lSize;
		}
	} else {
		memcpy_s((*pv->pCaLabCluster->SeverityString)->str, lSize, pv->data.szSeverity, lSize);
	}
	pv->pCaLabCluster->SeverityNumber = pv->data.nSeverity;
	lSize = strlen(pv->data.szStatus);
	if((*pv->pCaLabCluster->StatusString)->cnt != (int32)lSize) {
		DSDisposeHandle(pv->pCaLabCluster->StatusString);
		pv->pCaLabCluster->StatusString = (LStrHandle)DSNewHClr(sizeof(size_t)+lSize*sizeof(uChar[1]));
		if(DSCheckHandle(pv->pCaLabCluster->StatusString) != noErr) {
			//if(NumericArrayResize(uB, 1L, (UHandle*)&pv->pCaLabCluster->StatusString, (size_t)lSize) != noErr) {
			DbgTime();CaLabDbgPrintf("%s","Serious problem with LabVIEW while resizing status string!");
		} else {
			memcpy_s((*pv->pCaLabCluster->StatusString)->str, lSize, pv->data.szStatus, lSize);
			(*pv->pCaLabCluster->StatusString)->cnt = (int32)lSize;
		}
	} else {
		memcpy_s((*pv->pCaLabCluster->StatusString)->str, lSize, pv->data.szStatus, lSize);
	}
	pv->pCaLabCluster->StatusNumber = pv->data.nStatus;
	lSize = strlen(pv->data.szTimeStamp);
	if((*pv->pCaLabCluster->TimeStampString)->cnt != (int32)lSize) {
		DSDisposeHandle(pv->pCaLabCluster->TimeStampString);
		pv->pCaLabCluster->TimeStampString = (LStrHandle)DSNewHClr(sizeof(size_t)+lSize*sizeof(uChar[1]));
		if(DSCheckHandle(pv->pCaLabCluster->TimeStampString) != noErr) {
			//if(NumericArrayResize(uB, 1L, (UHandle*)&pv->pCaLabCluster->TimeStampString, (size_t)lSize) != noErr) {
			DbgTime();CaLabDbgPrintf("%s","Serious problem with LabVIEW while resizing time stamp string!");
		} else {
			memcpy_s((*pv->pCaLabCluster->TimeStampString)->str, lSize, pv->data.szTimeStamp, lSize);
			(*pv->pCaLabCluster->TimeStampString)->cnt = (int32)lSize;
		}
	} else {
		memcpy_s((*pv->pCaLabCluster->TimeStampString)->str, lSize, pv->data.szTimeStamp, lSize);
	}
	pv->pCaLabCluster->TimeStampNumber = pv->data.lTimeStamp;
	// Write error structure
	if(pv->data.lError == ECA_NORMAL)
		pv->pCaLabCluster->ErrorIO.code = 0;
	else
		pv->pCaLabCluster->ErrorIO.code = ERROR_OFFSET+pv->data.lError;
	lSize = strlen(pv->data.szError);
	if((*pv->pCaLabCluster->ErrorIO.source)->cnt != (int32)lSize) {
		DSDisposeHandle(pv->pCaLabCluster->ErrorIO.source);
		pv->pCaLabCluster->ErrorIO.source = (LStrHandle)DSNewHClr(sizeof(size_t)+lSize*sizeof(uChar[1]));
		if(DSCheckHandle(pv->pCaLabCluster->ErrorIO.source) != noErr) {
			//if(NumericArrayResize(uB, 1L, (UHandle*)&pv->pCaLabCluster->ErrorIO.source, (size_t)lSize) != noErr) {
			DbgTime();CaLabDbgPrintf("%s","Serious problem with LabVIEW while resizing Error IO source string!");
		} else {
			memcpy_s((*pv->pCaLabCluster->ErrorIO.source)->str, lSize, pv->data.szError, lSize);
			(*pv->pCaLabCluster->ErrorIO.source)->cnt = (int32)lSize;
		}
	} else {
		memcpy_s((*pv->pCaLabCluster->ErrorIO.source)->str, lSize, pv->data.szError, lSize);
	}
	if((pv->pCaLabCluster->ErrorIO.code-ERROR_OFFSET) & CA_K_ERROR) {
		pv->pCaLabCluster->ErrorIO.status = 1;
	} else {
		pv->pCaLabCluster->ErrorIO.status = 0;
	}
	pv->pCaLabCluster->Elements = pv->data.lElems;
	if(pv->lRefNum) {
		// Post it!
		if(PostLVUserEvent(pv->lRefNum, pv->pCaLabCluster) != mgNoErr)
			pv->lRefNum = 0;
	}
}

// Creates new LabView user event
//    RefNum:            reference number of event
//    ResultArrayHdl:    target item
extern "C" EXPORT void addEvent(LVUserEventRef *RefNum, sResult *ResultArrayHdl) {
	// Don't enter if library terminates
	if(bStopped)
		return;

	epicsMutexLockStatus	lockStatus;					// Status marker for mutex
	char					szName[MAX_STRING_SIZE];	// Name of current PV

	// copy PV name to szName
	memset(szName, 0, MAX_STRING_SIZE);
	if(DSCheckPtr(ResultArrayHdl) != noErr || DSCheckHandle(ResultArrayHdl->PVName) != noErr) {
		return;
	}
	CaLabMutexLock(pvLock, &lockStatus, "addEvent");
	memcpy_s(szName, MAX_STRING_SIZE, (*ResultArrayHdl->PVName)->str, (*ResultArrayHdl->PVName)->cnt);
	// associate ResultArray with pvList member and trigger post
	for(unsigned long lFind=0; lFind<lPVCounter; lFind++) {
		if(!strcmp(szName, pvList[lFind]->data.szVarName)) {
			pvList[lFind]->lRefNum = *RefNum;
			pvList[lFind]->pCaLabCluster = ResultArrayHdl;
			postEvent(pvList[lFind]);
			break;
		}
	}
	CaLabMutexUnlock(pvLock, &lockStatus);
}

// get context info for EPICS
//   InfoStringArray2D:     Container for results
//   InfoStringArraySize:   Elements in result container
//   FirstCall:             Indicator for first call
extern "C" EXPORT void info(sStringArray2DHdl *InfoStringArray2D, sResultArrayHdl *ResultArray, LVBoolean *FirstCall) {
	// Don't enter if library terminates
	if(bStopped)
		return;

	uInt32					arraySize = 0;					// Index and size of result array
	uInt32					count = 0;						// Number of environment variables
	double					dValue = 0;						// Double value of EPICS variable
	uInt32					infoArrayDimensions = 2;		// Currently we are using two array as result
	int32					iResult = 0;					// Result of LabVIEW functions
	unsigned long			lIndex = 0;						// Current index of local PV list
	epicsMutexLockStatus	lockStatus1;					// Status marker for mutex
	epicsMutexLockStatus	lockStatus2;					// Status marker for mutex
	uInt32					lStringArraySets = 0;			// Number of result arrays
	unsigned long			lResultArraySize = 0;			// Size of result array
	size_t					lSize = 0;						// Size of a string
	const ENV_PARAM**		ppParam = env_param_list;		// Environment variables of EPICS context
	char**					pszNames = 0;					// Name array
	char**					pszValues = 0;					// Value array
	const char*				pVal = 0;						// Pointer to environment variables of EPICS context
	uInt32					valueArraySize = 0;				// Size of value array

	while (*ppParam != NULL) {
		lStringArraySets++;
		ppParam++;
	}
	lStringArraySets += 2;
	CaLabMutexLock(putLock, &lockStatus1, "info");
	iGetCounter++;
	if(!pvList) {
		iGetCounter--;
		CaLabMutexUnlock(putLock, &lockStatus1);
		return;
	}
	pszNames = (char**)realloc(pszNames, lStringArraySets * sizeof(char*));
	for(uInt32 i=0; i<lStringArraySets; i++) {
		pszNames[i] = 0;
		pszNames[i] = (char*)realloc(pszNames[i], 255 * sizeof(char));
		memset(pszNames[i], 0, 255);
	}
	pszValues = (char**)realloc(pszValues, lStringArraySets * sizeof(char*));
	for(uInt32 i=0; i<lStringArraySets; i++) {
		pszValues[i] = 0;
		pszValues[i] = (char*)realloc(pszValues[i], 255 * sizeof(char));
		memset(pszValues[i], 0, 255);
	}
	ppParam = env_param_list;
	count = 0;
	while (*ppParam != NULL) {
		pVal = envGetConfigParamPtr(*ppParam);
		memcpy_s(pszNames[count], 255, (*ppParam)->name, strlen((*ppParam)->name));
		if(pVal)
			memcpy_s(pszValues[count], 255, pVal, strlen(pVal));
		else
			memcpy_s(pszValues[count], 255, "undefined", strlen("undefined"));
		count++;
		ppParam++;
	}
	memcpy_s(pszNames[count], 255, "CALAB_POLLING", strlen("CALAB_POLLING"));
	if(getenv ("CALAB_POLLING"))
		memcpy_s(pszValues[count], 255, getenv ("CALAB_POLLING"), strlen(getenv ("CALAB_POLLING")));
	else
		memcpy_s(pszValues[count], 255, "undefined", strlen("undefined"));
	count++;
	memcpy_s(pszNames[count], 255, "CALAB_NODBG", strlen("CALAB_NODBG"));
	if(getenv ("CALAB_NODBG"))
		memcpy_s(pszValues[count], 255, getenv ("CALAB_NODBG"), strlen(getenv ("CALAB_NODBG")));
	else
		memcpy_s(pszValues[count], 255, "undefined", strlen("undefined"));
	count++;
	if(*FirstCall) {
		if(DSCheckHandle(*InfoStringArray2D) == noErr) {
			DSDisposeHandle(*InfoStringArray2D);
			*InfoStringArray2D = 0x0;
		}
		if(DSCheckHandle(*ResultArray) == noErr) {
			disposeResultArray(ResultArray);
		}
	}
	// Create InfoStringArray2D or use previous one
	if(infoArrayDimensions > 0) {
		if(*InfoStringArray2D == 0x0) {
			*InfoStringArray2D = (sStringArray2DHdl)DSNewHClr(sizeof(uInt32[2])+(infoArrayDimensions*lStringArraySets*sizeof(LStrHandle[1])));
			(**InfoStringArray2D)->dimSizes[0] = lStringArraySets;
			(**InfoStringArray2D)->dimSizes[1] = infoArrayDimensions;
			uInt32 iNameCounter=0;
			uInt32 iValueCounter=0;
			size_t lSize = 0;
			for(uInt32 i=0; i<(infoArrayDimensions*lStringArraySets); i++) {
				if(i%2) {
					lSize = strlen(pszValues[iValueCounter]);
					(**InfoStringArray2D)->elt[i] = (LStrHandle)DSNewHClr(sizeof(size_t)+lSize*sizeof(uChar[1]));
					memcpy_s((*(**InfoStringArray2D)->elt[i])->str, lSize, pszValues[iValueCounter], lSize);
					(*(**InfoStringArray2D)->elt[i])->cnt = (int32)lSize;
					iValueCounter++;
				} else {
					lSize = strlen(pszNames[iNameCounter]);
					(**InfoStringArray2D)->elt[i] = (LStrHandle)DSNewHClr(sizeof(size_t)+lSize*sizeof(uChar[1]));
					memcpy_s((*(**InfoStringArray2D)->elt[i])->str, lSize, pszNames[iNameCounter], lSize);
					(*(**InfoStringArray2D)->elt[i])->cnt = (int32)lSize;
					iNameCounter++;
				}
			}
		}
	}
	for(uInt32 i=0; i<lStringArraySets; i++) {
		free(pszNames[i]);
		pszNames[i] = 0;
	}
	free(pszNames);
	pszNames = 0;
	for(uInt32 i=0; i<lStringArraySets; i++) {
		free(pszValues[i]);
		pszValues[i] = 0;
	}
	free(pszValues);
	pszValues = 0;
	lResultArraySize = lPVCounter;
	// Create result array (cluster) or use previous one
	if(*FirstCall && DSCheckHandle(*ResultArray) != noErr) {
		*ResultArray = (sResultArrayHdl)DSNewHClr(sizeof(size_t)+lResultArraySize*sizeof(sResult[1]));
	} else if(*ResultArray == 0x0) {
		*ResultArray = (sResultArrayHdl)DSNewHClr(sizeof(size_t)+lResultArraySize*sizeof(sResult[1]));
	} else if((**ResultArray)->dimSize != lResultArraySize) {
		disposeResultArray(ResultArray);
		*ResultArray = (sResultArrayHdl)DSNewHClr(sizeof(size_t)+lResultArraySize*sizeof(sResult[1]));
	}
	CaLabMutexLock(pvLock, &lockStatus2, "info 1");
	// Build result array (write content)
	for(lIndex = 0; !bStopped && pvList && lIndex<lResultArraySize; lIndex++) {
		if(strchr(pvList[lIndex]->data.szVarName, '.') || strstr(pvList[lIndex]->data.szVarName, "(null)") || !*pvList[lIndex]->data.szVarName)
			continue;
		// Create PVName or use previous one
		lSize = strlen(pvList[lIndex]->data.szVarName);
		if((**ResultArray)->result[arraySize].PVName == 0x0) {
			(**ResultArray)->result[arraySize].PVName = (LStrHandle)DSNewHClr(sizeof(size_t)+lSize*sizeof(uChar[1]));
		} else {
			if((*(**ResultArray)->result[arraySize].PVName)->cnt != (int32)lSize) {
				DSDisposeHandle((**ResultArray)->result[arraySize].PVName);
				(**ResultArray)->result[arraySize].PVName = (LStrHandle)DSNewHClr(sizeof(size_t)+lSize*sizeof(uChar[1]));
				if(DSCheckHandle((**ResultArray)->result[arraySize].PVName) != noErr) {
					DbgTime();CaLabDbgPrintf("%s","Error while creating LabVIEW array (1a)");
					disposeResultArray(ResultArray);
					iGetCounter--;
					CaLabMutexUnlock(pvLock, &lockStatus2);
					return;
				}
			}
		}
		memcpy_s((*(**ResultArray)->result[arraySize].PVName)->str, lSize, pvList[lIndex]->data.szVarName, lSize);
		(*(**ResultArray)->result[arraySize].PVName)->cnt = (int32)lSize;
		// Get size of value array
		valueArraySize = pvList[lIndex]->data.lElems;
		// Create StringValueArray2D or use previous one
		if(valueArraySize > 0) {
			if((**ResultArray)->result[arraySize].StringValueArray == 0x0) {
				(**ResultArray)->result[arraySize].StringValueArray = (sStringArrayHdl)DSNewHClr(sizeof(size_t)+valueArraySize*sizeof(LStrHandle[1]));
			} else {
				if((uInt32)((*(**ResultArray)->result[arraySize].StringValueArray)->dimSize) != valueArraySize) {
					// Remove values
					for(int32 ii=((int32)(*(**ResultArray)->result[arraySize].StringValueArray)->dimSize)-1; !bStopped && ii>=0; ii--) {
						if((*(**ResultArray)->result[arraySize].StringValueArray)->elt[ii] != 0x0) {
							(*(*(**ResultArray)->result[arraySize].StringValueArray)->elt[ii])->cnt = 0;
							iResult = DSDisposeHandle((*(**ResultArray)->result[arraySize].StringValueArray)->elt[ii]);
							(*(**ResultArray)->result[arraySize].StringValueArray)->elt[ii] = 0x0;
							if(iResult != noErr) {
								DbgTime();CaLabDbgPrintf("%s","Error while cleaning up LabVIEW array (2)");
								disposeResultArray(ResultArray);
								iGetCounter--;
								CaLabMutexUnlock(pvLock, &lockStatus2);
								CaLabMutexUnlock(putLock, &lockStatus1);
								return;
							}
						} else {
							DbgTime();CaLabDbgPrintf("%s","Error while cleaning up LabVIEW array (2a)");
							disposeResultArray(ResultArray);
							iGetCounter--;
							CaLabMutexUnlock(pvLock, &lockStatus2);
							CaLabMutexUnlock(putLock, &lockStatus1);
							return;
						}
					}
					if((*(**ResultArray)->result[arraySize].StringValueArray)->dimSize != (size_t)valueArraySize) {
						DSDisposeHandle((**ResultArray)->result[arraySize].StringValueArray);
						(**ResultArray)->result[arraySize].StringValueArray = (sStringArrayHdl)DSNewHClr(sizeof(size_t)+valueArraySize*sizeof(LStrHandle[1]));
						if(DSCheckHandle((**ResultArray)->result[arraySize].StringValueArray) != noErr) {
							DbgTime();CaLabDbgPrintf("%s","Error while creating LabVIEW array (2b)");
							disposeResultArray(ResultArray);
							iGetCounter--;
							CaLabMutexUnlock(pvLock, &lockStatus2);
							CaLabMutexUnlock(putLock, &lockStatus1);
							return;
						}
					}
				}
			}
			// Copy values from cache to LV array
			for(uInt32 lArrayIndex=0; !bStopped && lArrayIndex<(uInt32)valueArraySize; lArrayIndex++) {
				lSize = strlen(pvList[lIndex]->data.szValueArray+lArrayIndex*MAX_STRING_SIZE);
				if(*ResultArray == 0x0 || &(**ResultArray)->result[arraySize] == 0x0) {
					DbgTime();CaLabDbgPrintf("%s","bad handle size for ResultArray");
					disposeResultArray(ResultArray);
					iGetCounter--;
					CaLabMutexUnlock(pvLock, &lockStatus2);
					CaLabMutexUnlock(putLock, &lockStatus1);
					return;
				}
				if((*(**ResultArray)->result[arraySize].StringValueArray)->elt[lArrayIndex] == 0x0) {
					(*(**ResultArray)->result[arraySize].StringValueArray)->elt[lArrayIndex] = (LStrHandle) DSNewHClr(sizeof(size_t)+lSize*sizeof(uChar[1]));
				} else {
					if((*(*(**ResultArray)->result[arraySize].StringValueArray)->elt[lArrayIndex])->cnt != (int32)lSize) {
						DSDisposeHandle((*(**ResultArray)->result[arraySize].StringValueArray)->elt[lArrayIndex]);
						(*(**ResultArray)->result[arraySize].StringValueArray)->elt[lArrayIndex] = (LStrHandle) DSNewHClr(sizeof(size_t)+lSize*sizeof(uChar[1]));
						if(DSCheckHandle((*(**ResultArray)->result[arraySize].StringValueArray)->elt[lArrayIndex]) != noErr) {
							DbgTime();CaLabDbgPrintf("%s","Error while creating LabVIEW array (2c)");
							disposeResultArray(ResultArray);
							iGetCounter--;
							CaLabMutexUnlock(pvLock, &lockStatus2);
							CaLabMutexUnlock(putLock, &lockStatus1);
							return;
						}
					}
				}
				memcpy_s((*(*(**ResultArray)->result[arraySize].StringValueArray)->elt[lArrayIndex])->str, lSize, (pvList[lIndex]->data.szValueArray+lArrayIndex*MAX_STRING_SIZE), lSize);
				(*(*(**ResultArray)->result[arraySize].StringValueArray)->elt[lArrayIndex])->cnt = (int32)lSize;
			}
			(*(**ResultArray)->result[arraySize].StringValueArray)->dimSize = valueArraySize;
			// Create ValueNumberArray or use previous one
			if((**ResultArray)->result[arraySize].ValueNumberArray == 0x0) {
				(**ResultArray)->result[arraySize].ValueNumberArray = (sDoubleArrayHdl)DSNewHClr(sizeof(size_t)+valueArraySize*sizeof(double[1]));
			} else {
				if((uInt32)((*(**ResultArray)->result[arraySize].ValueNumberArray)->dimSize) != (size_t)valueArraySize) {
					DSDisposeHandle((**ResultArray)->result[arraySize].ValueNumberArray);
					(**ResultArray)->result[arraySize].ValueNumberArray = (sDoubleArrayHdl)DSNewHClr(sizeof(size_t)+valueArraySize*sizeof(double[1]));
					if(DSCheckHandle((((**ResultArray)->result)[lIndex]).ValueNumberArray) != noErr) {
						//if(NumericArrayResize(iQ, 1, (UHandle*)&(((**ResultArray)->result)[lIndex]).ValueNumberArray, valueArraySize) != noErr) {
						DbgTime();CaLabDbgPrintf("%s","Error while creating LabVIEW array (3)");
						disposeResultArray(ResultArray);
						iGetCounter--;
						CaLabMutexUnlock(pvLock, &lockStatus2);
						CaLabMutexUnlock(putLock, &lockStatus1);
						return;
					}
				}
			}
			for(uInt32 lArrayIndex=0; !bStopped && lArrayIndex<valueArraySize && lArrayIndex<(uInt32)valueArraySize; lArrayIndex++) {
				dValue = pvList[lIndex]->data.dValueArray[lArrayIndex];
				((*(**ResultArray)->result[arraySize].ValueNumberArray)->elt[lArrayIndex]) = dValue;
			}
			(*(**ResultArray)->result[arraySize].ValueNumberArray)->dimSize = valueArraySize;
		}
		// Create StatusString or use previous one
		lSize = strlen(pvList[lIndex]->data.szStatus);
		if((**ResultArray)->result[arraySize].StatusString == 0x0) {
			(**ResultArray)->result[arraySize].StatusString = (LStrHandle)DSNewHClr(sizeof(size_t)+lSize*sizeof(uChar[1]));
		} else {
			if((*(**ResultArray)->result[arraySize].StatusString)->cnt != (int32)lSize) {
				DSDisposeHandle((**ResultArray)->result[arraySize].StatusString);
				(**ResultArray)->result[arraySize].StatusString = (LStrHandle)DSNewHClr(sizeof(size_t)+lSize*sizeof(uChar[1]));
				if(DSCheckHandle((**ResultArray)->result[arraySize].StatusString) != noErr) {
					DbgTime();CaLabDbgPrintf("%s","Error while creating LabVIEW array (4)");
					disposeResultArray(ResultArray);
					iGetCounter--;
					CaLabMutexUnlock(pvLock, &lockStatus2);
					CaLabMutexUnlock(putLock, &lockStatus1);
					return;
				}
			}
		}
		memcpy_s((*(**ResultArray)->result[arraySize].StatusString)->str, lSize, pvList[lIndex]->data.szStatus, lSize);
		(*(**ResultArray)->result[arraySize].StatusString)->cnt = (int32)lSize;
		// Write StatusNumber
		if(valueArraySize > 0)
			(**ResultArray)->result[arraySize].StatusNumber = pvList[lIndex]->data.nStatus;
		else
			(**ResultArray)->result[arraySize].StatusNumber = 3;
		// Create SeverityString or use previous one
		lSize = strlen(pvList[lIndex]->data.szSeverity);
		if((**ResultArray)->result[arraySize].SeverityString == 0x0) {
			(**ResultArray)->result[arraySize].SeverityString = (LStrHandle)DSNewHClr(sizeof(size_t)+lSize*sizeof(uChar[1]));
		} else {
			if((*(**ResultArray)->result[arraySize].SeverityString)->cnt != (int32)lSize) {
				DSDisposeHandle((**ResultArray)->result[arraySize].SeverityString);
				(**ResultArray)->result[arraySize].SeverityString = (LStrHandle)DSNewHClr(sizeof(size_t)+lSize*sizeof(uChar[1]));
				if(DSCheckHandle((**ResultArray)->result[arraySize].SeverityString) != noErr) {
					DbgTime();CaLabDbgPrintf("%s","Error while creating LabVIEW array (5)");
					disposeResultArray(ResultArray);
					iGetCounter--;
					CaLabMutexUnlock(pvLock, &lockStatus2);
					CaLabMutexUnlock(putLock, &lockStatus1);
					return;
				}
			}
		}
		memcpy_s((*(**ResultArray)->result[arraySize].SeverityString)->str, lSize, pvList[lIndex]->data.szSeverity, lSize);
		(*(**ResultArray)->result[arraySize].SeverityString)->cnt = (int32)lSize;
		// Write SeverityNumber
		if(valueArraySize > 0)
			(**ResultArray)->result[arraySize].SeverityNumber = pvList[lIndex]->data.nSeverity;
		else
			(**ResultArray)->result[arraySize].SeverityNumber = 9;
		// Create TimeStampString or use previous one
		lSize = strlen(pvList[lIndex]->data.szTimeStamp);
		if((**ResultArray)->result[arraySize].TimeStampString == 0x0) {
			(**ResultArray)->result[arraySize].TimeStampString = (LStrHandle)DSNewHClr(sizeof(size_t)+lSize*sizeof(uChar[1]));
		} else {
			if((*(**ResultArray)->result[arraySize].TimeStampString)->cnt != (int32)lSize) {
				DSDisposeHandle((**ResultArray)->result[arraySize].TimeStampString);
				(**ResultArray)->result[arraySize].TimeStampString = (LStrHandle)DSNewHClr(sizeof(size_t)+lSize*sizeof(uChar[1]));
				if(DSCheckHandle((**ResultArray)->result[arraySize].TimeStampString) != noErr) {
					DbgTime();CaLabDbgPrintf("%s","Error while creating LabVIEW array (6)");
					disposeResultArray(ResultArray);
					iGetCounter--;
					CaLabMutexUnlock(pvLock, &lockStatus2);
					CaLabMutexUnlock(putLock, &lockStatus1);
					return;
				}
			}
		}
		memcpy_s((*(**ResultArray)->result[arraySize].TimeStampString)->str, lSize, pvList[lIndex]->data.szTimeStamp, (int32)lSize);
		(*(**ResultArray)->result[arraySize].TimeStampString)->cnt = (int32)lSize;
		// Write TimeStampNumber
		if(valueArraySize > 0)
			(**ResultArray)->result[arraySize].TimeStampNumber = pvList[lIndex]->data.lTimeStamp;
		else
			(**ResultArray)->result[arraySize].TimeStampNumber = 0;
		if(pvList[lIndex]->data.iFieldCount > 0) {
			// Create FieldNameArray or use previous one
			if((**ResultArray)->result[arraySize].FieldNameArray == 0x0) {
				(**ResultArray)->result[arraySize].FieldNameArray = (sStringArrayHdl)DSNewHClr(sizeof(size_t)+pvList[lIndex]->data.iFieldCount*sizeof(LStrHandle[1]));
			} else {
				if((*(**ResultArray)->result[arraySize].FieldNameArray)->dimSize != (size_t)pvList[lIndex]->data.iFieldCount) {
					// Remove values
					for(int32 iArrayIndex=((int32)(*(**ResultArray)->result[arraySize].FieldNameArray)->dimSize)-1; !bStopped && iArrayIndex>=0; iArrayIndex--) {
						if((*(**ResultArray)->result[arraySize].FieldNameArray)->elt[iArrayIndex] != 0x0) {
							(*(*(**ResultArray)->result[arraySize].FieldNameArray)->elt[iArrayIndex])->cnt = 0;
							iResult = DSDisposeHandle((*(**ResultArray)->result[arraySize].FieldNameArray)->elt[iArrayIndex]);
							if(iResult != noErr) {
								DbgTime();CaLabDbgPrintf("%s","Error while cleaning up LabVIEW array (7)");
								disposeResultArray(ResultArray);
								iGetCounter--;
								CaLabMutexUnlock(pvLock, &lockStatus2);
								CaLabMutexUnlock(putLock, &lockStatus1);
								return;
							}
						} else {
							DbgTime();CaLabDbgPrintf("%s","Error while cleaning up LabVIEW array (7a)");
							disposeResultArray(ResultArray);
							iGetCounter--;
							CaLabMutexUnlock(pvLock, &lockStatus2);
							CaLabMutexUnlock(putLock, &lockStatus1);
							return;
						}
					}
					if((*(**ResultArray)->result[arraySize].FieldNameArray)->dimSize != (size_t)pvList[lIndex]->data.iFieldCount) {
						DSDisposeHandle((**ResultArray)->result[arraySize].FieldNameArray);
						(**ResultArray)->result[arraySize].FieldNameArray = (sStringArrayHdl)DSNewHClr(sizeof(size_t)+pvList[lIndex]->data.iFieldCount*sizeof(LStrHandle[1]));
						if(DSCheckHandle((**ResultArray)->result[arraySize].FieldNameArray) != noErr) {
							DbgTime();CaLabDbgPrintf("%s","Error while creating LabVIEW array (7b)");
							disposeResultArray(ResultArray);
							iGetCounter--;
							CaLabMutexUnlock(pvLock, &lockStatus2);
							CaLabMutexUnlock(putLock, &lockStatus1);
							return;
						}
					}
				}
			}
			// If fields are available create field objects and write all of them
			for(uInt32 iArrayIndex=0; !bStopped && iArrayIndex<pvList[lIndex]->data.iFieldCount; iArrayIndex++) {
				// Create new FieldNameArray or use previous one
				if(pvList[lIndex]->data.szFieldNameArray == 0x0) {
					DbgTime();CaLabDbgPrintf("Error: %s has already been configured with different optional fields", pvList[lIndex]->data.szVarName);
					continue;
				}
				lSize = strlen(pvList[lIndex]->data.szFieldNameArray+iArrayIndex*MAX_STRING_SIZE);
				if(((*(**ResultArray)->result[arraySize].FieldNameArray)->elt[iArrayIndex]) == 0x0) {
					((*(**ResultArray)->result[arraySize].FieldNameArray)->elt[iArrayIndex]) = (LStrHandle) DSNewHClr(sizeof(size_t)+lSize*sizeof(uChar[1]));
				} else {
					if((*(*(**ResultArray)->result[arraySize].FieldNameArray)->elt[iArrayIndex])->cnt != (int32)lSize) {
						DSDisposeHandle((*(**ResultArray)->result[arraySize].FieldNameArray)->elt[iArrayIndex]);
						((*(**ResultArray)->result[arraySize].FieldNameArray)->elt[iArrayIndex]) = (LStrHandle) DSNewHClr(sizeof(size_t)+lSize*sizeof(uChar[1]));
						if(DSCheckHandle((*(**ResultArray)->result[arraySize].FieldNameArray)->elt[iArrayIndex]) != noErr) {
							DbgTime();CaLabDbgPrintf("%s","Error while creating LabVIEW array (7c)");
							disposeResultArray(ResultArray);
							iGetCounter--;
							CaLabMutexUnlock(pvLock, &lockStatus2);
							CaLabMutexUnlock(putLock, &lockStatus1);
							return;
						}
					}
				}
				// Write names to FieldNameArray
				if((*(**ResultArray)->result[arraySize].FieldNameArray)->elt[iArrayIndex] != 0x0) {
					memcpy_s((*(*(**ResultArray)->result[arraySize].FieldNameArray)->elt[iArrayIndex])->str, lSize, pvList[lIndex]->data.szFieldNameArray+iArrayIndex*MAX_STRING_SIZE, lSize);
					(*(*(**ResultArray)->result[arraySize].FieldNameArray)->elt[iArrayIndex])->cnt = (int32)lSize;
				} else {
					DbgTime();CaLabDbgPrintf("%s","Error while creating LabVIEW array (7d)");
					disposeResultArray(ResultArray);
					iGetCounter--;
					CaLabMutexUnlock(pvLock, &lockStatus2);
					CaLabMutexUnlock(putLock, &lockStatus1);
					return;
				}
			}
			if(!(pvList[lIndex]->data.szFieldNameArray == 0x0))
				(*(**ResultArray)->result[arraySize].FieldNameArray)->dimSize = pvList[lIndex]->data.iFieldCount;
			// Create new FieldValueArray or use previous one
			if((**ResultArray)->result[arraySize].FieldValueArray == 0x0) {
				(**ResultArray)->result[arraySize].FieldValueArray = (sStringArrayHdl)DSNewHClr(sizeof(size_t)+pvList[lIndex]->data.iFieldCount*sizeof(LStrHandle[1]));
			} else {
				if((*(**ResultArray)->result[arraySize].FieldValueArray)->dimSize != pvList[lIndex]->data.iFieldCount) {
					// Remove values
					for(int32 iArrayIndex=((int32)(*(**ResultArray)->result[arraySize].FieldValueArray)->dimSize)-1; !bStopped && iArrayIndex>=0; iArrayIndex--) {
						if((*(**ResultArray)->result[arraySize].FieldValueArray)->elt[iArrayIndex] != 0x0) {
							(*(*(**ResultArray)->result[arraySize].FieldValueArray)->elt[iArrayIndex])->cnt = 0;
							iResult = DSDisposeHandle((*(**ResultArray)->result[arraySize].FieldValueArray)->elt[iArrayIndex]);
							if(iResult != noErr) {
								DbgTime();CaLabDbgPrintf("%s","Error while cleaning up LabVIEW array (8)");
								disposeResultArray(ResultArray);
								iGetCounter--;
								CaLabMutexUnlock(pvLock, &lockStatus2);
								CaLabMutexUnlock(putLock, &lockStatus1);
								return;
							}
						} else {
							DbgTime();CaLabDbgPrintf("%s","Error while cleaning up LabVIEW array (8a)");
							disposeResultArray(ResultArray);
							iGetCounter--;
							CaLabMutexUnlock(pvLock, &lockStatus2);
							CaLabMutexUnlock(putLock, &lockStatus1);
							return;
						}
					}
					if((*(**ResultArray)->result[arraySize].FieldValueArray)->dimSize != pvList[lIndex]->data.iFieldCount) {
						DSDisposeHandle((**ResultArray)->result[arraySize].FieldValueArray);
						(**ResultArray)->result[arraySize].FieldValueArray = (sStringArrayHdl)DSNewHClr(sizeof(size_t)+pvList[lIndex]->data.iFieldCount*sizeof(LStrHandle[1]));
						if(DSCheckHandle((**ResultArray)->result[arraySize].FieldValueArray) != noErr) {
							DbgTime();CaLabDbgPrintf("%s","Error while creating LabVIEW array (8b)");
							disposeResultArray(ResultArray);
							iGetCounter--;
							CaLabMutexUnlock(pvLock, &lockStatus2);
							CaLabMutexUnlock(putLock, &lockStatus1);
							return;
						}
					}
				}
			}
			// Write values to FieldValueArray
			for(uInt32 iArrayIndex=0; !bStopped && iArrayIndex<pvList[lIndex]->data.iFieldCount; iArrayIndex++) {
				if(pvList[lIndex]->data.szFieldValueArray == 0x0) {
					continue;
				}
				lSize = strlen(pvList[lIndex]->data.szFieldValueArray);
				if(((*(**ResultArray)->result[arraySize].FieldValueArray)->elt[iArrayIndex]) == 0x0) {
					((*(**ResultArray)->result[arraySize].FieldValueArray)->elt[iArrayIndex]) = (LStrHandle) DSNewHClr(sizeof(size_t)+lSize*sizeof(uChar[1]));
				} else {
					if((*(*(**ResultArray)->result[arraySize].FieldValueArray)->elt[iArrayIndex])->cnt != (int32)lSize) {
						DSDisposeHandle((*(**ResultArray)->result[arraySize].FieldValueArray)->elt[iArrayIndex]);
						((*(**ResultArray)->result[arraySize].FieldValueArray)->elt[iArrayIndex]) = (LStrHandle) DSNewHClr(sizeof(size_t)+lSize*sizeof(uChar[1]));
						if(DSCheckHandle((*(**ResultArray)->result[arraySize].FieldValueArray)->elt[iArrayIndex]) != noErr) {
							DbgTime();CaLabDbgPrintf("%s","Error while creating LabVIEW array (8c)");
							disposeResultArray(ResultArray);
							iGetCounter--;
							CaLabMutexUnlock(pvLock, &lockStatus2);
							CaLabMutexUnlock(putLock, &lockStatus1);
							return;
						}
					}
				}
				if((*(**ResultArray)->result[arraySize].FieldValueArray)->elt[iArrayIndex] != 0x0) {
					memcpy_s((*(*(**ResultArray)->result[arraySize].FieldValueArray)->elt[iArrayIndex])->str, lSize, pvList[lIndex]->data.szFieldValueArray, lSize);
					(*(*(**ResultArray)->result[arraySize].FieldValueArray)->elt[iArrayIndex])->cnt = (int32)lSize;
				} else {
					DbgTime();CaLabDbgPrintf("%s","Error while creating LabVIEW array (8d)");
					disposeResultArray(ResultArray);
					iGetCounter--;
					CaLabMutexUnlock(pvLock, &lockStatus2);
					CaLabMutexUnlock(putLock, &lockStatus1);
					return;
				}
			}
			if(!(pvList[lIndex]->data.szFieldValueArray == 0x0))
				(*(**ResultArray)->result[arraySize].FieldValueArray)->dimSize = pvList[lIndex]->data.iFieldCount;
		}
		// Create ErrorIO and fill in structure
		lSize = strlen(pvList[lIndex]->data.szError);
		if(pvList[lIndex]->data.lError == ECA_NORMAL) {
			(**ResultArray)->result[arraySize].ErrorIO.code = 0;
		} else {
			(**ResultArray)->result[arraySize].ErrorIO.code = ERROR_OFFSET+pvList[lIndex]->data.lError;
		}
		(**ResultArray)->result[arraySize].ErrorIO.status = 0;
		if((**ResultArray)->result[arraySize].ErrorIO.source == 0x0) {
			(**ResultArray)->result[arraySize].ErrorIO.source = (LStrHandle)DSNewHClr(sizeof(size_t)+lSize*sizeof(uChar[1]));
		} else {
			if((*(**ResultArray)->result[arraySize].ErrorIO.source)->cnt != (int32)lSize) {
				DSDisposeHandle((**ResultArray)->result[arraySize].ErrorIO.source);
				(**ResultArray)->result[arraySize].ErrorIO.source = (LStrHandle)DSNewHClr(sizeof(size_t)+lSize*sizeof(uChar[1]));
				if(DSCheckHandle((**ResultArray)->result[arraySize].ErrorIO.source) != noErr) {
					DbgTime();CaLabDbgPrintf("%s","Error while creating LabVIEW array (9)");
					disposeResultArray(ResultArray);
					iGetCounter--;
					CaLabMutexUnlock(pvLock, &lockStatus2);
					CaLabMutexUnlock(putLock, &lockStatus1);
					return;
				}
			}
		}
		memcpy_s((*(**ResultArray)->result[arraySize].ErrorIO.source)->str, lSize, pvList[lIndex]->data.szError, lSize);
		(*(**ResultArray)->result[arraySize].ErrorIO.source)->cnt = (int32)lSize;
		arraySize++;
	}
	(**ResultArray)->dimSize = arraySize;
	iGetCounter--;
	CaLabMutexUnlock(pvLock, &lockStatus2);
	CaLabMutexUnlock(putLock, &lockStatus1);
}

// removes EPICS PVs from event service
//   pPVList:     list of PVs, to be removed
//   iListCount:  number of PVs in list
extern "C" EXPORT void removePVs(char** pPVList, long int iListCount) {
	char szPV[MAX_STRING_SIZE];
	char szField[MAX_STRING_SIZE];

	ca_attach_context(pcac);
	for(long int i=0; i<iListCount; i++) {
		for(unsigned long lFind=0; lFind<lPVCounter; lFind++) {
			strncpy_s(szPV, MAX_STRING_SIZE, pPVList[i], strlen(pPVList[i]));
			strncpy_s(szField, MAX_STRING_SIZE, pPVList[i], strlen(pPVList[i]));
			strncat_s(szField, MAX_STRING_SIZE, ".", 1);
			if(!strcmp(szPV, pvList[lFind]->data.szVarName) || !strstr(szField, pvList[lFind]->data.szVarName)) {
				// Unsubsribe value events
				if(pvList[lFind]->sEvid[0]) {
					ca_clear_subscription(pvList[lFind]->sEvid[0]);
					pvList[lFind]->sEvid[0] = 0;
				}
				// Unsubscribe enum events
				if(pvList[lFind]->sEvid[1]) {
					ca_clear_subscription(pvList[lFind]->sEvid[1]);
					pvList[lFind]->sEvid[1] = 0;
				}
				// Remove channel for values
				if(pvList[lFind]->id[0]) {
					ca_change_connection_event(pvList[lFind]->id[0], 0x0);
					ca_clear_channel(pvList[lFind]->id[0]);
					pvList[lFind]->id[0] = 0;
				}
				// Remove channel for enums
				if(pvList[lFind]->id[1]) {
					ca_change_connection_event(pvList[lFind]->id[1], 0x0);
					ca_clear_channel(pvList[lFind]->id[1]);
					pvList[lFind]->id[1] = 0;
				}
				initPV(pvList[lFind], 0x0, 0x0);
			}
		}
	}
	ca_detach_context();
}

// Global counter for tests
//   returns count of calls
extern "C" EXPORT uInt32 getCounter() {
	return ++globalCounter;
}

// Prepare the library before first using
void caLabLoad(void) {
	char buffer[256];
	const char* access_mode = "w";
	if(getenv("CALAB_POLLING")) {
		bCaLabPolling = true;
	} else {
		bCaLabPolling = false;
	}
	// If c:/data/log exists assume we are an ISIS instrument and hide debug message window
	if( !getenv("CALAB_NODBG") ) {
		if ( access("c:/data/log", 0) == 0 ) {
			access_mode = "a";
			strftime(buffer, sizeof(buffer), "CALAB_NODBG=c:/data/log/CALab-%Y%m%d.log", localtime(NULL));
			putenv(strdup(buffer));
		}
	}
	if(getenv("CALAB_NODBG")) {
		size_t size = strlen(getenv("CALAB_NODBG"));
		szCaLabDbg = (char*)realloc(szCaLabDbg, ++size*sizeof(char));
		if(szCaLabDbg)
			strncpy_s(szCaLabDbg, size, getenv("CALAB_NODBG"), size);
		pFile = fopen(szCaLabDbg,access_mode);
	} else {
		szCaLabDbg = 0x0;
	}
}

// Free all resources allocated by library
void caLabUnload(void) {
	if(!pcac)
		return;
	// Mark session as terminated
	bStopped = true;
	// Get CA context
	ca_attach_context(pcac);
	ca_add_exception_event(NULL, NULL);
	// Wait for termination of tasks and call backs
	uInt32 iTimeout = 1000;
	while((iConnectTaskCounter>0 || iValueChangedTaskCounter>0 || iConnectionChangedTaskCounter>0 || iPutTaskCounter>0 || iGetCounter>0 || iPutCounter>0) && iTimeout--)
		epicsThreadSleep(.001);
	if(pFile) {
		fclose(pFile);
		pFile = 0x0;
	}
	if(szCaLabDbg) {
		free(szCaLabDbg);
		szCaLabDbg = 0x0;
	}
	// Destroy PV list
	for(unsigned long lPvs=0; lPvs<lPVCounter; lPvs++) {
		// Unsubscribe value events
		if(pvList[lPvs]->sEvid[0]) {
			ca_clear_subscription(pvList[lPvs]->sEvid[0]);
			pvList[lPvs]->sEvid[0] = 0;
		}
		// Unsubscribe enum events
		if(pvList[lPvs]->sEvid[1]) {
			ca_clear_subscription(pvList[lPvs]->sEvid[1]);
			pvList[lPvs]->sEvid[1] = 0;
		}
		// Remove channel for values
		if(pvList[lPvs]->id[0]) {
			ca_change_connection_event(pvList[lPvs]->id[0], 0x0);
			ca_clear_channel(pvList[lPvs]->id[0]);
			pvList[lPvs]->id[0] = 0;
		}
		// Remove channel for enums
		if(pvList[lPvs]->id[1]) {
			ca_change_connection_event(pvList[lPvs]->id[1], 0x0);
			ca_clear_channel(pvList[lPvs]->id[1]);
			pvList[lPvs]->id[1] = 0;
		}
	}
	for(unsigned long lPvs=0; lPvs<lPVCounter; lPvs++) {
		// Delete double value array
		if(pvList[lPvs]->data.dValueArray) {
			free(pvList[lPvs]->data.dValueArray);
			pvList[lPvs]->data.dValueArray = 0;
		}
		// Delete string value array
		if(pvList[lPvs]->data.szValueArray) {
			free(pvList[lPvs]->data.szValueArray);
			pvList[lPvs]->data.szValueArray = 0;
		}
		// Delete field name array
		if(pvList[lPvs]->data.szFieldNameArray) {
			free(pvList[lPvs]->data.szFieldNameArray);
			pvList[lPvs]->data.szFieldNameArray = 0;
		}
		// Delete field value array
		if(pvList[lPvs]->data.szFieldValueArray) {
			free(pvList[lPvs]->data.szFieldValueArray);
			pvList[lPvs]->data.szFieldValueArray = 0;
		}
		// Delete PV structure
		free(pvList[lPvs]);
		pvList[lPvs] = 0;
	}
	// Delete PV array
	free(pvList);
	pvList = 0;
	lPVCounter = 0;
	// Remove mutexes
	epicsMutexDestroy(pvLock);
	pvLock = 0x0;
	epicsMutexDestroy(connectQueueLock);
	connectQueueLock = 0x0;
	epicsMutexDestroy(syncQueueLock);
	syncQueueLock = 0x0;
	epicsMutexDestroy(getLock);
	getLock = 0x0;
	epicsMutexDestroy(putLock);
	putLock = 0x0;
	epicsMutexDestroy(pollingLock);
	pollingLock = 0x0;
	epicsMutexDestroy(instanceQueueLock);
	instanceQueueLock = 0x0;
	// Destroy channel access context
	ca_context_destroy();
	if(pPVNameList) {
		// dispose pPVNameList
		for(size_t i=0; i<chidMap.size(); i++) {
			//if(pChannels[i])
			//	ca_clear_channel(pChannels[i]);
			free(pPVNameList[i]);
			pPVNameList[i] = 0;
		}
		free(pPVNameList);
		pPVNameList = NULL;
	}
	if(pChannels) {
		free(pChannels);
		pChannels = NULL;
	}
	chidMap.clear();
	pcac = 0x0;
	iConnectTaskCounter = 0;
	iGetCounter = 0;
	iPutCounter = 0;
	iValueChangedTaskCounter = 0;
	iConnectionChangedTaskCounter = 0;
	iPutTaskCounter = 0;
	bStopped = false;
	iInstances = 0;
#ifndef WIN32
	epicsThreadSleep(5);
#endif
}

// listener for loading and unloading library
//   hDll:       base address of the of library
//   dwReason:   indicator for call reasons
//   lpReserved: indicator for statically or dynamically call (DLL_PROCESS_ATTACH)
//               indicator for reason of termination: FreeLibrary call, a failure to load, or process termination (DLL_PROCESS_DETACH)
extern "C" EXPORT int __stdcall DllMain (void* hDll, unsigned long dwReason, void* lpReserved) {
	switch (dwReason) {
		// create CA context if library has been attached
	case DLL_PROCESS_ATTACH:
		caLabLoad();
		// DLL is loaded
		break;
	case DLL_PROCESS_DETACH:
		caLabUnload();
		// DLL is freed
		break;
	}
	return 1;
}
