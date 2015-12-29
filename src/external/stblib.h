#pragma once

#ifdef __cplusplus
extern "C" {
#endif

unsigned char* stblib_load(char const *filename, int *x, int *y, int *comp, int req_comp);
int stblib_write_png(char const *filename, int x, int y, int comp, const void *data, int stride_bytes);

#ifdef __cplusplus
}
#endif
