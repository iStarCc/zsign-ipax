#include "archive.h"
#include "archive_zip_progress.h"

#include "third-party/minizip/zip.h"
#include "third-party/minizip/unzip.h"

#include <atomic>
#include <sys/stat.h>

namespace {

std::atomic<bool> g_zipArchiveCancelRequested{ false };
std::atomic<bool> g_zipArchiveLastFailureWasUserCancel{ false };

} // namespace

void ZipBeginArchiveOperation()
{
	g_zipArchiveCancelRequested.store(false, std::memory_order_release);
	g_zipArchiveLastFailureWasUserCancel.store(false, std::memory_order_release);
}

void ZipRequestArchiveCancel()
{
	g_zipArchiveCancelRequested.store(true, std::memory_order_release);
}

bool ZipIsArchiveCancelRequested()
{
	return g_zipArchiveCancelRequested.load(std::memory_order_acquire);
}

bool ZipArchiveLastFailureWasUserCancel()
{
	return g_zipArchiveLastFailureWasUserCancel.load(std::memory_order_acquire);
}

static void ZipMarkArchiveStoppedByUserCancel()
{
	g_zipArchiveLastFailureWasUserCancel.store(true, std::memory_order_release);
}

#if !defined(S_ISREG) && defined(S_IFMT) && defined(S_IFREG)
#define S_ISREG(m) (((m)&S_IFMT) == S_IFREG)
#endif

static void zipStatFileMb(const string& strFullPath, uint64_t* outDoneMb, uint64_t* outTotalMb)
{
	struct stat st = { 0 };
	*outDoneMb = 0;
	*outTotalMb = 0;
	if (0 != stat(strFullPath.c_str(), &st))
		return;
	if (!S_ISREG(st.st_mode))
		return;
	uint64_t sz = (uint64_t)st.st_size;
	uint64_t mb = sz / (1024ULL * 1024ULL);
	if (sz > 0 && mb < 1)
		mb = 1;
	*outDoneMb = mb;
	*outTotalMb = mb;
}

/** ZIP 通用位标志 bit 11：文件名与注释使用 UTF-8（APPNOTE / Info-ZIP）。 */
static const uLong kZipGeneralPurposeFlagUTF8 = 0x800u;

void Zip::GetModificationTime(const char* path, void* zfi)
{
	zip_fileinfo* zi = (zip_fileinfo*)zfi;
	struct stat st = { 0 };
	memset(zi, 0, sizeof(zip_fileinfo));
	if (0 == stat(path, &st)) {
#ifdef _WIN32
		struct tm tm = { 0 };
		localtime_s(&tm, &st.st_mtime);
		zi->tmz_date.tm_sec = tm.tm_sec;
		zi->tmz_date.tm_min = tm.tm_min;
		zi->tmz_date.tm_hour = tm.tm_hour;
		zi->tmz_date.tm_mday = tm.tm_mday;
		zi->tmz_date.tm_mon = tm.tm_mon;
		zi->tmz_date.tm_year = tm.tm_year + 1900;
#else
		struct tm tm = { 0 };
		localtime_r(&st.st_mtime, &tm);
		zi->tmz_date.tm_sec = tm.tm_sec;
		zi->tmz_date.tm_min = tm.tm_min;
		zi->tmz_date.tm_hour = tm.tm_hour;
		zi->tmz_date.tm_mday = tm.tm_mday;
		zi->tmz_date.tm_mon = tm.tm_mon;
		zi->tmz_date.tm_year = tm.tm_year + 1900;
#endif
	}
}

