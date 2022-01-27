
#CC=arm-openwrt-linux-gcc
CC=gcc

SOURCE_PATH=.
SOURCE_FILE=$(SOURCE_PATH)/main.c 

BIN_PATH=$(SOURCE_PATH)/bin
INC_PATH=$(BIN_PATH)/include
LIB_PATH=$(BIN_PATH)/lib

BIN_FILE=$(BIN_PATH)/scanNTFS
INC_FILE=$(INC_PATH)/scanNTFS.h
LIB_FILE=$(LIB_PATH)/scanNTFS.so






.PHONY:clean all

all:$(BIN_FILE) $(INC_FILE) $(LIB_FILE)
	
$(LIB_FILE):$(SOURCE_PATH)/scanNTFS.o
	mkdir -p $(LIB_PATH)
	$(CC) -shared -o$(LIB_FILE)	$^
	
$(BIN_FILE):$(SOURCE_FILE) $(LIB_FILE) $(SOURCE_PATH)/system_call_search.o
	mkdir -p $(BIN_PATH)
	$(CC) -g -o$(BIN_FILE) $^
	
$(INC_FILE):$(SOURCE_PATH)/scanNTFS.h
	mkdir -p $(INC_PATH)
	cp $(SOURCE_PATH)/scanNTFS.h $(INC_PATH)
	

	
%.o:%.c
	$(CC) -c -g -o$@ -fPIC $^
	
clean:
	rm $(SOURCE_PATH)/*.o
	rm -rf $(BIN_PATH)
 
