//TODO japanese scoring
//TODO superko / pos super ko
//TODO group suicide
#include "go.h"

//// @variables ////
const char* board_letters[] = { "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", };
const char* board_numbers[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", };
const char* board_numbers_spacing[] = { " 1", " 2", " 3", " 4", " 5", " 6", " 7", " 8", " 9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", };
cstring winner_black = cstr_lit("Winner: Black");
cstring winner_white = cstr_lit("Winner: White");

Font* font;
Font* large_font;
vec2  board_pos;
vec2  board_dims;
u32   board_size;
u32   board_slots;
Rules rules;

b32 black_turn;
b32 turn_passed;
b32 game_over;
u32 whites_prev_placement;
u32 blacks_prev_placement;
u32 whites_prev_capture; //value is -1 if more than one was captured
u32 blacks_prev_capture;

Score score;
carray<u32>  slots; //row major matrix
carray<u32>  chained; //chain indexes of slots
array<Chain> chains(deshi_allocator);

//// @functions ////
#define ToLinear(row,col) ((board_size*(row))+(col))
#define LinearRow(pos) ((pos)/board_size)
#define LinearCol(pos) ((pos)%board_size)

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

//// @utils ////
//returns false if already chained or a different color
b32 fill_chain(u32 chain_idx, u32 row, u32 col, u32 color){
	u32 pos = ToLinear(row,col);
	
	//exit if not chainable
	if(SlotLinear(pos) != color) return false;
	if(ChainedLinear(pos) != -1) return true; //already added to chain, but same color
	
	//set to this chain
	if(chains.count == chain_idx) chains.add(Chain());
	ChainedLinear(pos) = chain_idx;
	
	//check neighbours
	u32 neighbour_count = 0;
	u32 max_neighbour_count = 0;
	if(col > 0           ){ max_neighbour_count++; if(fill_chain(chain_idx,row,  col-1,color)){ neighbour_count++; } } //LEFT
	if(row > 0           ){ max_neighbour_count++; if(fill_chain(chain_idx,row-1,col  ,color)){ neighbour_count++; } } //UP
	if(col < board_size-1){ max_neighbour_count++; if(fill_chain(chain_idx,row,  col+1,color)){ neighbour_count++; } } //RIGHT
	if(row < board_size-1){ max_neighbour_count++; if(fill_chain(chain_idx,row+1,col  ,color)){ neighbour_count++; } } //DOWN
	
	//check neighbour count to see if inner or outer part of chain
	if(neighbour_count == max_neighbour_count){
		chains[chain_idx].inner.add(pos);
	}else{
		chains[chain_idx].outer.add(pos);
	}
	
	return true;
}

u32 liberty_count(u32 row, u32 col){
	u32 count = 0;
	if(row > 0            && SlotIsEmpty(row-1,col)) count++;
	if(col > 0            && SlotIsEmpty(row,col-1)) count++;
	if(row < board_size-1 && SlotIsEmpty(row+1,col)) count++;
	if(col < board_size-1 && SlotIsEmpty(row,col+1)) count++;
	return count;
}
u32 liberty_count(Chain* chain){
	u32 count = 0;
	forE(chain->outer){ count += liberty_count(LinearRow(*it), LinearCol(*it)); }
	return count;
}

//// @interface ////
Rules        get_ruleset(){ return rules; }
array<u32>   get_board()  { return array<u32>(slots, deshi_allocator); }
array<Chain> get_chains() { return array<Chain>(chains, deshi_allocator); }
Score        get_score()  { return score; }

b32 in_seki(Chain* chain){
	if(SlotLinear(chain->outer[0]) == Slot_Empty) return false;
	
	NotImplemented; //TODO implement seki
	return true;
}

b32 valid_placement(u32 row, u32 col){
	u32 color = (black_turn) ? Slot_White : Slot_Black; //NOTE non-active player's color
	u32 pos = ToLinear(row,col);
	
	//find how many captures would be made and whether stone will be surrounded if placed at slot
#define CheckNeighbour(row,col) { if(SlotIsColor(row,col,color)){ if(liberty_count(&chains[Chained(row,col)]) == 1){ captures += ChainCount(row,col); } }else{ surrounded = false; } }
	u32 captures = 0;
	b32 surrounded = true;
	if(row > 0) CheckNeighbour(row-1,col);
	if(col > 0) CheckNeighbour(row,col-1);
	if(row < board_size-1) CheckNeighbour(row+1,col);
	if(col < board_size-1) CheckNeighbour(row,col+1);
#undef CheckNeighbour
	
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

b32 place_piece(u32 row, u32 col, b32 bypass_valid_check){
	if(!bypass_valid_check && !valid_placement(row,col)) return false;
	
	//update slot
	Slot(row,col) = (black_turn) ? Slot_Black : Slot_White;
	
	//remake chains
	clear(chained, 0xFF);
	chains.clear();
	forX(row, board_size){
		forX(col, board_size){
			fill_chain(chains.count, row, col, Slot(row,col));
		}
	}
	
	//check for chain liberties on enemy pieces (NOTE enemy before friendly so capturing has precedence over suicide)
	u32 enemy = (black_turn) ? Slot_White : Slot_Black;
	u32 prev_capture_index = -1;
	u32 capture_count = 0;
	forI(chains.count){
		if(SlotLinear(chains[i].outer[0]) != enemy) continue;
		if(liberty_count(&chains[i]) == 0){
			forE(chains[i].inner){ SlotLinear(*it) = Slot_Empty; prev_capture_index = (capture_count) ? -1 : *it; capture_count++; ChainedLinear(*it) = -1; }
			forE(chains[i].outer){ SlotLinear(*it) = Slot_Empty; prev_capture_index = (capture_count) ? -1 : *it; capture_count++; ChainedLinear(*it) = -1; }
			forE(chained){ if(*it != -1 && *it >= i){ *it -= 1; } }
			chains.remove(i--);
		}
	}
	
	//check for chain liberties on friendly pieces if suicide is legal
	u32 friendly = (black_turn) ? Slot_Black : Slot_White;
	u32 other_capture_count = 0;
	if(rules.legal_suicide){
		forI(chains.count){
			if(SlotLinear(chains[i].outer[0]) != friendly) continue;
			if(liberty_count(&chains[i]) == 0){
				forE(chains[i].inner){ SlotLinear(*it) = Slot_Empty; other_capture_count++; }
				forE(chains[i].outer){ SlotLinear(*it) = Slot_Empty; other_capture_count++; }
				forE(chained){ if(*it == i) *it = -1; }
				chains.remove(i--);
			}
		}
	}
	
	//update prev capture/placement and scores
	if(black_turn){
		blacks_prev_placement = ToLinear(row, col);
		blacks_prev_capture   = prev_capture_index;
		score.black_captures += capture_count;
		score.white_captures += other_capture_count;
		score.black_stones   += 1;
		score.white_stones   -= capture_count;
		score.black_stones   -= other_capture_count;
	}else{
		whites_prev_placement = ToLinear(row, col);
		whites_prev_capture   = prev_capture_index;
		score.white_captures += capture_count;
		score.black_captures += other_capture_count;
		score.white_stones   += 1;
		score.black_stones   -= capture_count;
		score.white_stones   -= other_capture_count;
	}
	score.black_score = score.black_stones;
	score.white_score = rules.komi + score.white_stones;
	score.empty_stones = board_slots - score.black_stones - score.white_stones;
	
	//factor territories into scoring
	if(rules.chinese_scoring){
		forE(chains){
			if(SlotLinear(it->outer[0]) != Slot_Empty) continue; //skip non-empty chains
			
			//find first color
			u32 color = Slot_Empty;
			u32 row = LinearRow(it->outer[0]), col = LinearCol(it->outer[0]);
			if     (row > 0            && !SlotIsEmpty(row-1,col)){ color = Slot(row-1,col); }
			else if(col > 0            && !SlotIsEmpty(row,col-1)){ color = Slot(row,col-1); }
			else if(row < board_size-1 && !SlotIsEmpty(row+1,col)){ color = Slot(row+1,col); }
			else if(col < board_size-1 && !SlotIsEmpty(row,col+1)){ color = Slot(row,col+1); }
			else{ Assert(false, "An outer slot on an empty chain should always have a colored neighbour"); }
			
			//check if all chain neighbours are that color
			b32 same_color = true;
			forI(it->outer.count){
				row = LinearRow(it->outer[i]); col = LinearCol(it->outer[i]);
				if(row > 0            && !SlotIsEmpty(row-1,col) && !SlotIsColor(row-1,col,color)){ same_color = false; break; }
				if(col > 0            && !SlotIsEmpty(row,col-1) && !SlotIsColor(row,col-1,color)){ same_color = false; break; }
				if(row < board_size-1 && !SlotIsEmpty(row+1,col) && !SlotIsColor(row+1,col,color)){ same_color = false; break; }
				if(col < board_size-1 && !SlotIsEmpty(row,col+1) && !SlotIsColor(row,col+1,color)){ same_color = false; break; }
			}
			
			//score the territory if not contested
			if(same_color){
				if(color == Slot_Black){
					score.black_score += it->inner.count + it->outer.count;
				}else{
					score.white_score += it->inner.count + it->outer.count;
				}
			}
		}
	}else if(rules.japanese_scoring){
		NotImplemented; //TODO implement japanese scoring
		
		forE(chains){
			if(in_seki(it)){
				
			}else{
				
			}
		}
	}else{
		NotImplemented;
	}
	
	//swap turns
	turn_passed = false;
	ToggleBool(black_turn);
	
	return true;
}
b32 place_piece(u32 row, u32 col){ return place_piece(row,col,false); }

void pass_turn(){
	if(turn_passed){
		game_over = true;
	}else{
		turn_passed = true;
		ToggleBool(black_turn);
	}
}

//// @commands ////
void add_go_commands(){
	const char* last_cmd_desc;
#define CMDSTART(name, desc) last_cmd_desc = desc; auto deshi__cmd__##name = [](array<cstring>& args) -> void
#define CMDEND(name, ...) ; Cmd::Add(deshi__cmd__##name, #name, last_cmd_desc, {__VA_ARGS__});
	
	CMDSTART(go_reset, "Resets the board to default state."){
		clear(slots);
		clear(chained, 0xFF);
		chains.clear();
		black_turn  = true;
		turn_passed = false;
		game_over   = false;
		whites_prev_placement = -1;
		blacks_prev_placement = -1;
		whites_prev_capture   = -1;
		blacks_prev_capture   = -1;
		score.black_stones   = 0;
		score.white_stones   = 0;
		score.empty_stones   = board_slots;
		score.black_captures = 0;
		score.white_captures = 0;
		score.black_score    = 0;
		score.white_score    = rules.komi;
	}CMDEND(go_reset);
	
#undef CMDSTART
#undef CMDEND
}

int main(int argc, char* argv[]){ //NOTE argv includes the entire command line (including .exe)
	//// @init ////
	deshi_init();
	DeshWindow->UpdateDecorations(Decoration_SystemDecorations);
	
	board_size = 9;
	board_slots = board_size*board_size;
	f32 board_width = (DeshWindow->width < DeshWindow->height) ? DeshWindow->width/2 : DeshWindow->height/2;
	board_pos = (DeshWindow->dimensions-vec2{board_width,board_width})/2;
	board_dims.set(board_width, board_width);
	init(&slots,   board_slots, deshi_allocator);
	init(&chained, board_slots, deshi_allocator);
	
	font = Storage::CreateFontFromFileTTF("Montserrat-Black.otf", 72).second;
	add_go_commands();
	Cmd::Run("go_reset");
	
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
	f32 font_height = row_gap/2.f;
	UI::PushVar(UIStyleVar_FontHeight, font_height); UI::PushFont(font); UI::PushColor(UIStyleCol_Text, Color_Black);
	forI(board_size){
		f32 offset = i*row_gap;
		UI::Line(board_pos+vec2{offset,0}, board_pos+vec2{offset,board_dims.x}, 1, Color_Black);
		UI::Line(board_pos+vec2{0,offset}, board_pos+vec2{board_dims.x,offset}, 1, Color_Black);
		UI::Text(board_letters[i], board_pos+vec2{offset-row_gap/6,-row_gap});
		UI::Text(board_letters[i], board_pos+vec2{offset-row_gap/6,board_dims.x+font_height});
		UI::Text(board_numbers_spacing[board_size-i-1], board_pos+vec2{-row_gap,offset-font_height/2});
		UI::Text(board_numbers_spacing[board_size-i-1], board_pos+vec2{board_dims.x+font_height,offset-font_height/2});
	}
	
	//draw scores and controls
	UI::PushColor(UIStyleCol_WindowBg, Color_DarkCyan); UI::PushColor(UIStyleCol_Separator, Color_Cyan);
	UI::BeginChild("go_scores", board_pos+vec2{board_dims.x+border.x,-border.y}, vec2{0,0}, UIWindowFlags_FitAllElements);{
		vec2 black_start = UI::GetPositionForNextItem();
		f32 max_width = 0;
		UI::Text(toStr("Stones: ", score.black_stones).str, UITextFlags_NoWrap);
		max_width = Max(max_width, UI::GetLastItemSize().x);
		UI::Text(toStr("Captures: ", score.black_captures).str, UITextFlags_NoWrap);
		max_width = Max(max_width, UI::GetLastItemSize().x);
		UI::Text(stringf("Score: %.1f", score.black_score).str, UITextFlags_NoWrap);
		max_width = Max(max_width, UI::GetLastItemSize().x);
		vec2 black_end = vec2{max_width, UI::GetLastItemPos().y + UI::GetLastItemSize().y};
		
		UI::Separator(10);
		
		vec2 white_start = UI::GetPositionForNextItem();
		max_width = 0;
		UI::PushColor(UIStyleCol_Text, Color_White);
		UI::Text(toStr("Stones: ", score.white_stones).str, UITextFlags_NoWrap);
		max_width = Max(max_width, UI::GetLastItemSize().x);
		UI::Text(toStr("Captures: ", score.white_captures).str, UITextFlags_NoWrap);
		max_width = Max(max_width, UI::GetLastItemSize().x);
		UI::Text(stringf("Score: %.1f", score.white_score).str, UITextFlags_NoWrap);
		max_width = Max(max_width, UI::GetLastItemSize().x);
		UI::PopColor();
		vec2 white_end = vec2{max_width, UI::GetLastItemPos().y + UI::GetLastItemSize().y};
		
		vec2 four{4.f,4.f};
		if(black_turn){
			UI::Rect(black_start-four, black_end-black_start+four+vec2{5.f*four.x,0}, Color_Grey);
		}else{
			UI::Rect(white_start-four, white_end-white_start+four+vec2{5.f*four.x,0}, Color_Grey);
		}
		
		UI::Separator(10);
		
		//pass turn if button is pressed, end game if both players pass turn
		UI::PushColor(UIStyleCol_Text, Color_Grey);
		if(UI::Button("Pass Turn", UIButtonFlags_ReturnTrueOnRelease)) pass_turn();
		UI::PopColor();
	}UI::EndChild();
	UI::PopColor(2);
	
	UI::PopFont(); UI::PopVar(); UI::PopColor();
	
	//draw pieces, check for hovered pieces, and handle piece placement
	u32 hovered_row = -1, hovered_col = -1;
	forX(row, board_size){
		forX(col, board_size){
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
					if(valid_placement(row,col)){
						if(DeshInput->KeyPressed(MouseButton::LEFT)){
							place_piece(row, col, true); //NOTE bypass placement validity b/c we do it above
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
	u32 chain_cntr = 0;
	forI(chains.count){
		if(SlotLinear(chains[i].outer[0]) == Slot_Empty) continue;
		UI::PushColor(UIStyleCol_Text, (SlotLinear(chains[i].outer[0]) == Slot_White) ? Color_Black : Color_White);
		forE(chains[i].inner){ UI::Text(toStr(chain_cntr).str, board_pos+vec2{(LinearRow(*it))*row_gap,(LinearCol(*it))*row_gap}); }
		forE(chains[i].outer){ UI::Text(toStr(chain_cntr).str, board_pos+vec2{(LinearRow(*it))*row_gap,(LinearCol(*it))*row_gap}); }
		UI::PopColor();
		chain_cntr++;
	}
#endif
	
	//game over
	if(game_over){ 
		UI::PushFont(font);
		cstring winner = (score.white_score > score.black_score) ? winner_white : winner_black;
		UI::PushVar(UIStyleVar_FontHeight, DeshWindow->height/7.9f); UI::PushColor(UIStyleCol_Text, Color_DarkGrey);
		UI::Text(winner.str, {DeshWindow->centerX - (UI::CalcTextSize(winner).x/2.f), DeshWindow->height/4.f});
		UI::PushVar(UIStyleVar_FontHeight, DeshWindow->height/8.0f); UI::PushColor(UIStyleCol_Text, Color_Grey);
		UI::Text(winner.str, {DeshWindow->centerX - (UI::CalcTextSize(winner).x/2.f), DeshWindow->height/4.f});
		UI::PopVar(2); UI::PopColor(2);
		UI::PopFont();
	}
	
	//reset board if f5 pressed
	if(DeshInput->KeyPressed(Key::F5)){
		Cmd::Run("go_reset");
	}
	
	UI::End();
	deshi_loop_end();
	deshi_cleanup();
	return 0;
}