bool Zip::_WriteFileToZip(void* hZip, const string& strFile, const string& strRelativePath, int zip_level, int completedEntriesBefore, int entryTotal)
{
	FILE* fp = NULL;
	_fopen64(fp, strFile.c_str(), "rb");
	if (NULL == fp) {
		ZLog::ErrorV(">>> Zip: Failed to open file: %s\n", strFile.c_str());
		return false;
	}

	struct stat st = { 0 };
	uint64_t uFileSize = 0;
	if (0 == stat(strFile.c_str(), &st)) {
		uFileSize = (uint64_t)st.st_size;
	}
	if (uFileSize == 0) {
		if (0 == fseek(fp, 0, SEEK_END)) {
			long long n = ftello(fp);
			if (n > 0) {
				uFileSize = (uint64_t)n;
			}
			fseek(fp, 0, SEEK_SET);
		}
	}

	zip_fileinfo zi = { 0 };
	GetModificationTime(strFile.c_str(), &zi);
	if (ZIP_OK != zipOpenNewFileInZip4_64(hZip, strRelativePath.c_str(), &zi, NULL, 0, NULL, 0, NULL, Z_DEFLATED, zip_level, 0, -MAX_WBITS, DEF_MEM_LEVEL, 0, NULL, 0, 0, kZipGeneralPurposeFlagUTF8, 0)) {
		fclose(fp);
		ZLog::ErrorV(">>> Zip: Failed to add file to zip: %s\n", strRelativePath.c_str());
		return false;
	}

	/* 单文件 < 阈值时仅依赖「条目级」进度；阈值过高时 10–40MB 主二进制也会长时间无换行。 */
	static const uint64_t kLargeFileThreshold = 8ULL * 1024ULL * 1024ULL;
	static const uint64_t kHeartbeatBytes = 4ULL * 1024ULL * 1024ULL;
	static const uint64_t kFirstPulseBytes = 1ULL * 1024ULL * 1024ULL;

	bool bRet = true;
	uint64_t written_total = 0;
	uint64_t since_heartbeat = 0;
	bool bDidFirstPulse = false;
	char buffer[4096];
	size_t bytes_read = fread(buffer, 1, sizeof(buffer), fp);
	while (bytes_read > 0) {
		if (ZipIsArchiveCancelRequested()) {
			ZipMarkArchiveStoppedByUserCancel();
			ZLog::Print(">>> Zip: archive cancelled (during file write).\n");
			bRet = false;
			break;
		}
		if (zipWriteInFileInZip(hZip, buffer, (uint32_t)bytes_read) < 0) {
			bRet = false;
			break;
		}
		written_total += (uint64_t)bytes_read;
		if (uFileSize >= kLargeFileThreshold) {
			uint64_t totalMb = uFileSize / (1024ULL * 1024ULL);
			if (uFileSize > 0 && totalMb < 1)
				totalMb = 1;
			uint64_t doneMb = written_total / (1024ULL * 1024ULL);
			if (!bDidFirstPulse && written_total >= kFirstPulseBytes) {
				ZipLogCompressUnified(completedEntriesBefore, entryTotal, strRelativePath, doneMb, totalMb, true);
				bDidFirstPulse = true;
				since_heartbeat = 0;
			} else {
				since_heartbeat += (uint64_t)bytes_read;
				if (since_heartbeat >= kHeartbeatBytes) {
					ZipLogCompressUnified(completedEntriesBefore, entryTotal, strRelativePath, doneMb, totalMb, true);
					since_heartbeat = 0;
				}
			}
		}
		bytes_read = fread(buffer, 1, sizeof(buffer), fp);
	}

	zipCloseFileInZip(hZip);
	fclose(fp);
	return bRet;
}

bool Zip::_CreateFolderToZip(void* hZip, const string& strFolder, const string& strRelativePath, int zip_level)
{
	zip_fileinfo zi = { 0 };
	GetModificationTime(strFolder.c_str(), &zi);
	if (ZIP_OK != zipOpenNewFileInZip4_64(hZip, strRelativePath.c_str(), &zi, NULL, 0, NULL, 0, NULL, Z_DEFLATED, zip_level, 0, -MAX_WBITS, DEF_MEM_LEVEL, 0, NULL, 0, 0, kZipGeneralPurposeFlagUTF8, 0)) {
		ZLog::ErrorV(">>> Zip: Failed to create folder to zip: %s\n", strRelativePath.c_str());
		return false;
	}
	zipCloseFileInZip(hZip);
	return true;
}

