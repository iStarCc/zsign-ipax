#include "archive_zip_progress.h"

#include <cstdlib>

static bool EnvPreferChinese()
{
	const char* z = getenv("ZSIGN_LANG");
	if (z && z[0] && ((z[0] == 'z' || z[0] == 'Z') && (z[1] == 'h' || z[1] == 'H')))
		return true;
	const char* lang = getenv("LANG");
	if (lang && strncmp(lang, "zh", 2) == 0)
		return true;
#ifdef _WIN32
	const char* ul = getenv("USER_LOCALE");
	if (ul && strncmp(ul, "zh", 2) == 0)
		return true;
#endif
	return false;
}

size_t ZipArchiveEntryCount(const string& strFolder)
{
	size_t n = 0;
	ZFile::EnumFolder(strFolder.c_str(), true, NULL, [&](bool bFolder, const string& strPath) {
		(void)bFolder;
		(void)strPath;
		n++;
		return false;
	});
	return n;
}

void ZipLogCompressProgress(int current, int total, const string& strRelativePath)
{
	int pct = 0;
	if (total > 0)
		pct = (int)((100LL * current) / total);
	string nameForLog = strRelativePath;
	while (!nameForLog.empty() && (nameForLog.back() == '/' || nameForLog.back() == '\\')) {
		nameForLog.pop_back();
	}
	string pathCopy = nameForLog;
	const char* base = pathCopy.empty() ? "" : ZUtil::GetBaseName(pathCopy.c_str());
	if (EnvPreferChinese())
		ZLog::PrintV("压缩中: %d/%d (%d%%) : %s\n", current, total, pct, base);
	else
		ZLog::PrintV("Compressing: %d/%d (%d%%) : %s\n", current, total, pct, base);
}

void ZipLogLargeFileCompressProgress(const string& strRelativePath, uint64_t bytesDone, uint64_t bytesTotal)
{
	if (bytesTotal == 0 || bytesDone > bytesTotal) {
		return;
	}
	string pathCopy = strRelativePath;
	const char* base = ZUtil::GetBaseName(pathCopy.c_str());
	uint64_t doneMb = bytesDone / (1024ULL * 1024ULL);
	uint64_t totalMb = bytesTotal / (1024ULL * 1024ULL);
	if (totalMb < 1) {
		totalMb = 1;
	}
	int pct = (int)((100ULL * bytesDone) / bytesTotal);
	if (EnvPreferChinese()) {
		ZLog::PrintV("压缩中(大文件): %s %llu/%llu MB (%d%%)\n", base, (unsigned long long)doneMb, (unsigned long long)totalMb, pct);
	} else {
		ZLog::PrintV("Compressing (large file): %s %llu/%llu MB (%d%%)\n", base, (unsigned long long)doneMb, (unsigned long long)totalMb, pct);
	}
}
