#pragma once

#include "common.h"

class Zip
{
public:
	
	static bool Archive(const string& strFolder, const string& strZipFile, int nZipLevel);
	/** 仅将 `…/Payload` 目录树打入 zip，条目前缀恒为 `Payload/…`（不把 Payload 的同级其它文件打进 IPA）。 */
	static bool ArchivePayloadFolderForIPA(const string& strPayloadFolder, const string& strZipFile, int nZipLevel);
	static bool Extract(const char* zip_file, const char* output_folder);

private:
	typedef function<bool(void* hFile, bool bFolder, const string& strPath)> enum_zip_items_callback;

private:
	static bool _EnumZipItems(const char* zip_file, enum_zip_items_callback callback);
	static bool _ReadFileFromZip(void* hZip, const string& strPath, const string& strRootFolder);
	static bool _Extract(const char* zip_file, const char* output_folder);
	static bool _WriteFileToZip(void* hZip, const string& strFile, const string& strRelativePath, int zip_level, int completedEntriesBefore, int entryTotal);
	static bool _CreateFolderToZip(void* hZip, const string& strFolder, const string& strRootFolder, int zip_level);
	static void GetModificationTime(const char* path, void* zi);
};