bool Zip::Archive(const string& strFolder, const string& strZipFile, int nZipLevel)
{
	 if (nZipLevel < 0 || nZipLevel > 9) {
		ZLog::ErrorV(">>> Zip: Invalid compression level: %d\n", nZipLevel);
        return false;
    }
    
    zipFile zf = zipOpen64(strZipFile.c_str(), 0);
    if (!zf) {
		ZLog::ErrorV(">>> Zip: Failed to create zip file: %s\n", strZipFile.c_str());
        return false;
    }

	const size_t nTotalEntries = ZipArchiveEntryCount(strFolder);
	int nDone = 0;

	bool bRet = true;
	ZFile::EnumFolder(strFolder.c_str(), true, NULL, [&](bool bFolder, const string& strPath) {
		if (ZipIsArchiveCancelRequested()) {
			ZipMarkArchiveStoppedByUserCancel();
			ZLog::Print(">>> Zip: archive cancelled (between entries).\n");
			bRet = false;
			return true;
		}
		string strRelativePath = strPath.substr(strFolder.size() + 1);
		ZUtil::StringReplace(strRelativePath, "\\", "/");

#ifdef _WIN32
		iconv ic;
		strRelativePath = ic.A2U8(strRelativePath);
#endif

		if (bFolder) {
			strRelativePath += "/";
			if (!_CreateFolderToZip(zf, strPath, strRelativePath, nZipLevel)) {
				bRet = false;
				return true;
			}
			nDone++;
			if (nTotalEntries > 0)
				ZipLogCompressUnified(nDone, (int)nTotalEntries, strRelativePath, 0, 0, false);
		} else {
			if (!_WriteFileToZip(zf, strPath, strRelativePath, nZipLevel, nDone, (int)nTotalEntries)) {
				bRet = false;
				return true;
			}
			nDone++;
			uint64_t dmb = 0, tmb = 0;
			zipStatFileMb(strPath, &dmb, &tmb);
			if (nTotalEntries > 0)
				ZipLogCompressUnified(nDone, (int)nTotalEntries, strRelativePath, dmb, tmb, false);
		}
		return false;
	});

    zipClose(zf, NULL);
	return bRet;
}

bool Zip::ArchivePayloadFolderForIPA(const string& strPayloadFolder, const string& strZipFile, int nZipLevel)
{
	if (nZipLevel < 0 || nZipLevel > 9) {
		ZLog::ErrorV(">>> Zip: Invalid compression level: %d\n", nZipLevel);
		return false;
	}

	zipFile zf = zipOpen64(strZipFile.c_str(), 0);
	if (!zf) {
		ZLog::ErrorV(">>> Zip: Failed to create zip file: %s\n", strZipFile.c_str());
		return false;
	}

	string strFolder = strPayloadFolder;
	while (!strFolder.empty() && (strFolder.back() == '/' || strFolder.back() == '\\')) {
		strFolder.pop_back();
	}

	const size_t nTotalEntries = ZipArchiveEntryCount(strFolder);
	int nDone = 0;

	bool bRet = true;
	ZFile::EnumFolder(strFolder.c_str(), true, NULL, [&](bool bFolder, const string& strPath) {
		if (ZipIsArchiveCancelRequested()) {
			ZipMarkArchiveStoppedByUserCancel();
			ZLog::Print(">>> Zip: archive cancelled (between entries).\n");
			bRet = false;
			return true;
		}
		string strRelativePath = strPath.substr(strFolder.size() + 1);
		ZUtil::StringReplace(strRelativePath, "\\", "/");
		strRelativePath = "Payload/" + strRelativePath;

#ifdef _WIN32
		iconv ic;
		strRelativePath = ic.A2U8(strRelativePath);
#endif

		if (bFolder) {
			strRelativePath += "/";
			if (!_CreateFolderToZip(zf, strPath, strRelativePath, nZipLevel)) {
				bRet = false;
				return true;
			}
			nDone++;
			if (nTotalEntries > 0)
				ZipLogCompressUnified(nDone, (int)nTotalEntries, strRelativePath, 0, 0, false);
		} else {
			if (!_WriteFileToZip(zf, strPath, strRelativePath, nZipLevel, nDone, (int)nTotalEntries)) {
				bRet = false;
				return true;
			}
			nDone++;
			uint64_t dmb = 0, tmb = 0;
			zipStatFileMb(strPath, &dmb, &tmb);
			if (nTotalEntries > 0)
				ZipLogCompressUnified(nDone, (int)nTotalEntries, strRelativePath, dmb, tmb, false);
		}
		return false;
	});

	zipClose(zf, NULL);
	return bRet;
}

