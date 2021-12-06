#include <windows.h>
#include <winbase.h>
#include <windef.h>
#include <stdio.h>
#include <stdlib.h>

typedef FARPROC dl_fn;

int main() {
  dl_fn fn;
  HINSTANCE lib = LoadLibrary("SDL2.dll");
  
  if (!lib) {
    printf("Error loading DLL\n");
    exit(1);
  }

  fn = GetProcAddress(lib, "SDL_Init");
  if (!fn) {
    printf("Error loading function\n");
    exit(1);
  }
  fn(1, 2);

  fn = GetProcAddress(lib, "SDL_CreateWindow");
  if (!fn) {
    printf("Error loading function\n");
    exit(1);
  }
  fn("Hello World!", 100, 100, 620, 387, 4);

  fn = GetProcAddress(lib, "SDL_Delay");
  if (!fn) {
    printf("Error loading function\n");
    exit(1);
  }
  fn(1000);

  FreeLibrary(lib);
  return 0;
}
