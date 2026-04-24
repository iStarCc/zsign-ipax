#pragma once
#include "common.h"

typedef function<bool (bool bFolder, const string& strPath)> enum_folder_callback;

class ZFile
{
public:
	static bool		ReadFile(const char* szFile, string& strData);
	static bool		ReadFileV(string& strData, const char* szPath, ...);
	static bool		WriteFile(const char* szFile, const string& strData);
	static bool		WriteFile(const char* szFile, const char* szData, size_t sLen);
	static bool		WriteFileV(string& strData, const char* szPath, ...);
	static bool		WriteFileV(const char* szData, size_t sLen, const char* szPath, ...);
	static bool		AppendFile(const char* szFile, const string& strData);
	static bool		AppendFile(const char* szFile, const char* szData, size_t sLen);
	static bool		IsRegularFile(const char* szFile);
	static bool		IsFolder(const char* szFolder);
	static bool		IsFolderV(const char* szPath, ...);
	static bool		CreateFolder(const char* szFolder);
	static bool		CreateFolderV(const char* szPath, ...);
	static bool		RemoveFile(const char* szFile);
	static bool		RemoveFileV(const char* szPath, ...);
	static bool		RemoveFolder(const char* szFolder);
	static bool		RemoveFolderV(const char* szPath, ...);
	static bool		IsFileExists(const char* szFile);
	static bool		IsFileExistsV(const char* szPath, ...);
	static int64_t	GetFileSize(FILE* fp);
	static int64_t	GetFileSize(const char* szPath);
	static int64_t	GetFileSizeV(const char* szPath, ...);
	static string	GetFileSizeString(const char* szFile);
	static bool		IsZipFile(const char* szFile);
	static bool		CopyFile(const char* szSrcFile, const char* szDestFile);
	static bool		CopyFileV(const char* szSrcFile, const char* szDestPath, ...);
	static string	GetFullPath(const char* szPath);
	static string	GetRealPathV(const char* szPath, ...);
	static void*	MapFile(const char* path, size_t offset, size_t size, size_t* psize, bool ro);
	static bool		UnmapFile(void* base, size_t size);
	static bool		IsPathSuffix(const string& strPath, const char* suffix);
	static const char* GetTempFolder();
	static bool		EnumFolder(const char* szFolder, bool bRecursive, enum_folder_callback filter, enum_folder_callback callback);

	static bool		PathRemoveFileSpec(string& path);
	/// 在打成 IPA 前删除 macOS/解压产生的无用项（含子目录）：`.DS_Store`、`__MACOSX`、以 `._` 开头的资源叉文件、`.Spotlight-V100`、`.Trashes`、`.fseventsd`、`.AppleDouble`、`.LSOverride` 等。对 `szRoot` 下整棵树递归；若路径无效则返回 false。
	static bool		RemoveIPAPackagingJunkFromFolder(const char* szRoot);

private:
	static int RemoveFolderCallBack(const char* fpath, const struct stat* sb, int typeflag, struct FTW* ftwbuf);

private:
	static map<void*, void*> s_mapFiles;
};