bool Zip::_ReadFileFromZip(
	void* hZip,
	const string& strPath,
	const string& strRootFolder,
	int entriesCompletedBefore,
	int entryTotal,
	const string& strRelativePathForLog,
	uint64_t uncompressedSize,
	bool withProgress)
{
	string strFile = strRootFolder + "/" + strPath;
	string strFolder = strFile;
	ZFile::PathRemoveFileSpec(strFolder);
	if (!ZFile::CreateFolder(strFolder.c_str())) {
		return false;
	}

	if (UNZ_OK != unzOpenCurrentFile(hZip)) {
		return false;
	}

	FILE* fp = NULL;
	_fopen64(fp, strFile.c_str(), "wb");
	if (NULL == fp) {
		unzCloseCurrentFile(hZip);
		return false;
	}

	static const uint64_t kLargeFileThreshold = 8ULL * 1024ULL * 1024ULL;
	static const uint64_t kHeartbeatBytes = 4ULL * 1024ULL * 1024ULL;
	static const uint64_t kFirstPulseBytes = 1ULL * 1024ULL * 1024ULL;

	bool bRet = true;
	uint32_t uBufSize = 512 * 1024;
	char* pbuff = (char*)malloc(uBufSize);
	uint64_t written_total = 0;
	uint64_t since_heartbeat = 0;
	bool bDidFirstPulse = false;
	if (NULL != pbuff) {
		int32_t nReaded = unzReadCurrentFile(hZip, pbuff, uBufSize);
		while (nReaded > 0) {
			if (nReaded != fwrite(pbuff, 1, nReaded, fp)) {
				bRet = false;
				break;
			}
			written_total += (uint64_t)nReaded;
			if (withProgress && entryTotal > 0) {
				const bool largeKnown = (uncompressedSize >= kLargeFileThreshold);
				const bool largeUnknown = (uncompressedSize == 0 && written_total >= kLargeFileThreshold);
				if (largeKnown || largeUnknown) {
					uint64_t totalMb = uncompressedSize / (1024ULL * 1024ULL);
					if (uncompressedSize > 0 && totalMb < 1)
						totalMb = 1;
					if (uncompressedSize == 0)
						totalMb = (written_total / (1024ULL * 1024ULL));
					if (totalMb < 1)
						totalMb = 1;
					uint64_t doneMb = written_total / (1024ULL * 1024ULL);
					if (!bDidFirstPulse && written_total >= kFirstPulseBytes) {
						ZipLogExtractUnified(entriesCompletedBefore, entryTotal, strRelativePathForLog, doneMb, totalMb, true);
						bDidFirstPulse = true;
						since_heartbeat = 0;
					} else {
						since_heartbeat += (uint64_t)nReaded;
						if (since_heartbeat >= kHeartbeatBytes) {
							ZipLogExtractUnified(entriesCompletedBefore, entryTotal, strRelativePathForLog, doneMb, totalMb, true);
							since_heartbeat = 0;
						}
					}
				}
			}
			nReaded = unzReadCurrentFile(hZip, pbuff, uBufSize);
		}
		free(pbuff);
	} else {
		bRet = false;
	}

	fclose(fp);
	unzCloseCurrentFile(hZip);
	return bRet;
}

