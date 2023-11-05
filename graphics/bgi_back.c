#include <SDL2/SDL_bgi.h>
#include <graphics.h>
#include <graphics/bgi_back.h>
#include <stdlib.h>

void initialise_bars(unsigned int *bars, unsigned int num_bars) {
  int i, gd = DETECT, gm;
  initgraph(&gd, &gm, "audio_stuff");
  setbkcolor(BLACK);

  cleardevice();
}

void draw_bars(unsigned int *bars, unsigned int num_bars) {
  int max_x = getmaxx();
  int max_y = getmaxy();
  cleardevice();
  for (int i = 0; i < num_bars; i++) {
    rectangle(i * (max_x / num_bars), max_y - bars[i],
              (i + 1) * (max_x / num_bars), max_y);
  }
}
