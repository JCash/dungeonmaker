
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "external/stblib.h"

#define JC_ROOMMAKER_IMPLEMENTATION
#include "jc_roommaker.h"

const int NUMCELLS = 256;
const int PIXELS_PER_ROOM = 1;
const int IMAGEDIMS = NUMCELLS * PIXELS_PER_ROOM;


static int mask_width = 0;
static int mask_height = 0;
static int mask_channels 	= 0;
static unsigned char* mask_image = 0;

struct SImage
{
    int             width;
    int             height;
    int             channels;
    int             stride;
    unsigned char*  bytes;
};

/*
static bool accept_room(int x, int y, int w, int h, void* ctx)
{
	int xx = x + w / 2;
	int yy = y + h / 2;

	uint8_t* image = (uint8_t*)ctx;

	uint8_t color_star[] = { 253, 173, 58 };
	uint8_t color_tree[] = { 0, 127, 56 };
	uint8_t color_trunk[] = { 104, 60, 21 };

	float fx = xx / (float)IMAGEDIMS;
	float fy = yy / (float)IMAGEDIMS;

	xx = (int)(fx * mask_width);
	yy = (int)(fy * mask_height);

	int index = yy * mask_width * mask_channels + xx * mask_channels;

	uint8_t* pixel = &image[ yy * mask_width * mask_channels + xx * mask_channels];

	if( pixel[0] == 0 && pixel[1] == 0 && pixel[2] == 0 )
		return false;
	return true;
}
*/
static void render_rooms(const SRooms* rooms, SImage* image)
{
    const uint8_t color_door[3] = { 170, 170, 100 };
    const uint8_t color_maze[3] = { 127, 127, 127 };

    uint8_t colors[256*3];
    for( int i = 0; i < 256; ++i)
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
            if( roomid == 0 )
            {
            	image->bytes[index+0] = 0;
            	image->bytes[index+1] = 0;
            	image->bytes[index+2] = 0;
            }
            else if( roomid == rooms->doorid )
            {
                image->bytes[index+0] = color_door[0];
                image->bytes[index+1] = color_door[1];
                image->bytes[index+2] = color_door[2];
            }
            else if( roomid < rooms->mazeid_start )
            {
				roomid = roomid % 256;
				image->bytes[index+0] = colors[roomid*3 + 0];
				image->bytes[index+1] = colors[roomid*3 + 1];
				image->bytes[index+2] = colors[roomid*3 + 2];
            }
            else
            {
				image->bytes[index+0] = color_maze[0];
				image->bytes[index+1] = color_maze[1];
				image->bytes[index+2] = color_maze[2];
            }
        }
    }
}

