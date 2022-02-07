//TODO game end
//TODO komi = 6.5
//TODO maybe superko?

//// @includes ////
#include "deshi.h"
#include "utils/array_utils.h"
#include "utils/carray.h"

//// @structures ////
enum{
	Slot_Empty = 0,
	Slot_White = 1,
	Slot_Black = 2,
};

struct Chain{ //TODO make this one array where negative numbers mean inner or outer
	array<u32> outer; //indexes of slots with liberties (can't be empty)
	array<u32> inner; //indexes of slots with no liberties b/c of friendly neighbours (can be empty)
	
	Chain(Allocator* a = deshi_allocator) : outer(a), inner(a) {}
};

//// @variables ////
const char* board_letters[] = { "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", };
const char* board_numbers[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", };
const char* board_numbers_spacing[] = { " 1", " 2", " 3", " 4", " 5", " 6", " 7", " 8", " 9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", };
Font* font;
vec2 board_pos;
vec2 board_dims;
u32  board_size;
b32 suicide_is_illegal = true;

b32 black_turn = true;
b32 turn_passed = false;
b32 game_over = false;
u32 whites_prev_placement = -1;
u32 blacks_prev_placement = -1;
u32 whites_prev_capture = -1; //value is -1 if more than one was captured
u32 blacks_prev_capture = -1;
u32 whites_capture_count = 0;
u32 blacks_capture_count = 0;

carray<u32>  slots; //row major matrix
carray<u32>  chained; //chain indexes of slots
array<Chain> chains(deshi_allocator);

//// @functions ////
#define ToLinear(row,col) ((board_size*(row))+(col))

#define SlotLinear(pos) slots[pos]
#define Slot(row,col) slots[ToLinear(row,col)]
#define SlotIsColor(row,col,color) (Slot(row,col) == color)
#define SlotIsEmpty(row,col) SlotIsColor(row,col,Slot_Empty)
#define SlotIsWhite(row,col) SlotIsColor(row,col,Slot_White)
#define SlotIsBlack(row,col) SlotIsColor(row,col,Slot_Black)

#define ChainAtLinear(pos) chains[pos]
#define ChainAt(row,col) chains[ToLinear(row,col)]
#define ChainedLinear(pos) chained[pos]
#define Chained(row,col) chained[ToLinear(row,col)]
#define ChainCount(row,col) (chains[Chained(row,col)].inner.count + chains[Chained(row,col)].outer.count)

//returns false if already chained or a different color
b32 FillChain(u32 chain_idx, u32 row, u32 col, u32 color){
	u32 pos = ToLinear(row,col);
	
	//exit if not chainable
	if(SlotLinear(pos) != color) return false;
	if(ChainedLinear(pos) != -1) return false;
	
	//set to this chain
	ChainedLinear(pos) = chain_idx;
	
	//check neighbours
	u32 neighbour_count = 0;
	if(col > 0            && FillChain(chain_idx,row,  col-1,color)){ neighbour_count++; } //LEFT
	if(row > 0            && FillChain(chain_idx,row-1,col  ,color)){ neighbour_count++; } //UP
	if(col < board_size-1 && FillChain(chain_idx,row,  col+1,color)){ neighbour_count++; } //RIGHT
	if(row < board_size-1 && FillChain(chain_idx,row+1,col  ,color)){ neighbour_count++; } //DOWN
	
	//check neighbour count to see if inner or outer part of chain
	if(neighbour_count == 4){
		chains[chain_idx].inner.add(pos);
	}else{
		chains[chain_idx].outer.add(pos);
	}
	
	return true;
}

u32 LibertyCount(u32 row, u32 col){
	u32 count = 0;
	if(row > 0            && SlotIsEmpty(row-1,col)) count++;
	if(col > 0            && SlotIsEmpty(row,col-1)) count++;
	if(row < board_size-1 && SlotIsEmpty(row+1,col)) count++;
	if(col < board_size-1 && SlotIsEmpty(row,col+1)) count++;
	return count;
}
u32 LibertyCount(Chain* chain){
	u32 count = 0;
	forE(chain->outer){ count += LibertyCount(*it / board_size, *it % board_size); }
	return count;
}

b32 IsValidPlacement(u32 row, u32 col){
	u32 color = (black_turn) ? Slot_White : Slot_Black; //NOTE non-active player's color
	u32 pos = ToLinear(row,col);
	
	//find how many captures would be made and whether stone will be surrounded if placed at slot
#define CheckNeighbor(row,col) { if(SlotIsColor(row,col,color)){ if(LibertyCount(&chains[Chained(row,col)]) == 1){ captures += ChainCount(row,col); } }else{ surrounded = false; } }
	u32 captures = 0;
	b32 surrounded = true;
	if(row > 0) CheckNeighbor(row-1,col);
	if(col > 0) CheckNeighbor(row,col-1);
	if(row < board_size-1) CheckNeighbor(row+1,col);
	if(col < board_size-1) CheckNeighbor(row,col+1);
#undef CheckNeighbor
	
	//ko rule is violated (and placement is invalid) if:
	//    placement will only capture one stone
	//    that stone was played on the previous player's move
	//    and that move only captured one stone
	if(captures == 1
	   && ((black_turn) ? whites_prev_capture   : blacks_prev_capture)   != -1
	   && ((black_turn) ? whites_prev_placement : blacks_prev_placement) != pos) return false;
	
	//if placement will captures something and we dont violate ko rule, placement is valid
	if(captures > 0) return true;
	
	//if placement wont capture anything and is suicidal by being surrounded by enemies, placement is invalid
	if(surrounded) return false;
	
	return true;
}

void PlacePiece(u32 row, u32 col){
	//update slot
	Slot(row,col) = (black_turn) ? Slot_Black : Slot_White;
	
	//remake chains
	clear(chained, 0xFF);
	chains.clear();
	forX(col, board_size){
		forX(row, board_size){
			chains.add(Chain());
			if(SlotIsEmpty(row,col) || !FillChain(chains.count-1, row, col, Slot(row,col))) chains.pop();
		}
	}
	
	//check for chain liberties on enemy pieces (NOTE enemy before friendly so capturing has precedence over suicide)
	u32 enemy = (black_turn) ? Slot_White : Slot_Black;
	u32 prev_capture_index = -1;
	u32 capture_count = 0;
	forI(chains.count){
		if(SlotLinear(chains[i].outer[0]) != enemy) continue;
		if(LibertyCount(&chains[i]) == 0){
			forE(chains[i].inner){ SlotLinear(*it) = Slot_Empty; prev_capture_index = (capture_count) ? -1 : *it; capture_count++; ChainedLinear(*it) = -1; }
			forE(chains[i].outer){ SlotLinear(*it) = Slot_Empty; prev_capture_index = (capture_count) ? -1 : *it; capture_count++; ChainedLinear(*it) = -1; }
			forE(chained){ if(*it != -1 && *it >= i){ *it -= 1; } }
			chains.remove(i--);
		}
	}
	
	//check for chain liberties on friendly pieces if suicide is legal
	u32 friendly = (black_turn) ? Slot_Black : Slot_White;
	u32 other_capture_count = 0;
	if(!suicide_is_illegal){
		forI(chains.count){
			if(SlotLinear(chains[i].outer[0]) != friendly) continue;
			if(LibertyCount(&chains[i]) == 0){
				forE(chains[i].inner){ SlotLinear(*it) = Slot_Empty; other_capture_count++; }
				forE(chains[i].outer){ SlotLinear(*it) = Slot_Empty; other_capture_count++; }
				forE(chained){ if(*it == i) *it = -1; }
				chains.remove(i--);
			}
		}
	}
	
	//update prev capture/placement and capture counts
	if(black_turn){
		blacks_prev_placement = ToLinear(row, col);
		blacks_prev_capture   = prev_capture_index;
		blacks_capture_count += capture_count;
		whites_capture_count += other_capture_count;
	}else{
		whites_prev_placement = ToLinear(row, col);
		whites_prev_capture   = prev_capture_index;
		whites_capture_count += capture_count;
		blacks_capture_count += other_capture_count;
	}
	
	//swap turns
	turn_passed = false;
	ToggleBool(black_turn);
}

void EndGame(){
	game_over = true;
	//TODO end game and point collection
}

void PassTurn(){
	if(turn_passed){
		EndGame();
	}else{
		turn_passed = true;
		ToggleBool(black_turn);
	}
}

void AddGoCommands();
int main(int argc, char* argv[]){ //NOTE argv includes the entire command line (including .exe)
	//// @init ////
	deshi_init();
	
	font = Storage::CreateFontFromFileTTF("STIXTwoMath-Regular.otf", 48).second;
	
	board_size = 9;
	f32 board_width = (DeshWindow->width < DeshWindow->height) ? DeshWindow->width/2 : DeshWindow->height/2;
	board_pos = (DeshWindow->dimensions-vec2{board_width,board_width})/2;
	board_dims.set(board_width, board_width);
	init(&slots, board_size*board_size, deshi_allocator);
	init(&chained, slots.count, deshi_allocator);
	AddGoCommands();
	
	DeshWindow->UpdateDecorations(Decoration_SystemDecorations);
	
	//// @update ////
	deshi_loop_start();
	//UI::ShowMetricsWindow();
	UI::Begin("go", vec2::ZERO, DeshWindow->dimensions, UIWindowFlags_NoInteract | UIWindowFlags_Invisible);
	
	f32 row_gap = board_dims.x / (board_size-1);
	f32 piece_radius = row_gap/2;
	vec2 border{row_gap, row_gap*3};
	b32 piece_placed = false;
	
	//draw background
	UI::RectFilled(board_pos-border, board_dims+(2*border), color(220,179,92));
	
	//draw grid and border text
	UI::PushVar(UIStyleVar_FontHeight, row_gap/2); UI::PushFont(font); UI::PushColor(UIStyleCol_Text, Color_Black);
	forI(board_size){
		f32 offset = i*row_gap;
		UI::Line(board_pos+vec2{offset,0}, board_pos+vec2{offset,board_dims.x}, 1, Color_Black);
		UI::Line(board_pos+vec2{0,offset}, board_pos+vec2{board_dims.x,offset}, 1, Color_Black);
		UI::Text(board_letters[i], board_pos+vec2{offset-row_gap/6,-row_gap});
		UI::Text(board_letters[i], board_pos+vec2{offset-row_gap/6,board_dims.x+row_gap/2});
		UI::Text(board_numbers_spacing[board_size-i-1], board_pos+vec2{-row_gap,offset-row_gap/4});
		UI::Text(board_numbers_spacing[board_size-i-1], board_pos+vec2{board_dims.x+row_gap/2,offset-row_gap/4});
	}
	UI::PopColor(); UI::PopFont(); UI::PopVar();
	
	//draw pieces, check for hovered pieces, and handle piece placement
	u32 hovered_row = -1, hovered_col = -1;
	forX(col, board_size){
		forX(row, board_size){
			vec2 offset{row*row_gap,col*row_gap};
			vec2 intersection = board_pos+offset;
			if      (SlotIsWhite(row,col)){
				UI::CircleFilled(intersection, piece_radius, 16, Color_White);
				if(DeshInput->mousePos.distanceTo(board_pos+offset) <= piece_radius){ hovered_row = row; hovered_col = col; }
			}else if(SlotIsBlack(row,col)){
				UI::CircleFilled(intersection, piece_radius, 16, Color_Black);
				if(DeshInput->mousePos.distanceTo(board_pos+offset) <= piece_radius){ hovered_row = row; hovered_col = col; }
			}else if(SlotIsEmpty(row,col) && !game_over){
				if(DeshInput->mousePos.distanceTo(board_pos+offset) <= piece_radius){
					UI::CircleFilled(intersection, piece_radius, 16, (black_turn) ? color(0,0,0,128) : color(255,255,255,128));
					hovered_row = row; hovered_col = col;
					if(IsValidPlacement(row,col)){
						if(DeshInput->KeyPressed(MouseButton::LEFT)){
							PlacePiece(row, col);
						}
					}else{
						f32 corner = piece_radius*0.70f;
						UI::Line(intersection+vec2{-corner,-corner}, intersection+vec2{ corner, corner}, 5, color(255,0,0,128));
						UI::Line(intersection+vec2{-corner, corner}, intersection+vec2{ corner,-corner}, 5, color(255,0,0,128));
					}
				}
			}
		}
	}
	
#if 1 //draw chains
	forI(chains.count){
		UI::PushColor(UIStyleCol_Text, (SlotLinear(chains[i].outer[0]) == Slot_White) ? Color_Black : Color_White);
		forE(chains[i].inner){ UI::Text(toStr(i).str, board_pos+vec2{(*it/board_size)*row_gap,(*it%board_size)*row_gap}); }
		forE(chains[i].outer){ UI::Text(toStr(i).str, board_pos+vec2{(*it/board_size)*row_gap,(*it%board_size)*row_gap}); }
		UI::PopColor();
	}
#endif
	
	//pass turn if button is pressed, end game if both players pass turn
	UI::PushVar(UIStyleVar_FontHeight, row_gap/2);
	if(UI::Button("Pass Turn", vec2::ZERO, UIButtonFlags_ReturnTrueOnRelease)) PassTurn();
	UI::PopVar();
	
	//reset board if f5 pressed
	if(DeshInput->KeyPressed(Key::F5)){
		Cmd::Run("go_reset");
	}
	
	UI::End();
	deshi_loop_end();
	deshi_cleanup();
	return 0;
}

//// @commands ////
void AddGoCommands(){
	const char* last_cmd_desc;
#define CMDSTART(name, desc) last_cmd_desc = desc; auto deshi__cmd__##name = [](array<cstring>& args) -> void
#define CMDEND(name, ...) ; Cmd::Add(deshi__cmd__##name, #name, last_cmd_desc, {__VA_ARGS__});
	
	CMDSTART(go_reset, "Resets the board to default state."){
		clear(slots);
		clear(chained, 0xFF);
		chains.clear();
		black_turn = true;
		turn_passed = false;
		whites_prev_placement = -1;
		blacks_prev_placement = -1;
		whites_prev_capture = -1;
		blacks_prev_capture = -1;
		whites_capture_count = 0;
		blacks_capture_count = 0;
	}CMDEND(go_reset);
	
#undef CMDSTART
#undef CMDEND
}