#ifndef JC_ROOMMAKER_H
#define JC_ROOMMAKER_H

#include <stdlib.h>
#include <assert.h>

#include <set>
#include <map>
#include <list>
#include <vector>
#include <sys/time.h>

#define JC_VORONOI_IMPLEMENTATION
#include "jc_voronoi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t 	u8;
typedef uint16_t 	u16;
typedef uint32_t 	u32;
typedef uint64_t 	u64;
typedef int8_t		i8;
typedef int16_t		i16;
typedef int32_t		i32;
typedef int64_t		i64;
typedef float       f32;
typedef double      f64;


struct SCoord
{
	u16	x;
	u16	y;
};

struct SRoomMakerContext
{
    i32 dimensions[2];
    i32 maxnumrooms;
    u32 seed;
    std::vector<i32> endpoints;

    void*	userctx;
    bool (*room_accept)(i32 x, i32 y, i32 w, i32 h, void* ctx);
};

struct SRoom
{
    i16    	pos[2];
    i16    	dims[2];
    i32    	doors[8];
    u16    	id;
    i16		numdoors;
};

struct SRooms
{
	i32    	dimensions[2];
	i32    	numrooms;
	i32    	nextid;
	i32*   	grid;
    SRoom* 	rooms;
    i32		mazeid_start;
    i32		mazeid_end;
    i32		doorid;
    i32		_pad;
};

SRooms* jc_roommaker_create(SRoomMakerContext* ctx);

void    jc_roommaker_free(SRoomMakerContext* ctx, SRooms* rooms);



#ifdef __cplusplus
} // extern C
#endif

#endif // JC_ROOMMAKER_H

#ifdef JC_ROOMMAKER_IMPLEMENTATION

class TimerScope
{
private:

    timeval start;
    const char* name;

public:
    TimerScope(const char* _name) : name(_name)
	{
    	gettimeofday(&start, NULL);
	}

    ~TimerScope()
    {
        timeval end;
        gettimeofday(&end, NULL);

        long seconds  = end.tv_sec  - start.tv_sec;
        long useconds = end.tv_usec - start.tv_usec;

        double duration = seconds + useconds/1000000.0;

        printf("%s took %5.6f ms\n", name, duration*1000.0);
    }
};

static i32 xoffsets[] = {1, 0, -1, 0};
static i32 yoffsets[] = {0, -1, 0, 1};

static void jc_roommaker_make_rooms_random(SRoomMakerContext* ctx, SRooms* rooms, i32 numattempts);
static void jc_roommaker_make_rooms_bsp(SRoomMakerContext* ctx, SRooms* rooms);
static void jc_roommaker_make_rooms_voronoi(SRoomMakerContext* ctx, SRooms* rooms);
static void jc_roommaker_make_mazes(SRoomMakerContext* ctx, SRooms* rooms);
static void jc_roommaker_find_doors(SRoomMakerContext* ctx, SRooms* rooms);
static void jc_roommaker_remove_dead_ends(SRoomMakerContext* ctx, SRooms* rooms);
static void jc_roommaker_cleanup_mazes(SRoomMakerContext* ctx, SRooms* rooms);

