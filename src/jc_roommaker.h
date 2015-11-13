#ifndef JC_ROOMMAKER_H
#define JC_ROOMMAKER_H

#ifdef __cplusplus
extern "C" {
#endif

struct SRoomMakerContext
{
    int dimensions[2];
    int maxnumrooms;
};

struct SRoom
{
    int     id;
    int     doors[4];
};

struct SRooms
{
    int     dimensions[2];
    int     numrooms;
    int*    grid;
    int     numrooms;
    SRoom*  rooms;
};

SRooms* jc_roommaker_create(SRoomMakerContext* ctx);

void    jc_roommaker_free(SRoomMakerContext* ctx, SRooms* rooms);



#ifdef __cplusplus
} // extern C
#endif

#endif // JC_ROOMMAKER_H

#ifdef JC_ROOMMAKER_IMPLEMENTATION


static void jc_roommaker_make_rooms(SRoomMakerContext* ctx, SRooms* rooms, int numrooms, int numattempts);

SRooms* jc_roommaker_create(SRoomMakerContext* ctx)
{
    SRooms* rooms   = (SRooms*)malloc(sizeof(SRooms));
    rooms->dimensions[0] = ctx->dimensions[0];
    rooms->dimensions[1] = ctx->dimensions[1];
    rooms->grid     = (int*)malloc( sizeof(int) * ctx->dimensions[0] * ctx->dimensions[1] );
    rooms->numrooms = 0;
    rooms->rooms    = (int*)malloc( sizeof(SRoom) * ctx->dimensions[0] * ctx->dimensions[1] );

    jc_roommaker_make_rooms(ctx, rooms, ctx->maxnumrooms, 100);
}

void jc_roommaker_free(SRoomMakerContext* ctx, SRooms* rooms)
{

}


static void jc_roommaker_make_rooms(SRoomMakerContext* ctx, SRooms* rooms, int numrooms, int numattempts)
{
    for( int i = 0; i < numattempts; ++i)
    {
        //rooms->
    }
}

#endif // JC_ROOMMAKER_IMPLEMENTATION
