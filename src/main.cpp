
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include "external/stblib.h"

#define JC_ROOMMAKER_IMPLEMENTATION
#include "jc_roommaker.h"

const int NUMCELLS = 256;
const int PIXELS_PER_ROOM = 1;
const int IMAGEDIMS = NUMCELLS * PIXELS_PER_ROOM;


class Timer {
private:

    timeval startTime;

public:

    void start(){
        gettimeofday(&startTime, NULL);
    }

    double stop(){
        timeval endTime;
        long seconds, useconds;
        double duration;

        gettimeofday(&endTime, NULL);

        seconds  = endTime.tv_sec  - startTime.tv_sec;
        useconds = endTime.tv_usec - startTime.tv_usec;

        duration = seconds + useconds/1000000.0;

        return duration;
    }

    static void printTime(double duration){
        printf("%5.6f seconds\n", duration);
    }
};


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

            image->bytes[index+0] = color[0];
            image->bytes[index+1] = color[1];
            image->bytes[index+2] = color[2];
        }
    }
}

static void render_rooms(const SRooms* rooms, SImage* image)
{
    //for( int i = 0; i < rooms->numrooms; ++i )
    //    render_room(&rooms->rooms[i], image);

    uint8_t colors[256*3] = { 0,0,0 };
    for( int i = 1; i < 256; ++i)
    {
        int index = i * 3;
        colors[index + 0] = 60 + (uint8_t)(jc_roommaker_rand01() * 120);
        colors[index + 1] = 60 + (uint8_t)(jc_roommaker_rand01() * 120);
        colors[index + 2] = 60 + (uint8_t)(jc_roommaker_rand01() * 120);
    }

    for( int y = 0; y < rooms->dimensions[1]; ++y)
    {
        for( int x = 0; x < rooms->dimensions[0]; ++x)
        {
            int index = (y * PIXELS_PER_ROOM) * image->width * image->channels + (x * PIXELS_PER_ROOM) * image->channels;

            int roomid = rooms->grid[y * rooms->dimensions[0] + x];
            roomid = roomid % 256;
            image->bytes[index+0] = colors[roomid*3 + 0];
            image->bytes[index+1] = colors[roomid*3 + 1];
            image->bytes[index+2] = colors[roomid*3 + 2];
        }
    }
}




int main(int argc, const char** argv)
{
    (void)argc;
    (void)argv;
    SRoomMakerContext roomctx;
    roomctx.dimensions[0]   = NUMCELLS;
    roomctx.dimensions[1]   = NUMCELLS;
    roomctx.maxnumrooms     = (int)sqrtf(NUMCELLS) * 40;
    roomctx.seed            = 0;

    printf("Dims: %d, %d\n", roomctx.dimensions[0], roomctx.dimensions[1]);
    printf("Seed: 0x%08x\n", roomctx.seed);

    Timer timer;
    timer.start();
    SRooms* rooms = jc_roommaker_create(&roomctx);
    double elapsed = timer.stop();

    printf("Dungeon generated in %f ms\n", elapsed*1000.0f);

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
    stblib_write_png(path, image.width, image.height, image.channels, image.bytes, image.width*image.channels);
    printf("wrote %s (%d x %d x %d)\n", path, image.width, image.height, image.channels);

    free(image.bytes);

    return 0;
}
