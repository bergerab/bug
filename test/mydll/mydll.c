#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

int add(int x, int y) {
  return x + y;
}

int doubleprint(char *x) {
  printf(x);
  printf(x);
  return 9;
}

struct ivec2 {
  int x, y;
};

struct rect {
  int x, y;
  int w, h;
};

struct four {
  int x, y;
  int w, h;
};

struct three {
  uint8_t x, y;
  uint8_t w;
};

struct color {
  char r, g, b, a;
};

void print_me() {
  printf("me\n");
}

void print_ivec2(struct ivec2 *v) {
  if (v == NULL) printf("<null>");
  else printf("Vec2<x=%d, y=%d>\n", v->x, v->y);
}

void print_four_by_value(struct four r) {
  printf("Rect<x=%d, y=%d w=%d, h=%d>\n", r.x, r.y, r.w, r.h);
}

void print_four(struct four *r) {
  if (r == NULL) printf("<null>");
  else printf("Rect<x=%d, y=%d w=%d, h=%d>\n", r->x, r->y, r->w, r->h);
}

void print_three(struct three *r) {
  if (r == NULL) printf("<null>");
  else printf("Three<x=%d, y=%d w=%d>\n", r->x, r->y, r->w);
}

void print_rect(struct rect *r) {
  if (r == NULL) printf("<null>");
  else printf("Rect<x=%d, y=%d w=%d, h=%d>\n", r->x, r->y, r->w, r->h);
}

void print_color(struct color *c) {
  printf("made it %p\n", c);
  printf("made it %d\n", ((char*)c)[0]);
  if (c == NULL) printf("<null>");
  else printf("Color<r=%d, g=%d, b=%d, a=%d>\n", c->r, c->g, c->b, c->a);
}

void print_rect2(struct rect *r0, struct rect *r1) {
  print_rect(r0);
  print_rect(r1);
}

void multitest(struct rect *r0, char *message, struct color *c0) {
  print_rect(r0);
  printf("%s", message);
  print_color(c0);
}