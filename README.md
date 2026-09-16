# rtdump-il2cpp-ps5

A PS5 port of [DeathRGH/rtdump-il2cpp-ps4](https://github.com/DeathRGH/rtdump-il2cpp-ps4).

Dumps the il2cpp runtime of a Unity game running on PS5 into:

| File | Contents |
|---|---|
| `/data/il2cpp/dump.cs` | C# class / method / field dump with RVAs |
| `/data/il2cpp/il2cpp.h` | C++ struct header (value-type structs, vtables) |

---

## How it works

On PS5, Unity games use the same `Il2CppUserAssemblies` native module and the
same exported C API as on PS4.  This port replaces:

| PS4 | PS5 |
|---|---|
| Visual Studio / PlayStation4 SDK | clang via **ps5-payload-sdk** |
| `.prx` / `.sprx` entry (`module_start`) | Flat ELF entry (`payload_main`) |
| `<kernel.h>` / `<libdbg.h>` / `<scebase.h>` | Inline type defs + ps5-payload-sdk stubs |
| `scePthread_*` | POSIX `pthread_*` |
| `sceKernelMkdir` | POSIX `mkdir` |
| `SceDbgModule` (int32) | `SceKernelModule` (int32 – same wire type) |
| `SceKernelModuleInfo.segmentInfo[0].address` | Same field (PS5-compatible struct) |

Everything else – all il2cpp logic, struct generation, metadata parsing – is
unchanged from the original.

---

## Requirements

### Build host (Linux / macOS)

1. Clone and set up [ps5-payload-dev/sdk](https://github.com/ps5-payload-dev/sdk):

   ```sh
   git clone https://github.com/ps5-payload-dev/sdk.git /opt/ps5-payload-sdk
   cd /opt/ps5-payload-sdk
   ./setup.sh          # downloads & builds the clang toolchain
   sudo -E ./libcxx.sh # builds LLVM libc++ (needed for std::string, mutex …)
   ```

2. Export the SDK path:

   ```sh
   export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
   ```

### PS5 console

- Jailbroken with a kernel exploit for your firmware.
- A payload loader listening on TCP port **9021**
  (e.g. [etaHEN](https://github.com/LightningMods/etaHEN) or
  [ps5-payload-loader](https://github.com/ps5-payload-dev/loader)).
- `/data` writable (standard on most jailbreaks).  
  If not, the dumper falls back to `/mnt/usb0/il2cpp` (USB stick in slot 0).
- A Unity il2cpp game running in the foreground.

> **Note on ps5debug-NG**  
> You can also inject the ELF into a specific game process using ps5debug-NG's
> `CMD_PROC_ELF` command.  The dumper auto-polls until il2cpp is ready, so
> timing is not critical.

---

## Build

```sh
make
```

Output: `rtdump-il2cpp-ps5.elf`

---

## Deploy

```sh
make send PS5_HOST=192.168.1.42
# or manually:
nc -q 1 192.168.1.42 9021 < rtdump-il2cpp-ps5.elf
```

---

## Usage

1. Launch a Unity il2cpp game on the PS5.
2. Send the ELF payload to your loader.
3. A system notification *"rtdump-il2cpp: dumper started"* confirms injection.
4. Wait for the dump to finish (typically 10–120 s depending on game size).
5. Retrieve `/data/il2cpp/dump.cs` and `/data/il2cpp/il2cpp.h` via FTP.

---

## Finding method RVAs

The `dump.cs` output includes both the **RVA** (relative to module base) and
the absolute **VA** for each method:

```
// RVA: 0x1A3F40 VA: 0x7FFA001A3F40
public static void SomeMethod() { }
```

Add the module base you observe in a debugger (or from the dump log) to the
RVA to find the method at runtime.

---

## Supported Unity il2cpp versions

| Metadata version | Status |
|---|---|
| 29 (Unity 2020.x – 2021.x) | ✅ Supported |
| 27 / 28 | Struct-gen skipped; dump.cs still generated |
| Others | Struct-gen skipped; dump.cs still generated |

To add a new metadata version, extend `header_constants.h` with a new
`header_vNN` string and add a `case NN:` in `il2cpp_struct_gen.cpp`.

---

## File layout

```
rtdump-il2cpp-ps5/
├── Makefile
├── README.md
└── src/
    ├── main.cpp              Entry point (payload_main)
    ├── ps5_sce.h             PS5 SCE type / function declarations
    ├── stdafx.h              Unified include header
    ├── sig.h / sig.cpp       Pattern scanner (sceKernelVirtualQuery based)
    ├── il2cpp_api.h/.cpp     il2cpp function-pointer resolution
    ├── il2cpp_class.h        il2cpp runtime struct layout
    ├── il2cpp_tabledefs.h    il2cpp enum / flag constants
    ├── il2cpp_dump.h/.cpp    dump.cs generation
    ├── il2cpp_struct_gen.h/.cpp  il2cpp.h generation
    └── header_constants.h    C++ header template strings per metadata version
```

---

## Credits

- Original PS4 tool: [DeathRGH](https://github.com/DeathRGH/rtdump-il2cpp-ps4)
- PS5 payload SDK: [ps5-payload-dev](https://github.com/ps5-payload-dev/sdk)
- PS5 debug reference: [OpenSourcereR-dev/ps5debug-NG](https://github.com/OpenSourcereR-dev/ps5debug-NG)
