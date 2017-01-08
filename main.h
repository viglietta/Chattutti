#ifndef MAIN_H
#define MAIN_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_bits.h>
#include <SDL2/SDL_net.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>

#define DTIME (1000.0/60.0)

#include "bitpack.h"
#include "network.h"

Uint32 last_time;
Uint8 namelen;
Uint8 name[15];
SDL_Color color;

#endif
