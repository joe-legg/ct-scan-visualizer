CC=clang
CFLAGS=-o visualizer -g -Wall -lglfw -lGL -ltiff -I extern/include/
SRC_FILES=visualizer.c extern/src/glad.c

all:
	$(CC) $(CFLAGS) $(SRC_FILES)

clean:
	rm visualizer

.PHONY: clean
