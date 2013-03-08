all:
	gcc -Wall adbfuse.c `pkg-config fuse --cflags --libs` -o adbfuse

