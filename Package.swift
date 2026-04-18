// swift-tools-version: 5.8

import PackageDescription

let package = Package(
	name: "Zsign",
	platforms: [
		.iOS(.v12),
		.macOS(.v10_15),
		.tvOS(.v12),
		.watchOS(.v8),
		.custom("xros", versionString: "1.3")
	],
	products: [
		.library(
			name: "zsign",
			targets: ["Zsign"]
		),
		.library(
			name: "ZsignSwift",
			targets: ["ZsignSwift"]
		),
	],
	dependencies: [
		.package(url: "https://github.com/krzyzanowskim/OpenSSL", from: "3.3.3001")
	],
	targets: [
		.target(
			name: "Zsign",
			dependencies: [
				.product(name: "OpenSSL", package: "OpenSSL")
			],
			path: "src",
			exclude: [
				"common/archive.cpp",
				"common/archive_zip_progress.cpp",
				"zsign.cpp",
				"metadata.cpp",
				"certcheck.cpp",
				"third-party"
			],
			sources: [
				"archo.cpp",
				"bundle.cpp",
				"macho.cpp",
				"openssl.cpp",
				"openssl_tools.mm",
				"signing.cpp",
				"zsign.mm",
				"common/base64.cpp",
				"common/fs.cpp",
				"common/json.cpp",
				"common/log.cpp",
				"common/sha.cpp",
				"common/timer.cpp",
				"common/util.cpp",
				"common/zlog_i18n.cpp"
			],
			publicHeadersPath: "include",
			cxxSettings: [
				.headerSearchPath("."),
				.headerSearchPath("common"),
				.unsafeFlags(["-std=c++17"])
			],
			linkerSettings: [
				.linkedFramework("OpenSSL"),
			]
		),
		.target(
			name: "ZsignSwift",
			dependencies: [
				"Zsign"
			],
			path: "Sources",
			sources: [
				"zsign.swift"
			]
		)
	]
)
