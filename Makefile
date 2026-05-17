# Makefile for MSVC (nmake) — Visual Studio x64
# Usage: nmake

CXX      = cl
RC       = rc
CXXFLAGS = /std:c++17 /W3 /EHsc /nologo
LDFLAGS  = /link /SUBSYSTEM:WINDOWS /NOLOGO
TARGET   = maplink.exe
SRC      = maplink.cpp
OBJ      = maplink.obj
RES      = maplink.res

all: $(TARGET)

$(TARGET): $(OBJ) $(RES)
	$(CXX) $(CXXFLAGS) /Fe:$(TARGET) $(OBJ) $(RES) $(LDFLAGS)

$(OBJ): $(SRC)
	$(CXX) $(CXXFLAGS) /c /Fo:$(OBJ) $(SRC)

$(RES): maplink.rc maplink.manifest
	$(RC) /nologo /fo $(RES) maplink.rc

clean:
	-del /Q $(TARGET) $(OBJ) $(RES) 2>nul

.PHONY: all clean
