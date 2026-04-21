# Zsign (Swift Package)

Swift Package Manager wrapper around the **zsign** signing and Mach-O utilities (C++/Objective-C++ core). Use **`ZsignSwift`** for the Swift API; depend on **`zsign`** only if you need the C module from Objective-C or custom bridging.

## Requirements

- Swift **5.8+**
- Platforms: iOS 12+, macOS 10.15+, tvOS 12+, watchOS 8+, visionOS 1.3+
- Dependency: [OpenSSL](https://github.com/krzyzanowskim/OpenSSL) (resolved via SPM)

## Add the package

**Local path** (monorepo / vendored copy):

```swift
dependencies: [
    .package(path: "../swift")  // adjust to your layout
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

The package name in `Package.swift` is `Zsign`; when using `path:`, the folder name is often `swift`—set `package` name in Xcode or match the directory name if you rename it.

**Remote** (after you publish this repo): use the Git URL and a version or branch instead of `path`.

## Products

| Product       | Use when |
|---------------|----------|
| **ZsignSwift** | Swift code: `import Zsign` and `Zsign.*` APIs below. |
| **zsign**      | Low-level `Zsign` C/ObjC module (`ZSign.h`), Objective-C++, or custom bridges. |

## Swift API overview

All APIs are on `public enum Zsign` (static methods). Paths are **filesystem paths** (typically absolute paths on device or simulator).

| Method | Purpose |
|--------|---------|
| `checkSigned(appExecutable:)` | Returns whether the Mach-O executable has a valid code signature. |
| `injectDyLib(appExecutable:with:weak:)` | Injects a dylib load command (`@rpath/...`, etc.). |
| `removeDylibs(appExecutable:using:)` | Removes load commands matching the given install names. |
| `listDylibs(appExecutable:)` | Lists dylib paths from load commands. |
| `changeDylibPath(appExecutable:for:with:)` | Replaces one dylib install name with another (in-place, same slot size constraints as upstream). |
| `sign(...)` | Signs an **`.app` bundle** (folder path). Matches CLI flags: `removeUISupportedDevices` (`-U`), `removeWatchApp` (`-W`), `enableDocuments` (`-S`), `minOSVersion` (`-M`), `removeExtensions` (`-E`). Set `zh: true` for Chinese `ZLog` output during the call (via `ZSIGN_LANG`). Optional `logHandler` installs a **real-time** log sink for that call only. |
| `signIPA(...)` | Signs and writes an **`.ipa`**: input may be an existing `.ipa` (extracted internally) or an **`.app` folder**; output path is the new `.ipa`. Uses minizip (same idea as CLI `-o`), UTF-8 entry names, optional `zipLevel` (0–9), `tempFolderPath`, `zh`, `logHandler`. |
| `archiveFolderToIPA(...)` | **Zip-only**: `folderPath` must be the **`Payload` directory** (path ends with `/Payload`). Only **immediate children** of `Payload` are checked (no recursion); among those there must be **exactly one** **`*.app` bundle** (`Payload/xxx.app`). No signing. Same compression logging as `signIPA`. |
| `extractIPA(...)` | **Unzip-only**: expands an **`.ipa`** (or any ZIP) into a **folder**. Validates zip magic like `signIPA`; same path-safety rules as the internal unzip step. Start line is `>>> Unzip:` with **basename + size only** (no `-> output` path, same as the unzip step inside `signIPA`), then **per-entry progress** like compression (`Unzipping files` / `正在解压`), UTF-8. Optional `zh`, `logHandler`, `completion`. |
| `setLogHandler(_:)` | Registers a **real-time** callback for every `ZLog` line (UTF-8, after i18n); pass `nil` to clear. |
| `checkRevokage(...)` | OCSP-style certificate check (async); see below. |

## Usage examples

### Check signature

```swift
import Zsign

let ok = Zsign.checkSigned(appExecutable: "/path/to/MyApp.app/MyApp")
```

### Inject / list / remove / replace dylib path

```swift
_ = Zsign.injectDyLib(appExecutable: bin, with: "@rpath/Foo.framework/Foo", weak: true)
let libs = Zsign.listDylibs(appExecutable: bin)
_ = Zsign.removeDylibs(appExecutable: bin, using: ["@rpath/Old.dylib"])
_ = Zsign.changeDylibPath(appExecutable: bin, for: oldPath, with: newPath)
```

### Sign an app bundle

Provide paths to the `.app` directory, provisioning profile (`.mobileprovision`), P12, password, and optional entitlements plist. Empty strings mean “leave default / omit” where the core allows.

```swift
let success = Zsign.sign(
    appPath: "/path/to/My.app",
    provisionPath: "/path/to/profile.mobileprovision",
    p12Path: "/path/to/key.p12",
    p12Password: "secret",
    entitlementsPath: "/path/to/app.entitlements",
    customIdentifier: "com.example.app",
    customName: "Display Name",
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
        print(error?.localizedDescription ?? "sign failed")
    }
}
```

Ad-hoc signing (no identity):

```swift
_ = Zsign.sign(appPath: "/path/to/My.app", adhoc: true)
```

### Sign and output an IPA (`signIPA`)

Input can be a `.app` directory or an existing `.ipa` (it will be unzipped, signed, and repacked). Output must be the destination `.ipa` path.

```swift
_ = Zsign.signIPA(
    inputPath: "/path/to/My.app",           // or "/path/to/existing.ipa"
    outputPath: "/path/to/out.ipa",
    provisionPath: "/path/to/profile.mobileprovision",
    p12Path: "/path/to/key.p12",
    p12Password: "secret",
    zipLevel: 6,
    tempFolderPath: "",                    // empty = system temp
    zh: false,
    logHandler: { line in print(line, terminator: "") }
) { success, error in
    if !success { print(error?.localizedDescription ?? "failed") }
}
```

### Pack a Payload tree into an IPA (`archiveFolderToIPA`)

Use when you already have **`…/Payload/Your.app/…`** and only need a `.ipa` zip—**no signing**.

- **`folderPath` must be the `Payload` folder itself** (not its parent). Only **top-level entries** under `Payload` are considered (nested paths like `Payload/A/B.app` are **not** scanned). There must be **exactly one** **`.app`** among those immediate children.

```swift
_ = Zsign.archiveFolderToIPA(
    folderPath: "/path/to/staging/Payload", // directory named Payload, containing Your.app
    outputPath: "/path/to/out.ipa",
    zipLevel: 6,
    zh: false
) { success, error in
    if !success { print(error?.localizedDescription ?? "failed") }
}
```

### Extract an IPA to a folder (`extractIPA`)

Use when you only need to unpack an **`.ipa`** (or another ZIP) to a directory—**no signing**. The destination folder is removed first if it already exists (same behavior as the unzip step inside `signIPA`). The first log line is **`>>> Unzip:`** with **basename + file size only** (no `-> output` path, same as the unzip step inside `signIPA`). Progress lines mirror **`signIPA` / `archiveFolderToIPA` compression** (entry index, basename, MB, overall percent; heartbeat on large files). Unzip lines use **`Unzipping files` / `正在解压`**, not “Compressing”.

```swift
_ = Zsign.extractIPA(
    ipaPath: "/path/to/App.ipa",
    outputFolderPath: "/path/to/extracted",   // directory will be created/overwritten
    zh: false,
    logHandler: { line in print(line, terminator: "") }
) { success, error in
    if !success { print(error?.localizedDescription ?? "failed") }
}
```

### Real-time log output

Each `ZLog` line (after `zlog_i18n`, respecting `zh` / `LANG` / `ZSIGN_LANG`) is also written to **stdout** with `fflush` (similar to `swift-old`). To stream logs into Swift (e.g. a text view), use a handler:

```swift
Zsign.setLogHandler { line in
    // May be called on a background thread; hop to MainActor for UI.
    print(line, terminator: "")
}

