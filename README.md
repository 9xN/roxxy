# roxxy
Minimal DLL proxy generator.

`roxxy` parses a target DLL’s export table and generates a C++ source file that forwards all exports to the original DLL using linker directives.

### Build with:
#### MSVC
1. Open the **x64 Native Tools Command Prompt for VS 20xx**.
2. Navigate to the directory containing `roxxy.c`.
3. Run the following command:
```
cl /nologo /O2 /D "NDEBUG" /D "_CONSOLE" /D "_UNICODE" /D "UNICODE" /utf-8 roxxy.c /link /SUBSYSTEM:CONSOLE /OUT:roxxy.exe
```
#### GCC (MinGW)
If you prefer using GCC on Windows:
```
gcc -O3 -DNDEBUG -municode -o roxxy.exe roxxy.c
```
---
### Run with:
```
roxxy.exe [options] <dll_path>
```
---
```
-o, --output <file>      Output C++ file (default: <dllname>.cpp)
-t, --targetdir <dir>    Custom forwarding directory
-v, --verbose            Print all exports while processing
--force-ordinals         Force forwarding by ordinal
```
<img width="1000" height="800" alt="image" src="https://github.com/user-attachments/assets/35834496-7288-4ce0-ab5f-410ca2670da3" />
<img width="1000" height="800" alt="image" src="https://github.com/user-attachments/assets/fc2b3e37-b8b6-4544-b748-f03aea8a9a10" />
