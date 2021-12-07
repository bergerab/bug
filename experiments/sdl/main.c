#include <windows.h>
#include <winbase.h>
#include <windef.h>
#include <stdio.h>
#include <stdlib.h>
#include <ffi.h>

typedef FARPROC dl_fn;

int main() {
  ffi_cif cif;
  ffi_type *arg_types[2];
  void *arg_values[1];
  ffi_status status;
  ffi_arg result;

  int arg1 = 3;

  dl_fn fn;
  HINSTANCE lib = LoadLibrary("SDL2.dll");
  
  if (!lib) {
    printf("Error loading DLL\n");
    exit(1);
  }

  arg_types[0] = &ffi_type_uint; 
  arg_values[0] = &arg1;
  if ((status = ffi_prep_cif(&cif, FFI_DEFAULT_ABI,
      1, &ffi_type_uint8, arg_types)) != FFI_OK) {
      /* Handle the ffi_status error. */
      printf("ERROR\n");
      exit(1);
  }

  fn = GetProcAddress(lib, "SDL_Init");
  if (!fn) {
    printf("Error loading function\n");
    exit(1);
  }
  ffi_call(&cif, FFI_FN(fn), &result, arg_values);
  printf("Return value %lu\n", (unsigned long)result);

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
