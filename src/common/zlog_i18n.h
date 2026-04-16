#pragma once

#include <string>

/** 在 ZLog 输出层对文本做「➤」前缀与可选中文翻译（见 zlog_i18n.cpp）。 */
namespace ZLogI18n
{
	/** 由 ZLog::_Print 调用：处理后的 UTF-8 文本写入 out */
	void Apply(const char* szLog, std::string& out);
}
