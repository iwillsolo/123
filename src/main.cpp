/*
 * rtdump-il2cpp-ps5 – main.cpp
 *
 * Entry point for the PS5 payload ELF.
 *
 * Execution model
 * ───────────────
 * The ps5-payload-sdk builds a self-contained FreeBSD ELF that is sent to
 * the console over a TCP socket (default port 9021) by a loader such as
 * etaHEN or ps5-payload-loader.  The loader maps and executes the ELF
 * *inside* the target process (typically a running Unity game) by injecting
 * a new thread.  This is functionally equivalent to the PS4 sprx model:
 * we share the address space with the game and can call il2cpp directly.
 *
 * If you are using ps5debug-NG you can also send this ELF via CMD_PROC_ELF
 * against any process – the effect is identical.
 *
 * Output
 * ──────
 *   /data/il2cpp/dump.cs    – C# class / method / field dump
 *   /data/il2cpp/il2cpp.h   – C++ struct header (value-type structs only)
 *
 * These paths are accessible after the dump from a PC via FTP on port 2121
 * (enabled separately).
 */

#include "stdafx.h"

// ─── Wait helper ─────────────────────────────────────────────────────────────

static void sleep_ms(unsigned int ms) {
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, nullptr);
}

// ─── Output directory ─────────────────────────────────────────────────────────

static bool ensure_output_dir() {
    // Try /data/il2cpp first (writable on most jailbroken PS5 firmwares).
    const char *dir = "/data/il2cpp";
    if (mkdir(dir, 0777) == 0 || errno == EEXIST) {
        printf("[main] Output directory: %s\n", dir);
        return true;
    }
    // Fallback: /mnt/usb0/il2cpp (USB stick at slot 0).
    dir = "/mnt/usb0/il2cpp";
    if (mkdir(dir, 0777) == 0 || errno == EEXIST) {
        printf("[main] Output directory (USB fallback): %s\n", dir);
        return true;
    }
    printf("[main] ERROR: cannot create output directory!\n");
    return false;
}

// ─── il2cpp readiness poll ────────────────────────────────────────────────────

/*
 * Unity initialises il2cpp asynchronously.  We cannot call any il2cpp_*
 * function until the runtime is fully up.  The reliable indicator is that
 * the module is loaded AND il2cpp_domain_get() returns a non-null pointer.
 *
 * We poll up to ~30 s in 500 ms intervals before giving up.
 */
static bool wait_for_il2cpp(unsigned int timeout_ms = 30000,
                             unsigned int poll_ms    = 500) {
    printf("[main] Waiting for il2cpp to initialise...\n");

    for (unsigned int elapsed = 0; elapsed < timeout_ms; elapsed += poll_ms) {
        // Quick module-list scan: is any variant of the module present?
        static const char *const kNames[] = {
            "Il2CppUserAssemblies",
            "libIl2CppUserAssemblies",
            nullptr,
        };

        bool module_present = false;
        for (const char *const *n = kNames; *n; ++n) {
            if (resolve_module_base(*n) != 0) {
                module_present = true;
                break;
            }
        }

        if (!module_present) {
            sleep_ms(poll_ms);
            continue;
        }

        // Module is mapped – try to resolve API and check the domain.
        if (!il2cpp_api::init()) {
            // init() may fail if exports aren't ready yet; retry.
            sleep_ms(poll_ms);
            continue;
        }

        if (il2cpp_domain_get && il2cpp_domain_get() != nullptr) {
            printf("[main] il2cpp ready (base 0x%lX).\n",
                   il2cpp_api::base_address);
            return true;
        }

        sleep_ms(poll_ms);
    }

    printf("[main] Timed out waiting for il2cpp!\n");
    return false;
}

// ─── Payload entry point ──────────────────────────────────────────────────────

/*
 * payload_main() is the conventional entry called by the ps5-payload-sdk CRT.
 * The 'args' pointer carries launch parameters from the loader – we ignore
 * them here since rtdump needs no configuration from the host.
 *
 * If your loader calls a plain 'main(int argc, char **argv)' simply rename
 * this function accordingly; the body is identical.
 */
extern "C" void payload_main(void *args) {
    (void)args;

    printf("========================================\n");
    printf("  rtdump-il2cpp-ps5  (by DeathRGH, PS5  \n");
    printf("  port by community)                     \n");
    printf("========================================\n");

    // 1. Ensure /data/il2cpp exists.
    if (!ensure_output_dir()) {
        return;
    }

    // 2. Poll until Unity's il2cpp runtime is fully up.
    if (!wait_for_il2cpp()) {
        return;
    }

    // 3. Launch the dump thread (writes dump.cs).
    //    il2cpp_dumper::run() acquires dumper_lock for the duration of the
    //    dump so that the struct-gen thread below starts only afterwards.
    printf("[main] Launching dump thread...\n");
    il2cpp_dumper::run();

    // 4. Launch the struct-gen thread (writes il2cpp.h).
    //    It blocks on dumper_lock, so it will start as soon as the dump is
    //    complete, even though we launch it right now.
    printf("[main] Launching struct-gen thread...\n");
    il2cpp_struct_gen::run();

    // 5. Send a PS5 system notification so the user can see something happened.
    //    The notification API is best-effort; ignore errors.
    const char *notify_msg = "rtdump-il2cpp: dumper started";
    sceKernelSendNotificationRequest(0, notify_msg,
                                     strlen(notify_msg), 0);

    printf("[main] Both threads launched – payload returning.\n");
}
