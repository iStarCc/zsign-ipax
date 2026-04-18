#pragma once
#include "common.h"

class ZLog
{
public:
	enum eLogType
	{
		E_NONE = 0,
		E_ERROR = 1,
		E_WARN = 2,
		E_INFO = 3,
		E_DEBUG = 4
	};

public:
	static bool IsDebug() { return (E_DEBUG == g_nLogLevel); }
	static void Print(const char* szLog);
	static void PrintV(const char* szFormat, ...);
	static void Debug(const char* szLog);
	static void DebugV(const char* szFormat, ...);
	static bool Warn(const char* szLog);
	static bool WarnV(const char* szFormat, ...);
	static bool Error(const char* szLog);
	static bool ErrorV(const char* szFormat, ...);
	static bool Success(const char* szLog);
	static bool SuccessV(const char* szFormat, ...);
	static bool PrintResult(bool bSuccess, const char* szLog);
	static bool PrintResultV(bool bSuccess, const char* szFormat, ...);
	static void Print(int nLevel, const char* szLog);
	static void PrintV(int nLevel, const char* szFormat, ...);
	static void SetLogLever(int nLogLevel) { g_nLogLevel = nLogLevel; }

private:
	static void _Print(const char* szLog, int nColor = 0);
	static int g_nLogLevel;
};

#ifdef __cplusplus
extern "C" {
#endif

/** 可选：除 stdout 外，将每条日志（已走 zlog_i18n）以 UTF-8 回调。color：0 默认，6 警告，10 成功，12 错误。 */
typedef void (*ZLogExternalSink)(const char* utf8Line, int color, void* userdata);
void ZLog_SetExternalSink(ZLogExternalSink sink, void* userdata);
void ZLog_ClearExternalSink(void);

#ifdef __cplusplus
}
#endif
