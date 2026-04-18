#pragma once

#include "common.h"

/** 与 Zip::Archive 相同的 EnumFolder 遍历，统计将写入 zip 的条目数（含目录项与文件）。 */
size_t ZipArchiveEntryCount(const string& strFolder);

/** 输出一行压缩进度；末尾为当前条目 basename，供前端展示「644/4918：文件名」。 */
void ZipLogCompressProgress(int current, int total, const string& strRelativePath);

/** 单个大文件写入 zip 时周期性输出（避免长时间无换行导致管道另一端 readline 阻塞）。 */
void ZipLogLargeFileCompressProgress(const string& strRelativePath, uint64_t bytesDone, uint64_t bytesTotal);
