CC = gcc
CFLAGS = -Wall -Wextra -Iinclude -g -D__USE_MINGW_ANSI_STDIO=1
LDFLAGS = -lws2_32
SRC_DIR = src
OBJ_DIR = obj

SOURCES = $(SRC_DIR)/packet.c $(SRC_DIR)/network.c $(SRC_DIR)/dashboard.c $(SRC_DIR)/rtt.c
OBJECTS = $(OBJ_DIR)/packet.o $(OBJ_DIR)/network.o $(OBJ_DIR)/dashboard.o $(OBJ_DIR)/rtt.o

all: create_dirs server.exe client.exe

create_dirs:
	@if not exist "$(OBJ_DIR)" mkdir "$(OBJ_DIR)"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

server.exe: $(OBJECTS) $(SRC_DIR)/server.c
	$(CC) $(CFLAGS) $(OBJECTS) $(SRC_DIR)/server.c -o server.exe $(LDFLAGS)

client.exe: $(OBJECTS) $(SRC_DIR)/client.c
	$(CC) $(CFLAGS) $(OBJECTS) $(SRC_DIR)/client.c -o client.exe $(LDFLAGS)

clean:
	@if exist "$(OBJ_DIR)" rmdir /s /q "$(OBJ_DIR)"
	@if exist "server.exe" del /q server.exe
	@if exist "client.exe" del /q client.exe

.PHONY: all create_dirs clean


