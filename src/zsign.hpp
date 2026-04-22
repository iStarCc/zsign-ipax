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

/// 仅 **`.ipa`**：扩展名须为 `.ipa`，且 zip 内须含 `Payload/xxx.app` 布局。解压到目标目录（不删除整目录）；若已存在 `Payload` 子目录则先重命名为 `Payload1`、`Payload2`… 再解压。起始日志仅包名与大小（无「-> 目标路径」），条目进度为 `Unzipping files` / `正在解压`。
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

/**
 * 请求取消当前正在进行的 minizip 归档（`Zip::Archive` / `ArchivePayloadFolderForIPA`，含 `signIPA` / `archiveFolderToIPA` 内的压缩阶段）。
 * 可在任意线程调用；底层在「条目之间」及「大文件分块写入」循环中轮询，尽量中止并产出 `NSURLErrorCancelled`。
 */
void ZsignRequestZipArchiveCancel(void);

/** 仅查询：上一轮归档失败是否因上述取消（供调试；业务侧以 completion 的 NSError 为准）。 */
bool ZsignZipArchiveLastFailureWasUserCancel(void);

NS_ASSUME_NONNULL_END

#ifdef __cplusplus
}
#endif

#endif /* zsign_hpp */
