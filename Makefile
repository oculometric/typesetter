SRC_DIR			:= src/
BIN_DIR			:= bin/
OBJ_DIR			:= bin/obj/

CC				:= g++
CC_FLAGS		:= -std=c++20 -g -Wall -O3 -D NDEBUG -D GUI
CC_INCLUDE		:= -I lib/inc -I lib/strn/inc

LD				:= g++
LD_FLAGS		:= -g
LD_INCLUDE		:= -lglfw -lxcb -L lib -L lib/strn/bin -lstrn

DEP_FLAGS		:= -MMD -MP

CC_FILES_IN		:= $(wildcard $(SRC_DIR)*.cpp)
CC_FILES_OUT	:= $(patsubst $(SRC_DIR)%.cpp, $(OBJ_DIR)%.o, $(CC_FILES_IN))
CC_FILES_DEP	:= $(patsubst $(SRC_DIR)%.cpp, $(OBJ_DIR)%.d, $(CC_FILES_IN))

EXE_OUT			:= $(BIN_DIR)iapetus-typesetter-gui

.PHONY: clean $(BIN_DIR) $(OBJ_DIR)

all: execute

$(OBJ_DIR)%.o: $(SRC_DIR)%.cpp
	@mkdir -p $(dir $@)
	@echo "Compiling" $< to $@
	@$(CC) $(CC_FLAGS) $(CC_INCLUDE) $(DEP_FLAGS) -c $< -o $@

-include $(CC_FILES_DEP)

$(EXE_OUT): $(CC_FILES_OUT)
	@echo "Linking" $@
	@ld -r -b binary -o $(BIN_DIR)icon.png.o iapetus.png
	@$(LD) $(LD_FLAGS) -o $(EXE_OUT) $(BIN_DIR)icon.png.o $^ $(LD_INCLUDE)

build: $(EXE_OUT)

execute: $(EXE_OUT)
	@$(EXE_OUT)

clean:
	@rm -r $(BIN_DIR)
	