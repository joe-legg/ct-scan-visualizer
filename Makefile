CC=clang
CFLAGS=-o visualizer -g -Wall -lglfw -lGL -ltiff -Iinclude/
SRC_FILES=visualizer.c glad.c

all:
	$(CC) $(CFLAGS) $(SRC_FILES)

clean:
	rm visualizer

.PHONY: clean
