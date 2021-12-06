/*

TODO
----
placement is valid in suicide situation if it captures a threatening piece
capturing pieces


*/

//// includes ////
#include "deshi.h"
#include "core/logging.h"
#include "utils/array_sorting.h"

//// structurs ////
enum{
	Slot_Empty = 0,
	Slot_White = 1,
	Slot_Black = 2,
};

struct Board{
	vec2 pos;
	vec2 dims;
	u32  size;
	u32* slots; //row major matrix
	u32* copy;
};

//// variables ////
const char* board_letters[] = {
	"A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S",
};
const char* board_numbers[] = {
	"1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19",
};
const char* board_numbers_spacing[] = {
	" 1", " 2", " 3", " 4", " 5", " 6", " 7", " 8", " 9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19",
};

Board board;
Font* font;
b32 black_turn = true;

//// functions ////
#define Slot(row,col) board.slots[(board.size*(row))+(col)]
#define SlotLinear(pos) board.slots[pos]
#define SlotIsEmpty(row,col) Slot(row,col) == Slot_Empty
#define SlotIsWhite(row,col) Slot(row,col) == Slot_White
#define SlotIsBlack(row,col) Slot(row,col) == Slot_Black

void FillChain(array<u32>& chain, u32 row, u32 col){
	u32 start = (board.size*row)+col;
	u32 color = board.slots[start];
	if(color == Slot_Empty) return;
	chain.add(start);
	
	if(col > 0){            //UP
		u32 cell = (board.size*(row))+(col-1);
		if(board.slots[cell] == color){
			b32 already = false;
			forI(chain.count){ if(cell == chain[i]){ already = true; break; } }
			if(!already){ FillChain(chain, row, col-1); }
		}
	}
	if(col < board.size-1){ //DOWN
		u32 cell = (board.size*(row))+(col+1);
		if(board.slots[cell] == color){
			b32 already = false;
			forI(chain.count){ if(cell == chain[i]){ already = true; break; } }
			if(!already){ FillChain(chain, row, col+1); }
		}
	}
	if(row > 0){            //LEFT
		u32 cell = (board.size*(row-1))+(col);
		if(board.slots[cell] == color){
			b32 already = false;
			forI(chain.count){ if(cell == chain[i]){ already = true; break; } }
			if(!already){ FillChain(chain, row-1, col); }
		}
	}
	if(row < board.size-1){ //RIGHT
		u32 cell = (board.size*(row+1))+(col);
		if(board.slots[cell] == color){
			b32 already = false;
			forI(chain.count){ if(cell == chain[i]){ already = true; break; } }
			if(!already){ FillChain(chain, row+1, col); }
		}
	}
}

b32 HasLiberty(u32 pos){
	u32 row = pos / board.size;
	u32 col = pos % board.size;
	if(row > 0 && SlotIsEmpty(row-1,col)) return true;
	if(col > 0 && SlotIsEmpty(row,col-1)) return true;
	if(row < board.size-1 && SlotIsEmpty(row+1,col)) return true;
	if(col < board.size-1 && SlotIsEmpty(row,col+1)) return true;
	return false;
}

b32 HasLiberty(u32 row, u32 col){
	if(row > 0 && SlotIsEmpty(row-1,col)) return true;
	if(col > 0 && SlotIsEmpty(row,col-1)) return true;
	if(row < board.size-1 && SlotIsEmpty(row+1,col)) return true;
	if(col < board.size-1 && SlotIsEmpty(row,col+1)) return true;
	return false;
}

b32 ChainHasLiberty(array<u32>& chain){
	forI(chain.count){
		if(HasLiberty(chain[i])){ 
			return true;
		}
	}
	return false;
}

