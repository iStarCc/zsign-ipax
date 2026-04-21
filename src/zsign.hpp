//
//  zsign.hpp
//  feather
//
//  Created by HAHALOSAH on 5/22/24.
//

#ifndef zsign_hpp
#define zsign_hpp

#include <stdio.h>
#import <Foundation/Foundation.h>

#ifdef __cplusplus
extern "C" {
#endif

NS_ASSUME_NONNULL_BEGIN

bool CheckIfSigned(NSString *filePath);
bool InjectDyLib(NSString *filePath, NSString *dylibPath, bool weakInject);
bool UninstallDylibs(NSString *filePath, NSArray<NSString *> *dylibPathsArray);
NSArray<NSString *> * _Nullable ListDylibs(NSString *filePath);
bool ChangeDylibPath(NSString *filePath, NSString *oldPath, NSString *newPath);

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
	bool dontGenerateEmbeddedMobileProvision,
	bool removeUISupportedDevices,
	bool removeWatchApp,
	bool enableDocuments,
	NSString * _Nullable minOSVersion,
	bool removeExtensions,
	bool zh,
	void (^ _Nullable completionHandler)(BOOL success, NSError * _Nullable error)
);

/// 签名并输出 IPA：支持输入 `.ipa`（先解压）或 `.app`，使用 minizip 打包；压缩过程会通过 `ZLog` 输出进度（如「压缩中: n/m」），经 `zlog_i18n`/`ZSIGN_LANG` 本地化。
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
);

/// 将 `Payload` 目录打包为 `.ipa`（仅压缩，不签名）。`folderPath` **必须**为名为 `Payload` 的目录；**仅看 Payload 下一级**（不递归子目录），须有且仅有一个 `*.app` bundle（即 `Payload/xxx.app`）；压缩逻辑与 `zsignIPA` 最终阶段一致（`Zip::Archive` + UTF-8 文件名）。
int zsignArchiveFolderToIPA(
	NSString *folderPath,
	NSString *outputPath,
	int zipLevel,
	bool zh,
	void (^ _Nullable completionHandler)(BOOL success, NSError * _Nullable error)
);

/// 将 `.ipa`（或任意 ZIP）解压到目录；与 `signIPA` 内部解压一致的安全规则；起始日志仅包名与大小（无「-> 目标路径」），并输出与压缩阶段同格式的条目进度（`Unzipping files` / `正在解压`）及大文件心跳。
int zsignExtractIPA(
	NSString *ipaPath,
	NSString *outputFolderPath,
	bool zh,
	void (^ _Nullable completionHandler)(BOOL success, NSError * _Nullable error)
);

int checkCert(
	NSString *prov,
	NSString *key,
	NSString *pass,
	void (^completionHandler)(int status, NSDate * _Nullable expirationDate, NSString * _Nullable error)
);

/** 实时日志：每次 ZLog 输出一行（已 UTF-8、已 zlog_i18n）时回调；传 nil 关闭。可在任意线程调用。 */
void ZsignSetLogHandler(void (^ _Nullable handler)(NSString * _Nullable line));

NS_ASSUME_NONNULL_END

#ifdef __cplusplus
}
#endif

#endif /* zsign_hpp */
