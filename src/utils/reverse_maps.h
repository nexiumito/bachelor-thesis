#ifndef REVERSE_MAPS_H
#define REVERSE_MAPS_H

#include "ps_set.h"

typedef struct {
    int* map_v;
    int* map_c1_bar;
    int* map_c2_bar;
    int map_size;
} ReverseMaps;

ReverseMaps* create_reverse_maps(int size);
void free_reverse_maps(ReverseMaps* rm);
void fill_reverse_map(int* map, int map_size, PS_Set* ps);
void clear_reverse_map(int* map, int map_size, PS_Set* ps);

#endif
