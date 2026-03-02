#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <imagehlp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "imagehlp.lib")

static const char* banner =
"\n"
"\x1b[38;2;0;255;255m          ┏━┓┏━┓╻ ╻╻ ╻╻\n"
"          ┣┳┛┃ ┃┏╋┛┏╋┛┃\n"
"          ╹┗╸┗━┛╹ ╹╹ ╹╹\n"
"\x1b[38;2;0;180;180m   DLL Proxy Generator – by github.com/9xN\n"
"\x1b[38;2;0;220;220m             ~~ 44.is-a.dev ~~\n\n";

typedef struct {
    const char* dll_path;
    const char* output_path;
    const char* target_dir;
    int verbose;
    int force_ordinals;
} Options;

static void print_usage(const char* prog) {
    fprintf(stderr,
        "Usage: %s [options] <dll_path>\n"
        "Options:\n"
        "  -o, --output <file>     Output C++ file (default: <dllname>.cpp)\n"
        "  -t, --targetdir <dir>   Custom target directory for forwarding\n"
        "                          (default: System32 on 64-bit, SysWOW64 on 32-bit)\n"
        "  -v, --verbose           List all exports while processing\n"
        "  --force-ordinals        Force forwarding by ordinal for named exports\n",
        prog);
}

static int parse_args(int argc, char* argv[], Options* opts) {
    memset(opts, 0, sizeof(Options));
    opts->dll_path = NULL;
    opts->output_path = NULL;
    opts->target_dir = NULL;
    opts->verbose = 0;
    opts->force_ordinals = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
            if (i + 1 < argc) opts->output_path = argv[++i];
            else { fprintf(stderr, "Error: -o requires an argument.\n"); return 0; }
        }
        else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--targetdir") == 0) {
            if (i + 1 < argc) opts->target_dir = argv[++i];
            else { fprintf(stderr, "Error: -t requires an argument.\n"); return 0; }
        }
        else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            opts->verbose = 1;
        }
        else if (strcmp(argv[i], "--force-ordinals") == 0) {
            opts->force_ordinals = 1;
        }
        else if (argv[i][0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 0;
        }
        else {
            if (opts->dll_path == NULL) opts->dll_path = argv[i];
            else { fprintf(stderr, "Error: multiple DLL paths specified.\n"); return 0; }
        }
    }

    if (!opts->dll_path) { print_usage(argv[0]); return 0; }
    return 1;
}

static int resolve_dll_path(const char* input, char* output, size_t out_sz) {
    size_t len = strlen(input);
    while (len > 0 && (input[len - 1] == '\\' || input[len - 1] == '/')) {
        len--;
    }
    if (len == 0) return 0;

    char temp[MAX_PATH];
    strncpy_s(temp, sizeof(temp), input, len);
    temp[len] = '\0';

    if (GetFileAttributesA(temp) != INVALID_FILE_ATTRIBUTES) {
        strncpy_s(output, out_sz, temp, _TRUNCATE);
        return 1;
    }
    if (!strchr(temp, '\\') && !strchr(temp, '/')) {
        char sysdir[MAX_PATH];
        GetSystemDirectoryA(sysdir, sizeof(sysdir));
        if (_snprintf_s(output, out_sz, _TRUNCATE, "%s\\%s", sysdir, temp) < 0)
            return 0;
        if (GetFileAttributesA(output) != INVALID_FILE_ATTRIBUTES) return 1;
    }
    return 0;
}

