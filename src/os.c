/** 
 * This file exists because unistd.h caused conflicts with functions like "read" and "dup" in bug code.
 * The include for unistd must be kept in this file, otherwise it would cause that conflict again.
 */

#include <unistd.h>

#include "os.h"

void change_directory(struct object *path) {
  OT("change-directory", 0, path, type_string);
  dynamic_byte_array_force_cstr(path);
  chdir(STRING_CONTENTS(path));
}

#define MAX_FILE_PATH_SIZE 500 

struct object *get_current_working_directory() {
  char buffer[MAX_FILE_PATH_SIZE];
  return string(getcwd(buffer, MAX_FILE_PATH_SIZE));
}