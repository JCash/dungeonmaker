
#include <math.h>
#include <stdlib.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"


#define JC_ROOMMAKER_IMPLEMENTATION
#include "jc_roommaker.h"

const int ROOMSDIMS = 64;
const int PIXELS_PER_ROOM = 1;
const int IMAGEDIMS = 64 * PIXELS_PER_ROOM;


struct SImage
{
    int             width;
    int             height;
    int             channels;
    int             stride;
    unsigned char*  bytes;
};

static void render_room(const SRoom* room, SImage* image)
{
    uint8_t color[3];
    color[0] = 60 + (uint8_t)(jc_roommaker_rand01() * 120);
    color[1] = 60 + (uint8_t)(jc_roommaker_rand01() * 120);
    color[2] = 60 + (uint8_t)(jc_roommaker_rand01() * 120);
    for( int yy = room->pos[1]; yy < room->dims[1] + room->pos[1]; ++yy )
    {
        for( int xx = room->pos[0]; xx < room->dims[0] + room->pos[0]; ++xx )
        {
            int index = (yy * PIXELS_PER_ROOM) * image->width * image->channels + (xx * PIXELS_PER_ROOM) * image->channels;

            int roomid = room->id;
            image->bytes[index+0] = color[0];
            image->bytes[index+1] = color[1];
            image->bytes[index+2] = color[2];
        }
    }
}

static void render_rooms(const SRooms* rooms, SImage* image)
{
    for( int i = 0; i < rooms->numrooms; ++i )
        render_room(&rooms->rooms[i], image);
}




int main(int argc, const char** argv)
{
    (void)argc;
    (void)argv;
    SRoomMakerContext roomctx;
    roomctx.dimensions[0]   = ROOMSDIMS;
    roomctx.dimensions[1]   = ROOMSDIMS;
    roomctx.maxnumrooms     = (int)sqrtf(ROOMSDIMS);
    roomctx.seed            = 0;

    printf("Dims: %d, %d\n", roomctx.dimensions[0], roomctx.dimensions[1]);
    printf("Seed: 0x%08x\n", roomctx.seed);

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

    const char* outputfile = "example.png";

    char path[512];
    sprintf(path, "%s", outputfile);
    stbi_write_png(path, image.width, image.height, image.channels, image.bytes, image.width*image.channels);
    printf("wrote %s\n", path);

    free(image.bytes);

    return 0;
}