static bool _IsPathSafe(const string& strPath)
{
	if (strPath.empty() || strPath[0] == '/') {
		return false;
	}

	size_t start = 0;
	size_t len = strPath.size();
	while (start < len) {
		size_t end = strPath.find('/', start);
		if (end == string::npos) {
			end = len;
		}
		size_t compLen = end - start;
		if (compLen == 2 && strPath[start] == '.' && strPath[start + 1] == '.') {
			return false;
		}
		start = end + 1;
	}
	return true;
}

bool Zip::_ExtractImpl(const char* zip_file, const char* output_folder, bool withProgress)
{
	unzFile uf = unzOpen64(zip_file);
	if (NULL == uf) {
		return false;
	}

	unz_global_info64 gi;
	if (UNZ_OK != unzGetGlobalInfo64(uf, &gi)) {
		unzClose(uf);
		return false;
	}

	int nEntryTotal = (gi.number_entry > (uint64_t)INT_MAX) ? INT_MAX : (int)gi.number_entry;
	int nDone = 0;

	bool bRet = true;
	unz_file_info64 fi = { 0 };
	char szPath[PATH_MAX] = { 0 };
	for (uint64_t i = 0; i < gi.number_entry; i++) {
		if (UNZ_OK != unzGetCurrentFileInfo64(uf, &fi, szPath, PATH_MAX, NULL, 0, NULL, 0)) {
			bRet = false;
			break;
		}

		string strPath = szPath;
		ZUtil::StringTrim(strPath);

#ifdef _WIN32
		iconv ic;
		strPath = ic.U82A(strPath);
#endif

		bool bFolder = false;
		if (!strPath.empty() && ('/' == strPath.back())) {
			bFolder = true;
			strPath.pop_back();
		}

		if (strPath.empty()) {
			if (i < gi.number_entry - 1) {
				if (UNZ_OK != unzGoToNextFile(uf)) {
					bRet = false;
					break;
				}
			}
			continue;
		}

		if (!_IsPathSafe(strPath)) {
			ZLog::ErrorV(">>> Zip: Skipping unsafe path: %s\n", strPath.c_str());
			if (i < gi.number_entry - 1) {
				if (UNZ_OK != unzGoToNextFile(uf)) {
					bRet = false;
					break;
				}
			}
			continue;
		}

		if (bFolder) {
			if (!ZFile::CreateFolderV("%s/%s", output_folder, strPath.c_str())) {
				bRet = false;
				break;
			}
			nDone++;
			if (withProgress && nEntryTotal > 0) {
				string strRelFolder = strPath + "/";
				ZipLogExtractUnified(nDone, nEntryTotal, strRelFolder, 0, 0, false);
			}
		} else {
			if (!_ReadFileFromZip(
				    uf,
				    strPath,
				    output_folder,
				    nDone,
				    nEntryTotal,
				    strPath,
				    (uint64_t)fi.uncompressed_size,
				    withProgress)) {
				bRet = false;
				break;
			}
			nDone++;
			if (withProgress && nEntryTotal > 0) {
				string strOutFile = string(output_folder) + "/" + strPath;
				uint64_t dmb = 0, tmb = 0;
				zipStatFileMb(strOutFile, &dmb, &tmb);
				ZipLogExtractUnified(nDone, nEntryTotal, strPath, dmb, tmb, false);
			}
		}

		if (i < gi.number_entry - 1) {
			if (UNZ_OK != unzGoToNextFile(uf)) {
				bRet = false;
				break;
			}
		}
	}

	unzClose(uf);
	return bRet;
}

bool Zip::Extract(const char* zip_file, const char* output_folder)
{
	ZFile::RemoveFolder(output_folder);
	if (!_ExtractImpl(zip_file, output_folder, false)) {
		ZFile::RemoveFolder(output_folder);
		return false;
	}
	return true;
}

bool Zip::ExtractWithProgress(const char* zip_file, const char* output_folder)
{
	ZFile::RemoveFolder(output_folder);
	if (!_ExtractImpl(zip_file, output_folder, true)) {
		ZFile::RemoveFolder(output_folder);
		return false;
	}
	return true;
}
