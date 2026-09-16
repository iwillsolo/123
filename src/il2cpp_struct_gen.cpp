#include "stdafx.h"

// ─── Metadata version reader ──────────────────────────────────────────────────

/*
 * On both PS4 and PS5, Unity game data lives under /app0/.
 * The global-metadata.dat file is always at:
 *   /app0/Media/Metadata/global-metadata.dat
 *
 * The il2cpp metadata version is stored as an int32 at byte offset 0x04.
 */
int32_t get_il2cpp_version() {
    const char *file_path = "/app0/Media/Metadata/global-metadata.dat";

    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        printf("[struct_gen] Failed to open \"%s\"!\n", file_path);
        return -1;
    }

    file.seekg(0x04, std::ios::beg);
    if (!file) {
        printf("[struct_gen] Error seeking in \"%s\"!\n", file_path);
        return -1;
    }

    int32_t value = -1;
    file.read(reinterpret_cast<char *>(&value), sizeof(value));
    if (!file) {
        printf("[struct_gen] Error reading from \"%s\"!\n", file_path);
        return -1;
    }

    file.close();
    return value;
}

// ─── Type-name helpers (C++ / header-gen variants) ───────────────────────────

std::string resolve_full_type(Il2CppClass *klass) {
    std::string full_name = klass->namespaze;
    if (!full_name.empty()) {
        std::replace(full_name.begin(), full_name.end(), '.', '_');
        full_name += "_";
    }
    full_name += il2cpp_class_get_name(klass);
    return full_name;
}

std::string il2cpp_class_get_name_cpp(Il2CppClass *klass) {
    std::string n = il2cpp_class_get_name(klass);

    if (n == "Object")  return "object";
    if (n == "Void")    return "void";
    if (n == "Boolean") return "bool";
    if (n == "Byte")    return "uint8_t";
    if (n == "SByte")   return "int8_t";
    if (n == "Int16")   return "int16_t";
    if (n == "UInt16")  return "uint16_t";
    if (n == "Int32")   return "int32_t";
    if (n == "UInt32")  return "uint32_t";
    if (n == "Int64")   return "int64_t";
    if (n == "UInt64")  return "uint64_t";
    if (n == "Single")  return "float";
    if (n == "Double")  return "double";
    if (n == "Char")    return "char";
    if (n == "String")  return "struct System_String_o*";

    if (n == "Object[]")  return "object[]";
    if (n == "Boolean[]") return "bool[]";
    if (n == "Byte[]")    return "byte[]";
    if (n == "SByte[]")   return "sbyte[]";
    if (n == "Int16[]")   return "short[]";
    if (n == "UInt16[]")  return "ushort[]";
    if (n == "Int32[]")   return "int[]";
    if (n == "UInt32[]")  return "uint[]";
    if (n == "Int64[]")   return "long[]";
    if (n == "UInt64[]")  return "ulong[]";
    if (n == "Single[]")  return "float[]";
    if (n == "Double[]")  return "double[]";
    if (n == "Char[]")    return "char[]";
    if (n == "String[]")  return "string[]";

    return "struct " + resolve_full_type(klass);
}

std::string resolve_generic_type_cpp(Il2CppClass *klass) {
    if (!klass->generic_class) {
        return il2cpp_class_get_name_cpp(klass);
    }

    std::string generic_name = klass->name;
    size_t pos = generic_name.find("`");
    if (pos != std::string::npos) {
        generic_name.erase(pos, 2);
    }
    generic_name += "<";

    Il2CppGenericInst *inst = klass->generic_class->context;
    for (size_t i = 0; i < inst->type_argc; ++i) {
        if (i > 0) generic_name += ", ";
        Il2CppClass *arg_class = il2cpp_class_from_type(inst->type_argv[i]);
        generic_name += resolve_generic_type_cpp(arg_class);
    }

    generic_name += ">";
    return generic_name;
}

std::string resolve_type_cpp(const Il2CppType *type) {
    switch (type->type) {
    case IL2CPP_TYPE_SZARRAY: {
        std::string element = resolve_type_cpp(type->data.type);
        return element + "[]";
    }
    default:
        Il2CppClass *klass = il2cpp_class_from_type(type);
        return resolve_generic_type_cpp(klass);
    }
}

// ─── Struct generation routines ───────────────────────────────────────────────

std::string generate_field(FieldInfo *field) {
    std::stringstream output;
    output << "\t";
    const Il2CppType *field_type      = il2cpp_field_get_type(field);
    std::string       field_type_name = resolve_type_cpp(field_type);
    output << field_type_name << " " << il2cpp_field_get_name(field) << ";\n";
    return output.str();
}

