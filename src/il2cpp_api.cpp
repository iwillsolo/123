#include "stdafx.h"

// ─── Global state ─────────────────────────────────────────────────────────────

uintptr_t      il2cpp_api::base_address = 0;
SceKernelModule il2cpp_api::handle      = -1;
bool           il2cpp_api::has_error    = false;

// ─── Function pointer globals ─────────────────────────────────────────────────

il2cpp_get_corlib_t il2cpp_get_corlib;

// assembly
il2cpp_assembly_get_image_t il2cpp_assembly_get_image;

// domain
il2cpp_domain_get_t            il2cpp_domain_get;
il2cpp_domain_get_assemblies_t il2cpp_domain_get_assemblies;

// image
il2cpp_image_get_class_t       il2cpp_image_get_class;
il2cpp_image_get_class_count_t il2cpp_image_get_class_count;
il2cpp_image_get_name_t        il2cpp_image_get_name;

// class
il2cpp_class_from_name_t           il2cpp_class_from_name;
il2cpp_class_from_system_type_t    il2cpp_class_from_system_type;
il2cpp_class_from_type_t           il2cpp_class_from_type;
il2cpp_class_get_fields_t          il2cpp_class_get_fields;
il2cpp_class_get_flags_t           il2cpp_class_get_flags;
il2cpp_class_get_interfaces_t      il2cpp_class_get_interfaces;
il2cpp_class_get_method_from_name_t il2cpp_class_get_method_from_name;
il2cpp_class_get_methods_t         il2cpp_class_get_methods;
il2cpp_class_get_name_t            il2cpp_class_get_name;
il2cpp_class_get_namespace_t       il2cpp_class_get_namespace;
il2cpp_class_get_parent_t          il2cpp_class_get_parent;
il2cpp_class_get_properties_t      il2cpp_class_get_properties;
il2cpp_class_get_type_t            il2cpp_class_get_type;
il2cpp_class_is_enum_t             il2cpp_class_is_enum;
il2cpp_class_is_valuetype_t        il2cpp_class_is_valuetype;

// field
il2cpp_field_get_flags_t        il2cpp_field_get_flags;
il2cpp_field_get_name_t         il2cpp_field_get_name;
il2cpp_field_get_offset_t       il2cpp_field_get_offset;
il2cpp_field_get_type_t         il2cpp_field_get_type;
il2cpp_field_static_get_value_t il2cpp_field_static_get_value;

// method
il2cpp_method_get_flags_t      il2cpp_method_get_flags;
il2cpp_method_get_name_t       il2cpp_method_get_name;
il2cpp_method_get_param_t      il2cpp_method_get_param;
il2cpp_method_get_param_count_t il2cpp_method_get_param_count;
il2cpp_method_get_param_name_t il2cpp_method_get_param_name;
il2cpp_method_get_return_type_t il2cpp_method_get_return_type;

// string
il2cpp_string_new_t il2cpp_string_new;

// type
il2cpp_type_get_name_t  il2cpp_type_get_name;
il2cpp_type_is_byref_t  il2cpp_type_is_byref;

// property
il2cpp_property_get_get_method_t il2cpp_property_get_get_method;
il2cpp_property_get_set_method_t il2cpp_property_get_set_method;
il2cpp_property_get_name_t       il2cpp_property_get_name;

// ─── Internal helpers ─────────────────────────────────────────────────────────

uintptr_t il2cpp_api::resolve_function_by_name(const char *name,
                                                bool allow_fail) {
    if (handle < 0) {
        return 0;
    }

    void *addr = nullptr;
    int   rc   = sceKernelDlsym(handle, name, &addr);
    if (rc != SCE_OK || addr == nullptr) {
        if (!allow_fail) {
            il2cpp_api::has_error = true;
        }
        printf("[il2cpp_api] WARN: could not resolve \"%s\" (rc=0x%X)\n",
               name, rc);
        return 0;
    }

    return reinterpret_cast<uintptr_t>(addr);
}

// ─── Public API ───────────────────────────────────────────────────────────────

