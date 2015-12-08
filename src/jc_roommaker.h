#ifndef JC_ROOMMAKER_H
#define JC_ROOMMAKER_H

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

struct SRoomMakerContext
{
    int dimensions[2];
    int maxnumrooms;
    int seed;
};

struct SRoom
{
    uint16_t    id;
    uint16_t    pos[2];
    uint16_t    dims[2];
    uint16_t    doors[4];
};

struct SRooms
{
    uint16_t    dimensions[2];
    uint16_t    numrooms;
    uint16_t    nextid;
    uint16_t*   grid;
    SRoom*      rooms;
};

SRooms* jc_roommaker_create(SRoomMakerContext* ctx);

void    jc_roommaker_free(SRoomMakerContext* ctx, SRooms* rooms);



#ifdef __cplusplus
} // extern C
#endif

#endif // JC_ROOMMAKER_H

#ifdef JC_ROOMMAKER_IMPLEMENTATION

static void jc_roommaker_make_rooms(SRoomMakerContext* ctx, SRooms* rooms, int numrooms, int numattempts);
static void jc_roommaker_make_mazes(SRoomMakerContext* ctx, SRooms* rooms);

SRooms* jc_roommaker_create(SRoomMakerContext* ctx)
{
    // TODO: Separate the context creation from the running of the algorithms
    SRooms* rooms   = (SRooms*)malloc(sizeof(SRooms));
    rooms->dimensions[0] = ctx->dimensions[0];
    rooms->dimensions[1] = ctx->dimensions[1];
    rooms->grid     = (uint16_t*)malloc( sizeof(uint16_t) * ctx->dimensions[0] * ctx->dimensions[1] );
    rooms->numrooms = 0;
    rooms->rooms    = (SRoom*)malloc( sizeof(SRoom) * ctx->dimensions[0] * ctx->dimensions[1] );
    rooms->nextid   = 1;

    memset(rooms->grid, 0, sizeof(uint16_t) * ctx->dimensions[0] * ctx->dimensions[1] );

    jc_roommaker_make_rooms(ctx, rooms, ctx->maxnumrooms, 1000);
    jc_roommaker_make_mazes(ctx, rooms);

    return rooms;
}

void jc_roommaker_free(SRoomMakerContext* ctx, SRooms* rooms)
{
    (void)ctx;
    (void)rooms;
    free(rooms->grid);
    free(rooms->rooms);
    free(rooms);
}

static inline float jc_roommaker_rand01()
{
    return rand() / (float)RAND_MAX;
}

static inline uint16_t jc_roommaker_max(uint16_t a, uint16_t b)
{
    return a > b ? a : b;
}

static inline uint16_t jc_roommaker_min(uint16_t a, uint16_t b)
{
    return a < b ? a : b;
}

static bool jc_roommaker_is_overlapping(uint16_t aleft, uint16_t atop, uint16_t aright, uint16_t abottom, uint16_t bleft, uint16_t btop, uint16_t bright, uint16_t bbottom, int debug)
{
	if(debug)
	{
		printf("A: %d, %d, %d, %d\n", aleft, atop, aright, abottom);
		printf("B: %d, %d, %d, %d\n", bleft, btop, bright, bbottom);
		printf("  aleft > bright = %d, %d -> %d\n", aleft, bright, aleft > bright);
		printf("  aright < bleft = %d, %d -> %d\n", aright, bleft, aright < bleft);
		printf("  atop > bbottom = %d, %d -> %d\n", atop, bbottom, atop > bbottom);
		printf("  abottom < btop = %d, %d -> %d\n", abottom, btop, abottom < btop);
	}
    return !(aleft > bright || aright < bleft || atop > bbottom || abottom < btop);
}

static bool jc_roommaker_is_overlapping(const SRooms* rooms, uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    uint16_t x2 = x + width - 1;
    uint16_t y2 = y + height - 1;
    // Grow the current room by 2, and the others by one
    // thus leaving a space of at least 3 cells between rooms
    x = x < 2 ? 0 : x - 2;
    y = y < 2 ? 0 : y - 2;
    x2 = x2 + 2;
    y2 = y2 + 2;
    int extra = 1;
    for( int i = 0; i < rooms->numrooms; ++i )
    {
        const SRoom* room = &rooms->rooms[i];
        uint16_t bx = room->pos[0] < extra ? 0 : room->pos[0] - extra;
        uint16_t by = room->pos[1] < extra ? 0 : room->pos[1] - extra;
        uint16_t bx2 = room->pos[0] + room->dims[0] - 1 + extra;
        uint16_t by2 = room->pos[1] + room->dims[1] - 1 + extra;
        if( jc_roommaker_is_overlapping(x, y, x2, y2, bx, by, bx2, by2, 0) )
            return true;
    }
    return false;
}