std::string generate_fields(Il2CppClass *klass, std::string full_type_name) {
    std::stringstream output;

    Il2CppClass *parent_class = il2cpp_class_get_parent(klass);
    if (parent_class && parent_class->field_count > 0) {
        std::string full_parent = resolve_full_type(parent_class);
        output << "struct " << full_type_name << "_Fields : "
               << full_parent << "_Fields {\n";
    }
    else {
        output << "struct " << full_type_name << "_Fields {\n";
    }

    void *iter = nullptr;
    while (FieldInfo *field = il2cpp_class_get_fields(klass, &iter)) {
        int32_t flags = il2cpp_field_get_flags(field);
        if (flags & FIELD_ATTRIBUTE_LITERAL && il2cpp_class_is_enum(klass)) continue;
        if (flags & FIELD_ATTRIBUTE_STATIC) continue;
        output << generate_field(field);
    }

    output << "};\n";
    return output.str();
}

std::string generate_vtable(Il2CppClass *klass, std::string full_type_name) {
    if (klass->vtable_count <= 0) return "";

    std::stringstream output;
    output << "struct " << full_type_name << "_VTable {\n";

    for (int32_t i = 0; i < klass->vtable_count; ++i) {
        output << "\tVirtualInvokeData _" << i << "_";
        if (klass->vtable[i].method) {
            std::string method_name =
                il2cpp_method_get_name(klass->vtable[i].method);
            std::replace(method_name.begin(), method_name.end(), '.', '_');
            output << method_name;
        }
        output << ";\n";
    }

    output << "};\n";
    return output.str();
}

std::string generate_struct_c(Il2CppClass *klass, std::string full_type_name) {
    std::stringstream output;
    output << "struct " << full_type_name << "_c {\n";
    output << "\tIl2CppClass_1 _1;\n";

    if (klass->static_fields_size > 0) {
        output << "\tstruct " << full_type_name << "_StaticFields* static_fields;\n";
    }
    else {
        output << "\tvoid *static_fields;\n";
    }

    output << "\tIl2CppRGCTXData* rgctx_data;\n";
    output << "\tIl2CppClass_2 _2;\n";

    if (klass->vtable_count > 0) {
        output << "\t" << full_type_name << "_VTable vtable;\n";
    }

    output << "};\n";
    return output.str();
}

std::string generate_struct_o(Il2CppClass *klass, std::string full_type_name) {
    std::stringstream output;
    output << "struct " << full_type_name << "_o {\n";

    if (klass->declaringType && il2cpp_class_is_valuetype(klass->declaringType)) {
        output << "\t" << full_type_name << "_c *klass;\n";
        output << "\tvoid *monitor;\n";
    }

    output << "\t" << full_type_name << "_Fields fields;\n";
    output << "};\n";
    return output.str();
}

std::string generate_static_fields(Il2CppClass *klass,
                                   std::string  full_type_name) {
    if (klass->static_fields_size <= 0) return "";

    std::stringstream output;
    output << "struct " << full_type_name << "_StaticFields {\n";

    void *iter = nullptr;
    while (FieldInfo *field = il2cpp_class_get_fields(klass, &iter)) {
        int32_t flags = il2cpp_field_get_flags(field);
        if (flags & FIELD_ATTRIBUTE_LITERAL && il2cpp_class_is_enum(klass)) continue;
        if (!(flags & FIELD_ATTRIBUTE_STATIC)) continue;
        output << generate_field(field);
    }

    output << "};\n";
    return output.str();
}

std::string generate_type(const Il2CppType *type) {
    std::stringstream output;
    Il2CppClass *klass = il2cpp_class_from_type(type);

    bool is_valuetype = il2cpp_class_is_valuetype(klass);
    if (!is_valuetype) return "";  // only generate struct defs for value types

    std::string full_type = resolve_full_type(klass);

    output << generate_fields(klass, full_type);
    output << generate_vtable(klass, full_type);
    output << generate_struct_c(klass, full_type);
    output << generate_struct_o(klass, full_type);
    output << generate_static_fields(klass, full_type);

    return output.str();
}

// ─── Main generation routine ──────────────────────────────────────────────────

