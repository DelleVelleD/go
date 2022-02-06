//TODO ko rule when only one stone is captured (and optional superko)
//TODO passing turn
//TODO game end
//TODO komi = 6.5

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
	array<u32> outer = array<u32>(deshi_allocator); //indexes of slots with liberties (can't be empty)
	array<u32> inner = array<u32>(deshi_allocator); //indexes of slots with no liberties b/c of friendly neighbours (can be empty)
};

struct Board{
	vec2 pos; //draw position
	vec2 dims; //draw dimensions
	u32  size; //number of rows/cols
	carray<u32> slots; //row major matrix
};

//// @variables ////
const char* board_letters[] = { "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", };
const char* board_numbers[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", };
const char* board_numbers_spacing[] = { " 1", " 2", " 3", " 4", " 5", " 6", " 7", " 8", " 9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", };
Font* font;
b32 suicide_is_illegal = true;

Board board;
b32 black_turn = true;
b32 turn_passed = false;
array<Chain> chains(deshi_allocator);
array<u32>  chained(deshi_allocator);

//// @functions ////
#define ToLinear(row,col) ((board.size*(row))+(col))
#define SlotLinear(pos) board.slots[pos]

#define Slot(row,col) board.slots[ToLinear(row,col)]
#define SlotIsColor(row,col,color) (Slot(row,col) == color)
#define SlotIsEmpty(row,col) SlotIsColor(row,col,Slot_Empty)
#define SlotIsWhite(row,col) SlotIsColor(row,col,Slot_White)
#define SlotIsBlack(row,col) SlotIsColor(row,col,Slot_Black)

//returns false if already chained or a different color
b32 FillChain(Chain* chain, u32 row, u32 col, u32 color){
	u32 pos = ToLinear(row,col);
	
	//exit if not chainable
	if(SlotLinear(pos) != color) return false;
	forI(chained.count){ if(pos == chained[i]){ return false; } }
	
	//add to chained list
	chained.add(pos);
	
	//check neighbours
	u32 neighbour_count = 0;
	if(col > 0            && FillChain(chain,row,  col-1,color)){ neighbour_count++; } //LEFT
	if(row > 0            && FillChain(chain,row-1,col  ,color)){ neighbour_count++; } //UP
	if(col < board.size-1 && FillChain(chain,row,  col+1,color)){ neighbour_count++; } //RIGHT
	if(row < board.size-1 && FillChain(chain,row+1,col  ,color)){ neighbour_count++; } //DOWN
	
	//check neighbour count to see if inner or outer part of chain
	if(neighbour_count == 4){
		chain->inner.add(pos);
	}else{
		chain->outer.add(pos);
	}
	
	return true;
}

u32 LibertyCount(u32 row, u32 col){
	u32 count = 0;
	if(row > 0            && SlotIsEmpty(row-1,col)) count++;
	if(col > 0            && SlotIsEmpty(row,col-1)) count++;
	if(row < board.size-1 && SlotIsEmpty(row+1,col)) count++;
	if(col < board.size-1 && SlotIsEmpty(row,col+1)) count++;
	return count;
}
u32 LibertyCount(Chain* chain){
	u32 count = 0;
	forE(chain->outer){ count += LibertyCount(*it / board.size, *it % board.size); }
	return count;
}

b32 IsValidPlacement(u32 row, u32 col){
	//first, return true if placement will capture something
	//then,  return false check if placement is suicidal (if suicide is not allowed)
	u32 color = (black_turn) ? Slot_White : Slot_Black;
	if(row == 0){
		if(col == 0){                  //TOP LEFT
			if(   (SlotIsColor(row+1,col,color) && LibertyCount(row+1,col) == 1)
			   || (SlotIsColor(row,col+1,color) && LibertyCount(row,col+1) == 1)) return true;
			if(   suicide_is_illegal
			   && SlotIsColor(row+1,col,color)
			   && SlotIsColor(row,col+1,color)) return false;
		}else if(col == board.size-1){ //TOP RIGHT
			if(   (SlotIsColor(row+1,col,color) && LibertyCount(row+1,col) == 1)
			   || (SlotIsColor(row,col-1,color) && LibertyCount(row,col-1) == 1)) return true;
			if(   suicide_is_illegal
			   && SlotIsColor(row+1,col,color)
			   && SlotIsColor(row,col-1,color)) return false;
		}else{                         //TOP EDGE
			if(   (SlotIsColor(row+1,col,color) && LibertyCount(row+1,col) == 1)
			   || (SlotIsColor(row,col+1,color) && LibertyCount(row,col+1) == 1)
			   || (SlotIsColor(row,col+1,color) && LibertyCount(row,col+1) == 1)) return true;
			if(   suicide_is_illegal
			   && SlotIsColor(row+1,col,color)
			   && SlotIsColor(row,col+1,color)
			   && SlotIsColor(row,col-1,color)) return false;
		}
	}else if(row == board.size-1){
		if(col == 0){                  //BOT LEFT
			if(   (SlotIsColor(row-1,col,color) && LibertyCount(row-1,col) == 1)
			   || (SlotIsColor(row,col+1,color) && LibertyCount(row,col+1) == 1)) return true;
			if(   suicide_is_illegal
			   && SlotIsColor(row-1,col,color)
			   && SlotIsColor(row,col+1,color)) return false;
		}else if(col == board.size-1){ //BOT RIGHT
			if(   (SlotIsColor(row-1,col,color) && LibertyCount(row-1,col) == 1)
			   || (SlotIsColor(row,col-1,color) && LibertyCount(row,col-1) == 1)) return true;
			if(   suicide_is_illegal
			   && SlotIsColor(row-1,col,color)
			   && SlotIsColor(row,col-1,color)) return false;
		}else{                         //BOT EDGE
			if(   (SlotIsColor(row-1,col,color) && LibertyCount(row-1,col) == 1)
			   || (SlotIsColor(row,col+1,color) && LibertyCount(row,col+1) == 1)
			   || (SlotIsColor(row,col-1,color) && LibertyCount(row,col-1) == 1)) return true;
			if(   suicide_is_illegal
			   && SlotIsColor(row-1,col,color)
			   && SlotIsColor(row,col+1,color)
			   && SlotIsColor(row,col-1,color)) return false;
		}
	}else{
		if(col == 0){                  //LEFT EDGE
			if(   (SlotIsColor(row-1,col,color) && LibertyCount(row-1,col) == 1)
			   || (SlotIsColor(row+1,col,color) && LibertyCount(row+1,col) == 1)
			   || (SlotIsColor(row,col+1,color) && LibertyCount(row,col+1) == 1)) return true;
			if(   suicide_is_illegal
			   && SlotIsColor(row-1,col,color)
			   && SlotIsColor(row+1,col,color)
			   && SlotIsColor(row,col+1,color)) return false;
		}else if(col == board.size-1){ //RIGHT EDGE
			if(   (SlotIsColor(row-1,col,color) && LibertyCount(row-1,col) == 1)
			   || (SlotIsColor(row+1,col,color) && LibertyCount(row+1,col) == 1)
			   || (SlotIsColor(row,col-1,color) && LibertyCount(row,col-1) == 1)) return true;
			if(   suicide_is_illegal
			   && SlotIsColor(row-1,col,color)
			   && SlotIsColor(row+1,col,color)
			   && SlotIsColor(row,col-1,color)) return false;
		}else{                         //NON EDGE OR CORNER
			if(   (SlotIsColor(row-1,col,color) && LibertyCount(row-1,col) == 1)
			   || (SlotIsColor(row+1,col,color) && LibertyCount(row+1,col) == 1)
			   || (SlotIsColor(row,col-1,color) && LibertyCount(row,col-1) == 1)
			   || (SlotIsColor(row,col+1,color) && LibertyCount(row,col+1) == 1)) return true;
			if(   suicide_is_illegal
			   && SlotIsColor(row-1,col,color)
			   && SlotIsColor(row+1,col,color)
			   && SlotIsColor(row,col-1,color)
			   && SlotIsColor(row,col+1,color)) return false;
		}
	}
	return true;
}

int main(int argc, char* argv[]){ //NOTE argv includes the entire command line (including .exe)
	//// @init ////
	deshi_init();
	
	font = Storage::CreateFontFromFileTTF("STIXTwoMath-Regular.otf", 48).second;
	
	f32 board_width = (DeshWindow->width < DeshWindow->height) ? DeshWindow->width/2 : DeshWindow->height/2;
	board.pos = (DeshWindow->dimensions-vec2{board_width,board_width})/2;
	board.dims.set(board_width, board_width);
	board.size = 9;
	init(&board.slots, board.size*board.size, deshi_allocator);
	
	DeshWindow->UpdateDecorations(Decoration_SystemDecorations);
	
	//// @update ////
	deshi_loop_start();
	//UI::ShowMetricsWindow();
	UI::Begin("go", vec2::ZERO, DeshWindow->dimensions, UIWindowFlags_NoInteract | UIWindowFlags_Invisible);
	
	
	f32 row_gap = board.dims.x / (board.size-1);
	f32 piece_radius = row_gap/2;
	vec2 border{row_gap, row_gap*3};
	b32 piece_placed = false;
	
	//draw background
	UI::RectFilled(board.pos-border, board.dims+(2*border), color(220,179,92));
	
	//draw grid and border text
	UI::PushVar(UIStyleVar_FontHeight, row_gap/2); UI::PushFont(font); UI::PushColor(UIStyleCol_Text, Color_Black);
	forI(board.size){
		f32 offset = i*row_gap;
		UI::Line(board.pos+vec2{offset,0}, board.pos+vec2{offset,board.dims.x}, 1, Color_Black);
		UI::Line(board.pos+vec2{0,offset}, board.pos+vec2{board.dims.x,offset}, 1, Color_Black);
		UI::Text(board_letters[i], board.pos+vec2{offset-row_gap/6,-row_gap});
		UI::Text(board_letters[i], board.pos+vec2{offset-row_gap/6,board.dims.x+row_gap/2});
		UI::Text(board_numbers_spacing[board.size-i-1], board.pos+vec2{-row_gap,offset-row_gap/4});
		UI::Text(board_numbers_spacing[board.size-i-1], board.pos+vec2{board.dims.x+row_gap/2,offset-row_gap/4});
	}
	UI::PopColor(); UI::PopFont(); UI::PopVar();
	
	//draw pieces, check for hovered pieces, and handle piece placement
	u32 hovered_row = -1, hovered_col = -1;
	forX(col, board.size){
		forX(row, board.size){
			vec2 offset{row*row_gap,col*row_gap};
			vec2 intersection = board.pos+offset;
			if      (SlotIsWhite(row,col)){
				UI::CircleFilled(intersection, piece_radius, 16, Color_White);
				if(DeshInput->mousePos.distanceTo(board.pos+offset) <= piece_radius){ hovered_row = row; hovered_col = col; }
			}else if(SlotIsBlack(row,col)){
				UI::CircleFilled(intersection, piece_radius, 16, Color_Black);
				if(DeshInput->mousePos.distanceTo(board.pos+offset) <= piece_radius){ hovered_row = row; hovered_col = col; }
			}else if(SlotIsEmpty(row,col)){
				if(DeshInput->mousePos.distanceTo(board.pos+offset) <= piece_radius){
					UI::CircleFilled(intersection, piece_radius, 16, (black_turn) ? color(0,0,0,128) : color(255,255,255,128));
					hovered_row = row; hovered_col = col;
					if(IsValidPlacement(row,col)){
						if(DeshInput->KeyPressed(MouseButton::LEFT)){
							Slot(row,col) = (black_turn) ? Slot_Black : Slot_White;
							piece_placed = true;
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
	
	//if a piece is placed, check for captures
	//TODO extract piece placement from drawing into here
	if(piece_placed){
		//clear chains
		chains.clear();
		chained.clear();
		
		//remake chains
		forX(col, board.size){
			forX(row, board.size){
				Chain chain;
				if(!SlotIsEmpty(row,col) && FillChain(&chain, row, col, Slot(row,col))){
					chains.add(chain);
				}
			}
		}
		
		//check for chain liberties on enemy pieces (NOTE this is first so capturing enemies has precedence)
		u32 enemy = (black_turn) ? Slot_White : Slot_Black;
		forI(chains.count){
			if(SlotLinear(chains[i].outer[0]) != enemy) continue;
			if(LibertyCount(&chains[i]) == 0){
				forE(chains[i].inner){ SlotLinear(*it) = Slot_Empty; }
				forE(chains[i].outer){ SlotLinear(*it) = Slot_Empty; }
				chains.remove(i--);
			}
		}
		
		//check for chain liberties on friendly pieces
		u32 friendly = (black_turn) ? Slot_Black : Slot_White;
		forI(chains.count){
			if(SlotLinear(chains[i].outer[0]) != friendly) continue;
			if(LibertyCount(&chains[i]) == 0){
				forE(chains[i].inner){ SlotLinear(*it) = Slot_Empty; }
				forE(chains[i].outer){ SlotLinear(*it) = Slot_Empty; }
				chains.remove(i--);
			}
		}
		
		//swap turns
		ToggleBool(black_turn);
	}
	
#if 1 //draw chains
	forI(chains.count){
		UI::PushColor(UIStyleCol_Text, (SlotLinear(chains[i].outer[0]) == Slot_White) ? Color_Black : Color_White);
		forE(chains[i].inner){ UI::Text(toStr(i).str, board.pos+vec2{(*it/board.size)*row_gap,(*it%board.size)*row_gap}); }
		forE(chains[i].outer){ UI::Text(toStr(i).str, board.pos+vec2{(*it/board.size)*row_gap,(*it%board.size)*row_gap}); }
		UI::PopColor();
	}
#endif
	
	UI::End();
	deshi_loop_end();
	deshi_cleanup();
	return 0;
}