b32 IsValidPlacement(u32 row, u32 col){
	if      (row == 0){
		if      (col == 0){            //LEFT TOP
			if(black_turn){
				if(SlotIsWhite(row+1,col) && SlotIsWhite(row,col+1)) return false;
			}else{
				if(SlotIsBlack(row+1,col) && SlotIsBlack(row,col+1)) return false;
			}
		}else if(col == board.size-1){ //LEFT BOT
			if(black_turn){
				if(SlotIsWhite(row+1,col) && SlotIsWhite(row,col-1)) return false;
			}else{
				if(SlotIsBlack(row+1,col) && SlotIsBlack(row,col-1)) return false;
			}
		}else{                         //LEFT EDGE
			if(black_turn){
				if(SlotIsWhite(row+1,col) && SlotIsWhite(row,col+1) && SlotIsWhite(row,col-1)) return false;
			}else{
				if(SlotIsBlack(row+1,col) && SlotIsBlack(row,col+1) && SlotIsBlack(row,col-1)) return false;
			}
		}
	}else if(row == board.size-1){
		if      (col == 0){            //RIGHT TOP
			if(black_turn){
				if(SlotIsWhite(row-1,col) && SlotIsWhite(row,col+1)) return false;
			}else{
				if(SlotIsBlack(row-1,col) && SlotIsBlack(row,col+1)) return false;
			}
		}else if(col == board.size-1){ //RIGHT BOT
			if(black_turn){
				if(SlotIsWhite(row-1,col) && SlotIsWhite(row,col-1)) return false;
			}else{
				if(SlotIsBlack(row-1,col) && SlotIsBlack(row,col-1)) return false;
			}
		}else{                         //RIGHT EDGE
			if(black_turn){
				if(SlotIsWhite(row-1,col) && SlotIsWhite(row,col+1) && SlotIsWhite(row,col-1)) return false;
			}else{
				if(SlotIsBlack(row-1,col) && SlotIsBlack(row,col+1) && SlotIsBlack(row,col-1)) return false;
			}
		}
	}else{
		if      (col == 0){            //TOP EDGE
			if(black_turn){
				if(SlotIsWhite(row-1,col) && SlotIsWhite(row+1,col) && SlotIsWhite(row,col+1)) return false;
			}else{
				if(SlotIsBlack(row-1,col) && SlotIsBlack(row+1,col) && SlotIsBlack(row,col+1)) return false;
			}
		}else if(col == board.size-1){ //BOT EDGE
			if(black_turn){
				if(SlotIsWhite(row-1,col) && SlotIsWhite(row+1,col) && SlotIsWhite(row,col-1)) return false;
			}else{
				if(SlotIsBlack(row-1,col) && SlotIsBlack(row+1,col) && SlotIsBlack(row,col-1)) return false;
			}
		}else{
			if(black_turn){
				if(SlotIsWhite(row-1,col) && SlotIsWhite(row+1,col) && SlotIsWhite(row,col-1) && SlotIsWhite(row,col+1)) return false;
			}else{
				if(SlotIsBlack(row-1,col) && SlotIsBlack(row+1,col) && SlotIsBlack(row,col-1) && SlotIsBlack(row,col+1)) return false;
			}
		}
	}
	return true;
}

void SetupBoard(vec2 pos, f32 width, u32 size){
	board.pos = pos;
	board.dims.set(width, width);
	board.size = size;
	board.slots = (u32*)calloc(size*size, sizeof(u32));
}

void UpdateBoard(){
	f32 row_gap = board.dims.x / (board.size-1);
	f32 piece_radius = row_gap/2;
	vec2 border{row_gap, row_gap*3};
	
	UI::RectFilled(board.pos-border, board.dims+(2*border), color(220,179,92));
	
	//grid and border text
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
	
	//pieces
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
							ToggleBool(black_turn);
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
	
	//debug
#if 1 //draw hovered chain
	if(hovered_row != -1 && hovered_col != -1){
		array<u32> chain; 
		FillChain(chain, hovered_row, hovered_col);
		bubble_sort_low_to_high(chain);
		if(chain.count > 1){
			for(int i=1; i < chain.count; i++){
				vec2 prev = board.pos+vec2{(chain[i-1]/board.size)*row_gap,(chain[i-1]%board.size)*row_gap};
				vec2 curr = board.pos+vec2{(chain[i  ]/board.size)*row_gap,(chain[i  ]%board.size)*row_gap};
				UI::Line(prev, curr, 5, Color_Red);
			}
		}
	}
#endif
}

int main(){
	//init
	deshi::init();
	Render::UseDefaultViewProjMatrix();
	
	font = Storage::CreateFontFromFileTTF("STIXTwoMath-Regular.otf", 48).second;
	f32 board_width = (DeshWindow->width < DeshWindow->height) ? DeshWindow->width/2 : DeshWindow->height/2;
	SetupBoard((DeshWindow->dimensions-vec2{board_width,board_width})/2, board_width, 9);
	
	//update
	TIMER_START(t_f);
	while(!deshi::shouldClose()){
		DeshiImGui::NewFrame();
		DeshTime->Update();
		DeshWindow->Update();
		DeshInput->Update();
		Console2::Update();
		
		UI::Begin("go", vec2::ZERO, DeshWindow->dimensions, UIWindowFlags_NoInteract | UIWindowFlags_Invisible);
		UpdateBoard();
		UI::End();
		
		UI::Update();
		Render::Update();
		DeshTime->frameTime = TIMER_END(t_f); TIMER_RESET(t_f);
	}
	
	//cleanup
	deshi::cleanup();
}