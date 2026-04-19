#pragma once

#include "common.h"

/** 与 Zip::Archive 相同的 EnumFolder 遍历，统计将写入 zip 的条目数（含目录项与文件）。 */
size_t ZipArchiveEntryCount(const string& strFolder);

/**
 * 单行压缩进度：当前文件名、当前文件 MB 进度、总条目序号/总数、整包总百分比（大文件心跳与条目完成共用同一格式，总进度单调）。
 * - isPartialForCurrentFile=true：大文件写入过程中心跳；entriesCompletedBefore 为当前文件之前已完成条目数。
 * - isPartialForCurrentFile=false：某条条目刚写完；entriesCompletedBefore 为含本条在内的已完成数；文件夹为 0/0 MB。
 */
void ZipLogCompressUnified(
	int entriesCompletedBefore,
	int entryTotal,
	const string& strRelativePath,
	uint64_t fileDoneMb,
	uint64_t fileTotalMb,
	bool isPartialForCurrentFile);
