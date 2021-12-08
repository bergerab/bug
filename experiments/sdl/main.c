#include <windows.h>
#include <winbase.h>
#include <windef.h>
#include <stdio.h>
#include <stdlib.h>
#include <ffi.h>

typedef FARPROC dl_fn;

int main() {
  ffi_cif cif;
  ffi_type *arg_types[9];
  void *arg_values[9];
  ffi_status status;
  ffi_arg result;

  int arg1 = 100;
  int arg2 = 100;
  int arg3 = 620;
  int arg4 = 387;
  int arg5 = 4;
  int arg6 = 8;

  char *str = "wef";

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

  arg_types[0] = &ffi_type_pointer; 
  arg_types[1] = &ffi_type_uint; 
  arg_types[2] = &ffi_type_uint; 
  arg_types[3] = &ffi_type_uint; 
  arg_types[4] = &ffi_type_uint; 
  arg_types[5] = &ffi_type_uint; 
  arg_values[0] = &str;
  arg_values[1] = &arg1;
  arg_values[2] = &arg2;
  arg_values[3] = &arg3;
  arg_values[4] = &arg4;
  arg_values[5] = &arg5;
  if ((status = ffi_prep_cif(&cif, FFI_DEFAULT_ABI,
      6, &ffi_type_pointer, arg_types)) != FFI_OK) {
      /* Handle the ffi_status error. */
      printf("ERROR\n");
      exit(1);
  }

  fn = GetProcAddress(lib, "SDL_CreateWindow");
  if (!fn) {
    printf("Error loading function\n");
    exit(1);
  }
  ffi_call(&cif, FFI_FN(fn), &result, arg_values);

  fn = GetProcAddress(lib, "SDL_Delay");
  if (!fn) {
    printf("Error loading function\n");
    exit(1);
  }
  fn(1000);

  FreeLibrary(lib);
  return 0;
}