SRooms* jc_roommaker_create(SRoomMakerContext* ctx)
{
    // TODO: Separate the context creation from the running of the algorithms
    SRooms* rooms   = (SRooms*)malloc(sizeof(SRooms));
    memset(rooms, 0, sizeof(SRooms));
    rooms->dimensions[0] = ctx->dimensions[0];
    rooms->dimensions[1] = ctx->dimensions[1];
    rooms->grid     = (int*)malloc( sizeof(int) * (size_t)(ctx->dimensions[0] * ctx->dimensions[1]) );
    rooms->numrooms = 0;
    rooms->rooms    = (SRoom*)malloc( sizeof(SRoom) * (size_t)(ctx->dimensions[0] * ctx->dimensions[1]) );
    rooms->nextid   = 0;

    memset(rooms->grid, 0, sizeof(int) * (size_t)(ctx->dimensions[0] * ctx->dimensions[1]) );

    if(false)
    {
    	TimerScope timer("make_rooms_random");
    	jc_roommaker_make_rooms_random(ctx, rooms, 1000);
    }
    if(false)
    {
    	TimerScope timer("make_rooms_bsp");
    	jc_roommaker_make_rooms_bsp(ctx, rooms);
    }
    if(true)
    {
        TimerScope timer("make_rooms_voronoi");
        jc_roommaker_make_rooms_voronoi(ctx, rooms);
    }
    {
		TimerScope timer("make_mazes");
		jc_roommaker_make_mazes(ctx, rooms);
    }
    {
    	TimerScope timer("find_doors");
    	jc_roommaker_find_doors(ctx, rooms);
    }
    {
		TimerScope timer("remove_dead_ends");
		jc_roommaker_remove_dead_ends(ctx, rooms);
    }

    //
    {
		TimerScope timer("cleanup_mazes");
		jc_roommaker_cleanup_mazes(ctx, rooms);
    }

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

static inline i32 jc_roommaker_max(i32 a, i32 b)
{
    return a > b ? a : b;
}

static inline i32 jc_roommaker_min(i32 a, i32 b)
{
    return a < b ? a : b;
}

static bool jc_roommaker_is_overlapping(i32 aleft, i32 atop, i32 aright, i32 abottom,
										i32 bleft, i32 btop, i32 bright, i32 bbottom, i32 debug)
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

static bool jc_roommaker_is_overlapping(const SRooms* rooms, i32 x, i32 y, i32 width, i32 height)
{
    i32 x2 = x + width - 1;
    i32 y2 = y + height - 1;
    // Grow the current room by 2, and the others by one
    // thus leaving a space of at least 3 cells between rooms
    x = x < 2 ? 0 : x - 2;
    y = y < 2 ? 0 : y - 2;
    x2 = x2 + 2;
    y2 = y2 + 2;
    int extra = 1;
    for( i32 i = 0; i < rooms->numrooms; ++i )
    {
        const SRoom* room = &rooms->rooms[i];
        i32 bx = room->pos[0] < extra ? 0 : room->pos[0] - extra;
        i32 by = room->pos[1] < extra ? 0 : room->pos[1] - extra;
        i32 bx2 = room->pos[0] + room->dims[0] - 1 + extra;
        i32 by2 = room->pos[1] + room->dims[1] - 1 + extra;
        if( jc_roommaker_is_overlapping(x, y, x2, y2, bx, by, bx2, by2, 0) )
            return true;
    }
    return false;
}


static inline void jc_roommaker_draw_room(SRooms* rooms, const SRoom* room)
{
    //printf("room: %d   x, y: %d, %d   w, h: %d, %d\n", room->id, room->pos[0], room->pos[1], room->dims[0], room->dims[1]);

    for( int y = room->pos[1]; y < (room->pos[1] + room->dims[1]) && y < rooms->dimensions[1]; ++y)
    {
        for( int x = room->pos[0]; x < (room->pos[0] + room->dims[0]) && x < rooms->dimensions[0]; ++x)
        {
            rooms->grid[ y * rooms->dimensions[0] + x ] = room->id;
        }
    }
}

static inline void jc_roommaker_adjust_pos_and_dims(SRoomMakerContext* ctx, i32& x, i32& y, i32& width, i32& height)
{
	const i32 minroomsize = 3;

	/*
    // Align them on even positions
    x  &= ~0x1u;
    y  &= ~0x1u;
    // Make them odd sizes
    width = jc_roommaker_max(minroomsize, width | 0x1);
    height = jc_roommaker_max(minroomsize, height | 0x1);
    */
	if( x & 1 )
	{
		x += 1;
		width -= 1;
	}
	if( y & 1 )
	{
		y += 1;
		height -= 1;
	}
	if( (width & 1) == 0 )
	{
		width -= 1;
	}
	if( (height & 1) == 0 )
	{
		height -= 1;
	}

	width = jc_roommaker_max(minroomsize, width);
	height = jc_roommaker_max(minroomsize, height);

	/*
    float userratio = 0.5f; // 0.0 <-> 1.0
    float ratio = (float)width / (float)height;
    if( ratio < userratio )
    {
    	width = (i32)(height * userratio) | 0x1;
    }
    ratio = (float)height / (float)width;
    if( ratio < userratio )
    {
    	height = (i32)(width * userratio) | 0x1;
    }*/

    // If the room exceeds the bounds of the area
    if( x + width > ctx->dimensions[0] )
        width = ctx->dimensions[0] - x;
    if( y + height > ctx->dimensions[1] )
        height = ctx->dimensions[1] - y;
}

static void jc_roommaker_make_rooms_random(SRoomMakerContext* ctx, SRooms* rooms, i32 numattempts)
{
    (void)ctx;

    srand(ctx->seed);

    uint16_t minroomsize = 3;
    uint16_t roomsize = 20 - minroomsize;

    i32 i = 0;
    for( ; i < numattempts && rooms->numrooms < ctx->maxnumrooms; ++i)
    {
        i32 posx = (i32)( jc_roommaker_rand01() * (ctx->dimensions[0] - 1));
        i32 posy = (i32)(jc_roommaker_rand01() * (ctx->dimensions[1] - 1));
        i32 width = (i32)(minroomsize + jc_roommaker_rand01() * roomsize);
        i32 height = (i32)(minroomsize + jc_roommaker_rand01() * roomsize);

        jc_roommaker_adjust_pos_and_dims(ctx, posx, posy, width, height);

        if( width < minroomsize || height < minroomsize )
            continue;

        bool overlapping = jc_roommaker_is_overlapping(rooms, posx, posy, width, height);

        bool accepted = true;
    	if( !overlapping && ctx->room_accept != 0 )
    	{
    		accepted = ctx->room_accept(posx, posy, width, height, ctx->userctx);
    	}
        if( !overlapping && accepted )
        {
            SRoom* room = &rooms->rooms[rooms->numrooms];
            memset(room, 0, sizeof(SRoom));
            room->id        = (u16)++rooms->nextid;
            room->pos[0]    = (i16)posx;
            room->pos[1]    = (i16)posy;
            room->dims[0]   = (i16)width;
            room->dims[1]   = (i16)height;
            ++rooms->numrooms;

            jc_roommaker_draw_room(rooms, room);
        }
    }
}

struct SBspArea
{
	i32 x, y, w, h, area;
	float ratio;
};

static bool jc_roommaker_comp_areas( const SBspArea& a, const SBspArea& b )
{
	return (a.area * a.ratio) > (b.area * b.ratio);
}

static inline void jc_roommaker_create_area( SBspArea* area, i32 x, i32 y, i32 w, i32 h)
{
	area->x = x;
	area->y = y;
	area->w = w;
	area->h = h;
	area->area = w * h;
	area->ratio = w > h ? w/(float)h : h/(float)w;

	assert(area->area != 0);
}

static void jc_roommaker_make_rooms_bsp(SRoomMakerContext* ctx, SRooms* rooms)
{
    i32 width = rooms->dimensions[0];
    i32 height = rooms->dimensions[1];

	std::list<SBspArea> areas;
	SBspArea mainarea;
	jc_roommaker_create_area(&mainarea, 0, 0, width, height);
	areas.insert(areas.end(), mainarea);

	while( (i32)areas.size() < ctx->maxnumrooms )
	{
		SBspArea biggest = areas.front();
		areas.pop_front();

		// Needs to be minsize+1 in order to split well
		if( biggest.w < 8 && biggest.h < 8 )
		{
			areas.insert( areas.begin(), biggest );
			break;
		}

		float split = 0.5f + jc_roommaker_rand01() * 0.25f;
		if( biggest.w >= biggest.h )
		{
			SBspArea left;
			SBspArea right;
			i32 isplit = (i32)(biggest.w * split);
			if(isplit & 1)
				isplit -= 1;
			jc_roommaker_create_area(&left, biggest.x, biggest.y, isplit, biggest.h);

			jc_roommaker_create_area(&right, biggest.x + left.w, biggest.y, biggest.w - left.w, biggest.h);

			std::list<SBspArea>::iterator it = std::lower_bound( areas.begin(), areas.end(), left, jc_roommaker_comp_areas );
			areas.insert( it, left );
			it = std::lower_bound( areas.begin(), areas.end(), right, jc_roommaker_comp_areas );
			areas.insert( it, right );

			/*
			printf("Split\n");
			printf("  left   %d, %d  w,h: %d, %d\n", left.x, left.y, left.w, left.h);
			printf("  right  %d, %d  w,h: %d, %d\n", right.x, right.y, right.w, right.h);
			*/
		}
		else
		{
			SBspArea top;
			SBspArea bottom;
			i32 isplit = (i32)(biggest.h * split);
			if(isplit & 1)
				isplit -= 1;
			jc_roommaker_create_area(&top, biggest.x, biggest.y, biggest.w, isplit);
			jc_roommaker_create_area(&bottom, biggest.x, biggest.y + top.h, biggest.w, biggest.h - top.h);

			std::list<SBspArea>::iterator it = std::lower_bound( areas.begin(), areas.end(), top, jc_roommaker_comp_areas );
			areas.insert( it, top );
			it = std::lower_bound( areas.begin(), areas.end(), bottom, jc_roommaker_comp_areas );
			areas.insert( it, bottom );

			/*
			printf("Split\n");
			printf("  top     %d, %d  w,h: %d, %d\n", top.x, top.y, top.w, top.h);
			printf("  bottom  %d, %d  w,h: %d, %d\n", bottom.x, bottom.y, bottom.w, bottom.h);
			*/
		}
	}

	for( std::list<SBspArea>::iterator it = areas.begin(); it != areas.end(); ++it )
	{
		SBspArea& area = *it;

		//printf("area: %d, %d   w, h: %d, %d\n", area.x, area.y, area.w, area.h);

		// Make each area some random amount smaller
		float percent = 0.60f;
		if( area.w >= 6 )
		{
			float r = jc_roommaker_rand01();
			i32 decrease = 1 + (i32)((area.w-4) * r * percent);
			//printf("decrease w %d (%f)  r %f\n", decrease, (area.w-3) * r * percent, r);

			if( r < 0.5f )
				area.x += decrease;
			area.w -= decrease;
		}
		if( area.h >= 6 )
		{
			float r = jc_roommaker_rand01();
			i32 decrease = 1 + (i32)((area.h-4) * r * percent);

			//printf("decrease h %d (%f)  r %f\n", decrease, (area.h-3) * r * percent, r);

			if( r < 0.5f )
				area.y += decrease;
			area.h -= decrease;
		}

        if( area.w < 3 || area.h < 3 )
        	continue;

        jc_roommaker_adjust_pos_and_dims(ctx, area.x, area.y, area.w, area.h);

		SRoom* room = &rooms->rooms[rooms->numrooms];
		memset(room, 0, sizeof(SRoom));
		room->id        = (u16)++rooms->nextid;
		room->pos[0]    = (i16)area.x;
		room->pos[1]    = (i16)area.y;
		room->dims[0]   = (i16)area.w;
		room->dims[1]   = (i16)area.h;
		++rooms->numrooms;

		//printf("room: %d, %d   w, h: %d, %d\n", area.x, area.y, area.w, area.h);

        jc_roommaker_draw_room(rooms, room);
	}
}

static void jc_roommaker_make_rooms_voronoi(SRoomMakerContext* ctx, SRooms* rooms)
{
    i32 edgeoffset = 3;
    i32 width = rooms->dimensions[0];
    i32 height = rooms->dimensions[1];
    i32 width_inner = width - edgeoffset*2;
    i32 height_inner = height - edgeoffset*2;

    std::vector<jcv_point> points;
    points.resize(ctx->maxnumrooms);

    for( int i = 0; i < ctx->maxnumrooms; ++i )
    {
        points[i].x = jc_roommaker_rand01() * width_inner + edgeoffset;
        points[i].y = jc_roommaker_rand01() * height_inner + edgeoffset;
    }

    jcv_diagram diagram;
    memset(&diagram, 0, sizeof(jcv_diagram));
    jcv_diagram_generate((i32)points.size(), (const jcv_point*)&points[0], width, height, &diagram);

    const jcv_site* sites = jcv_diagram_get_sites(&diagram);
    for( int i = 0; i < diagram.numsites; ++i )
    {
        const jcv_site* site = &sites[i];
        jcv_point sum = site->p;
        int count = 1;

        const jcv_graphedge* edge = site->edges;

        f32 minx = width + 1;
        f32 maxx = -1;
        f32 miny = height + 1;
        f32 maxy = -1;
        while( edge )
        {
            const jcv_point& pos = edge->pos[0];
            sum.x += pos.x;
            sum.y += pos.y;
            ++count;
            edge = edge->next;

            minx = minx <= pos.x ? minx : pos.x;
            maxx = maxx >= pos.x ? maxx : pos.x;
            miny = miny <= pos.y ? miny : pos.y;
            maxy = maxy >= pos.y ? maxy : pos.y;
        }

        f32 fx = sum.x / count;
        f32 fy = sum.y / count;

        f32 shrink = 0.5f;
        //f32 area = (maxx - minx) * (maxy - miny);
        //f32 sideratio = (maxx - minx) / (maxy - miny);
        //f32 side = sqrtf(area);
        f32 fw = (maxx - minx) * shrink;
        f32 fh = (maxy - miny) * shrink;

        i32 x = (i32)fx;
        i32 y = (i32)fy;
        i32 w = (i32)fw;
        i32 h = (i32)fh;
        jc_roommaker_adjust_pos_and_dims(ctx, x, y, w, h);

        int everyother = 0;
        while( jc_roommaker_is_overlapping(rooms, x, y, w, h) && w >= 5 && h >= 5 )
        {
            if( everyother )
            {
                if( w > h )
                    x += 2;
                else
                    y += 2;
            }

            if( w > h )
                w -= 2;
            else
                h -= 2;

            everyother = (everyother+1) & 0x1;
        }

        if( jc_roommaker_is_overlapping(rooms, x, y, w, h) )
        {
            //printf("DISCARDED room %d: %d, %d   w, h: %d, %d   overlapping: %d\n", i, x, y, w, h, jc_roommaker_is_overlapping(rooms, x, y, w, h) ? 1 : 0 );
            continue;
        }

        //printf("room %d: %d, %d   w, h: %d, %d   overlapping: %d\n", i, x, y, w, h, jc_roommaker_is_overlapping(rooms, x, y, w, h) ? 1 : 0 );


        SRoom* room = &rooms->rooms[rooms->numrooms];
        memset(room, 0, sizeof(SRoom));
        room->id        = (u16)++rooms->nextid;
        room->pos[0]    = (i16)x;
        room->pos[1]    = (i16)y;
        room->dims[0]   = (i16)w;
        room->dims[1]   = (i16)h;
        ++rooms->numrooms;

        jc_roommaker_draw_room(rooms, room);
    }
}



static void jc_roommaker_make_mazes(SRoomMakerContext* ctx, SRooms* rooms)
{
	(void)ctx;

    // loop through all pixels
    i32 width = rooms->dimensions[0];
    i32 height = rooms->dimensions[1];

    int maxstacksize = 0;

    int* stack = (int*)malloc( sizeof(int) * width*height * 10 );

    rooms->mazeid_start = ++rooms->nextid;
    i32 id = rooms->mazeid_start;

    for( i32 cy = 0; cy < height; cy += 2 )
    {
        for( i32 cx = 0; cx < width; cx += 2 )
        {
            i32 index = cy * width + cx;
            if( rooms->grid[index] )
                continue;

            // We've got a start cell!
            int stacksize = 0;
            stack[stacksize] = index;
            stacksize++;

            //int endpoints[256];
            //int numendpoints = 0;
            //endpoints[numendpoints++] = index;
            ctx->endpoints.push_back(index);

            i32 nextindex = index;
            int iter = 0;
            while(stacksize)
            {
            	++iter;

            	i32 currentindex = nextindex;
                const int currentx = currentindex % width;
                const int currenty = currentindex / width;

                bool debug = false && currentx == 2 && currenty == 1;
                if(debug)
                	printf("current %d, %d\n", currentx, currenty);

				int next = -1;
                int fillindex = currenty * width + currentx;
                assert( fillindex != (1 * width + 2) );
                rooms->grid[fillindex] = id;

				// 0 = E, 1 = N, 2 = W, 3 = S
				i32 dir = rand() % 4;
				i32 outofbounds = 0;
				for( uint16_t d = 0; d < 4; ++d )
				{
					int dd = (dir+d)%4;
					int testx = currentx + xoffsets[dd]*2;
					int testy = currenty + yoffsets[dd]*2;

					if( testx < 0 || testx >= width || testy < 0 || testy >= height)
					{
						++outofbounds;
						continue;
					}

					int testnext = testy * width + testx;
					if( rooms->grid[testnext] == 0 )
					{
						next = testnext;
						break;
					}
				}

                // Carve the space between the two cells
                if( next != -1 )
                {
                    stack[stacksize] = currentindex;
                    ++stacksize;
                    nextindex = next;
                    //assert(stacksize < (int)(sizeof(stack)/sizeof(stack[0])));

                    const int nextx = next % width;
                    const int nexty = next / width;
                	fillindex = (currenty + nexty)/2 * width + (currentx + nextx)/2;
                	if( rooms->grid[fillindex] == 0 )
                		rooms->grid[fillindex] = (i32)id;

                	if(debug)
                	{
                		printf("  push %d, %d\n", currentx, currenty);
                		printf("    next %d, %d\n", nextx, nexty);
                		printf("    stackindex %d\n", stacksize-1);
                	}
                }
                else
                {
                    // Nowhere to go
                    //endpoints[numendpoints++] = currenty * width + currentx;
                    //printf("numendpoints %d\n", numendpoints);
                	if( !outofbounds )//&& ctx->endpoints.size() < 1 )
                	{
                		//printf("Adding endpoint: %d, %d  %d\n", currentx, currenty, currenty * width + currentx);
                		ctx->endpoints.push_back(currenty * width + currentx);
                	}

                    --stacksize;
                    nextindex = stack[stacksize];
                    const int nextx = nextindex % width;
                    const int nexty = nextindex / width;

                	if(debug)
                	{
                		printf("  pop %d, %d    %d\n", nextx, nexty, nextindex);
                		printf("    prev %d, %d\n", currentx, currenty);
                		printf("    stackindex %d\n", stacksize);
                	}
                }

                //printf("stacksize: %d\n", stacksize);
                maxstacksize = maxstacksize < stacksize ? stacksize : maxstacksize;
            }

            id = ++rooms->nextid;
        }
    }

    free(stack);

    rooms->mazeid_end = rooms->nextid;

    printf("maxstacksize: %d\n", maxstacksize);
}

struct SConnector
{
	i32 pos[2];
	i32 score; // higher score, better
};


static void jc_roommaker_find_doors(SRoomMakerContext* ctx, SRooms* rooms)
{
	(void)ctx;

    i32 width = rooms->dimensions[0];
    i32 height = rooms->dimensions[1];
    rooms->doorid = ++rooms->nextid;
    i32 id = rooms->doorid;

    std::map< i64, std::vector<SConnector> > connectors;

    for( i32 cy = 1; cy < height-1; ++cy )
    {
        for( i32 cx = 1; cx < width-1; ++cx )
        {
            i32 index = cy * width + cx;
            if( rooms->grid[index] )
                continue;

            bool debug = false;//cx == 7 && cy == 25;

            // Solid rock
            i32 prevx = cx - 1 < 0 ? -1 : cy * width + cx - 1;
            i32 nextx = cx + 1 >= width ? -1 : cy * width + cx + 1;
            i32 prevy = cy - 1 < 0 ? -1 : (cy - 1) * width + cx;
            i32 nexty = cy + 1 >= height ? -1 : (cy + 1) * width + cx;
            i32 prevx_id = prevx >= 0 ? rooms->grid[prevx] : 0;
            i32 nextx_id = nextx >= 0 ? rooms->grid[nextx] : 0;
            i32 prevy_id = prevy >= 0 ? rooms->grid[prevy] : 0;
            i32 nexty_id = nexty >= 0 ? rooms->grid[nexty] : 0;
            prevx_id = prevx_id != id ? prevx_id : 0;
            nextx_id = nextx_id != id ? nextx_id : 0;
            prevy_id = prevy_id != id ? prevy_id : 0;
            nexty_id = nexty_id != id ? nexty_id : 0;

            if(debug)
            {
            	printf("prevx_id %d   nextx_id % d\n", prevx_id, nextx_id);
            	printf("prevy_id %d   nexty_id % d\n", prevy_id, nexty_id);
            }

            i64 connectorid = 0;
            i32 region_a = 0;
            i32 region_b = 0;
            if( prevx_id && nextx_id && (prevx_id != nextx_id) )
            {
            	region_a = prevx_id;
            	region_b = nextx_id;
            }
            else if(prevy_id && nexty_id && (prevy_id != nexty_id) )
            {
            	region_a = prevy_id;
            	region_b = nexty_id;
            }

            if( region_b < region_a )
            {
            	i32 tmp = region_a;
            	region_a = region_b;
            	region_b = tmp;
            }

            connectorid = ((i64)region_a << 32) | region_b;
            if( connectorid )
			{
            	rooms->grid[index] = id;

            	SConnector connector = { {cx, cy}, 1 };

            	// TODO: Insert based on highest score!
            	connectors[connectorid].push_back( connector );
			}
		}
    }

    // Now we have a unique list of connectorid's
    // A connector id always contains at least one room
    std::set<i32> mainregions;
    std::set<i32> regions;

    mainregions.insert(1);

    for( i32 i = 1; i < rooms->numrooms; ++i )
    {
    	regions.insert( i+1 );
    }
    for( i32 i = rooms->mazeid_start; i < rooms->mazeid_end; ++i )
    {
    	regions.insert( i );
    }

	bool debug = false;
	if(debug)
	{
		for( std::set<i32>::iterator it = mainregions.begin(); it != mainregions.end(); ++it )
		{
			printf("mainregion %d   %s\n", *it, *it >= rooms->mazeid_start ? "(maze)" : "");
		}
		for( std::set<i32>::iterator it = regions.begin(); it != regions.end(); ++it )
		{
			printf("region     %d   %s\n", *it, *it >= rooms->mazeid_start ? "(maze)" : "");
		}
		printf("\n");
	}

    while( !regions.empty() )
    {
    	i32 region_a = 0;
    	i32 region_b = 0;

    	i64 connectorid = 0;
    	std::map<i64, std::vector<SConnector> >::iterator connectorit = connectors.begin();
    	for( ; connectorit != connectors.end(); ++connectorit )
    	{
			connectorid = connectorit->first;
			region_a = (i32)((connectorid>>32) & 0xFFFFFFFF);
			region_b = (i32)(connectorid & 0xFFFFFFFF);

			bool in_main_a = mainregions.find(region_a) != mainregions.end();
			bool in_main_b = mainregions.find(region_b) != mainregions.end();

			if( in_main_a != in_main_b )
			{
				if( !in_main_a )
				{
					regions.erase(region_a);
					mainregions.insert(region_a);
				}

				if( !in_main_b )
				{
					regions.erase(region_b);
					mainregions.insert(region_b);
				}

				if( debug )
				{
					printf("connectorid: %lld\n", connectorid);
					printf("a, b: %d, %d\n", region_a, region_b);
					printf("inmain a, b: %d, %d\n", in_main_a, in_main_b);
				}

				break;
			}
    	}

    	if(debug)
    	{
    	    for( std::set<i32>::iterator it = mainregions.begin(); it != mainregions.end(); ++it )
    	    {
    	    	printf("mainregion %d   %s\n", *it, *it >= rooms->mazeid_start ? "(maze)" : "");
    	    }
    	    for( std::set<i32>::iterator it = regions.begin(); it != regions.end(); ++it )
    	    {
    	    	printf("region     %d   %s\n", *it, *it >= rooms->mazeid_start ? "(maze)" : "");
    	    }
    	    printf("\n");
    	}

    	assert(connectorid != 0);
    	assert(connectorit != connectors.end());

    	// The region itself is added, now pick one (or more) of the connector positions
    	int chosen = rand();
    	int kept = 0;
    	const std::vector<SConnector>& connectorpositions = connectorit->second;
    	for( size_t i = 0; i < connectorpositions.size(); ++i )
    	{
			const SConnector& door = connectorpositions[i];
			i32 doorindex = door.pos[1] * width + door.pos[0];

    		// keep the first (highest score), clear the rest
    		bool keep = i == ((size_t)chosen % connectorpositions.size());

			if(debug)
			{
				printf("connectorid: %lld\n", connectorit->first);
				printf("door: %d, %d    keep  %d\n", door.pos[0], door.pos[1], keep);
			}

    		if( keep )
    		{
    			++kept;

    			// region a should be the smallest id, thus it should be a room
    			assert( region_a < region_b );
    			assert( region_a < rooms->mazeid_start );

    			SRoom* room_a = &rooms->rooms[region_a - 1];
    			assert( (size_t)room_a->numdoors < (sizeof(room_a->doors)/sizeof(room_a->doors[0])) );
    			room_a->doors[room_a->numdoors] = doorindex;
    			++room_a->numdoors;

    			// If the other region was also a room...
    			if( region_b < rooms->mazeid_start )
    			{
        			SRoom* room_b = &rooms->rooms[region_b - 1];
        			assert( (size_t)room_b->numdoors < (sizeof(room_b->doors)/sizeof(room_b->doors[0])) );
        			room_b->doors[room_b->numdoors] = doorindex;
        			++room_b->numdoors;
    			}
    		}
    		else
    		{
    			rooms->grid[doorindex] = 0;
    		}
    	}

    	assert(kept >= 1);

    	connectors.erase(connectorit);
    }

    // delete any remaining connectors
    std::map<i64, std::vector<SConnector> >::iterator it = connectors.begin();
    for( ; it != connectors.end(); ++it )
    {
    	const std::vector<SConnector>& connectorpositions = it->second;
    	for( size_t i = 0; i < connectorpositions.size(); ++i )
    	{
			const SConnector& door = connectorpositions[i];
			i32 doorindex = door.pos[1] * width + door.pos[0];

			rooms->grid[doorindex] = 0;
    	}
    }
}

/*
void jc_roommaker_remove_dead_ends_old(SRoomMakerContext* ctx, SRooms* rooms)
{
	i32 width = rooms->dimensions[0];
	i32 height = rooms->dimensions[1];
	for( size_t i = 0; i < ctx->endpoints.size(); ++i )
	{
		i32 index = ctx->endpoints[i];
		assert( rooms->grid[index] );

		rooms->grid[index] = 0;

		while( true )
		{
			i32 cx = index % width;
			i32 cy = index / width;

			//printf("p %d, %d\n", cx, cy);

			i32 count = 0;
			i32 nx = 0;
			i32 ny = 0;

		    // 0 = E, 1 = N, 2 = W, 3 = S
		    for( i32 d = 0; d < 4; ++d )
			{
		    	i32 xx = cx + xoffsets[d];
		    	i32 yy = cy + yoffsets[d];

				if( xx < 0 || xx >= width )
					continue;
				if( yy < 0 || yy >= height )
					continue;

				i32 neighbor = yy * width + xx;

				i32 filled = rooms->grid[neighbor] != 0 ? 1 : 0;
				count += filled;
				if( filled )
				{
					nx = xx;
					ny = yy;
				}

				//printf("   n %d, %d   = %d\n", nx, ny, rooms->grid[neighbor]);
			}

			if( count > 1 )
				break;

			rooms->grid[index] = 0;
			index = ny * width + nx;
		}
	}

	ctx->endpoints.clear();
}
*/

void jc_roommaker_remove_dead_ends(SRoomMakerContext* ctx, SRooms* rooms)
{
	i32 width = rooms->dimensions[0];
	i32 height = rooms->dimensions[1];

	int numdeadends = 0;
	for( i32 i = 0; i < width * height; i += 2 )
	{
		i32 index = i;

		if( rooms->grid[index] < rooms->mazeid_start || rooms->grid[index] == rooms->doorid )
			continue;

		bool debug = false;// ((index % width) == 52) && ((index / width) == 2);

		i32 count = 0;
		int first = 1;
		do
		{
			i32 cx = index % width;
			i32 cy = index / width;

			if(debug)
				printf("p %d, %d\n", cx, cy);

			i32 nx = 0;
			i32 ny = 0;
			count = 0;

		    // 0 = E, 1 = N, 2 = W, 3 = S
		    for( i32 d = 0; d < 4; ++d )
			{
		    	i32 xx = cx + xoffsets[d];
		    	i32 yy = cy + yoffsets[d];

				if( xx < 0 || xx >= width )
					continue;
				if( yy < 0 || yy >= height )
					continue;

				i32 neighbor = yy * width + xx;

				i32 filled = (rooms->grid[neighbor] != 0 && rooms->grid[neighbor] != 1) ? 1 : 0;
				count += filled;
				if( filled )
				{
					nx = xx;
					ny = yy;
				}

				if(debug)
					printf("   n %d, %d   = %d %s  filled: %d\n", xx, yy, rooms->grid[neighbor], rooms->grid[neighbor] < rooms->mazeid_start ? "" : (rooms->grid[neighbor] == rooms->doorid ? "door" : "maze"), filled);
			}

			if(debug)
				printf("   count  %d\n", count);

			if( count == 1 )
			{
				rooms->grid[index] = 0;

				if(first)
					++numdeadends;
				first = 0;
			}
			index = ny * width + nx;

		} while( count == 1 );

		//if( numdeadends == 500 )
		//	break;
		//break; // test
	}
}

static bool jc_roommaker_maze_is_single_connected(const SRooms* rooms, i32 startx, i32 starty, i32 endx, i32 endy, i32 dir, bool debug)
{
	i32 width = rooms->dimensions[0];
	i32 height = rooms->dimensions[1];
	i32 roomid = rooms->grid[starty * width + startx];

	if(debug) printf("  roomid  %d\n", roomid);

	if(debug)
	{
		printf(" startx, starty  %d, %d  dir %d\n", startx, starty, dir);
		printf(" endx, endy  %d, %d\n", endx, endy);
	}

	while( startx != endx || starty != endy )
	{
		if(debug)
		{
			printf(" startx/starty  %d, %d\n", startx, starty);
		}

		// 0 = E, 1 = N, 2 = W, 3 = S
		i32 x1 = startx + xoffsets[dir];
		i32 y1 = starty + yoffsets[dir];
		if( x1 < 0 || x1 >= width || y1 < 0 || y1 >= height )
			return false;

		i32 nextroomid = rooms->grid[y1 * width + x1];
		if( nextroomid != roomid )
		{
			if(debug) printf("(a) nextroomid != roomid   %d != %d\n", nextroomid, roomid);
			return false;
		}

		if(debug)
		{
			printf(" x1, y1  %d, %d\n", x1, y1);
		}
		i32 x2 = startx + xoffsets[dir]*2;
		i32 y2 = starty + yoffsets[dir]*2;

		if(debug)
		{
			printf(" x2, y2  %d, %d\n", x2, y2);
		}
		// Figure out where to go next
		nextroomid = 0;
		i32 count = 0;
		i32 dd = 0;
		i32 xx = 0;
		i32 yy = 0;
		for( i32 d = 0; d < 4; ++d )
		{
			if( d == ((dir+2)%4) )
				continue; // Don't go back where we came from
			i32 tx = x2 + xoffsets[d];
			i32 ty = y2 + yoffsets[d];
			i32 testroomid = rooms->grid[ty * width + tx];
			count += testroomid != 0 ? 1 : 0;

			if( testroomid == roomid )
			{
				if(debug) printf("  x, y  %d, %d   d %d   ((dir+2)%%4)) %d\n", tx, ty, d, (dir+2)%4);
				dd = d;
				xx = tx;
				yy = ty;
			}
		}
		// Check for doors next to the mid segment
		i32 tx = x1 + xoffsets[(dir+1)%4];
		i32 ty = y1 + yoffsets[(dir+1)%4];
		i32 testroomid = rooms->grid[ty * width + tx];
		count += testroomid != 0 ? 1 : 0;
		tx = x1 + xoffsets[(dir-1+4)%4];
		ty = y1 + yoffsets[(dir-1+4)%4];
		testroomid = rooms->grid[ty * width + tx];
		count += testroomid != 0 ? 1 : 0;

		// If we encounter a crossing, it's not single connected
		if( count > 1 )
		{
			if(debug) printf("Count > 1 (%d)\n", count);
			return false;
		}

		nextroomid = rooms->grid[yy * width + xx];
		if( nextroomid != roomid )
		{
			if(debug) printf("(b) nextroomid != roomid   %d != %d\n", nextroomid, roomid);
			return false;
		}

		startx 	= x2;
		starty 	= y2;
		dir		= dd;
	}
	return true;
}

static void jc_roommaker_maze_remove(const SRooms* rooms, i32 startx, i32 starty, i32 endx, i32 endy, i32 dir, bool debug, i32 debugid)
{
	i32 width = rooms->dimensions[0];
	i32 height = rooms->dimensions[1];
	i32 roomid = rooms->grid[starty * width + startx];
	rooms->grid[starty * width + startx] = 0;

	while( startx != endx || starty != endy )
	{
		// 0 = E, 1 = N, 2 = W, 3 = S
		i32 nextx = startx + xoffsets[dir];
		i32 nexty = starty + yoffsets[dir];
		if( nextx < 0 || nextx >= width || nexty < 0 || nexty >= height )
			return;

		i32 nextroomid = rooms->grid[nexty * width + nextx];
		assert( nextroomid == roomid );

		nextx = startx + xoffsets[dir]*2;
		nexty = starty + yoffsets[dir]*2;

		i32 id = debugid != -1 ? debugid : 0;

		rooms->grid[nexty * width + nextx] = id;
		rooms->grid[(starty + nexty)/2 * width + (startx + nextx)/2] = id;

		// Figure out where to go next
		i32 dd = 0;
		for( i32 d = 0; d < 4; ++d )
		{
			if( d == ((dir+2)%4) )
				continue; // Don't go back where we came from
			i32 x = nextx + xoffsets[d];
			i32 y = nexty + yoffsets[d];
			i32 testroomid = rooms->grid[y * width + x];
			if( testroomid == roomid )
			{
				dd = d;
				break;
			}
		}

		startx 	= nextx;
		starty 	= nexty;
		dir		= dd;
	}
}

static void jc_roommaker_cleanup_mazes(SRoomMakerContext* ctx, SRooms* rooms)
{
	(void)ctx;

	i32 width = rooms->dimensions[0];
	i32 height = rooms->dimensions[1];

	for( i32 y = 0; y < height; y += 2 )
	{
		for( i32 x = 0; x < width; x += 2 )
		{
			i32 roomid = rooms->grid[y * width + x];
			if( roomid < rooms->mazeid_start )
				continue;

			// 0 = E, 1 = N, 2 = W, 3 = S
			for( i32 d = 0; d < 4; ++d )
			{
				i32 x2 = x + xoffsets[d]*2;
				i32 y2 = y + yoffsets[d]*2;

				if( x2 < 0 || x2 >= width || y2 < 0 || y2 >= height )
					continue;

				i32 candidate = y2 * width + x2;
				if( rooms->grid[candidate] != roomid )
					continue;

				// Now make sure the closest neighbor is 0
				i32 x1 = x + xoffsets[d];
				i32 y1 = y + yoffsets[d];
				if( rooms->grid[y1 * width + x1] != 0 )
					continue;

				bool debug = false && x == 4 && y == 2;

				// Ok, found a candidate
				// Now let's trace the orthogonal directions to see if we get to that point
				bool connected1 = jc_roommaker_maze_is_single_connected(rooms, x, y, x2, y2, (d+1) % 4, debug);
				bool connected2 = jc_roommaker_maze_is_single_connected(rooms, x, y, x2, y2, (d-1+4) % 4, debug);

				if(debug)
				{
					printf("roomid = %d\n", roomid);
					printf("connected1: %d   %d, %d -> %d, %d  d %d\n", connected1, x, y, x2, y2, (d+1) % 4);
					printf("connected2: %d   %d, %d -> %d, %d  d %d\n", connected2, x, y, x2, y2, (d-1+4) % 4);
					printf("   x1, y2 == %d, %d   %d\n", x1, y1, rooms->grid[y1 * width + x1]);
				}

				if( connected1 )
					jc_roommaker_maze_remove(rooms, x, y, x2, y2, (d+1) % 4, debug, -1);
				else if( connected2 )
					jc_roommaker_maze_remove(rooms, x, y, x2, y2, (d-1+4) % 4, debug, -1);

				if( connected1 || connected2 )
				{
					i32 id = roomid;
					rooms->grid[y * width + x] = id;
					rooms->grid[y1 * width + x1] = id;
					rooms->grid[y2 * width + x2] = id;
				}
			}
		}
	}
}

static void jc_roommaker_cleanup_maze(SRoomMakerContext* ctx, SRooms* rooms, i32 x, i32 y, i32 dir, i32 id)
{
	(void)ctx;

	i32 width = rooms->dimensions[0];
	i32 height = rooms->dimensions[1];

	i32 count = 1;
	while( count == 1 )
	{
		i32 x1 = x + xoffsets[dir];
		i32 y1 = y + yoffsets[dir];
		i32 x2 = x + xoffsets[dir]*2;
		i32 y2 = y + yoffsets[dir]*2;

		count = 0;
		i32 nextdir = 0;
		for( i32 d = 0; d < 4; ++d )
		{
			if( d == (dir+2)%4 )
				continue; // skip the dir we're coming from

			// First, check for junctions, and what direction we'll take next
			i32 tx = x + xoffsets[d];
			i32 ty = y + yoffsets[d];

			if( tx < 0 || tx >= width || ty < 0 || ty >= height )
				continue;

			i32 testid = rooms->grid[ty * width + tx];
			if( testid != 0 )
				++count;
			if( testid == id )
			{
				nextdir = d;
			}

			// Second, check if we have a distant maze with same id
			tx = x + xoffsets[d]*2;
			ty = y + yoffsets[d]*2;

			if( tx < 0 || tx >= width || ty < 0 || ty >= height )
				continue;

			i32 testid2 = rooms->grid[ty * width + tx];
			if( testid2 == id && testid != id )
			{
				bool debug = false && x == 4 && y == 2;

				// Ok, found a candidate
				// Now let's trace the orthogonal directions to see if we get to that point
				bool connected1 = jc_roommaker_maze_is_single_connected(rooms, x, y, tx, ty, (d+1) % 4, debug);
				bool connected2 = jc_roommaker_maze_is_single_connected(rooms, x, y, tx, ty, (d-1+4) % 4, debug);

				if( connected1 )
					jc_roommaker_maze_remove(rooms, x, y, x2, y2, (d+1) % 4, debug, -1);
				else if( connected2 )
					jc_roommaker_maze_remove(rooms, x, y, x2, y2, (d-1+4) % 4, debug, -1);

				if( connected1 || connected2 )
				{
					rooms->grid[y * width + x] = id;
					rooms->grid[y1 * width + x1] = id;
					rooms->grid[y2 * width + x2] = id;
				}
			}
		}
		// Check for doors next to the mid segment
		i32 tx = x1 + xoffsets[(dir+1)%4];
		i32 ty = y1 + yoffsets[(dir+1)%4];
		i32 testid = rooms->grid[ty * width + tx];
		count += testid != 0 ? 1 : 0;
		tx = x1 + xoffsets[(dir-1+4)%4];
		ty = y1 + yoffsets[(dir-1+4)%4];
		testid = rooms->grid[ty * width + tx];
		count += testid != 0 ? 1 : 0;

		// if the next step is a junction, then break
		if(count > 1)
			break;



		// step
		x	= x2;
		y	= y2;
		dir	= nextdir;
	}

	/*
	// 0 = E, 1 = N, 2 = W, 3 = S
	for( i32 d = 0; d < 4; ++d )
	{
		i32 x2 = x + xoffsets[d]*2;
		i32 y2 = y + yoffsets[d]*2;

		if( x2 < 0 || x2 >= width || y2 < 0 || y2 >= height )
			continue;

		i32 candidate = y2 * width + x2;
		if( rooms->grid[candidate] != roomid )
			continue;

		// Now make sure the closest neighbor is 0
		i32 x1 = x + xoffsets[d];
		i32 y1 = y + yoffsets[d];
		if( rooms->grid[y1 * width + x1] != 0 )
			continue;

		bool debug = false && x == 4 && y == 2;

		// Ok, found a candidate
		// Now let's trace the orthogonal directions to see if we get to that point
		bool connected1 = jc_roommaker_maze_is_single_connected(rooms, x, y, x2, y2, (d+1) % 4, debug);
		bool connected2 = jc_roommaker_maze_is_single_connected(rooms, x, y, x2, y2, (d-1+4) % 4, debug);

		if(debug)
		{
			printf("roomid = %d\n", roomid);
			printf("connected1: %d   %d, %d -> %d, %d  d %d\n", connected1, x, y, x2, y2, (d+1) % 4);
			printf("connected2: %d   %d, %d -> %d, %d  d %d\n", connected2, x, y, x2, y2, (d-1+4) % 4);
			printf("   x1, y2 == %d, %d   %d\n", x1, y1, rooms->grid[y1 * width + x1]);
		}

		if( connected1 )
			jc_roommaker_maze_remove(rooms, x, y, x2, y2, (d+1) % 4, debug, -1);
		else if( connected2 )
			jc_roommaker_maze_remove(rooms, x, y, x2, y2, (d-1+4) % 4, debug, -1);

		if( connected1 || connected2 )
		{
			i32 id = roomid;
			rooms->grid[y * width + x] = id;
			rooms->grid[y1 * width + x1] = id;
			rooms->grid[y2 * width + x2] = id;
		}
		*/
}

/* currently not working
static void jc_roommaker_cleanup_mazes_new(SRoomMakerContext* ctx, SRooms* rooms)
{
	i32 width = rooms->dimensions[0];
	i32 height = rooms->dimensions[1];

	for( i32 r = 0; r < rooms->numrooms; ++r )
	{
		const SRoom* room = &rooms->rooms[r];
		for( i32 door = 0; door < room->numdoors; ++door )
		{
			i32 doorindex = room->doors[door];
			i32 doorx = doorindex % width;
			i32 doory = doorindex / width;

			printf(" doorindex, doorx, doory  %d  %d, %d\n", doorindex, doorx, doory);

			// 0 = E, 1 = N, 2 = W, 3 = S
			for( i32 d = 0; d < 4; ++d )
			{
				i32 startx = doorx + xoffsets[d];
				i32 starty = doory + yoffsets[d];

				i32 id = rooms->grid[starty * width + startx];
				if( id < rooms->mazeid_start )
					continue;

				// Since the doors may be on totally uneven positions, we need to take that into account
				i32 startxleft 	= startx - (startx & 1);
				i32 startxright = startx + (startx & 1);
				i32 startyleft 	= starty - (starty & 1);
				i32 startyright = starty + (starty & 1);
				i32 dirleft		= (d+1)%4;
				i32 dirright	= (d-1+4)%4;

				jc_roommaker_cleanup_maze(ctx, rooms, startxleft, startyleft, dirleft, id);
				jc_roommaker_cleanup_maze(ctx, rooms, startxright, startyright, dirright, id);
			}
		}
	}
}
*/

#endif // JC_ROOMMAKER_IMPLEMENTATION
