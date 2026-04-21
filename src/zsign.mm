//
//  zsign.mm
//  Zsign Swift 封装 — 基于 master（命令行版）核心库
//

#include "zsign.hpp"
#include "common.h"
#include "openssl.h"
#include "macho.h"
#include "bundle.h"
#include "timer.h"
#include "log.h"
#include "common/archive.h"

#include <cstdlib>
#include <string>

namespace {

struct ZsignLangScope {
	bool on_;
	std::string prev_;
	bool hadPrev_;
	explicit ZsignLangScope(bool zh) : on_(zh), hadPrev_(false) {
		if (!on_) {
			return;
		}
		const char* p = getenv("ZSIGN_LANG");
		if (p) {
			prev_.assign(p);
			hadPrev_ = true;
		}
		setenv("ZSIGN_LANG", "zh", 1);
	}
	~ZsignLangScope() {
		if (!on_) {
			return;
		}
		if (!hadPrev_) {
			unsetenv("ZSIGN_LANG");
		} else {
			setenv("ZSIGN_LANG", prev_.c_str(), 1);
		}
	}
};

} // namespace

#include <openssl/ocsp.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/asn1.h>

extern "C" {

bool CheckIfSigned(NSString *filePath) {
	ZTimer gtimer;
	@autoreleasepool {
		std::string filePathStr = [filePath UTF8String];

		ZMachO machO;
		bool initSuccess = machO.Init(filePathStr.c_str());
		if (!initSuccess) {
			gtimer.Print(">>> Failed to initialize ZMachO.");
			return false;
		}

		bool success = machO.CheckSignature();

		machO.Free();

		if (success) {
			gtimer.Print(">>> MachO is signed!");
			return true;
		} else {
			gtimer.Print(">>> MachO is not signed.");
			return false;
		}
	}
}

bool InjectDyLib(NSString *filePath, NSString *dylibPath, bool weakInject) {
	ZTimer gtimer;
	@autoreleasepool {
		std::string filePathStr = [filePath UTF8String];
		std::string dylibPathStr = [dylibPath UTF8String];

		ZMachO machO;
		bool initSuccess = machO.Init(filePathStr.c_str());
		if (!initSuccess) {
			gtimer.Print(">>> Failed to initialize ZMachO.");
			return false;
		}

		bool success = machO.InjectDylib(weakInject, dylibPathStr.c_str());

		machO.Free();

		if (success) {
			gtimer.Print(">>> Dylib injected successfully!");
			return true;
		} else {
			gtimer.Print(">>> Failed to inject dylib.");
			return false;
		}
	}
}

bool UninstallDylibs(NSString *filePath, NSArray<NSString *> *dylibPathsArray) {
	ZTimer gtimer;
	@autoreleasepool {
		std::string filePathStr = [filePath UTF8String];
		std::set<std::string> dylibsToRemove;

		for (NSString *dylibPath in dylibPathsArray) {
			dylibsToRemove.insert([dylibPath UTF8String]);
		}

		ZMachO machO;
		bool initSuccess = machO.Init(filePathStr.c_str());
		if (!initSuccess) {
			gtimer.Print(">>> Failed to initialize ZMachO.");
			return false;
		}

		machO.RemoveDylibs(dylibsToRemove);

		machO.Free();

		gtimer.Print(">>> Dylibs uninstalled successfully!");
		return true;
	}
}

NSArray<NSString *> *ListDylibs(NSString *filePath) {
	ZTimer gtimer;
	@autoreleasepool {
		NSMutableArray<NSString *> *dylibPathsArray = [NSMutableArray array];

		std::string filePathStr = [filePath UTF8String];

		ZMachO machO;
		bool initSuccess = machO.Init(filePathStr.c_str());
		if (!initSuccess) {
			gtimer.Print(">>> Failed to initialize ZMachO.");
			return nil;
		}

		std::vector<std::string> dylibPaths = machO.ListDylibs();

		if (!dylibPaths.empty()) {
			gtimer.Print(">>> List of dylibs in the Mach-O file:");
			for (const std::string &dylibPath : dylibPaths) {
				NSString *dylibPathStr = [NSString stringWithUTF8String:dylibPath.c_str()];
				[dylibPathsArray addObject:dylibPathStr];
			}
		} else {
			gtimer.Print(">>> No dylibs found in the Mach-O file.");
		}

		machO.Free();

		return [dylibPathsArray copy];
	}
}

bool ChangeDylibPath(NSString *filePath, NSString *oldPath, NSString *newPath) {
	ZTimer gtimer;
	@autoreleasepool {
		std::string filePathStr = [filePath UTF8String];
		std::string oldPathStr = [oldPath UTF8String];
		std::string newPathStr = [newPath UTF8String];

		ZMachO machO;
		bool initSuccess = machO.Init(filePathStr.c_str());
		if (!initSuccess) {
			gtimer.Print(">>> Failed to initialize ZMachO.");
			return false;
		}

		bool success = machO.ChangeDylibPath(oldPathStr.c_str(), newPathStr.c_str());

		machO.Free();

		if (success) {
			gtimer.Print(">>> Dylib path changed successfully!");
			return true;
		} else {
			gtimer.Print(">>> Failed to change dylib path.");
			return false;
		}
	}
}

int zsign(
	NSString *app,
	NSString *prov,
	NSString *key,
	NSString *pass,
	NSString *entitlement,
	NSString *bundleid,
	NSString *displayname,
	NSString *bundleversion,
	bool adhoc,
	bool excludeprovion,
	bool removeUISupportedDevices,
	bool removeWatchApp,
	bool enableDocuments,
	NSString * _Nullable minOSVersion,
	bool removeExtensions,
	bool zh,
	void (^ _Nullable completionHandler)(BOOL success, NSError * _Nullable error)
) {
	ZsignLangScope langScope(zh);

	ZTimer atimer;
	ZTimer gtimer;

	bool bForce = true;
	bool bWeakInject = false;
	bool bAdhoc = adhoc;
	bool bSHA256Only = false;

	string strCertFile;
	string strPKeyFile;
	string strProvFile;
	string strPassword;
	string strBundleId;
	string strBundleVersion;
	string strDisplayName;
	string strEntitleFile;
	vector<string> arrDylibFiles;
	vector<string> arrRemoveDylibNames;

	strPKeyFile = [key cStringUsingEncoding:NSUTF8StringEncoding];
	strProvFile = [prov cStringUsingEncoding:NSUTF8StringEncoding];
	strPassword = [pass cStringUsingEncoding:NSUTF8StringEncoding];
	strEntitleFile = [entitlement cStringUsingEncoding:NSUTF8StringEncoding];

	strBundleId = [bundleid cStringUsingEncoding:NSUTF8StringEncoding];
	strDisplayName = [displayname cStringUsingEncoding:NSUTF8StringEncoding];
	strBundleVersion = [bundleversion cStringUsingEncoding:NSUTF8StringEncoding];

	string strPath = [app cStringUsingEncoding:NSUTF8StringEncoding];
	if (!ZFile::IsFileExists(strPath.c_str())) {
		ZLog::ErrorV(">>> Invalid path! %s\n", strPath.c_str());
		return -1;
	}

	ZSignAsset zsa;
	if (!zsa.Init(strCertFile, strPKeyFile, strProvFile, strEntitleFile, strPassword, bAdhoc, bSHA256Only, false)) {
		return -1;
	}

	bool bEnableCache = true;
	string strFolder = strPath;

	atimer.Reset();
	ZBundle bundle;
	bundle.m_bRemoveUISupportedDevices = removeUISupportedDevices;
	bundle.m_bRemoveWatchApp = removeWatchApp;
	bundle.m_bEnableDocuments = enableDocuments;
	bundle.m_strMinVersion = minOSVersion != nil ? string([minOSVersion UTF8String]) : string();
	bundle.m_bRemoveExtensions = removeExtensions;
	bool bRet = bundle.SignFolder(&zsa, strFolder, strBundleId, strBundleVersion, strDisplayName, arrDylibFiles, arrRemoveDylibNames, bForce, bWeakInject, bEnableCache, excludeprovion);
	ZLog::PrintV(">>> Signing:\t%s %s\n", ZUtil::GetBaseName(strPath.c_str()), (bAdhoc ? " (Ad-hoc)" : ""));
	atimer.PrintResult(bRet, ">>> Signed %s!", bRet ? "OK" : "Failed");

	NSError* signError = nil;
	if (!bRet) {
		NSDictionary* userInfo = @{
			NSLocalizedDescriptionKey : @"Signing failed."
		};
		signError = [NSError errorWithDomain:@"Failed to Sign" code:-1 userInfo:userInfo];
	}

	if (completionHandler) {
		completionHandler(bRet, signError);
	}

	gtimer.Print(">>> Done.");
	return bRet ? 0 : -1;
}

int zsignIPA(
	NSString *inputPath,
	NSString *outputPath,
	NSString *prov,
	NSString *key,
	NSString *pass,
	NSString *entitlement,
	NSString *bundleid,
	NSString *displayname,
	NSString *bundleversion,
	bool adhoc,
	bool dontGenerateEmbeddedMobileProvision,
	bool removeUISupportedDevices,
	bool removeWatchApp,
	bool enableDocuments,
	NSString * _Nullable minOSVersion,
	bool removeExtensions,
	int zipLevel,
	NSString * _Nullable tempFolder,
	bool zh,
	void (^ _Nullable completionHandler)(BOOL success, NSError * _Nullable error)
) {
	ZsignLangScope langScope(zh);

	ZTimer atimer;
	ZTimer gtimer;

	bool bForce = true;
	bool bWeakInject = false;
	bool bAdhoc = adhoc;
	bool bSHA256Only = false;
	int nZipLevel = (zipLevel >= 0 && zipLevel <= 9) ? zipLevel : 6;

	string strCertFile;
	string strPKeyFile;
	string strProvFile;
	string strPassword;
	string strBundleId;
	string strBundleVersion;
	string strDisplayName;
	string strEntitleFile;
	vector<string> arrDylibFiles;
	vector<string> arrRemoveDylibNames;

	strPKeyFile = [key cStringUsingEncoding:NSUTF8StringEncoding];
	strProvFile = [prov cStringUsingEncoding:NSUTF8StringEncoding];
	strPassword = [pass cStringUsingEncoding:NSUTF8StringEncoding];
	strEntitleFile = [entitlement cStringUsingEncoding:NSUTF8StringEncoding];
	strBundleId = [bundleid cStringUsingEncoding:NSUTF8StringEncoding];
	strDisplayName = [displayname cStringUsingEncoding:NSUTF8StringEncoding];
	strBundleVersion = [bundleversion cStringUsingEncoding:NSUTF8StringEncoding];

	string strPath = [inputPath cStringUsingEncoding:NSUTF8StringEncoding];
	string strOutputFile = [outputPath cStringUsingEncoding:NSUTF8StringEncoding];

	if (!ZFile::IsFileExists(strPath.c_str())) {
		ZLog::ErrorV(">>> Invalid path! %s\n", strPath.c_str());
		if (completionHandler) {
			completionHandler(NO, [NSError errorWithDomain:@"Zsign" code:-1 userInfo:@{NSLocalizedDescriptionKey: @"Invalid input path"}]);
		}
		return -1;
	}
	if (strOutputFile.empty()) {
		ZLog::Error(">>> Output path is required.\n");
		if (completionHandler) {
			completionHandler(NO, [NSError errorWithDomain:@"Zsign" code:-1 userInfo:@{NSLocalizedDescriptionKey: @"Output path is required"}]);
		}
		return -1;
	}

	string strTempFolder = (tempFolder != nil && [tempFolder length] > 0)
		? string([tempFolder cStringUsingEncoding:NSUTF8StringEncoding])
		: ZFile::GetTempFolder();
	if (!strTempFolder.empty() && !ZFile::IsFolder(strTempFolder.c_str())) {
		ZLog::ErrorV(">>> Invalid temp folder! %s\n", strTempFolder.c_str());
		if (completionHandler) {
			completionHandler(NO, [NSError errorWithDomain:@"Zsign" code:-1 userInfo:@{NSLocalizedDescriptionKey: @"Invalid temp folder"}]);
		}
		return -1;
	}

	ZSignAsset zsa;
	if (!zsa.Init(strCertFile, strPKeyFile, strProvFile, strEntitleFile, strPassword, bAdhoc, bSHA256Only, false)) {
		if (completionHandler) {
			completionHandler(NO, [NSError errorWithDomain:@"Zsign" code:-1 userInfo:@{NSLocalizedDescriptionKey: @"Failed to init certificate"}]);
		}
		return -1;
	}

	bool bZipFile = ZFile::IsZipFile(strPath.c_str());
	bool bTempFolder = false;
	bool bEnableCache = true;
	string strFolder = strPath;

	if (bZipFile) {
		bForce = true;
		bTempFolder = true;
		bEnableCache = false;
		strFolder = ZFile::GetRealPathV("%s/zsign_folder_%llu", strTempFolder.c_str(), ZUtil::GetMicroSecond());
		ZLog::PrintV(">>> Unzip:\t%s (%s) ... \n", ZUtil::GetBaseName(strPath.c_str()), ZFile::GetFileSizeString(strPath.c_str()).c_str());
		if (!Zip::Extract(strPath.c_str(), strFolder.c_str())) {
			ZLog::ErrorV(">>> Unzip failed!\n");
			if (completionHandler) {
				completionHandler(NO, [NSError errorWithDomain:@"Zsign" code:-1 userInfo:@{NSLocalizedDescriptionKey: @"Unzip failed"}]);
			}
			return -1;
		}
		atimer.PrintResult(true, ">>> Unzip OK!");
	}

	atimer.Reset();
	ZBundle bundle;
	bundle.m_bRemoveUISupportedDevices = removeUISupportedDevices;
	bundle.m_bRemoveWatchApp = removeWatchApp;
	bundle.m_bEnableDocuments = enableDocuments;
	bundle.m_strMinVersion = minOSVersion != nil ? string([minOSVersion UTF8String]) : string();
	bundle.m_bRemoveExtensions = removeExtensions;

	bool bRet = bundle.SignFolder(&zsa, strFolder, strBundleId, strBundleVersion, strDisplayName, arrDylibFiles, arrRemoveDylibNames, bForce, bWeakInject, bEnableCache, dontGenerateEmbeddedMobileProvision);
	ZLog::PrintV(">>> Signing:\t%s %s\n", ZUtil::GetBaseName(strPath.c_str()), (bAdhoc ? " (Ad-hoc)" : ""));
	atimer.PrintResult(bRet, ">>> Signed %s!", bRet ? "OK" : "Failed");

	if (bRet && !strOutputFile.empty()) {
		atimer.Reset();
		ZLog::PrintV(">>> Archiving: \t%s ... \n", ZUtil::GetBaseName(strOutputFile.c_str()));
		string strBaseFolder;
		bool bNeedCleanPayload = false;
		size_t pos = bundle.m_strAppFolder.rfind("Payload");
		if (pos != string::npos && pos > 0) {
			strBaseFolder = bundle.m_strAppFolder.substr(0, pos - 1);
		} else {
			string strPayloadRoot = ZFile::GetRealPathV("%s/zsign_payload_%llu", strTempFolder.c_str(), ZUtil::GetMicroSecond());
			string strPayloadFolder = strPayloadRoot + "/Payload";
			string strAppName = ZUtil::GetBaseName(bundle.m_strAppFolder.c_str());
			NSString *srcApp = [NSString stringWithUTF8String:bundle.m_strAppFolder.c_str()];
			NSString *destApp = [NSString stringWithUTF8String:(strPayloadFolder + "/" + strAppName).c_str()];
			NSFileManager *fm = [NSFileManager defaultManager];
			if (![fm createDirectoryAtPath:[NSString stringWithUTF8String:strPayloadFolder.c_str()] withIntermediateDirectories:YES attributes:nil error:nil]) {
				ZLog::Error(">>> Failed to create Payload directory.\n");
				bRet = false;
			} else {
				NSError *copyErr = nil;
				if (![fm copyItemAtPath:srcApp toPath:destApp error:&copyErr]) {
					ZLog::ErrorV(">>> Failed to copy app into Payload: %s\n", copyErr ? [[copyErr localizedDescription] UTF8String] : "unknown");
					bRet = false;
					ZFile::RemoveFolder(strPayloadRoot.c_str());
				} else {
					strBaseFolder = strPayloadRoot;
					bNeedCleanPayload = true;
				}
			}
		}
		if (bRet && !strBaseFolder.empty()) {
			if (!Zip::Archive(strBaseFolder, strOutputFile, nZipLevel)) {
				ZLog::Error(">>> Archive failed!\n");
				bRet = false;
			} else {
				atimer.PrintResult(true, ">>> Archive OK! (%s)", ZFile::GetFileSizeString(strOutputFile.c_str()).c_str());
			}
			if (bNeedCleanPayload) {
				ZFile::RemoveFolder(strBaseFolder.c_str());
			}
		} else if (bRet) {
			ZLog::Error(">>> Can't find payload directory!\n");
			bRet = false;
		}
	}

	if (bTempFolder) {
		ZFile::RemoveFolder(strFolder.c_str());
	}

	NSError *signError = nil;
	if (!bRet) {
		signError = [NSError errorWithDomain:@"Failed to Sign" code:-1 userInfo:@{NSLocalizedDescriptionKey: @"Signing or archiving failed."}];
	}

	if (completionHandler) {
		completionHandler(bRet, signError);
	}

	gtimer.Print(">>> Done.");
	return bRet ? 0 : -1;
}

int zsignArchiveFolderToIPA(
	NSString *folderPath,
	NSString *outputPath,
	int zipLevel,
	bool zh,
	void (^ _Nullable completionHandler)(BOOL success, NSError * _Nullable error)
) {
	ZsignLangScope langScope(zh);

	ZTimer atimer;
	ZTimer gtimer;

	int nZipLevel = (zipLevel >= 0 && zipLevel <= 9) ? zipLevel : 6;

	if (!folderPath || [folderPath length] == 0) {
		ZLog::Error(">>> Archive: folder path is required.\n");
		if (completionHandler) {
			completionHandler(NO, [NSError errorWithDomain:@"Zsign" code:-1 userInfo:@{NSLocalizedDescriptionKey: @"Folder path is required"}]);
		}
		return -1;
	}
	if (!outputPath || [outputPath length] == 0) {
		ZLog::Error(">>> Archive: output path is required.\n");
		if (completionHandler) {
			completionHandler(NO, [NSError errorWithDomain:@"Zsign" code:-1 userInfo:@{NSLocalizedDescriptionKey: @"Output path is required"}]);
		}
		return -1;
	}

	NSString *standardized = [folderPath stringByStandardizingPath];
	BOOL isDir = NO;
	NSFileManager *fm = [NSFileManager defaultManager];
	if (![fm fileExistsAtPath:standardized isDirectory:&isDir] || !isDir) {
		ZLog::ErrorV(">>> Archive: invalid folder: %s\n", [standardized UTF8String]);
		if (completionHandler) {
			completionHandler(NO, [NSError errorWithDomain:@"Zsign" code:-1 userInfo:@{NSLocalizedDescriptionKey: @"Invalid folder path"}]);
		}
		return -1;
	}

	if ([[standardized lastPathComponent] compare:@"Payload" options:NSCaseInsensitiveSearch] != NSOrderedSame) {
		ZLog::Error(">>> Archive: path must be the Payload directory (…/Payload).\n");
		if (completionHandler) {
			completionHandler(NO, [NSError errorWithDomain:@"Zsign" code:-1 userInfo:@{NSLocalizedDescriptionKey: @"Path must be the Payload directory (ending with /Payload)"}]);
		}
		return -1;
	}

	NSError *enumErr = nil;
	/* 仅枚举 Payload 下一级条目（不递归子文件夹）；与「仅允许 Payload/xxx.app」约定一致。 */
	NSArray<NSString *> *contents = [fm contentsOfDirectoryAtPath:standardized error:&enumErr];
	if (!contents) {
		ZLog::ErrorV(">>> Archive: cannot list Payload: %s\n", enumErr ? [[enumErr localizedDescription] UTF8String] : "unknown");
		if (completionHandler) {
			completionHandler(NO, [NSError errorWithDomain:@"Zsign" code:-1 userInfo:@{NSLocalizedDescriptionKey: @"Cannot read Payload directory"}]);
		}
		return -1;
	}

	NSUInteger appCount = 0;
	for (NSString *name in contents) {
		if ([[name pathExtension] compare:@"app" options:NSCaseInsensitiveSearch] != NSOrderedSame) {
			continue;
		}
		NSString *full = [standardized stringByAppendingPathComponent:name];
		isDir = NO;
		if ([fm fileExistsAtPath:full isDirectory:&isDir] && isDir) {
			appCount++;
		}
	}
	if (appCount == 0) {
		ZLog::Error(">>> Archive: Payload must contain exactly one .app bundle (none found).\n");
		if (completionHandler) {
			completionHandler(NO, [NSError errorWithDomain:@"Zsign" code:-1 userInfo:@{NSLocalizedDescriptionKey: @"Payload must contain exactly one .app bundle (none found)"}]);
		}
		return -1;
	}
	if (appCount > 1) {
		ZLog::Error(">>> Archive: Payload must contain exactly one .app bundle (multiple .app found).\n");
		if (completionHandler) {
			completionHandler(NO, [NSError errorWithDomain:@"Zsign" code:-1 userInfo:@{NSLocalizedDescriptionKey: @"Payload must contain exactly one .app bundle (multiple found)"}]);
		}
		return -1;
	}

	// 只打包 Payload 目录树，条目前缀为 Payload/…；勿使用 Payload 父目录作为 zip 根，否则会打进同级其它文件（如已解压的 .ipa、元数据等）。
	string strPayloadFolder = [standardized UTF8String];
	string strOutput = [outputPath UTF8String];

	atimer.Reset();
	ZLog::PrintV(">>> Archiving: \t%s ... \n", ZUtil::GetBaseName(strOutput.c_str()));
	bool bRet = Zip::ArchivePayloadFolderForIPA(strPayloadFolder, strOutput, nZipLevel);
	if (!bRet) {
		ZLog::Error(">>> Archive failed!\n");
	} else {
		atimer.PrintResult(true, ">>> Archive OK! (%s)", ZFile::GetFileSizeString(strOutput.c_str()).c_str());
	}

	NSError *archiveError = nil;
	if (!bRet) {
		archiveError = [NSError errorWithDomain:@"Zsign" code:-1 userInfo:@{NSLocalizedDescriptionKey: @"Archive failed"}];
	}

	if (completionHandler) {
		completionHandler(bRet, archiveError);
	}

	gtimer.Print(">>> Done.");
	return bRet ? 0 : -1;
}

int zsignExtractIPA(
	NSString *ipaPath,
	NSString *outputFolderPath,
	bool zh,
	void (^ _Nullable completionHandler)(BOOL success, NSError * _Nullable error)
) {
	ZsignLangScope langScope(zh);

	ZTimer atimer;
	ZTimer gtimer;

	if (!ipaPath || [ipaPath length] == 0) {
		ZLog::Error(">>> Extract: input path is required.\n");
		if (completionHandler) {
			completionHandler(NO, [NSError errorWithDomain:@"Zsign" code:-1 userInfo:@{NSLocalizedDescriptionKey: @"Input path is required"}]);
		}
		return -1;
	}
	if (!outputFolderPath || [outputFolderPath length] == 0) {
		ZLog::Error(">>> Extract: output folder is required.\n");
		if (completionHandler) {
			completionHandler(NO, [NSError errorWithDomain:@"Zsign" code:-1 userInfo:@{NSLocalizedDescriptionKey: @"Output folder is required"}]);
		}
		return -1;
	}

	string strPath = [ipaPath cStringUsingEncoding:NSUTF8StringEncoding];
	string strOut = [outputFolderPath cStringUsingEncoding:NSUTF8StringEncoding];

	if (!ZFile::IsFileExists(strPath.c_str())) {
		ZLog::ErrorV(">>> Invalid path! %s\n", strPath.c_str());
		if (completionHandler) {
			completionHandler(NO, [NSError errorWithDomain:@"Zsign" code:-1 userInfo:@{NSLocalizedDescriptionKey: @"Invalid input path"}]);
		}
		return -1;
	}
	if (!ZFile::IsZipFile(strPath.c_str())) {
		ZLog::Error(">>> Extract: not a zip file.\n");
		if (completionHandler) {
			completionHandler(NO, [NSError errorWithDomain:@"Zsign" code:-1 userInfo:@{NSLocalizedDescriptionKey: @"Not a zip file"}]);
		}
		return -1;
	}

	atimer.Reset();
	ZLog::PrintV(">>> Unzip:\t%s (%s) ... \n", ZUtil::GetBaseName(strPath.c_str()), ZFile::GetFileSizeString(strPath.c_str()).c_str());
	bool bRet = Zip::ExtractWithProgress(strPath.c_str(), strOut.c_str());
	if (!bRet) {
		ZLog::ErrorV(">>> Unzip failed!\n");
	} else {
		atimer.PrintResult(true, ">>> Unzip OK!");
	}

	NSError *extractError = nil;
	if (!bRet) {
		extractError = [NSError errorWithDomain:@"Zsign" code:-1 userInfo:@{NSLocalizedDescriptionKey: @"Unzip failed"}];
	}

	if (completionHandler) {
		completionHandler(bRet, extractError);
	}

	gtimer.Print(">>> Done.");
	return bRet ? 0 : -1;
}


int checkCert(
	NSString *prov,
	NSString *key,
	NSString *pass,
	void (^completionHandler)(int status, NSDate * _Nullable expirationDate, NSString * _Nullable error)
) {
	ZTimer gtimer;

	string strCertFile;
	string strPKeyFile;
	string strProvFile;
	string strPassword;
	string strEntitleFile;

	if (!key || !prov || !pass) {
		completionHandler(2, nil, @"One or more required paths or password is missing.");
		return -1;
	}

	strPKeyFile = [key cStringUsingEncoding:NSUTF8StringEncoding];
	strProvFile = [prov cStringUsingEncoding:NSUTF8StringEncoding];
	strPassword = [pass cStringUsingEncoding:NSUTF8StringEncoding];

	__block ZSignAsset zsa;

	if (!zsa.Init(strCertFile, strPKeyFile, strProvFile, strEntitleFile, strPassword, false, false, false)) {
		completionHandler(2, nil, @"Unable to initialize certificate. Please check your password.");
		return -1;
	}

	X509* cert = (X509*)zsa.m_x509Cert;
	BIO *brother1;
	unsigned long issuerHash = X509_issuer_name_hash((X509*)cert);
	if (0x817d2f7a == issuerHash) {
		brother1 = BIO_new_mem_buf(ZSignAsset::s_szAppleDevCACert, (int)strlen(ZSignAsset::s_szAppleDevCACert));
	} else if (0x9b16b75c == issuerHash) {
		brother1 = BIO_new_mem_buf(ZSignAsset::s_szAppleDevCACertG3, (int)strlen(ZSignAsset::s_szAppleDevCACertG3));
	} else {
		completionHandler(2, nil, @"Unable to determine issuer of the certificate. It is signed by Apple Developer?");
		return -2;
	}

	if (!brother1)
	{
		completionHandler(2, nil, @"Unable to initialize issuer certificate.");
		return -3;
	}

	X509 *issuer = PEM_read_bio_X509(brother1, NULL, 0, NULL);

	if (!cert || !issuer) {
		completionHandler(2, nil, @"Error loading cert or issuer");
		return -4;
	}

	// Extract OCSP URL from cert
	STACK_OF(ACCESS_DESCRIPTION)* aia = (STACK_OF(ACCESS_DESCRIPTION)*)X509_get_ext_d2i((X509*)cert, NID_info_access, 0, 0);
	if (!aia) {
		completionHandler(2, nil, @"No AIA (OCSP) extension found in certificate");
		return -5;
	}

	ASN1_IA5STRING* uri = nullptr;
	for (int i = 0; i < sk_ACCESS_DESCRIPTION_num(aia); i++) {
		ACCESS_DESCRIPTION* ad = sk_ACCESS_DESCRIPTION_value(aia, i);
		if (OBJ_obj2nid(ad->method) == NID_ad_OCSP &&
			ad->location->type == GEN_URI) {
			uri = ad->location->d.uniformResourceIdentifier;

			break;
		}
	}

	if (!uri) {
		completionHandler(2, nil, @"No OCSP URI found in certificate.");
		return -6;
	}

	OCSP_REQUEST* req = OCSP_REQUEST_new();
	OCSP_CERTID* cert_id = OCSP_cert_to_id(nullptr, (X509*)cert, issuer);
	OCSP_request_add0_id(req, cert_id);  // Ownership transferred to request
	cert_id = OCSP_cert_to_id(nullptr, (X509*)cert, issuer);
	unsigned char* der = 0;
	int len = i2d_OCSP_REQUEST(req, &der);

	NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithUTF8String:(const char *)uri->data]]];
	[request setHTTPMethod:@"POST"];
	[request setHTTPBody:[NSData dataWithBytes:der length:len]];
	[request setValue:@"application/ocsp-request" forHTTPHeaderField:@"Content-Type"];
	[request setValue:@"application/ocsp-response" forHTTPHeaderField:@"Accept"];

	OPENSSL_free(der);
	if (aia) {
		sk_ACCESS_DESCRIPTION_pop_free(aia, ACCESS_DESCRIPTION_free);
	}
	OCSP_REQUEST_free(req);
	X509_free(issuer);
	BIO_free(brother1);

	NSURLSession *session = [NSURLSession sharedSession];
	NSURLSessionDataTask *task = [session dataTaskWithRequest:request
											completionHandler:^(NSData * _Nullable data,
																NSURLResponse * _Nullable response,
																NSError * _Nullable error) {
		if (error) {
			completionHandler(2, nil, error.localizedDescription);
			return;
		}

		NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse *)response;
		if (httpResponse.statusCode == 200 && data) {
			const void *respBytes = [data bytes];
			OCSP_RESPONSE *resp;
			d2i_OCSP_RESPONSE(&resp, (const unsigned char**)&respBytes, data.length);
			OCSP_BASICRESP *basic = OCSP_response_get1_basic(resp);
			ASN1_TIME *expirationDateAsn1 = X509_get_notAfter(cert);
			NSString *fullDateString = [NSString stringWithFormat:@"20%s", expirationDateAsn1->data];

			NSDateFormatter *formatter = [[NSDateFormatter alloc] init];
			formatter.dateFormat = @"yyyyMMddHHmmss'Z'";
			formatter.timeZone = [NSTimeZone timeZoneWithAbbreviation:@"UTC"];
			formatter.locale = NSLocale.currentLocale;
			NSDate *expirationDate = [formatter dateFromString:fullDateString];

			int status, reason;
			if (OCSP_resp_find_status(basic, cert_id, &status, &reason, NULL, NULL, NULL)) {
				completionHandler(status, expirationDate, nil);
			} else {
				completionHandler(2, expirationDate, nil);
			}

			OCSP_CERTID_free(cert_id);
			OCSP_BASICRESP_free(basic);
			OCSP_RESPONSE_free(resp);


		} else {
			completionHandler(2, nil, @"Invalid response or no data");
			return;
		}
	}];

	[task resume];
	return 1;
}

static void (^ _Nullable gLogHandler)(NSString * _Nullable line) = nil;

static void ZsignLogSinkTrampoline(const char* utf8Line, int color, void* userdata)
{
	(void)color;
	(void)userdata;
	void (^ _Nullable h)(NSString * _Nullable) = gLogHandler;
	if (!h || !utf8Line) {
		return;
	}
	NSString *s = [NSString stringWithUTF8String:utf8Line];
	if (s) {
		h(s);
	}
}

void ZsignSetLogHandler(void (^ _Nullable handler)(NSString * _Nullable line))
{
	gLogHandler = [handler copy];
	if (handler) {
		ZLog_SetExternalSink(ZsignLogSinkTrampoline, NULL);
	} else {
		ZLog_ClearExternalSink();
	}
}

}
