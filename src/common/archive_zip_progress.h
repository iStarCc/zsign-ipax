#pragma once

#include "common.h"

/** 与 Zip::Archive 相同的 EnumFolder 遍历，统计将写入 zip 的条目数（含目录项与文件）。 */
size_t ZipArchiveEntryCount(const string& strFolder);

/**
 * 单行压缩进度（中文前缀「正在压缩」/ 英文 "Compressing files"）：当前文件名、MB、总条目与整包百分比（大文件心跳与条目完成共用同一格式，总进度单调）。
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

/** 与 ZipLogCompressUnified 相同格式；中文前缀「正在解压」/ 英文 "Unzipping files"，用于解压 ZIP/IPA。 */
void ZipLogExtractUnified(
	int entriesCompletedBefore,
	int entryTotal,
	const string& strRelativePath,
	uint64_t fileDoneMb,
	uint64_t fileTotalMb,
	bool isPartialForCurrentFile);
