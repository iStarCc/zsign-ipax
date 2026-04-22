import XCTest
import ZsignSwift

final class ExtractIPATests: XCTestCase {
	/// 构造最小 IPA（zip 内含 Payload/Test.app/Info.plist），调用 `extractIPA` 并校验输出目录结构。
	func testExtractMinimalIPA() throws {
		let tmp = FileManager.default.temporaryDirectory
			.appendingPathComponent("zsign-extract-test-\(UUID().uuidString)", isDirectory: true)
		try FileManager.default.createDirectory(at: tmp, withIntermediateDirectories: true)
		defer { try? FileManager.default.removeItem(at: tmp) }

		let payload = tmp.appendingPathComponent("Payload", isDirectory: true)
		let appDir = payload.appendingPathComponent("Test.app", isDirectory: true)
		try FileManager.default.createDirectory(at: appDir, withIntermediateDirectories: true)
		let plist = """
		<?xml version="1.0" encoding="UTF-8"?>
		<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
		<plist version="1.0"><dict>
		<key>CFBundleIdentifier</key><string>com.zsign.test</string>
		</dict></plist>
		"""
		try plist.write(to: appDir.appendingPathComponent("Info.plist"), atomically: true, encoding: .utf8)

		let ipaURL = tmp.appendingPathComponent("Minimal.ipa")
		let zip = Process()
		zip.executableURL = URL(fileURLWithPath: "/usr/bin/zip")
		zip.arguments = ["-q", "-r", ipaURL.path, "Payload"]
		zip.currentDirectoryURL = tmp
		try zip.run()
		zip.waitUntilExit()
		XCTAssertEqual(zip.terminationStatus, 0, "需要系统 /usr/bin/zip")
		XCTAssertTrue(FileManager.default.fileExists(atPath: ipaURL.path))

		let outDir = tmp.appendingPathComponent("extracted", isDirectory: true)
		try FileManager.default.createDirectory(at: outDir, withIntermediateDirectories: true)

		let ok = Zsign.extractIPA(
			ipaPath: ipaURL.path,
			outputFolderPath: outDir.path,
			zh: false
		)
		XCTAssertTrue(ok, "extractIPA 应成功")

		let infoOut = outDir.appendingPathComponent("Payload/Test.app/Info.plist")
		XCTAssertTrue(FileManager.default.fileExists(atPath: infoOut.path), "应解压出 Payload/Test.app/Info.plist")
	}

	/// 目标目录已有 Payload 时，应先重命名为 Payload1 再写入新 Payload。
	func testExtractRenamesExistingPayload() throws {
		let tmp = FileManager.default.temporaryDirectory
			.appendingPathComponent("zsign-extract-payload1-\(UUID().uuidString)", isDirectory: true)
		try FileManager.default.createDirectory(at: tmp, withIntermediateDirectories: true)
		defer { try? FileManager.default.removeItem(at: tmp) }

		let payload = tmp.appendingPathComponent("Payload", isDirectory: true)
		let appDir = payload.appendingPathComponent("Test.app", isDirectory: true)
		try FileManager.default.createDirectory(at: appDir, withIntermediateDirectories: true)
		try "old".write(to: appDir.appendingPathComponent("marker.txt"), atomically: true, encoding: .utf8)

		let ipaURL = tmp.appendingPathComponent("Second.ipa")
		let zip = Process()
		zip.executableURL = URL(fileURLWithPath: "/usr/bin/zip")
		zip.arguments = ["-q", "-r", ipaURL.path, "Payload"]
		zip.currentDirectoryURL = tmp
		try zip.run()
		zip.waitUntilExit()
		XCTAssertEqual(zip.terminationStatus, 0)

		let outDir = tmp.appendingPathComponent("out", isDirectory: true)
		try FileManager.default.createDirectory(at: outDir, withIntermediateDirectories: true)
		let oldPayload = outDir.appendingPathComponent("Payload", isDirectory: true)
		try FileManager.default.createDirectory(at: oldPayload, withIntermediateDirectories: true)
		try "keep".write(to: oldPayload.appendingPathComponent("before.txt"), atomically: true, encoding: .utf8)

		let ok = Zsign.extractIPA(
			ipaPath: ipaURL.path,
			outputFolderPath: outDir.path,
			zh: false
		)
		XCTAssertTrue(ok)

		XCTAssertTrue(FileManager.default.fileExists(atPath: outDir.appendingPathComponent("Payload1/before.txt").path))
		XCTAssertTrue(FileManager.default.fileExists(atPath: outDir.appendingPathComponent("Payload/Test.app/marker.txt").path))
	}
}