/*
 * On PS5, Unity games ship their il2cpp native library as one of:
 *   • Il2CppUserAssemblies.prx          (same name as PS4)
 *   • libIl2CppUserAssemblies.sprx      (alternative naming)
 *
 * We try each name in turn and stop at the first one that loads successfully.
 */
static const char *const kIl2CppModuleNames[] = {
    "Il2CppUserAssemblies.prx",
    "libIl2CppUserAssemblies.sprx",
    "Il2CppUserAssemblies",
    nullptr,
};

bool il2cpp_api::init() {
    il2cpp_api::has_error = false;

    // ── 1. Resolve module base (already loaded by the game) ──────────────────
    for (const char *const *name = kIl2CppModuleNames; *name; ++name) {
        il2cpp_api::base_address = resolve_module_base(*name);
        if (il2cpp_api::base_address != 0) {
            printf("[il2cpp_api::init] base_address 0x%lX (module: %s)\n",
                   il2cpp_api::base_address, *name);
            break;
        }
    }

    if (il2cpp_api::base_address == 0) {
        printf("[il2cpp_api::init] ERROR: il2cpp module not found in process "
               "module list.\n");
        return false;
    }

    // ── 2. Obtain a dlsym-capable handle ─────────────────────────────────────
    //
    // sceKernelLoadStartModule with an already-loaded path will typically
    // increment the refcount and hand back the existing handle – exactly what
    // we want so that sceKernelDlsym resolves symbols from that module's
    // export table.  We try each candidate path in order.
    for (const char *const *name = kIl2CppModuleNames; *name; ++name) {
        int res = 0;
        SceKernelModule m = sceKernelLoadStartModule(*name, 0, nullptr, 0,
                                                     nullptr, &res);
        if (m >= 0) {
            il2cpp_api::handle = m;
            printf("[il2cpp_api::init] handle %d (module: %s)\n", m, *name);
            break;
        }
    }

    if (il2cpp_api::handle < 0) {
        printf("[il2cpp_api::init] ERROR: failed to get a module handle for "
               "il2cpp.\n");
        return false;
    }

    // ── 3. Resolve all il2cpp function pointers ───────────────────────────────

    il2cpp_get_corlib =
        (il2cpp_get_corlib_t)resolve_function_by_name("il2cpp_get_corlib");

    il2cpp_assembly_get_image =
        (il2cpp_assembly_get_image_t)resolve_function_by_name(
            "il2cpp_assembly_get_image");

    il2cpp_domain_get =
        (il2cpp_domain_get_t)resolve_function_by_name("il2cpp_domain_get");
    il2cpp_domain_get_assemblies =
        (il2cpp_domain_get_assemblies_t)resolve_function_by_name(
            "il2cpp_domain_get_assemblies");

    // il2cpp_image_get_class / _count were added in Unity 2018.3 – allow fail.
    il2cpp_image_get_class =
        (il2cpp_image_get_class_t)resolve_function_by_name(
            "il2cpp_image_get_class", /*allow_fail=*/true);
    il2cpp_image_get_class_count =
        (il2cpp_image_get_class_count_t)resolve_function_by_name(
            "il2cpp_image_get_class_count", /*allow_fail=*/true);
    il2cpp_image_get_name =
        (il2cpp_image_get_name_t)resolve_function_by_name(
            "il2cpp_image_get_name");

    il2cpp_class_from_name =
        (il2cpp_class_from_name_t)resolve_function_by_name(
            "il2cpp_class_from_name");
    il2cpp_class_from_system_type =
        (il2cpp_class_from_system_type_t)resolve_function_by_name(
            "il2cpp_class_from_system_type");
    il2cpp_class_from_type =
        (il2cpp_class_from_type_t)resolve_function_by_name(
            "il2cpp_class_from_type");
    il2cpp_class_get_fields =
        (il2cpp_class_get_fields_t)resolve_function_by_name(
            "il2cpp_class_get_fields");
    il2cpp_class_get_flags =
        (il2cpp_class_get_flags_t)resolve_function_by_name(
            "il2cpp_class_get_flags");
    il2cpp_class_get_interfaces =
        (il2cpp_class_get_interfaces_t)resolve_function_by_name(
            "il2cpp_class_get_interfaces");
    il2cpp_class_get_method_from_name =
        (il2cpp_class_get_method_from_name_t)resolve_function_by_name(
            "il2cpp_class_get_method_from_name");
    il2cpp_class_get_methods =
        (il2cpp_class_get_methods_t)resolve_function_by_name(
            "il2cpp_class_get_methods");
    il2cpp_class_get_name =
        (il2cpp_class_get_name_t)resolve_function_by_name(
            "il2cpp_class_get_name");
    il2cpp_class_get_namespace =
        (il2cpp_class_get_namespace_t)resolve_function_by_name(
            "il2cpp_class_get_namespace");
    il2cpp_class_get_parent =
        (il2cpp_class_get_parent_t)resolve_function_by_name(
            "il2cpp_class_get_parent");
    il2cpp_class_get_properties =
        (il2cpp_class_get_properties_t)resolve_function_by_name(
            "il2cpp_class_get_properties");
    il2cpp_class_get_type =
        (il2cpp_class_get_type_t)resolve_function_by_name(
            "il2cpp_class_get_type");
    il2cpp_class_is_enum =
        (il2cpp_class_is_enum_t)resolve_function_by_name(
            "il2cpp_class_is_enum");
    il2cpp_class_is_valuetype =
        (il2cpp_class_is_valuetype_t)resolve_function_by_name(
            "il2cpp_class_is_valuetype");

    il2cpp_field_get_flags =
        (il2cpp_field_get_flags_t)resolve_function_by_name(
            "il2cpp_field_get_flags");
    il2cpp_field_get_name =
        (il2cpp_field_get_name_t)resolve_function_by_name(
            "il2cpp_field_get_name");
    il2cpp_field_get_offset =
        (il2cpp_field_get_offset_t)resolve_function_by_name(
            "il2cpp_field_get_offset");
    il2cpp_field_get_type =
        (il2cpp_field_get_type_t)resolve_function_by_name(
            "il2cpp_field_get_type");
    il2cpp_field_static_get_value =
        (il2cpp_field_static_get_value_t)resolve_function_by_name(
            "il2cpp_field_static_get_value");

    il2cpp_method_get_flags =
        (il2cpp_method_get_flags_t)resolve_function_by_name(
            "il2cpp_method_get_flags");
    il2cpp_method_get_name =
        (il2cpp_method_get_name_t)resolve_function_by_name(
            "il2cpp_method_get_name");
    il2cpp_method_get_param =
        (il2cpp_method_get_param_t)resolve_function_by_name(
            "il2cpp_method_get_param");
    il2cpp_method_get_param_count =
        (il2cpp_method_get_param_count_t)resolve_function_by_name(
            "il2cpp_method_get_param_count");
    il2cpp_method_get_param_name =
        (il2cpp_method_get_param_name_t)resolve_function_by_name(
            "il2cpp_method_get_param_name");
    il2cpp_method_get_return_type =
        (il2cpp_method_get_return_type_t)resolve_function_by_name(
            "il2cpp_method_get_return_type");

    il2cpp_string_new =
        (il2cpp_string_new_t)resolve_function_by_name("il2cpp_string_new");

    il2cpp_type_get_name =
        (il2cpp_type_get_name_t)resolve_function_by_name(
            "il2cpp_type_get_name");
    il2cpp_type_is_byref =
        (il2cpp_type_is_byref_t)resolve_function_by_name(
            "il2cpp_type_is_byref");

    il2cpp_property_get_get_method =
        (il2cpp_property_get_get_method_t)resolve_function_by_name(
            "il2cpp_property_get_get_method");
    il2cpp_property_get_set_method =
        (il2cpp_property_get_set_method_t)resolve_function_by_name(
            "il2cpp_property_get_set_method");
    il2cpp_property_get_name =
        (il2cpp_property_get_name_t)resolve_function_by_name(
            "il2cpp_property_get_name");

    return !il2cpp_api::has_error;
}
