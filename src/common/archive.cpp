#include "archive.h"
#include "archive_zip_progress.h"

#include "third-party/minizip/zip.h"
#include "third-party/minizip/unzip.h"

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

bool Zip::_WriteFileToZip(void* hZip, const string& strFile, const string& strRelativePath, int zip_level)
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
	if (ZIP_OK != zipOpenNewFileInZip3_64(hZip, strRelativePath.c_str(), &zi, NULL, 0, NULL, 0, NULL, Z_DEFLATED, zip_level, 0, -MAX_WBITS, DEF_MEM_LEVEL, 0, NULL, 0, 0)) {
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
		if (zipWriteInFileInZip(hZip, buffer, (uint32_t)bytes_read) < 0) {
			bRet = false;
			break;
		}
		written_total += (uint64_t)bytes_read;
		if (uFileSize >= kLargeFileThreshold) {
			if (!bDidFirstPulse && written_total >= kFirstPulseBytes) {
				ZipLogLargeFileCompressProgress(strRelativePath, written_total, uFileSize);
				bDidFirstPulse = true;
				since_heartbeat = 0;
			} else {
				since_heartbeat += (uint64_t)bytes_read;
				if (since_heartbeat >= kHeartbeatBytes) {
					ZipLogLargeFileCompressProgress(strRelativePath, written_total, uFileSize);
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
	if (ZIP_OK != zipOpenNewFileInZip3_64(hZip, strRelativePath.c_str(), &zi, NULL, 0, NULL, 0, NULL, Z_DEFLATED, zip_level, 0, -MAX_WBITS, DEF_MEM_LEVEL, 0, NULL, 0, 0)) {
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

	ZFile::RemoveMacPackagingJunk(strFolder.c_str());

    zipFile zf = zipOpen64(strZipFile.c_str(), 0);
    if (!zf) {
		ZLog::ErrorV(">>> Zip: Failed to create zip file: %s\n", strZipFile.c_str());
        return false;
    }

	const size_t nTotalEntries = ZipArchiveEntryCount(strFolder);
	int nDone = 0;

	bool bRet = true;
	ZFile::EnumFolder(strFolder.c_str(), true, NULL, [&](bool bFolder, const string& strPath) {
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
		} else {
			if (!_WriteFileToZip(zf, strPath, strRelativePath, nZipLevel)) {
				bRet = false;
				return true;
			}
		}
		nDone++;
		if (nTotalEntries > 0)
			ZipLogCompressProgress(nDone, (int)nTotalEntries, strRelativePath);
		return false;
	});

    zipClose(zf, NULL);
	return bRet;
}

bool Zip::_EnumZipItems(const char* zip_file, enum_zip_items_callback callback)
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

		if (NULL != callback) {
			if (!callback(uf, bFolder, strPath)) {
				bRet = false;
				break;
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

bool Zip::_ReadFileFromZip(void* hZip, const string& strPath, const string& strRootFolder)
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

	bool bRet = true;
	uint32_t uBufSize = 512 * 1024;
	char* pbuff = (char*)malloc(uBufSize);
	if (NULL != pbuff) {
		int32_t nReaded = unzReadCurrentFile(hZip, pbuff, uBufSize);
		while (nReaded > 0) {
			if (nReaded != fwrite(pbuff, 1, nReaded, fp)) {
				bRet = false;
				break;
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

bool Zip::_Extract(const char* zip_file, const char* output_folder)
{
	return _EnumZipItems(zip_file, [&](unzFile uFile, bool bFolder, const string& strPath) {
		if (!_IsPathSafe(strPath)) {
			ZLog::ErrorV(">>> Zip: Skipping unsafe path: %s\n", strPath.c_str());
			return true;
		}
		if (bFolder) {
			if (!ZFile::CreateFolderV("%s/%s", output_folder, strPath.c_str())) {
				return false;
			}
		} else {
			if (!_ReadFileFromZip(uFile, strPath, output_folder)) {
				return false;
			}
		}
		return true;
	});
}

bool Zip::Extract(const char* zip_file, const char* output_folder)
{
	ZFile::RemoveFolder(output_folder);
	if (!_Extract(zip_file, output_folder)) {
		ZFile::RemoveFolder(output_folder);
		return false;
	}
	return true;
}
