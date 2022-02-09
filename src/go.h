#pragma once
#ifndef GO_H
#define GO_H

#include "deshi.h"
#include "kigu/array_utils.h"
#include "kigu/carray.h"

enum{
	Slot_Empty = 0,
	Slot_White = 1,
	Slot_Black = 2, //TODO maybe make this ~1 so they can be checked as opposites
};

//TODO make this one array where negative numbers mean inner or outer
/// OR add an eyes array
struct Chain{
	array<u32> outer; //indexes of slots with liberties or enemy neighbors (can't be empty)
	array<u32> inner; //indexes of slots with no liberties b/c of friendly neighbours (can be empty)
	
	Chain(Allocator* a = deshi_allocator) : outer(a), inner(a) {}
};

struct Rules{
	b32 legal_suicide = true;
	b32 chinese_scoring = true;
	b32 japanese_scoring = false;
	f32 komi = 5.5;
};

struct Score{
	u32 black_stones;
	u32 white_stones;
	u32 empty_stones;
	u32 black_captures;
	u32 white_captures;
	f32 black_score;
	f32 white_score;
};

Rules        get_rules();
array<u32>   get_board();
array<Chain> get_chains();
Score        get_score();

b32  in_seki(Chain* chain);
b32  valid_placement(u32 row, u32 col);
b32  place_piece(u32 row, u32 col); //if invalid placement, nothing changes and returns false
void pass_turn();

#endif //GO_H
