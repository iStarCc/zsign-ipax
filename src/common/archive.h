#pragma once

#include "common.h"

/** 新一轮「仅归档」会话：在每次即将调用 `Zip::Archive` / `ArchivePayloadFolderForIPA` 之前由桥接层调用，清除上一轮的取消位与失败原因（避免上一任务残留的取消请求影响本次）。 */
void ZipBeginArchiveOperation();
void ZipRequestArchiveCancel();
bool ZipIsArchiveCancelRequested();
/** 上一轮 `Archive*` 返回 false 时，是否为「用户请求取消」（而非 IO/磁盘错误）。 */
bool ZipArchiveLastFailureWasUserCancel();

class Zip
{
public:
	
	static bool Archive(const string& strFolder, const string& strZipFile, int nZipLevel);
	/** 仅将 `…/Payload` 目录树打入 zip，条目前缀恒为 `Payload/…`（不把 Payload 的同级其它文件打进 IPA）。 */
	static bool ArchivePayloadFolderForIPA(const string& strPayloadFolder, const string& strZipFile, int nZipLevel);
	static bool Extract(const char* zip_file, const char* output_folder);
	/** 解压 ZIP/IPA 到目录；与 `Extract` 相同清理/安全路径规则，额外输出与压缩一致的条目进度与大文件心跳（`ZipLogExtractUnified`）。 */
	static bool ExtractWithProgress(const char* zip_file, const char* output_folder);
	/** 校验 zip 是否为 IPA 布局：至少存在 `Payload/xxx.app` 路径（条目路径，UTF-8）。 */
	static bool HasIpaLayout(const char* zip_file);
	/** 解压到已有目录：不删除输出根目录；若目录不存在则创建。失败时不会自动清空整目录。 */
	static bool ExtractWithProgressIntoExisting(const char* zip_file, const char* output_folder);

private:
	static bool _ReadFileFromZip(
		void* hZip,
		const string& strPath,
		const string& strRootFolder,
		int entriesCompletedBefore,
		int entryTotal,
		const string& strRelativePathForLog,
		uint64_t uncompressedSize,
		bool withProgress);
	static bool _ExtractImpl(const char* zip_file, const char* output_folder, bool withProgress);
	static bool _WriteFileToZip(void* hZip, const string& strFile, const string& strRelativePath, int zip_level, int completedEntriesBefore, int entryTotal);
	static bool _CreateFolderToZip(void* hZip, const string& strFolder, const string& strRootFolder, int zip_level);
	static void GetModificationTime(const char* path, void* zi);
};