int main(int argc, char* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    Options opts;
    if (!parse_args(argc, argv, &opts)) return 1;

    printf("%s", banner);

    char dll_full[MAX_PATH];
    if (!resolve_dll_path(opts.dll_path, dll_full, sizeof(dll_full))) {
        fprintf(stderr, "Error: file not found: %s\n", opts.dll_path);
        return 1;
    }
    printf("[*] Processing DLL: %s\n", dll_full);

    char output_file[MAX_PATH];
    const char* out_to_use = opts.output_path;

    if (!out_to_use) {
        const char* base = strrchr(dll_full, '\\');
        if (!base) base = strrchr(dll_full, '/');
        if (!base) base = dll_full; else base++;
        char name_copy[MAX_PATH];
        strncpy_s(name_copy, sizeof(name_copy), base, _TRUNCATE);
        char* dot = strrchr(name_copy, '.');
        if (dot) *dot = '\0';
        if (_snprintf_s(output_file, sizeof(output_file), _TRUNCATE, "%s.cpp", name_copy) < 0) {
            fprintf(stderr, "Error: output filename too long.\n");
            return 1;
        }
        out_to_use = output_file;
    }

    if (!strchr(out_to_use, '\\') && !strchr(out_to_use, '/') && !(out_to_use[0] && out_to_use[1] == ':')) {
        char exe_full[MAX_PATH];
        DWORD len = GetModuleFileNameA(NULL, exe_full, MAX_PATH);
        if (len > 0 && len < MAX_PATH) {
            char* last_slash = strrchr(exe_full, '\\');
            if (last_slash) {
                *last_slash = '\0';
                char final_output_path[MAX_PATH];
                if (_snprintf_s(final_output_path, sizeof(final_output_path), _TRUNCATE,
                    "%s\\%s", exe_full, out_to_use) > 0) {
                    out_to_use = final_output_path;
                }
            }
        }
    }

    char abs_output[MAX_PATH];
    if (!GetFullPathNameA(out_to_use, sizeof(abs_output), abs_output, NULL)) {
        strncpy_s(abs_output, sizeof(abs_output), out_to_use, _TRUNCATE);
    }
    printf("[*] Output file : %s\n", abs_output);

    HANDLE hFile = CreateFileA(dll_full, GENERIC_READ, FILE_SHARE_READ, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Error: cannot open file (error %lu)\n", GetLastError());
        return 1;
    }
    HANDLE hMap = CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!hMap) {
        CloseHandle(hFile);
        fprintf(stderr, "Error: cannot create file mapping (error %lu)\n", GetLastError());
        return 1;
    }
    LPVOID pBase = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
    if (!pBase) {
        CloseHandle(hMap); CloseHandle(hFile);
        fprintf(stderr, "Error: cannot map view of file (error %lu)\n", GetLastError());
        return 1;
    }

    PIMAGE_NT_HEADERS pNT = ImageNtHeader(pBase);
    if (!pNT) {
        fprintf(stderr, "Error: invalid PE file.\n");
        UnmapViewOfFile(pBase); CloseHandle(hMap); CloseHandle(hFile);
        return 1;
    }

    DWORD exportSize;
    PIMAGE_EXPORT_DIRECTORY pExport = (PIMAGE_EXPORT_DIRECTORY)
        ImageDirectoryEntryToData(pBase, FALSE, IMAGE_DIRECTORY_ENTRY_EXPORT, &exportSize);

    if (!pExport) {
        fprintf(stderr, "Warning: no export directory found. Generating minimal proxy (DllMain only).\n");
    }

    DWORD numFuncs = 0, numNames = 0, baseOrd = 0;
    DWORD* funcAddrs = NULL;
    DWORD* nameAddrs = NULL;
    WORD* nameOrds = NULL;

    if (pExport) {
        funcAddrs = (DWORD*)ImageRvaToVa(pNT, pBase, pExport->AddressOfFunctions, NULL);
        nameAddrs = (DWORD*)ImageRvaToVa(pNT, pBase, pExport->AddressOfNames, NULL);
        nameOrds = (WORD*)ImageRvaToVa(pNT, pBase, pExport->AddressOfNameOrdinals, NULL);

        if (!funcAddrs || !nameAddrs || !nameOrds) {
            fprintf(stderr, "Error: cannot locate export arrays.\n");
            UnmapViewOfFile(pBase); CloseHandle(hMap); CloseHandle(hFile);
            return 1;
        }

        numFuncs = pExport->NumberOfFunctions;
        numNames = pExport->NumberOfNames;
        baseOrd = pExport->Base;
    }

    printf("[*] Total function slots : %lu\n", numFuncs);
    printf("[*] Named exports        : %lu\n", numNames);
    if (opts.force_ordinals)
        printf("[*] Forcing ordinal forwarding for named exports.\n");

    typedef struct {
        char* name;
        WORD  ordinal;
        int   is_com;
    } NamedExport;

    typedef struct {
        char* stub_name;
        WORD  ordinal;
    } OrdinalExport;

    NamedExport* namedExports = malloc(numNames * sizeof(NamedExport));
    OrdinalExport* ordinalExports = malloc(numFuncs * sizeof(OrdinalExport));
    BOOL* hasName = calloc(numFuncs, sizeof(BOOL));

    if (!namedExports || !ordinalExports || !hasName) {
        fprintf(stderr, "Error: memory allocation failed.\n");
        free(namedExports); free(ordinalExports); free(hasName);
        UnmapViewOfFile(pBase); CloseHandle(hMap); CloseHandle(hFile);
        return 1;
    }

    int namedCount = 0, ordinalCount = 0;

    if (pExport) {
        for (DWORD i = 0; i < numNames; i++) {
            char* name = (char*)ImageRvaToVa(pNT, pBase, nameAddrs[i], NULL);
            if (!name) continue;

            WORD idx = nameOrds[i];
            if (idx >= numFuncs) continue;

            hasName[idx] = TRUE;
            namedExports[namedCount].name = _strdup(name);
            namedExports[namedCount].ordinal = (WORD)(baseOrd + idx);

            if (strcmp(name, "DllCanUnloadNow") == 0 ||
                strcmp(name, "DllGetClassObject") == 0 ||
                strcmp(name, "DllInstall") == 0 ||
                strcmp(name, "DllRegisterServer") == 0 ||
                strcmp(name, "DllUnregisterServer") == 0) {
                namedExports[namedCount].is_com = 1;
            }
            else {
                namedExports[namedCount].is_com = 0;
            }

            if (opts.verbose) {
                printf("  [%s] %s @%u\n",
                    namedExports[namedCount].is_com ? "COM" : "   ",
                    name, namedExports[namedCount].ordinal);
            }
            namedCount++;
        }

        for (DWORD i = 0; i < numFuncs; i++) {
            if (funcAddrs[i] != 0 && !hasName[i]) {
                WORD ord = (WORD)(baseOrd + i);
                char stub[64];
                if (_snprintf_s(stub, sizeof(stub), _TRUNCATE, "__proxy%u", ord) < 0) continue;
                ordinalExports[ordinalCount].stub_name = _strdup(stub);
                ordinalExports[ordinalCount].ordinal = ord;
                if (opts.verbose) {
                    printf("  [ORD] %s @%u\n", stub, ord);
                }
                ordinalCount++;
            }
        }
    }
    free(hasName);

    const char* basename = strrchr(dll_full, '\\');
    if (!basename) basename = strrchr(dll_full, '/');
    if (!basename) basename = dll_full; else basename++;

    const char* targetDir = opts.target_dir;

    FILE* fout = fopen(out_to_use, "w");
    if (!fout) {
        fprintf(stderr, "Error: cannot create output file: %s\n", out_to_use);
        for (int i = 0; i < namedCount; i++) free(namedExports[i].name);
        for (int i = 0; i < ordinalCount; i++) free(ordinalExports[i].stub_name);
        free(namedExports); free(ordinalExports);
        UnmapViewOfFile(pBase); CloseHandle(hMap); CloseHandle(hFile);
        return 1;
    }

    fprintf(fout,
        "/*\n"
        " *       ┏━┓┏━┓╻ ╻╻ ╻╻\n"
        " *       ┣┳┛┃ ┃┏╋┛┏╋┛┃\n"
        " *       ╹┗╸┗━┛╹ ╹╹ ╹╹\n"
        " *\n"
        " * Proxy DLL generated by roxxy\n"
        " * Source DLL: %s\n"
        " * Options: force-ordinals=%d\n"
        " * ------------------------------------------------------------------\n"
        " */\n\n"
        "#include <Windows.h>\n\n",
        dll_full, opts.force_ordinals);

    int hasRegular = 0, hasCOM = 0;
    for (int i = 0; i < namedCount; i++) {
        if (namedExports[i].is_com) hasCOM = 1; else hasRegular = 1;
    }
    int hasOrdinal = (ordinalCount > 0);

    if (hasRegular || hasCOM || hasOrdinal) {
        fprintf(fout, "#ifdef _WIN64\n");
        if (targetDir) {
            if (hasRegular) {
                if (opts.force_ordinals)
                    fprintf(fout, "#define MAKE_EXPORT(func, ord) \"/EXPORT:\" func \"=\\\\\\.\\\\GLOBALROOT\\\\%s\\\\%s.#\" #ord \",@\" #ord \",NONAME\"\n", targetDir, basename);
                else
                    fprintf(fout, "#define MAKE_EXPORT(func) \"/EXPORT:\" func \"=\\\\\\.\\\\GLOBALROOT\\\\%s\\\\%s.\" func\n", targetDir, basename);
            }
            if (hasCOM) {
                if (opts.force_ordinals)
                    fprintf(fout, "#define MAKE_EXPORT_PRIVATE(func, ord) \"/EXPORT:\" func \"=\\\\\\.\\\\GLOBALROOT\\\\%s\\\\%s.#\" #ord \",@\" #ord \",PRIVATE,NONAME\"\n", targetDir, basename);
                else
                    fprintf(fout, "#define MAKE_EXPORT_PRIVATE(func) \"/EXPORT:\" func \"=\\\\\\.\\\\GLOBALROOT\\\\%s\\\\%s.\" func \",PRIVATE\"\n", targetDir, basename);
            }
            if (hasOrdinal) {
                fprintf(fout, "#define MAKE_EXPORT_ORDINAL(func, ord) \"/EXPORT:\" func \"=\\\\\\.\\\\GLOBALROOT\\\\%s\\\\%s.#\" #ord \",@\" #ord \",NONAME\"\n", targetDir, basename);
            }
        }
        else {
            // default System32
            if (hasRegular) {
                if (opts.force_ordinals)
                    fprintf(fout, "#define MAKE_EXPORT(func, ord) \"/EXPORT:\" func \"=\\\\\\.\\\\GLOBALROOT\\\\SystemRoot\\\\System32\\\\%s.#\" #ord \",@\" #ord \",NONAME\"\n", basename);
                else
                    fprintf(fout, "#define MAKE_EXPORT(func) \"/EXPORT:\" func \"=\\\\\\.\\\\GLOBALROOT\\\\SystemRoot\\\\System32\\\\%s.\" func\n", basename);
            }
            if (hasCOM) {
                if (opts.force_ordinals)
                    fprintf(fout, "#define MAKE_EXPORT_PRIVATE(func, ord) \"/EXPORT:\" func \"=\\\\\\.\\\\GLOBALROOT\\\\SystemRoot\\\\System32\\\\%s.#\" #ord \",@\" #ord \",PRIVATE,NONAME\"\n", basename);
                else
                    fprintf(fout, "#define MAKE_EXPORT_PRIVATE(func) \"/EXPORT:\" func \"=\\\\\\.\\\\GLOBALROOT\\\\SystemRoot\\\\System32\\\\%s.\" func \",PRIVATE\"\n", basename);
            }
            if (hasOrdinal) {
                fprintf(fout, "#define MAKE_EXPORT_ORDINAL(func, ord) \"/EXPORT:\" func \"=\\\\\\.\\\\GLOBALROOT\\\\SystemRoot\\\\System32\\\\%s.#\" #ord \",@\" #ord \",NONAME\"\n", basename);
            }
        }
        fprintf(fout, "#else  // _WIN32\n");
        if (targetDir) {
            if (hasRegular) {
                if (opts.force_ordinals)
                    fprintf(fout, "#define MAKE_EXPORT(func, ord) \"/EXPORT:\" func \"=\\\\\\.\\\\GLOBALROOT\\\\%s\\\\%s.#\" #ord \",@\" #ord \",NONAME\"\n", targetDir, basename);
                else
                    fprintf(fout, "#define MAKE_EXPORT(func) \"/EXPORT:\" func \"=\\\\\\.\\\\GLOBALROOT\\\\%s\\\\%s.\" func\n", targetDir, basename);
            }
            if (hasCOM) {
                if (opts.force_ordinals)
                    fprintf(fout, "#define MAKE_EXPORT_PRIVATE(func, ord) \"/EXPORT:\" func \"=\\\\\\.\\\\GLOBALROOT\\\\%s\\\\%s.#\" #ord \",@\" #ord \",PRIVATE,NONAME\"\n", targetDir, basename);
                else
                    fprintf(fout, "#define MAKE_EXPORT_PRIVATE(func) \"/EXPORT:\" func \"=\\\\\\.\\\\GLOBALROOT\\\\%s\\\\%s.\" func \",PRIVATE\"\n", targetDir, basename);
            }
            if (hasOrdinal) {
                fprintf(fout, "#define MAKE_EXPORT_ORDINAL(func, ord) \"/EXPORT:\" func \"=\\\\\\.\\\\GLOBALROOT\\\\%s\\\\%s.#\" #ord \",@\" #ord \",NONAME\"\n", targetDir, basename);
            }
        }
        else {
            // default SysWOW64
            if (hasRegular) {
                if (opts.force_ordinals)
                    fprintf(fout, "#define MAKE_EXPORT(func, ord) \"/EXPORT:\" func \"=\\\\\\.\\\\GLOBALROOT\\\\SystemRoot\\\\SysWOW64\\\\%s.#\" #ord \",@\" #ord \",NONAME\"\n", basename);
                else
                    fprintf(fout, "#define MAKE_EXPORT(func) \"/EXPORT:\" func \"=\\\\\\.\\\\GLOBALROOT\\\\SystemRoot\\\\SysWOW64\\\\%s.\" func\n", basename);
            }
            if (hasCOM) {
                if (opts.force_ordinals)
                    fprintf(fout, "#define MAKE_EXPORT_PRIVATE(func, ord) \"/EXPORT:\" func \"=\\\\\\.\\\\GLOBALROOT\\\\SystemRoot\\\\SysWOW64\\\\%s.#\" #ord \",@\" #ord \",PRIVATE,NONAME\"\n", basename);
                else
                    fprintf(fout, "#define MAKE_EXPORT_PRIVATE(func) \"/EXPORT:\" func \"=\\\\\\.\\\\GLOBALROOT\\\\SystemRoot\\\\SysWOW64\\\\%s.\" func \",PRIVATE\"\n", basename);
            }
            if (hasOrdinal) {
                fprintf(fout, "#define MAKE_EXPORT_ORDINAL(func, ord) \"/EXPORT:\" func \"=\\\\\\.\\\\GLOBALROOT\\\\SystemRoot\\\\SysWOW64\\\\%s.#\" #ord \",@\" #ord \",NONAME\"\n", basename);
            }
        }
        fprintf(fout, "#endif // _WIN64\n\n");
    }
    else {
        fprintf(fout, "// No exports found in the original DLL.\n\n");
    }

    for (int i = 0; i < namedCount; i++) {
        if (namedExports[i].is_com) {
            if (opts.force_ordinals)
                fprintf(fout, "#pragma comment(linker, MAKE_EXPORT_PRIVATE(\"%s\", %u))\n",
                    namedExports[i].name, namedExports[i].ordinal);
            else
                fprintf(fout, "#pragma comment(linker, MAKE_EXPORT_PRIVATE(\"%s\"))\n",
                    namedExports[i].name);
        }
        else {
            if (opts.force_ordinals)
                fprintf(fout, "#pragma comment(linker, MAKE_EXPORT(\"%s\", %u))\n",
                    namedExports[i].name, namedExports[i].ordinal);
            else
                fprintf(fout, "#pragma comment(linker, MAKE_EXPORT(\"%s\"))\n",
                    namedExports[i].name);
        }
    }

    for (int i = 0; i < ordinalCount; i++) {
        fprintf(fout, "#pragma comment(linker, MAKE_EXPORT_ORDINAL(\"%s\", %u))\n",
            ordinalExports[i].stub_name, ordinalExports[i].ordinal);
    }

    fprintf(fout,
        "\nBOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)\n"
        "{\n"
        "    switch (fdwReason)\n"
        "    {\n"
        "        case DLL_PROCESS_ATTACH:\n"
        "            break;\n"
        "        case DLL_THREAD_ATTACH:\n"
        "            break;\n"
        "        case DLL_THREAD_DETACH:\n"
        "            break;\n"
        "        case DLL_PROCESS_DETACH:\n"
        "            break;\n"
        "    }\n"
        "    return TRUE;\n"
        "}\n");

    fclose(fout);
    printf("[*] Successfully generated: %s\n", abs_output);

    for (int i = 0; i < namedCount; i++) free(namedExports[i].name);
    for (int i = 0; i < ordinalCount; i++) free(ordinalExports[i].stub_name);
    free(namedExports); free(ordinalExports);
    UnmapViewOfFile(pBase); CloseHandle(hMap); CloseHandle(hFile);
    return 0;
}