/*
static inline int clamp(int v, int a, int b)
{
	return v < a ? a : (v > b ? b : v);
}

static void render_rooms_mask(const SRooms* rooms, SImage* image)
{
    const uint8_t color_door[3] = { 190, 190, 190 };
    //const uint8_t color_maze[3] = { 110, 110, 110 };
    const uint8_t color_maze[3] = { 100, 110, 120 };

    uint8_t rand[256] = { 0 };
    int maxrand = 50;
    int halfrand = maxrand / 2;
    for( int i = 0; i < 256; ++i)
    {
    	rand[i] = (uint8_t)(jc_roommaker_rand01() * maxrand);
    }

    for( int y = 0; y < rooms->dimensions[1]; ++y)
    {
        for( int x = 0; x < rooms->dimensions[0]; ++x)
        {
            int index = (y * PIXELS_PER_ROOM) * image->width * image->channels + (x * PIXELS_PER_ROOM) * image->channels;

            int roomid = rooms->grid[y * rooms->dimensions[0] + x];
            if( roomid == 0 )
            	continue;

            if( roomid < rooms->mazeid_start )
            {
            	const SRoom* room = &rooms->rooms[roomid-1];
            	int xx = room->pos[0] + room->dims[0]/2;
            	int yy = room->pos[1] + room->dims[1]/2;
            	float fx = xx / (float)IMAGEDIMS;
            	float fy = yy / (float)IMAGEDIMS;

            	bool debug = room->pos[0] == 252 && room->pos[1] == 12 && x == room->pos[0] && y == room->pos[1];
            	debug |= room->pos[0] == 246 && room->pos[1] == 38 && x == room->pos[0] && y == room->pos[1];
            	debug |= room->pos[0] == 286 && room->pos[1] == 32 && x == room->pos[0] && y == room->pos[1];
            	debug = false;
            	if( debug )
            	{
            		printf("  xx/yy  %d, %d\n", xx, yy);
            		printf("  fx/fy  %f, %f\n", fx, fy);
            	}

            	xx = (int)(fx * mask_width);
            	yy = (int)(fy * mask_height);

            	uint8_t* pixel = &mask_image[yy * mask_width * mask_channels + xx * mask_channels];

            	if( debug )
				{
					printf("  xx/yy  %d, %d\n", xx, yy);
				}

        		uint8_t c = rand[roomid%256];
            	if( pixel[0] == 0 && pixel[1] == 0 && pixel[2] == 0 )
            	{
    				image->bytes[index+0] = (uint8_t)clamp(0 + c - halfrand, 0, 255);
    				image->bytes[index+1] = (uint8_t)clamp(80 + c - halfrand, 0, 255);
    				image->bytes[index+2] = (uint8_t)clamp(120 + c - halfrand, 0, 255);
            	}
            	else
            	{
    				image->bytes[index+0] = (uint8_t)clamp(pixel[0] + c - halfrand, 0, 255);
    				image->bytes[index+1] = (uint8_t)clamp(pixel[1] + c - halfrand, 0, 255);
    				image->bytes[index+2] = (uint8_t)clamp(pixel[2] + c - halfrand, 0, 255);
            	}
            }
            else
            {
            	uint8_t c = (uint8_t)(jc_roommaker_rand01() * 30);
				image->bytes[index+0] = (uint8_t)clamp(color_maze[0] + c, 0, 255);
				image->bytes[index+1] = (uint8_t)clamp(color_maze[1] + c, 0, 255);
				image->bytes[index+2] = (uint8_t)clamp(color_maze[2] + c, 0, 255);
            }
        }
    }


    for( int i = 0; i < rooms->numrooms; ++i )
    {
    	const SRoom* room = &rooms->rooms[i];
    	for( int d = 0; d < room->numdoors; ++d )
    	{
    		int x = room->doors[d] % rooms->dimensions[0];
    		int y = room->doors[d] / rooms->dimensions[0];

            int index = (y * PIXELS_PER_ROOM) * image->width * image->channels + (x * PIXELS_PER_ROOM) * image->channels;

            image->bytes[index+0] = color_door[0];
            image->bytes[index+1] = color_door[1];
            image->bytes[index+2] = color_door[2];
    	}
    }
}
*/



int main(int argc, const char** argv)
{
    (void)argc;
    (void)argv;
    SRoomMakerContext roomctx;
    roomctx.dimensions[0]   = NUMCELLS;
    roomctx.dimensions[1]   = NUMCELLS;
    roomctx.maxnumrooms     = (int)sqrtf(NUMCELLS) * 20;
    roomctx.seed            = 0;

    //roomctx.room_accept = accept_room;

    /*
    mask_image = stblib_load("xmastreemask.png", &mask_width, &mask_height, &mask_channels, 0);
    if( !mask_image )
    {
    	printf("didn't find mask image");
    	return 1;
    }
    */
    mask_image			= 0;
    roomctx.userctx		= (void*)mask_image;

    printf("Mask: %d, %d, %d\n", mask_width, mask_height, mask_channels);

    printf("Dims: %d, %d\n", roomctx.dimensions[0], roomctx.dimensions[1]);
    printf("Seed: 0x%08x\n", roomctx.seed);

    SRooms* rooms = 0;
    {
		TimerScope timer("Dungeon generation");
		rooms = jc_roommaker_create(&roomctx);
    }

    printf("Generated %d rooms (out of max %d)\n", rooms->numrooms, roomctx.maxnumrooms);

    SImage image;
    image.width     = IMAGEDIMS;
    image.height    = IMAGEDIMS;
    image.channels  = 3;

    size_t imagesize = (size_t)(image.width * image.height * image.channels);
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
