//
//  Zsign.swift
//  Feather
//
//  Created by samara on 17.04.2025.
//

import Zsign

public enum Zsign {
	/// 注册实时日志：底层每次 `ZLog` 输出一行（UTF-8，已应用 `zlog_i18n` / `zh` 环境）时回调。传 `nil` 关闭。回调可能在后台线程执行，更新 UI 时请自行切主线程。
	public static func setLogHandler(_ handler: ((String) -> Void)?) {
		if let handler {
			ZsignSetLogHandler { (line: String?) in
				guard let line else { return }
				handler(line)
			}
		} else {
			ZsignSetLogHandler(nil)
		}
	}

	/// Checks if the MachO-file is properly signed
	/// - Parameter appExecutable: Executable
	/// - Returns: True if its signed
	static public func checkSigned(appExecutable: String) -> Bool {
		CheckIfSigned(appExecutable)
	}
	/// Injects a load command to an executable
	/// - Parameters:
	///   - appExecutable: Executable
	///   - path: Load command (i.e. `@rpath/CydiaSubstrate.framework`)
	///   - weak: Weak inject
	/// - Returns: True if its successful
	static public func injectDyLib(appExecutable: String, with path: String, weak: Bool = true) -> Bool {
		InjectDyLib(appExecutable, path, weak)
	}
	/// Removes load commands from an executable
	/// - Parameters:
	///   - appExecutable: Executable
	///   - dylibs: Load commands (i.e. `@rpath/CydiaSubstrate.framework...`)
	/// - Returns: True if its successful
	static public func removeDylibs(appExecutable: String, using dylibs: [String]) -> Bool  {
		UninstallDylibs(appExecutable, dylibs)
	}
	/// List load commands from an executable
	/// - Parameter appExecutable: Executable
	/// - Returns: String array with load commands if any
	static public func listDylibs(appExecutable: String) -> [String] {
		ListDylibs(appExecutable) ?? []
	}
	/// Matches and replaces load commands to an executable
	/// - Parameters:
	///   - appExecutable: Executable
	///   - old: Old load command (i.e. `/Library/Frameworks/CydiaSubstrate.framework/CydiaSubstrate`)
	///   - new: New load command (i.e. `@rpath/CydiaSubstrate.framework/CydiaSubstrate`)
	/// - Returns: True if its successful
	static public func changeDylibPath(appExecutable: String, for old: String, with new: String) -> Bool {
		ChangeDylibPath(appExecutable, old, new)
	}
	/// Signs a folder (application bundle) using Zsign
	/// - Parameters:
	///   - appPath: Relative path to app bundle
	///   - provisionPath: Relative path to a provisioning file (i.e. `samara.mobileprovision`)
	///   - p12Path: Relative path to a key file (i.e. `samara.p12`)
	///   - p12Password: Password to the key file
	///   - entitlementsPath: Relative path to an entitlements file
	///   - customIdentifier: Custom indentifier for the app bundle
	///   - customName: Custom display name for the app bundle
	///   - customVersion: Custom version for the app bundle
	///   - adhoc: If the app bundle should be signed using Adhoc (no signing identity)
	///   - removeProvision: If `embedded.mobileprovision` should be excluded when signing
	///   - removeUISupportedDevices: If `UISupportedDevices` should be removed from `Info.plist` (same as zsign `-U` / `--rm_uisd`)
	///   - removeWatchApp: If the Watch app should be stripped from the bundle (same as zsign `-W` / `--rm_watch`)
	///   - enableDocuments: Enable `UISupportsDocumentBrowser` and `UIFileSharingEnabled` (same as `-S` / `--enable_docs`)
	///   - minOSVersion: Sets `MinimumOSVersion` in `Info.plist` when non-empty (same as `-M` / `--min_version`)
	///   - removeExtensions: Remove PlugIns and Extensions (same as `-E` / `--rm_extensions`)
	///   - zh: When `true`, temporarily sets `ZSIGN_LANG=zh` for this call so `ZLog` uses Chinese strings from `zlog_i18n` (restored after signing).
	///   - logHandler: 若设置，仅在本轮 `sign` 期间启用实时日志（结束后会 `setLogHandler(nil)`；若应用内已用 `setLogHandler` 注册全局回调，请先改用本参数或自行在前后保存/恢复）。
	/// - Returns: True if its successful
	static public func sign(
		appPath: String = "",
		provisionPath: String = "",
		p12Path: String = "",
		p12Password: String = "",
		entitlementsPath: String = "",
		customIdentifier: String = "",
		customName: String = "",
		customVersion: String = "",
		adhoc: Bool = false,
		removeProvision: Bool = false,
		removeUISupportedDevices: Bool = false,
		removeWatchApp: Bool = false,
		enableDocuments: Bool = false,
		minOSVersion: String = "",
		removeExtensions: Bool = false,
		zh: Bool = false,
		logHandler: ((String) -> Void)? = nil,
		completion: ((Bool, Error?) -> Void)? = nil
	) -> Bool {
		if let logHandler {
			ZsignSetLogHandler { (line: String?) in
				guard let line else { return }
				logHandler(line)
			}
		}
		defer {
			if logHandler != nil {
				ZsignSetLogHandler(nil)
			}
		}
		if zsign(
			appPath,
			provisionPath,
			p12Path,
			p12Password,
			entitlementsPath,
			customIdentifier,
			customName,
			customVersion,
			adhoc,
			removeProvision,
			removeUISupportedDevices,
			removeWatchApp,
			enableDocuments,
			minOSVersion,
			removeExtensions,
			zh,
			completion.map { callback in
				{ success, error in
					callback(success, error)
				}
			}
		) != 0 {
			return false
		}
		return true
	}

