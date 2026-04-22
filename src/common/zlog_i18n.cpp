#include "zlog_i18n.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <vector>

#if defined(_WIN32)
#include <string.h>
static int ZsignStrNCaseCmp(const char* a, const char* b, size_t n)
{
	return _strnicmp(a, b, (int)n);
}
#else
#include <strings.h>
static int ZsignStrNCaseCmp(const char* a, const char* b, size_t n)
{
	return strncasecmp(a, b, n);
}
#endif

namespace {

const char* kArrowSpace = "\xe2\x9e\xa4 "; /* U+27A4 + ASCII space */

static bool EnvLangIsChinese()
{
	const char* z = getenv("ZSIGN_LANG");
	if (z && *z) {
		if ((z[0] == 'z' || z[0] == 'Z') && (z[1] == 'h' || z[1] == 'H'))
			return true;
		if (z[0] == 'C' || z[0] == 'c') {
			if (z[1] == 'N' || z[1] == 'n')
				return false; /* C / POSIX */
		}
		if (ZsignStrNCaseCmp(z, "en", 2) == 0)
			return false;
	}
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

static void ReplaceAll(std::string& s, const char* from, const std::string& to)
{
	const size_t flen = strlen(from);
	if (flen == 0)
		return;
	for (size_t pos = 0; (pos = s.find(from, pos)) != std::string::npos;) {
		s.replace(pos, flen, to);
		pos += to.size();
	}
}

/** 英文片段 -> 中文；按源串长度降序排序后依次替换 */
static const char* gPairs[][2] = {
	/* zsign 帮助与说明 */
	{") is a codesign alternative for iOS12+ on macOS, Linux and Windows. \nVisit https://github.com/zhlynn/zsign for more information.\n\n",
		") 是适用于 iOS12+ 的 codesign 替代工具，支持 macOS、Linux 与 Windows。\n详见 https://github.com/zhlynn/zsign\n\n"},
	{"Usage: zsign [-options] [-k privkey.pem] [-m dev.prov] [-o output.ipa] file|folder\n",
		"用法：zsign [-选项] [-k 私钥.pem] [-m 描述文件.prov] [-o 输出.ipa] 文件|目录\n"},
	{"options:\n", "选项：\n"},
	{"-k, --pkey\t\tPath to private key or p12 file. (PEM or DER format)\n",
		"-k, --pkey\t\t私钥或 p12 路径（PEM 或 DER）\n"},
	{"-m, --prov\t\tPath to mobile provisioning profile.\n", "-m, --prov\t\t移动设备描述文件路径。\n"},
	{"-c, --cert\t\tPath to certificate file. (PEM or DER format)\n", "-c, --cert\t\t证书路径（PEM 或 DER）\n"},
	{"-a, --adhoc\t\tPerform ad-hoc signature only.\n", "-a, --adhoc\t\t仅执行临时签名（Ad-hoc）。\n"},
	{"-d, --debug\t\tGenerate debug output files. (.zsign_debug folder)\n", "-d, --debug\t\t生成调试输出（.zsign_debug 目录）。\n"},
	{"-f, --force\t\tForce sign without cache when signing folder.\n", "-f, --force\t\t签名目录时不使用缓存。\n"},
	{"-o, --output\t\tPath to output ipa file.\n", "-o, --output\t\t输出 ipa 路径。\n"},
	{"-p, --password\t\tPassword for private key or p12 file.\n", "-p, --password\t\t私钥或 p12 密码。\n"},
	{"-b, --bundle_id\t\tNew bundle id to change.\n", "-b, --bundle_id\t\t要修改的 Bundle ID。\n"},
	{"-n, --bundle_name\tNew bundle name to change.\n", "-n, --bundle_name\t要修改的 Bundle 显示名。\n"},
	{"-r, --bundle_version\tSet CFBundleShortVersionString only (CFBundleVersion unchanged).\n",
		"-r, --bundle_version\t仅设置 CFBundleShortVersionString（显示版本），不修改 CFBundleVersion（构建号）。\n"},
	{"-e, --entitlements\tNew entitlements to change.\n", "-e, --entitlements\t要修改的 entitlements。\n"},
	{"-z, --zip_level\t\tCompressed level when output the ipa file. (0-9)\n",
		"-z, --zip_level\t\t输出 ipa 时的压缩级别（0-9）。\n"},
	{"-l, --dylib\t\tPath to inject dylib file. Use -l multiple time to inject multiple dylib files at once.\n",
		"-l, --dylib\t\t要注入的 dylib 路径；可多次指定以注入多个。\n"},
	{"-D, --rm_dylib\t\tName of dylib to remove. Use -D multiple times to remove multiple dylibs at once.\n",
		"-D, --rm_dylib\t\t要移除的 dylib 名；可多次指定。\n"},
	{"-w, --weak\t\tInject dylib as LC_LOAD_WEAK_DYLIB.\n", "-w, --weak\t\t以 LC_LOAD_WEAK_DYLIB 注入。\n"},
	{"-i, --install\t\tInstall ipa file using ideviceinstaller command for test.\n",
		"-i, --install\t\t使用 ideviceinstaller 安装 ipa 用于测试。\n"},
	{"-t, --temp_folder\tPath to temporary folder for intermediate files.\n", "-t, --temp_folder\t中间文件临时目录。\n"},
	{"-2, --sha256_only\tSerialize a single code directory that uses SHA256.\n",
		"-2, --sha256_only\t仅序列化使用 SHA256 的单个代码目录。\n"},
	{"-C, --check\t\tCheck certificate validity and OCSP revocation status.\n",
		"-C, --check\t\t检查证书有效性与 OCSP 吊销状态。\n"},
	{"-q, --quiet\t\tQuiet operation.\n", "-q, --quiet\t\t静默模式。\n"},
	{"-x, --metadata\t\tExtract metadata and icon to the specified directory.\n",
		"-x, --metadata\t\t将元数据与图标导出到指定目录。\n"},
	{"-R, --rm_provision\tRemove mobileprovision file after signing.\n", "-R, --rm_provision\t签名后删除 embedded.mobileprovision。\n"},
	{"-S, --enable_docs\tEnable UISupportsDocumentBrowser and UIFileSharingEnabled.\n",
		"-S, --enable_docs\t开启文档浏览与文件共享相关能力。\n"},
	{"-M, --min_version\tSet MinimumOSVersion in Info.plist.\n", "-M, --min_version\t设置 Info.plist 中的 MinimumOSVersion。\n"},
	{"-E, --rm_extensions\tRemove all app extensions (PlugIns/Extensions).\n", "-E, --rm_extensions\t移除所有 App 扩展（PlugIns/Extensions）。\n"},
	{"-W, --rm_watch\t\tRemove watch app from the bundle.\n", "-W, --rm_watch\t\t从包中移除 Watch 应用。\n"},
	{"-U, --rm_uisd\t\tRemove UISupportedDevices from Info.plist.\n", "-U, --rm_uisd\t\t从 Info.plist 移除 UISupportedDevices。\n"},
	{"-v, --version\t\tShows version.\n", "-v, --version\t\t显示版本。\n"},
	{"-h, --help\t\tShows help (this message).\n", "-h, --help\t\t显示本帮助。\n"},

	/* 运行过程 */
	{"Option:\t-", "选项：\t-"},
	{"Argument:\t", "参数：\t"},
	{"Invalid temp folder! ", "无效的临时目录："},
	{"Invalid path! ", "无效路径："},
	{"Invalid zip level! Please input 0 - 9.\n", "无效的 zip 级别！请输入 0 - 9。\n"},
	{"Dylib file not found! ", "未找到 dylib 文件："},
	{"Invalid dylib file! Not a valid Mach-O format. ", "无效的 dylib 文件（非 Mach-O）："},
	{"Invalid mach-o file! ", "无效的 Mach-O 文件："},
	{"Signing:\t", "正在签名：\t"},
	{" (Ad-hoc)", "（临时签名）"},
	{"Use -o option to specify the output file.\n", "请使用 -o 指定输出文件。\n"},
	{"Unzip:\t", "解压：\t"},
	{" -> ", " → "},
	{"Unzip failed!\n", "解压失败！\n"},
	{"Extract: input must be an .ipa file.\n", "Extract：输入必须是 .ipa 文件。\n"},
	{"Extract: not a valid IPA (missing Payload/xxx.app).\n", "Extract：不是有效的 IPA（缺少 Payload/xxx.app）。\n"},
	{"Extract: output path must be a directory.\n", "Extract：输出路径必须是目录。\n"},
	{"Extract: output directory could not be created.\n", "Extract：无法创建输出目录。\n"},
	{"Extract: too many Payload backups.\n", "Extract：Payload 备份过多。\n"},
	{"Failed to init provision: ", "初始化描述文件失败："},
	{"Archiving: \t", "正在打包：\t"},
	{"Archive failed!\n", "打包失败！\n"},
	{"Can't find payload directory!\n", "未找到 Payload 目录！\n"},
	{"Done.", "完成。"},
	{"Signed OK!", "签名成功！"},
	{"Signed Failed!", "签名失败！"},
	{"Unzip OK!", "解压成功！"},
	{"Archive OK!", "打包成功！"},

	/* bundle */
	{"Removed embedded.mobileprovision\n", "已移除 embedded.mobileprovision\n"},
	{"SignFile: \t", "签名文件：\t"},
	{"Warning: Skipping non-Mach-O file: \t", "警告：跳过非 Mach-O 文件：\t"},
	{"Can't get BundleID or BundleExecute or Info.plist SHASum in Info.plist! ",
		"无法从 Info.plist 读取 BundleID、可执行文件或 Info.plist 校验和："},
	{"SignFolder: ", "签名目录："},
	{"Can't parse BundleExecute file! ", "无法解析可执行文件："},
	{"Create CodeResources failed! ", "创建 CodeResources 失败："},
	{"Can't get changed file SHASum! ", "无法计算已修改文件的 SHA 校验和："},
	{"Changed file: ", "已变更文件："},
	{"Writing CodeResources failed! ", "写入 CodeResources 失败："},
	{"Can't write embedded.mobileprovision!\n", "无法写入 embedded.mobileprovision！\n"},
	{"Can't find Plugin's Info.plist! ", "未找到插件的 Info.plist："},
	{"BundleId: \t", "BundleId：\t"},
	{", Plugin\n", "，插件\n"},
	{", Plugin-WKCompanionAppBundleIdentifier\n", "，Plugin-WKCompanionAppBundleIdentifier\n"},
	{", NSExtension-NSExtensionAttributes-WKAppBundleIdentifier\n", "，NSExtension-NSExtensionAttributes-WKAppBundleIdentifier\n"},
	{"Can't find app's Info.plist! ", "未找到主应用 Info.plist："},
	{"BundleName: ", "Bundle 名称："},
	{"CFBundleShortVersionString: ", "显示版本（CFBundleShortVersionString）："},
	{"Enabled documents support\n", "已启用文档能力\n"},
	{"MinimumOSVersion: ", "最低系统版本："},
	{"Removed UISupportedDevices\n", "已移除 UISupportedDevices\n"},
	{"Can't find app folder! ", "未找到应用目录："},
	{"Can't get BundleID, BundleVersion, or BundleExecute in Info.plist! ",
		"无法从 Info.plist 读取 BundleID、版本或可执行文件名："},
	{"AppName: \t", "应用名：\t"},
	{"Version: \t", "版本：\t"},
	{"TeamId: \t", "团队 ID：\t"},
	{"SubjectCN: \t", "证书主题 CN：\t"},
	{"ReadCache: \tYES\n", "使用缓存：\t是\n"},
	{"ReadCache: \tNO\n", "使用缓存：\t否\n"},
	{"Signed:\tYes\n\n", "已签名：\t是\n\n"},
	{"Signed:\tYes\n", "已签名：\t是\n"},
	{"Signed:\tNo\n\n", "已签名：\t否\n\n"},
	{"Signed:\tNo\n", "已签名：\t否\n"},

	/* archo / macho */
	{"File is not signed.\n", "文件未签名。\n"},
	{"File is signed.\n", "文件已签名。\n"},
	{"MachO Info: \n", "Mach-O 信息：\n"},
	{"FileType: \t", "文件类型：\t"},
	{"TotalSize: \t", "总大小：\t"},
	{"Platform: \t", "平台：\t"},
	{"CPUArch: \t", "CPU 架构：\t"},
	{"CPUType: \t", "CPU 类型：\t"},
	{"CPUSubType: \t", "CPU 子类型：\t"},
	{"BigEndian: \t", "大端：\t"},
	{"Encrypted: \t", "加密：\t"},
	{"CommandCount: \t", "Load Command 数：\t"},
	{"CodeLength: \t", "代码长度：\t"},
	{"SignLength: \t", "签名长度：\t"},
	{"SpareLength: \t", "剩余长度：\t"},
	{"MIN_IPHONEOS: \t", "MIN_IPHONEOS：\t"},
	{"LC_RPATH: \t", "LC_RPATH：\t"},
	{"LC_LOAD_DYLIB: \n", "LC_LOAD_DYLIB：\n"},
	{"LC_LOAD_WEAK_DYLIB: \n", "LC_LOAD_WEAK_DYLIB：\n"},
	{" (weak)\n", "（弱链接）\n"},
	{"Embedded Info.plist: \n", "内嵌 Info.plist：\n"},
	{"length: \t", "长度：\t"},
	{"content: \t", "内容：\t"},
	{"Can't find CodeSignature segment!\n", "未找到 CodeSignature 段！\n"},
	{"Build CodeSignature failed!\n", "构建 CodeSignature 失败！\n"},
	{"No enough CodeSignature space (now: ", "CodeSignature 空间不足（当前："},
	{", need: ", "，需要："},
	{").\n", "）。\n"},
	{"Can't find free space of LoadCommands for CodeSignature!\n", "LoadCommands 中无足够空间写入 CodeSignature！\n"},
	{"Can't find free space of LoadCommands for LC_LOAD_DYLIB or LC_LOAD_WEAK_DYLIB!\n",
		"LoadCommands 中无足够空间添加 LC_LOAD_DYLIB 或 LC_LOAD_WEAK_DYLIB！\n"},
	{"\tclear\n", "\t已清除\n"},
	{"Invalid arch file in fat mach-o file!\n", "Fat Mach-O 中存在无效架构！\n"},
	{"Invalid mach-o file!\n", "无效的 Mach-O 文件！\n"},
	{"Invalid mach-o file (magic: 0x", "无效的 Mach-O 文件（魔数：0x"},
	{"CodeSign write(munmap) failed! Error: ", "CodeSign 写入（munmap）失败："},
	{"Realloc CodeSignature space... \n", "重新分配 CodeSignature 空间…\n"},
	{"InjectDylib: ", "注入 dylib："},
	{"Failed!\n", "失败！\n"},
	{"Success!\n", "成功！\n"},

	/* signing.cpp 调试字段 */
	{"type: \t\t", "类型：\t\t"},
	{"offset: \t", "偏移：\t"},
	{"magic: \t\t", "魔数：\t\t"},
	{"length: \t", "长度：\t"},
	{"entitlements: \n", "entitlements：\n"},
	{"version: \t", "版本：\t"},
	{"flags: \t\t", "标志：\t\t"},
	{"hashOffset: \t", "hashOffset：\t"},
	{"identOffset: \t", "identOffset：\t"},
	{"nSpecialSlots: \t", "nSpecialSlots：\t"},
	{"nCodeSlots: \t", "nCodeSlots：\t"},
	{"codeLimit: \t", "codeLimit：\t"},
	{"hashSize: \t", "hashSize：\t"},
	{"hashType: \t", "hashType：\t"},
	{"spare1: \t", "spare1：\t"},
	{"pageSize: \t", "pageSize：\t"},
	{"spare2: \t", "spare2：\t"},
	{"scatterOffset: \t", "scatterOffset：\t"},
	{"teamOffset: \t", "teamOffset：\t"},
	{"spare3: \t", "spare3：\t"},
	{"codeLimit64: \t", "codeLimit64：\t"},
	{"execSegBase: \t", "execSegBase：\t"},
	{"execSegLimit: \t", "execSegLimit：\t"},
	{"execSegFlags: \t", "execSegFlags：\t"},
	{"identifier: \t", "identifier：\t"},
	{"teamid: \t", "teamid：\t"},
	{"SpecialSlots:\n", "SpecialSlots：\n"},
	{"CodeSlots:\n", "CodeSlots：\n"},
	{"CodeSlots: \tomitted. (use -d option for details)\n", "CodeSlots：\t已省略（使用 -d 查看详情）\n"},
	{"Certificates: \n", "证书链：\n"},
	{"SignedAttrs: \n", "SignedAttrs：\n"},
	{"ContentType: \t", "ContentType：\t"},
	{"SigningTime: \t", "SigningTime：\t"},
	{"MsgDigest: \t", "MsgDigest：\t"},
	{"CDHashes: \t", "CDHashes：\t"},
	{"CDHashes2: \t", "CDHashes2：\t"},
	{"UnknownAttr: \t", "UnknownAttr：\t"},
	{"CodeSignature Segment: \n", "CodeSignature 段：\n"},
	{"slots: \t\t", "槽位数：\t\t"},

	/* certcheck */
	{"Signed:\t", "已签名：\t"},
	{"Name:\t", "名称：\t"},
	{"Type:\t", "类型：\t"},
	{"Org:\t", "组织：\t"},
	{"Team:\t", "团队：\t"},
	{"Serial:\t", "序列号：\t"},
	{"Issued:\t", "签发：\t"},
	{"Expires:\t", "到期：\t"},
	{" (EXPIRED ", "（已过期 "},
	{" days ago)\n", " 天前）\n"},
	{" days remaining!)\n", " 天，即将过期！）\n"},
	{" days remaining)\n", " 天剩余）\n"},
	{"Algorithm:\t", "算法：\t"},
	{"Issuer:\t", "颁发者：\t"},
	{"OCSP:\tValid (ocsp.apple.com)\n", "OCSP：\t有效（ocsp.apple.com）\n"},
	{"OCSP:\tREVOKED\n", "OCSP：\t已吊销\n"},
	{"Revoked:\t", "吊销时间：\t"},
	{"OCSP:\tUnknown\n", "OCSP：\t未知\n"},
	{"OCSP:\tError\n", "OCSP：\t错误\n"},
	{"Detail:\t", "详情：\t"},
	{"Cannot read file: ", "无法读取文件："},
	{"Unknown file type: ", "未知文件类型："},
	{"Check:\t", "检查：\t"},
	{"Failed to load certificate from ", "从以下路径加载证书失败："},
	{"OCSP:\tSkipped (non-WWDR issuer)\n", "OCSP：\t已跳过（非 WWDR 颁发者）\n"},

	/* archive / fs / util / metadata */
	{"Zip: Failed to open file: ", "Zip：无法打开文件："},
	{"Zip: Failed to add file to zip: ", "Zip：无法添加文件："},
	{"Zip: Failed to create folder to zip: ", "Zip：无法创建目录项："},
	{"Zip: Invalid compression level: ", "Zip：无效的压缩级别："},
	{"Zip: Failed to create zip file: ", "Zip：无法创建 zip 文件："},
	{"Zip: Skipping unsafe path: ", "Zip：跳过不安全路径："},
	{"WriteFile: Failed in fopen! ", "WriteFile：fopen 失败："},
	{"AppendFile: Failed in fopen! ", "AppendFile：fopen 失败："},
	{"SystemExec: \"", "SystemExec：\""},
	{"\", error!\n", "\"，执行失败！\n"},
	{"GetMetadata: Can't read ", "GetMetadata：无法读取 "},
	{"GetMetadata: Can't write ", "GetMetadata：无法写入 "},
	{"Metadata:\t", "元数据：\t"},

	/* openssl */
	{"NCONF_new failed\n", "NCONF_new 失败\n"},
	{"NCONF_load_bio failed ", "NCONF_load_bio 失败 "},
	{"NCONF_get_string failed\n", "NCONF_get_string 失败\n"},
	{"Unknown issuer hash!\n", "未知的颁发者哈希！\n"},
	{"Can't read entitlements file!\n", "无法读取 entitlements 文件！\n"},
	{"Can't find provision file!\n", "未找到描述文件！\n"},
	{"Can't find TeamId!\n", "未找到 TeamId！\n"},
	{"Can't load p12 or private key file. Please input the correct file and password!\n",
		"无法加载 p12 或私钥，请检查文件与密码！\n"},
	{"Can't find paired certificate and private key!\n", "未找到匹配的证书与私钥！\n"},
	{"Can't find paired certificate subject common name!\n", "未找到匹配证书的 Subject CN！\n"},
};

static void ApplyChinese(std::string& s)
{
	std::vector<std::pair<std::string, std::string>> table;
	const size_t n = sizeof(gPairs) / sizeof(gPairs[0]);
	table.reserve(n);
	for (size_t i = 0; i < n; ++i)
		table.emplace_back(std::string(gPairs[i][0]), std::string(gPairs[i][1]));
	std::sort(table.begin(), table.end(),
		[](const std::pair<std::string, std::string>& a, const std::pair<std::string, std::string>& b) {
			return a.first.size() > b.first.size();
		});
	for (const auto& p : table)
		ReplaceAll(s, p.first.c_str(), p.second);
}

static bool Utf8StartsWithArrow(const char* p, size_t len)
{
	static const unsigned char kAr[] = { 0xe2, 0x9e, 0xa4 };
	return len >= 3 && (unsigned char)p[0] == kAr[0] && (unsigned char)p[1] == kAr[1] &&
		(unsigned char)p[2] == kAr[2];
}

/** 为尚未带 “➤” 的行统一加前缀（空行、纯分隔线、以 Tab 开头的续行不加） */
static void EnsureLineArrowPrefix(std::string& s)
{
	if (s.empty())
		return;
	std::string out;
	out.reserve(s.size() + 64);
	size_t pos = 0;
	while (pos < s.size()) {
		size_t nl = s.find('\n', pos);
		if (nl == std::string::npos)
			nl = s.size();
		size_t lineLen = nl - pos;
		while (lineLen > 0 && s[pos + lineLen - 1] == '\r')
			lineLen--;
		const char* lp = s.c_str() + pos;
		bool blank = true;
		for (size_t i = 0; i < lineLen; i++) {
			char c = lp[i];
			if (c != ' ' && c != '\t') {
				blank = false;
				break;
			}
		}
		size_t dashRun = 0;
		for (size_t i = 0; i < lineLen && lp[i] == '-'; i++)
			dashRun++;
		bool sepDash = (dashRun >= 12 && dashRun == lineLen);
		bool skip = blank || sepDash || (lineLen > 0 && lp[0] == '\t') || Utf8StartsWithArrow(lp, lineLen);
		if (!skip)
			out.append(kArrowSpace);
		out.append(lp, lineLen);
		if (nl < s.size())
			out.push_back('\n');
		pos = nl + 1;
	}
	s.swap(out);
}

} // namespace

void ZLogI18n::Apply(const char* szLog, std::string& out)
{
	if (!szLog) {
		out.clear();
		return;
	}
	out.assign(szLog);

	/* 统一 “>>>” 为 “➤ ”；特殊缩进 */
	ReplaceAll(out, ">>>\t\t ", std::string(kArrowSpace) + "\t\t ");
	ReplaceAll(out, ">>> ", kArrowSpace);
	ReplaceAll(out, "\n  > ", std::string("\n") + kArrowSpace);

	if (EnvLangIsChinese())
		ApplyChinese(out);

	EnsureLineArrowPrefix(out);
}