static void jc_roommaker_make_rooms(SRoomMakerContext* ctx, SRooms* rooms, int numrooms, int numattempts)
{
    (void)ctx;
    (void)numattempts;

    srand(ctx->seed);

    uint16_t minroomsize = 3;
    uint16_t roomsize = 20;

    int i = 0;
    for( ; i < numattempts && rooms->numrooms < ctx->maxnumrooms; ++i)
    {
        uint16_t posx = (uint16_t)( jc_roommaker_rand01() * (ctx->dimensions[0] - 1));
        uint16_t posy = (uint16_t)(jc_roommaker_rand01() * (ctx->dimensions[1] - 1));
        uint16_t width = (uint16_t)(jc_roommaker_rand01() * roomsize);
        uint16_t height = (uint16_t)(jc_roommaker_rand01() * roomsize);
        width = jc_roommaker_max(minroomsize, width);
        height = jc_roommaker_max(minroomsize, height);

        float userratio = 0.5f; // 0.0 <-> 1.0
        float ratio = (float)width / (float)height;
        if( ratio < userratio )
        {
        	width = (uint16_t)(height * userratio);
        }
        ratio = (float)height / (float)width;
        if( ratio < userratio )
        {
        	height = (uint16_t)(width * userratio);
        }

        // If the room exceeds the bounds of the area
        if( posx + width > ctx->dimensions[0] )
            width = ctx->dimensions[0] - posx;
        if( posy + height > ctx->dimensions[1] )
            height = ctx->dimensions[1] - posy;

        if( width < minroomsize || height < minroomsize )
            continue;

        bool overlapping = jc_roommaker_is_overlapping(rooms, posx, posy, width, height);
        if( !overlapping )
        {
            SRoom* room = &rooms->rooms[rooms->numrooms];
            memset(room, 0, sizeof(SRoom));
            room->id        = ++rooms->nextid;
            room->pos[0]    = posx;
            room->pos[1]    = posy;
            room->dims[0]   = width;
            room->dims[1]   = height;
            ++rooms->numrooms;

            //printf("room: %d   x, y: %d, %d   w, h: %d, %d\n", room->id, room->pos[0], room->pos[1], room->dims[0], room->dims[1]);

            for( int y = posy; y < (posy + height) && y < ctx->dimensions[1]; ++y)
            {
                for( int x = posx; x < (posx + width) && x < ctx->dimensions[0]; ++x)
                {
                    rooms->grid[ y * ctx->dimensions[0] + x ] = room->id;
                }
            }

        }
    }

    printf("Generated %d rooms (out of max %d) in %d attempts\n", rooms->numrooms, ctx->maxnumrooms, i);
}


static void jc_roommaker_make_mazes(SRoomMakerContext* ctx, SRooms* rooms)
{
    // loop through all pixels
    uint16_t width = rooms->dimensions[0];
    uint16_t height = rooms->dimensions[1];
    // 0 = E, 1 = N, 2 = W, 3 = S
    int16_t xoffsets[] = {2, 0, -2, 0};
    int16_t yoffsets[] = {0, -2, 0, 2};
    for( uint16_t cy = 0; cy < rooms->dimensions[1]; cy += 2 )
    {
        for( uint16_t cx = 0; cx < rooms->dimensions[0]; cx += 2 )
        {
            uint16_t index = cy * width + cx;
            if( rooms->grid[index] )
                continue;

            // We've got a start cell!
            uint16_t stack[256];
            int num_branches;

            uint16_t endpoints[256];
            uint16_t numendpoints = 0;

            endpoints[0] = index;
            numendpoints = 1;

            uint16_t id = rooms->nextid++;

            rooms->grid[index] = id;
            int currentx = cx;
            int currenty = cy;
            while( true )
            {

                // 0 = E, 1 = N, 2 = W, 3 = S
                int dir = rand() % 4;
                int nextx = -1;
                int nexty = -1;
                for( uint16_t d = 0; d < 4; ++d )
                {
                    int dd = (dir+d)%4;
                    int testx = cx + xoffsets[dd];
                    int testy = cy + yoffsets[dd];
                    if( testx < 0 || testx >= width || testy < 0 || testy >= height)
                        continue;

                    int testnext = testy * width + testx;
                    if( rooms->grid[testnext] == 0 )
                    {
                        nextx = testx;
                        nexty = testy;
                        dir = dd;
                        break;
                    }
                }

                if( nextx == -1 )
                    break;

                rooms->grid[nexty * width + nextx] = id;
                rooms->grid[(currenty + nexty)/2 * width + (currentx + nextx)/2] = id;
            }

        }
    }

    printf("id: %d\n", rooms->nextid);
}

#endif // JC_ROOMMAKER_IMPLEMENTATION
