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

bool CheckIfSigned(NSString *filePath);
bool InjectDyLib(NSString *filePath, NSString *dylibPath, bool weakInject);
bool UninstallDylibs(NSString *filePath, NSArray<NSString *> *dylibPathsArray);
NSArray<NSString *> *ListDylibs(NSString *filePath);
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
	NSString *minOSVersion,
	bool removeExtensions,
	bool zh,
	void(^ _Nullable completionHandler)(BOOL success, NSError * _Nullable error)
);

int checkCert(
	NSString *prov,
	NSString *key,
	NSString *pass,
	void(^completionHandler)(int status, NSDate* expirationDate, NSString *error)
);

/** 实时日志：每次 ZLog 输出一行（已 UTF-8、已 zlog_i18n）时回调；传 nil 关闭。可在任意线程调用。 */
void ZsignSetLogHandler(void (^ _Nullable handler)(NSString * _Nullable line));

#ifdef __cplusplus
}
#endif

#endif /* zsign_hpp */
