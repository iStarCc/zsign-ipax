# Zsign（Swift Package）

本目录为 **zsign**（iOS 代码签名与 Mach-O 操作）的 **Swift Package Manager** 封装，底层为 C++/Objective-C++。日常 Swift 开发请依赖 **`ZsignSwift`** 并 `import Zsign`；仅在 Objective-C/自定义桥接需要时再直接依赖 **`zsign`** 模块。

## 环境要求

- Swift **5.8+**
- 平台：iOS 12+、macOS 10.15+、tvOS 12+、watchOS 8+、visionOS 1.3+
- 依赖：通过 SPM 拉取 [OpenSSL](https://github.com/krzyzanowskim/OpenSSL)

## 引入方式

**本地路径**（同仓库或拷贝目录）示例：

```swift
dependencies: [
    .package(path: "../swift")  // 按实际相对路径修改
],
targets: [
    .target(
        name: "YourApp",
        dependencies: [
            .product(name: "ZsignSwift", package: "Zsign")
        ]
    )
]
```

`Package.swift` 里包名为 `Zsign`；若文件夹名不是 `Zsign`，在 Xcode 中添加本地包时注意与 `package` 名称一致，或使用远程 Git 地址与版本号。

## 产物说明

| 产物 | 适用场景 |
|------|----------|
| **ZsignSwift** | Swift：`import Zsign`，使用下文 API。 |
| **zsign** | C/ObjC 模块头文件 `ZSign.h`、Objective-C++ 或自行桥接。 |

## Swift API 一览

所有接口均在 `public enum Zsign` 上，以**静态方法**调用。路径参数为**文件系统路径**（真机/模拟器上一般为绝对路径）。

| 方法 | 作用 |
|------|------|
| `checkSigned(appExecutable:)` | 判断 Mach-O 可执行文件是否具备有效代码签名。 |
| `injectDyLib(appExecutable:with:weak:)` | 注入 dylib 的加载命令（如 `@rpath/...`）。 |
| `removeDylibs(appExecutable:using:)` | 按安装名移除对应 load command。 |
| `listDylibs(appExecutable:)` | 列出 load command 中的 dylib 路径。 |
| `changeDylibPath(appExecutable:for:with:)` | 将某一 dylib 安装名替换为另一个（受 Mach-O 槽位长度等限制，与上游行为一致）。 |
| `sign(...)` | 对 **`.app` 目录（bundle）** 进行签名。可选：`removeUISupportedDevices`（`-U`）、`removeWatchApp`（`-W`）、`enableDocuments`（`-S`）、`minOSVersion`（`-M`）、`removeExtensions`（`-E`）、`zh`、`logHandler`（仅本轮签名期间的实时日志）。 |
| `signIPA(...)` | **签名并输出 IPA**：输入可为已有 `.ipa`（内部先解压）或 **`.app` 目录**；输出为新的 `.ipa`。底层 minizip，与命令行 `-o` 思路一致；支持 `zipLevel`、`tempFolderPath`、`zh`、`logHandler`；ZIP 内文件名为 UTF-8。 |
| `archiveFolderToIPA(...)` | **仅压缩、不签名**：`folderPath` **必须**为 **`Payload`** 目录；**只检查 Payload 下一级**（不递归子文件夹），其中须有且仅有一个 **`.app`**（即 `Payload/xxx.app`）。 |
| `setLogHandler(_:)` | 注册全局实时日志回调（每条 `ZLog` 一行）；传 `nil` 取消。 |
| `checkRevokage(...)` | 证书 OCSP 相关检查（异步回调），见下文。 |

## 示例

### 检查是否已签名

```swift
import Zsign

let ok = Zsign.checkSigned(appExecutable: "/path/to/MyApp.app/MyApp")
```

### 注入 / 列举 / 移除 / 替换 dylib 路径

```swift
_ = Zsign.injectDyLib(appExecutable: bin, with: "@rpath/Foo.framework/Foo", weak: true)
let libs = Zsign.listDylibs(appExecutable: bin)
_ = Zsign.removeDylibs(appExecutable: bin, using: ["@rpath/Old.dylib"])
_ = Zsign.changeDylibPath(appExecutable: bin, for: oldPath, with: newPath)
```

### 签名应用包

传入 `.app` 目录路径、描述文件（`.mobileprovision`）、P12、密码及可选的 entitlements 路径。不需要改动的参数可传空字符串（与底层约定一致）。

```swift
let success = Zsign.sign(
    appPath: "/path/to/My.app",
    provisionPath: "/path/to/profile.mobileprovision",
    p12Path: "/path/to/key.p12",
    p12Password: "密码",
    entitlementsPath: "/path/to/app.entitlements",
    customIdentifier: "com.example.app",
    customName: "显示名",
    customVersion: "1.0.0",
    adhoc: false,
    removeProvision: false,
    removeUISupportedDevices: false,
    removeWatchApp: false,
    enableDocuments: false,
    minOSVersion: "",
    removeExtensions: false,
    zh: false
) { signed, error in
    if !signed {
        print(error?.localizedDescription ?? "签名失败")
    }
}
```

仅 Ad-hoc 签名（无开发者证书身份）：

```swift
_ = Zsign.sign(appPath: "/path/to/My.app", adhoc: true)
```

### 签名并导出 IPA（`signIPA`）

输入可为 **`.app` 目录**或已有 **`.ipa`**（会先解压再签名、再打包）。`outputPath` 为目标 `.ipa` 路径。

```swift
_ = Zsign.signIPA(
    inputPath: "/path/to/My.app",           // 或 "/path/to/existing.ipa"
    outputPath: "/path/to/out.ipa",
    provisionPath: "/path/to/profile.mobileprovision",
    p12Path: "/path/to/key.p12",
    p12Password: "密码",
    zipLevel: 6,
    tempFolderPath: "",                    // 空则使用系统临时目录
    zh: true,
    logHandler: { line in print(line, terminator: "") }
) { success, error in
    if !success { print(error?.localizedDescription ?? "失败") }
}
```

### 仅将 Payload 目录树打成 IPA（`archiveFolderToIPA`）

在已具备 **`…/Payload/xxx.app/…`**、**不需要再签名**时，只生成 `.ipa` 压缩包。

- **`folderPath` 必须指向 `Payload` 目录本身**（不能传其父目录）；**只统计直接子项**（不遍历子目录），其中须有且仅有一个 **`.app`**（若 `.app` 在子文件夹内则不会被计入）。

```swift
_ = Zsign.archiveFolderToIPA(
    folderPath: "/path/to/staging/Payload", // 名为 Payload 的文件夹，内含 Your.app
    outputPath: "/path/to/out.ipa",
    zipLevel: 6,
    zh: true
) { success, error in
    if !success { print(error?.localizedDescription ?? "失败") }
}
```

### 实时日志

每条 `ZLog` 在经 `zlog_i18n` 处理后除写入标准输出（并已 `fflush`，行为接近 `swift-old`）外，可通过回调在 Swift 侧实时接收（UTF-8 文本一行）：

```swift
Zsign.setLogHandler { line in
    // 可能在后台线程回调，刷新 UI 请切主线程 / MainActor
    print(line, terminator: "")
}

Zsign.setLogHandler(nil) // 结束监听
```

仅在一次 `sign` 期间启用（函数返回后会清除回调，与 `setLogHandler(nil)` 等效；若曾设置全局 `setLogHandler` 会被本轮覆盖）：

```swift
Zsign.sign(appPath: path, logHandler: { line in
    print(line, terminator: "")
})
```

### 吊销 / OCSP 检查（`checkRevokage`）

底层通过 **OCSP** 与证书链交互。完成回调中的 `Int32` 为状态码、`Date?` 为过期时间、`String?` 为错误说明（具体含义与桥接层及 OpenSSL 行为一致，可与上游 zsign 对照）。

```swift
Zsign.checkRevokage(
    provisionPath: "/path/to/profile.mobileprovision",
    p12Path: "/path/to/key.p12",
    p12Password: "密码"
) { status, expiration, message in
    // 若更新 UI，请切回主线程
}
```

## 命令行编译

```bash
cd swift
swift build
```

针对真机架构请在 Xcode 中打开 Package 或使用 `xcodebuild` 指定 destination。

## 说明与限制

- 所有操作针对**磁盘上的文件**；请确保应用具备读写相应路径的权限（沙盒、企业分发场景等需自行处理）。
- 本 SPM 目标**排除**命令行入口 `zsign.cpp` 以及 `metadata.cpp`、`certcheck.cpp` 等；**IPA 打包/重打包**仍通过 `signIPA`、`archiveFolderToIPA` 与 minizip 提供，详见 `Package.swift` 中的 `sources` 与 `exclude`。
- 英文说明见 [README.md](./README.md)。

## 许可

以仓库根目录及上游 zsign 所附许可证为准。
