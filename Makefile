CC       = gcc
CXX      = g++
CFLAGS   = -Wall -Wextra -std=c99 -g -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -Iinclude
CXXFLAGS = -Wall -std=c++17 -g -Iinclude
LDFLAGS  = -lpthread -lm -lsfml-graphics -lsfml-window -lsfml-system -lstdc++

TARGET   = bin/traffic_simulator

C_SRCS   = src/main.c src/vehicle.c src/intersection.c src/parking.c src/ipc.c src/display.c
CPP_SRCS = src/graphics.cpp

C_OBJS   = $(patsubst src/%.c,   bin/%.o, $(C_SRCS))
CPP_OBJS = $(patsubst src/%.cpp, bin/%.o, $(CPP_SRCS))
ALL_OBJS = $(C_OBJS) $(CPP_OBJS)

all: bin $(TARGET)

bin:
	mkdir -p bin

$(TARGET): $(ALL_OBJS)
	$(CXX) $(ALL_OBJS) -o $(TARGET) $(LDFLAGS)
	@echo "Build successful! Run with: ./bin/traffic_simulator"

bin/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

bin/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf bin/

run: all
	./bin/traffic_simulator

.PHONY: all clean run bin
