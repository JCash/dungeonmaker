#pragma once

/**
 * Features:
 * * Rooms + corridors
 * * Doors with keys
 * * Solver/verifier
 * * Rooms with "holes" in the ground (i.e. the room is split in two)
 * * Rooms with unwalkable areas, until a switch is triggered
 */


struct SDungeonCreateContext
{
    int max_dimensions[2];
};

struct SDungeon
{
    int max_dimensions[]
};
