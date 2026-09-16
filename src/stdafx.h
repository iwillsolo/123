#pragma once

// ─── Standard C / POSIX ───────────────────────────────────────────────────────
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>   // mkdir
#include <pthread.h>    // POSIX threads (ps5-payload-sdk exposes these)

// ─── Standard C++ ─────────────────────────────────────────────────────────────
// Requires libcxx to be installed via:  sudo -E ./libcxx.sh
#include <mutex>
#include <string>
#include <sstream>
#include <fstream>
#include <vector>
#include <algorithm>

// ─── PS5 SCE types & API ──────────────────────────────────────────────────────
#include "ps5_sce.h"

// ─── il2cpp headers ───────────────────────────────────────────────────────────
#include "sig.h"
#include "il2cpp_class.h"
#include "il2cpp_tabledefs.h"
#include "il2cpp_api.h"
#include "il2cpp_dump.h"

// ─── struct gen ───────────────────────────────────────────────────────────────
#include "header_constants.h"
#include "il2cpp_struct_gen.h"
