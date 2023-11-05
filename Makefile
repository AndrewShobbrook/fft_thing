CC=gcc
CFLAGS=-Iinclude -O2
LDFLAGS = -lfftw3 -lfftw3f -lSDL_bgi -lSDL2 -lportaudio -lm
FILES=main.c input/portaudio_back.c
fft_thing: $(FILES)
	$(CC) $(CFLAGS) -o fft_thing $(FILES) $(LDFLAGS)
