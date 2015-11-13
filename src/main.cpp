
#include <stdlib.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"


#define JC_ROOMMAKER_IMPLEMENTATION
#include "jc_roommaker.h"

#define IMAGEDIMS 1024
#define ROOMSDIMS IMAGEDIMS / 64


struct SImage
{
    int             width;
    int             height;
    int             channels;
    int             stride;
    unsigned char*  bytes;
};

static void render_rooms(const SRooms* rooms, SImage* image)
{

}




int main(int argc, const char** argv)
{
    SRoomMakerContext roomctx;
    roomctx.dimensions[0]   = ROOMSDIMS;
    roomctx.dimensions[1]   = ROOMSDIMS;
    roomctx.maxnumrooms     = ROOMSDIMS/2;
    SRooms* rooms = jc_roommaker_create(&roomctx);

    SImage image;
    image.width     = IMAGEDIMS;
    image.height    = IMAGEDIMS;
    image.channels  = 3;

    size_t imagesize = image.width * image.height * image.channels;
    image.bytes     = (unsigned char*)malloc(imagesize);
    memset(image.bytes, 0, imagesize);

    render_rooms(rooms, &image);

    jc_roommaker_free(&roomctx, rooms);

    const char* outputfile = "emaple.png";

    char path[512];
    sprintf(path, "%s", outputfile);
    stbi_write_png(path, image.width, image.height, image.channels, image.bytes, image.width*image.channels);
    printf("wrote %s\n", path);

    free(image.bytes);

    return 0;
}