void il2cpp_structs_generate() {
    int32_t il2cpp_version = get_il2cpp_version();
    if (il2cpp_version < 0) {
        printf("[struct_gen] Failed to detect metadata version!\n");
    }
    else {
        printf("[struct_gen] Detected metadata version %d\n", il2cpp_version);
    }

    std::vector<std::string> output_array;

    size_t size;
    Il2CppDomain *domain = il2cpp_domain_get();
    const Il2CppAssembly **assemblies =
        il2cpp_domain_get_assemblies(domain, &size);
    printf("[struct_gen] Generating header for %zu images...\n", size);

    if (il2cpp_image_get_class) {
        printf("[struct_gen] Version >= 2018.3 path\n");
        for (int32_t i = 0; i < (int32_t)size; ++i) {
            printf("[struct_gen] Processing %d/%zu...\n", i + 1, size);
            const Il2CppImage *image =
                il2cpp_assembly_get_image(assemblies[i]);
            std::stringstream image_str;
            size_t class_count = il2cpp_image_get_class_count(image);
            for (int32_t j = 0; j < (int32_t)class_count; ++j) {
                const Il2CppClass *klass = il2cpp_image_get_class(image, j);
                const Il2CppType  *type  =
                    il2cpp_class_get_type((Il2CppClass *)klass);
                std::string out = image_str.str() + generate_type(type);
                output_array.push_back(out);
            }
        }
    }
    else {
        printf("[struct_gen] Version < 2018.3 reflection path\n");
        const Il2CppImage *corlib = il2cpp_get_corlib();
        Il2CppClass *assembly_class =
            il2cpp_class_from_name(corlib, "System.Reflection", "Assembly");
        const MethodInfo *assembly_load =
            il2cpp_class_get_method_from_name(assembly_class, "Load", 1);
        const MethodInfo *assembly_get_types =
            il2cpp_class_get_method_from_name(assembly_class, "GetTypes", 0);

        if (!assembly_load || !assembly_load->methodPointer) {
            printf("[struct_gen] miss Assembly::Load\n");
            return;
        }
        if (!assembly_get_types || !assembly_get_types->methodPointer) {
            printf("[struct_gen] miss Assembly::GetTypes\n");
            return;
        }

        for (int32_t i = 0; i < (int32_t)size; ++i) {
            printf("[struct_gen] Processing %d/%zu...\n", i + 1, size);
            const Il2CppImage *image =
                il2cpp_assembly_get_image(assemblies[i]);
            std::stringstream image_str;
            const char *image_name = il2cpp_image_get_name(image);

            std::string std_image_name    = std::string(image_name);
            size_t      pos               = std_image_name.rfind('.');
            std::string image_name_no_ext = std_image_name.substr(0, pos);
            Il2CppString *asm_name =
                il2cpp_string_new(image_name_no_ext.data());

            void *reflection_asm =
                ((Assembly_Load_t)assembly_load->methodPointer)(
                    nullptr, asm_name, nullptr);
            Il2CppArray *reflection_types =
                ((Assembly_GetTypes_t)assembly_get_types->methodPointer)(
                    reflection_asm, nullptr);
            void **items = reflection_types->vector;

            for (int32_t j = 0; j < (int32_t)reflection_types->max_length; ++j) {
                Il2CppClass *klass =
                    il2cpp_class_from_system_type(
                        (Il2CppReflectionType *)items[j]);
                const Il2CppType *type = il2cpp_class_get_type(klass);
                std::string out = image_str.str() + generate_type(type);
                output_array.push_back(out);
            }
        }
    }

    printf("[struct_gen] Writing il2cpp.h...\n");
    std::ofstream out_stream("/data/il2cpp/il2cpp.h");
    out_stream << generic_header;

    switch (il2cpp_version) {
    case 29:
        out_stream << header_v29;
        break;
    default:
        printf("[struct_gen] ERROR: unsupported metadata version %d (0x%X) - "
               "skipping header generation!\n",
               il2cpp_version, il2cpp_version);
        out_stream.close();
        return;
    }

    for (const auto &entry : output_array) {
        out_stream << entry;
    }

    out_stream.close();
    printf("[struct_gen] Header generation done!\n");
}

// ─── Thread launcher ──────────────────────────────────────────────────────────

void il2cpp_struct_gen::run() {
    pthread_t struct_gen_thread;
    pthread_create(&struct_gen_thread, nullptr, [](void *) -> void * {
        // Wait until the dump thread releases the lock.
        std::lock_guard<std::mutex> _(dumper_lock);
        printf("[struct_gen] Starting struct generator thread...\n");
        il2cpp_structs_generate();
        return nullptr;
    }, nullptr);
    pthread_detach(struct_gen_thread);
}