Zsign.setLogHandler(nil) // when done
```

Or only for one `sign` call (handler is cleared when `sign` returns):

```swift
Zsign.sign(appPath: path, logHandler: { line in
    print(line, terminator: "")
})
```

### Certificate revocation / OCSP (`checkRevokage`)

This calls into native code that performs an **OCSP** request. The completion handler receives a **status** (`Int32`), optional **expiration** `Date`, and optional **error string**. Interpretation follows the OpenSSL/OCSP flow used in the bridge (see upstream zsign behavior for status codes).

```swift
Zsign.checkRevokage(
    provisionPath: "/path/to/profile.mobileprovision",
    p12Path: "/path/to/key.p12",
    p12Password: "secret"
) { status, expiration, message in
    // Handle on main thread if you update UI
}
```

## Build from command line

```bash
cd swift
swift build
```

To build for a specific Apple platform, use `xcodebuild` or open a generated Xcode project (`swift package generate-xcodeproj` is deprecated; prefer opening `Package.swift` in Xcode).

## Notes

- Signing and Mach-O editing operate on **on-disk files**; ensure your app has appropriate sandbox / entitlement access to those paths.
- The core is shared with the **command-line zsign** tree. This SPM target **excludes** the CLI entrypoint (`zsign.cpp`) and tools such as `metadata.cpp` / `certcheck.cpp`; **IPA create/repack** is still provided via `signIPA` / `archiveFolderToIPA` and minizip—see `Package.swift` sources.
- For details on the underlying project, see the main repository README and [README.zh-CN.md](./README.zh-CN.md).

## License

Follow the license files in the repository root (zsign upstream and any additional notices shipped with this fork).
