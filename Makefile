CC = g++

SOURCE = *.c ../common/*.c 
TARGET = ffplay
LIBDIR = -L/usr/local/lib -L../lib/
INCDIR = -I/usr/local/include/ -I../common/ -I../include/
OPTION = -O2 -w -g 
LIB_FFMPEG = -lavformat -lavcodec -lavformat -lavutil -lswresample -lswscale
LIB_SDL2 = -lSDL2 -lSDL2main
#LIB_OTHER = -lx264 -lx265 -lvpx -lmp3lame -lopus -lfdk-aac -lX11 -lva -lvdpau -lva-drm -lva-x11 -lvorbisenc -lvorbis -ltheoraenc -ltheoradec -ldl -lm -lpthread -lz 


all:
	$(CC) $(SOURCE) -o  $(TARGET) $(LIBDIR) $(INCDIR) $(OPTION) $(LIB_FFMPEG) $(LIB_OTHER) $(LIB_SDL2)
	ulimit -c unlimited


clean:
	rm -f *.out 
