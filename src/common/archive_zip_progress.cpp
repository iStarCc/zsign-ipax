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

/** entriesCompletedBefore：已完成条目数；大文件心跳时指当前文件之前的条目数。isPartialForCurrentFile：正在写入当前文件（按 MB 比例计入总进度）。 */
void ZipLogCompressUnified(
	int entriesCompletedBefore,
	int entryTotal,
	const string& strRelativePath,
	uint64_t fileDoneMb,
	uint64_t fileTotalMb,
	bool isPartialForCurrentFile)
{
	if (entryTotal <= 0)
		return;

	const double w = 100.0 / (double)entryTotal;
	double overallD;
	if (isPartialForCurrentFile) {
		const double denom = (fileTotalMb > 0) ? (double)fileTotalMb : 1.0;
		const double frac = (double)fileDoneMb / denom;
		overallD = (double)entriesCompletedBefore * w + frac * w;
	} else {
		overallD = (double)entriesCompletedBefore * w;
	}
	int overall = (int)(overallD + 0.5);
	if (overall < 0)
		overall = 0;
	if (overall > 100)
		overall = 100;

	const int displayEntry = isPartialForCurrentFile ? (entriesCompletedBefore + 1) : entriesCompletedBefore;

	string pathCopy = strRelativePath;
	while (!pathCopy.empty() && (pathCopy.back() == '/' || pathCopy.back() == '\\')) {
		pathCopy.pop_back();
	}
	const char* base = pathCopy.empty() ? "" : ZUtil::GetBaseName(pathCopy.c_str());

	/* 单行：条目进度 + 当前文件 MB + 总百分比，便于 Swift 解析；basename 夹在中间避免整行被长文件名占满 */
	if (EnvPreferChinese()) {
		ZLog::PrintV("正在压缩（%d/%d）： %s（%llu/%llu MB）总计（%d%%）\n",
			displayEntry,
			entryTotal,
			base,
			(unsigned long long)fileDoneMb,
			(unsigned long long)fileTotalMb,
			overall);
	} else {
		ZLog::PrintV("Compressing files (%d/%d): %s (%llu/%llu MB) overall (%d%%)\n",
			displayEntry,
			entryTotal,
			base,
			(unsigned long long)fileDoneMb,
			(unsigned long long)fileTotalMb,
			overall);
	}
}

void ZipLogExtractUnified(
	int entriesCompletedBefore,
	int entryTotal,
	const string& strRelativePath,
	uint64_t fileDoneMb,
	uint64_t fileTotalMb,
	bool isPartialForCurrentFile)
{
	if (entryTotal <= 0)
		return;

	const double w = 100.0 / (double)entryTotal;
	double overallD;
	if (isPartialForCurrentFile) {
		const double denom = (fileTotalMb > 0) ? (double)fileTotalMb : 1.0;
		const double frac = (double)fileDoneMb / denom;
		overallD = (double)entriesCompletedBefore * w + frac * w;
	} else {
		overallD = (double)entriesCompletedBefore * w;
	}
	int overall = (int)(overallD + 0.5);
	if (overall < 0)
		overall = 0;
	if (overall > 100)
		overall = 100;

	const int displayEntry = isPartialForCurrentFile ? (entriesCompletedBefore + 1) : entriesCompletedBefore;

	string pathCopy = strRelativePath;
	while (!pathCopy.empty() && (pathCopy.back() == '/' || pathCopy.back() == '\\')) {
		pathCopy.pop_back();
	}
	const char* base = pathCopy.empty() ? "" : ZUtil::GetBaseName(pathCopy.c_str());

	if (EnvPreferChinese()) {
		ZLog::PrintV("正在解压（%d/%d）： %s（%llu/%llu MB）总计（%d%%）\n",
			displayEntry,
			entryTotal,
			base,
			(unsigned long long)fileDoneMb,
			(unsigned long long)fileTotalMb,
			overall);
	} else {
		ZLog::PrintV("Unzipping files (%d/%d): %s (%llu/%llu MB) overall (%d%%)\n",
			displayEntry,
			entryTotal,
			base,
			(unsigned long long)fileDoneMb,
			(unsigned long long)fileTotalMb,
			overall);
	}
}
