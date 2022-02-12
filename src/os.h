#ifndef _OS_H
#define _OS_H

#include "bug.h"

void change_directory(struct object *path);
struct object *get_current_working_directory();

#endif