# cclank  
A C++ build and project manager inspired by Rust’s build system.
---
---

## Usage

```bash
cclank new <n>        # Create new project with default structure  
cclank build          # Build using dev profile  
cclank build --release   # Build using release profile  
cclank run            # Build and run (dev profile)  
cclank run --release  # Build and run (release profile)  
cclank clean          # Remove build directory  
````

---

## Supported Build Types and Platforms

*(No full cross-compiling — see note below.)*

| Type            | Windows        | Linux      | macOS        | Notes                                              |
| --------------- | -------------- | ---------- | ------------ | -------------------------------------------------- |
| **Binary**      | `.exe`         | *(no ext)* | *(no ext)*   | Standard executable                                |
| **Static Lib**  | `.lib` or `.a` | `lib*.a`   | `lib*.a`     | Uses `ar` tool, adds `lib` prefix on Unix          |
| **Dynamic Lib** | `.dll`         | `lib*.so`  | `lib*.dylib` | Uses `-shared`, `-fPIC` on Unix, adds `lib` prefix |

---

## TOML to g++ Flag Mapping

| cyz.toml Setting    | g++ Flags                                          |
| ------------------- | -------------------------------------------------- |
| **Optimization**    |                                                    |
| `opt-level = 0`     | *(no flag)* or `-O0`                               |
| `opt-level = 1`     | `-O1`                                              |
| `opt-level = 2`     | `-O2`                                              |
| `opt-level = 3`     | `-O3`                                              |
| **Debug**           |                                                    |
| `debug = true`      | `-g`                                               |
| `debug = false`     | *(no flag)*                                        |
| **LTO**             |                                                    |
| `lto = "fat"`       | `-flto`                                            |
| `lto = "thin"`      | `-flto=thin` *(if supported)*                      |
| `lto = "off"`       | *(no flag)*                                        |
| **Codegen Units**   |                                                    |
| `codegen-units = N` | `-flto=N` *(when LTO enabled)*                     |
| **Platform/Type**   |                                                    |
| `type = "bin"`      | *(default exe)*                                    |
| `type = "lib"`      | `-c` *(compile only, then `ar rcs libname.a *.o`)* |
| `type = "dylib"`    | `-shared` *(creates `.dll` on Windows)*            |
| **Icon (Windows)**  |                                                    |
| `icon = "icon.ico"` | Link with `resource.o` from `.rc` file             |
| **Standard flags**  |                                                    |
| *(always include)*  | `-static -lshlwapi` *(for Windows)*                |

---

## Example `cclank.toml`

```
[package]
name = "yippee"
version = "0.1.0"
platform = "win"
type = "bin"
icon = "icon.ico"

[features]

[profile.dev]
opt-level = 0
debug = true
codegen-units = 4

[profile.release]
opt-level = 3
debug = false
lto = "fat"
codegen-units = 1
```

---

## Example Source Structure
```
yippee/
├─ cclank.toml
├─ icon.ico
└─ src/
   └─ main.cpp
```

---

## Platform Support

This build tool is **designed to run only on Windows**, but it can **compile static and dynamically linked libraries** for other platforms.
However, it **cannot produce full runnable binaries** for non-Windows systems.

---

## Disclaimer

This program may have been AI-generated — I can’t prove it because I paid someone on an anonymous platform to code it for me (I was lazy).
I provided the full specification and created the icon myself.
I’ve personally reviewed the source code and found no malicious behavior — it’s functional and safe to use.

---
# cclank
A C++ build and project manager inspired by Rust’s build system.
