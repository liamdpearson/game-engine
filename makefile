# Shell. Make defaults to sh.exe and silently falls back to cmd.exe when it is not
# on PATH, so the recipes would change meaning depending on the terminal used to
# invoke make. Pin cmd.exe and write the shell commands below in cmd syntax.
SHELL    := cmd.exe

# Target executable name
TARGET   := app.exe

# Compilers and flags
CC       := gcc
CXX      := g++
WARNINGS := -Wall -Wextra
CPPFLAGS := -Ithird_party/glad/include -Ithird_party/glfw/include -Ithird_party
CFLAGS    = $(WARNINGS) -O2 -MMD -MP
CXXFLAGS  = $(WARNINGS) -O2 -MMD -MP
LDFLAGS  := -Lthird_party/glfw/lib
LDLIBS   := -lglfw3 -lopengl32 -lgdi32

# Directories
SRC_DIR  := src
TP_DIR   := third_party
OBJ_DIR  := build

# Shell helpers ($1 is a make-style path; cmd needs backslashes)
MKDIR     = if not exist "$(subst /,\,$1)" mkdir "$(subst /,\,$1)"
RM        = if exist "$(subst /,\,$1)" del /Q "$(subst /,\,$1)"
RMDIR     = if exist "$(subst /,\,$1)" rmdir /S /Q "$(subst /,\,$1)"

# Locate all source files and determine object/dependency files
SRCS     := $(wildcard $(SRC_DIR)/*.cpp) $(wildcard $(SRC_DIR)/*/*.cpp)
TP_SRCS  := $(TP_DIR)/glad/src/glad.c $(TP_DIR)/xatlas/xatlas.cpp
OBJS     := $(patsubst %.c,$(OBJ_DIR)/%.o,$(patsubst %.cpp,$(OBJ_DIR)/%.o,$(SRCS) $(TP_SRCS)))
DEPS     := $(OBJS:.o=.d)

# Third-party code is not ours to fix, so build it without the warning flags
$(OBJ_DIR)/$(TP_DIR)/%.o: WARNINGS :=

# Default rule (links the final executable)
$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) $^ -o $@ $(LDLIBS)

# Pattern rules (compile individual source files into object files)
$(OBJ_DIR)/%.o: %.cpp
	@$(call MKDIR,$(@D))
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: %.c
	@$(call MKDIR,$(@D))
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# Include automatically generated dependency files
-include $(DEPS)

# Clean rule
.PHONY: clean
clean:
	@$(call RM,$(TARGET))
	@$(call RMDIR,$(OBJ_DIR))
