#include <stdio.h>
#include <stdlib.h>

int add(int x, int y) {
  return x + y;
}

int doubleprint(char *x) {
  printf(x);
  printf(x);
  return 9;
}

struct rect {
  int x, y;
  int w, h;
};

void print_me() {
  printf("me\n");
}

void print_rect(struct rect *r) {
  printf("Rect<x=%d, y=%d w=%d, h=%d>\n", r->x, r->y, r->w, r->h);
}