	/// 签名并打包为 IPA：输入可为 `.ipa`（内部先解压）或 `.app` 目录，输出为 `.ipa`（minizip，与命令行 zsign `-o` 一致）。
	/// 压缩阶段会通过 `ZLog` 输出条目进度（如 `Compressing:` / `压缩中:`）及大文件心跳日志；若传入 `logHandler`，可同时收到这些行（与 `sign` 相同，UTF-8）。
	/// - Parameters:
	///   - zipLevel: ZIP 压缩级别 0–9，默认 6。
	///   - tempFolderPath: 临时目录（解压 / 中间 Payload）；空则使用系统临时目录。
	///   - zh: 为本次调用设置 `ZSIGN_LANG=zh`，使 `ZLog`/`压缩进度` 等走中文（结束后恢复）。
	static public func signIPA(
		inputPath: String,
		outputPath: String,
		provisionPath: String = "",
		p12Path: String = "",
		p12Password: String = "",
		entitlementsPath: String = "",
		customIdentifier: String = "",
		customName: String = "",
		customVersion: String = "",
		adhoc: Bool = false,
		removeProvision: Bool = false,
		removeUISupportedDevices: Bool = false,
		removeWatchApp: Bool = false,
		enableDocuments: Bool = false,
		minOSVersion: String = "",
		removeExtensions: Bool = false,
		zipLevel: Int = 6,
		tempFolderPath: String = "",
		zh: Bool = false,
		logHandler: ((String) -> Void)? = nil,
		completion: ((Bool, Error?) -> Void)? = nil
	) -> Bool {
		if let logHandler {
			ZsignSetLogHandler { (line: String?) in
				guard let line else { return }
				logHandler(line)
			}
		}
		defer {
			if logHandler != nil {
				ZsignSetLogHandler(nil)
			}
		}
		let zl = min(max(zipLevel, 0), 9)
		if zsignIPA(
			inputPath,
			outputPath,
			provisionPath,
			p12Path,
			p12Password,
			entitlementsPath,
			customIdentifier,
			customName,
			customVersion,
			adhoc,
			removeProvision,
			removeUISupportedDevices,
			removeWatchApp,
			enableDocuments,
			minOSVersion,
			removeExtensions,
			Int32(zl),
			tempFolderPath.isEmpty ? nil : tempFolderPath,
			zh,
			completion.map { callback in
				{ success, error in
					callback(success, error)
				}
			}
		) != 0 {
			return false
		}
		return true
	}

	/// 将 `Payload` 目录打包为 `.ipa`（仅压缩，不签名）。`folderPath` **必须**指向名为 `Payload` 的文件夹；**只检查其直接子项**（不遍历子文件夹），须有且仅有一个 `.app` bundle（即 `Payload/xxx.app`）；与 `signIPA` 最终归档阶段相同（含压缩进度日志、UTF-8 文件名）。
	/// - Parameters:
	///   - zipLevel: ZIP 级别 0–9，默认 6。
	///   - zh: 为本次调用设置 `ZSIGN_LANG=zh`，使压缩进度等日志为中文。
	static public func archiveFolderToIPA(
		folderPath: String,
		outputPath: String,
		zipLevel: Int = 6,
		zh: Bool = false,
		logHandler: ((String) -> Void)? = nil,
		completion: ((Bool, Error?) -> Void)? = nil
	) -> Bool {
		if let logHandler {
			ZsignSetLogHandler { (line: String?) in
				guard let line else { return }
				logHandler(line)
			}
		}
		defer {
			if logHandler != nil {
				ZsignSetLogHandler(nil)
			}
		}
		let zl = min(max(zipLevel, 0), 9)
		if zsignArchiveFolderToIPA(
			folderPath,
			outputPath,
			Int32(zl),
			zh,
			completion.map { callback in
				{ success, error in
					callback(success, error)
				}
			}
		) != 0 {
			return false
		}
		return true
	}

	/// Check revokage
	/// - Parameters:
	///   - provisionPath: Relative path to a provisioning file (i.e. `samara.mobileprovision`)
	///   - p12Path: Relative path to a key file (i.e. `samara.p12`)
	///   - p12Password: Password to the key file
	///   - completionHandler: Handler
	static public func checkRevokage(
		provisionPath: String = "",
		p12Path: String = "",
		p12Password: String = "",
		completionHandler: @escaping (Int32, Date?, String?) -> Void
	) {
		checkCert(
			provisionPath,
			p12Path,
			p12Password
		) { status, expirationDate, error in
			completionHandler(status, expirationDate, error)
		}
	}
}
