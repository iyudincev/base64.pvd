CXX = cl.exe
LINK = link.exe
CXXFLAGS = /O2 /W3 /LD /EHsc
LIBS = gdiplus.lib user32.lib ole32.lib

!IFNDEF ARCH
ARCH = x86
!ENDIF

OUT_DIR = out\$(ARCH)

TARGET_DLL = $(OUT_DIR)\Base64.dll
TARGET_PVD = $(OUT_DIR)\Base64.pvd

all: $(OUT_DIR) $(TARGET_PVD) clean_build

$(OUT_DIR):
	@if not exist "$(OUT_DIR)" mkdir "$(OUT_DIR)"

$(TARGET_DLL): Base64.cpp exports.def
	$(CXX) $(CXXFLAGS) Base64.cpp /Fo"$(OUT_DIR)\\" /Fe"$(TARGET_DLL)" $(LIBS) /link /DEF:exports.def /SUBSYSTEM:WINDOWS

$(TARGET_PVD): $(TARGET_DLL)
	@if exist "$(TARGET_PVD)" del /f /q "$(TARGET_PVD)"
	ren "$(TARGET_DLL)" Base64.pvd

clean_build:
	@if exist "$(OUT_DIR)\Base64.obj" del /q "$(OUT_DIR)\Base64.obj"
	@if exist "$(OUT_DIR)\Base64.lib" del /q "$(OUT_DIR)\Base64.lib"
	@if exist "$(OUT_DIR)\Base64.exp" del /q "$(OUT_DIR)\Base64.exp"

clean:
	@if exist "$(OUT_DIR)" rmdir /s /q "$(OUT_DIR)"
