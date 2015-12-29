#include "stblib.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

unsigned char* stblib_load(char const *filename, int *x, int *y, int *comp, int req_comp)
{
	return stbi_load(filename, x, y, comp, req_comp);
}

int stblib_write_png(char const *filename, int x, int y, int comp, const void *data, int stride_bytes)
{
	return stbi_write_png(filename, x, y, comp, data, stride_bytes);
}
