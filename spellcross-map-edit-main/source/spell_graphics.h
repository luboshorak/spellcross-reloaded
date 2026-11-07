#pragma once

//#include "sprites.h"
#include <vector>
#include "cstdint"

#include <wx/rawbmp.h>

using namespace std;

// forward refs
class AnimPNM;


class SpellGraphicItem
{
public:
	int x_size;
	int y_size;
	int x_ofs;
	int y_ofs;
	char name[13];
	vector<uint8_t> pixels;
	uint8_t (*pal)[3];

	uint8_t *GetPixels(int y = 0, int x = 0);
	int Render(uint8_t *buf,uint8_t* buf_end,int buf_x_size,int x_pos,int y_pos,int in_black=false,int* y_buffer=NULL);
	wxBitmap* Render(int x_size=-1,int y_size=-1,bool transparent=false,bool invert=false);
	wxBitmap* Render(bool transparent,bool invert=false);
	wxCursor* RenderCUR(bool is_grayscale=true);
	int RenderMask(uint8_t* buf,uint8_t* buf_end);
};

class SpellProjectile
{
public:
	char name[13];
	SpellGraphicItem* glyphs[16];
	SpellProjectile(SpellGraphicItem &glyph);
	int Insert(SpellGraphicItem &glyph);
	int Check();
	SpellGraphicItem* GetGlyph(double angle);
};

// generic graphic resource
class SpellGraphics
{
private:
	vector<SpellGraphicItem> items;
	vector<AnimPNM*> pnms;
	vector<SpellProjectile> projectiles;
public:
	SpellGraphics();
	~SpellGraphics();
	int AddRaw(uint8_t *data, int dlen, int x_size, int y_size, const char *name,uint8_t pal[][3],int with_ext=0,int fix_black=false);
	int AddICO(uint8_t* data,int dlen,const char* name,uint8_t pal[][3]);
	int AddCUR(uint8_t* data,int dlen,const char* name,uint8_t pal[][3]);
	int AddLED(int color,const char* name,uint8_t pal[][3]);
	int Count();
	SpellGraphicItem *GetResource(int index);
	SpellGraphicItem *GetResource(const char *name);
	wxCursor* RenderCUR(const char* name);

	// PNM animations	
	int AddPNM(uint8_t* data,int dlen,const char* name);
	AnimPNM *GetPNM(const char *name);

	// direct links to common items
	SpellGraphicItem *wm_hud = nullptr;
	SpellGraphicItem *wm_hud_enemy = nullptr;
	SpellGraphicItem *wm_hud_sides = nullptr;
	SpellGraphicItem* wm_hud_overlay = nullptr;
	SpellGraphicItem* wm_form[3]{};
	SpellGraphicItem* red_led_off = nullptr;
	SpellGraphicItem* red_led_on = nullptr;
	SpellGraphicItem* yellow_led_on = nullptr;
	SpellGraphicItem* wm_exp_mark = nullptr;
	SpellGraphicItem* wm_freeze = nullptr;
	SpellGraphicItem* wm_paralyze = nullptr;
	SpellGraphicItem* wm_btn_idle = nullptr;
	SpellGraphicItem* wm_btn_hover = nullptr;
	SpellGraphicItem* wm_btn_press = nullptr;
	SpellGraphicItem* wm_glyph_air = nullptr;
	SpellGraphicItem* wm_glyph_center_unit = nullptr;
	SpellGraphicItem* wm_glyph_down = nullptr;
	SpellGraphicItem* wm_glyph_up = nullptr;
	SpellGraphicItem* wm_glyph_radar_down = nullptr;
	SpellGraphicItem* wm_glyph_radar_up = nullptr;
	SpellGraphicItem* wm_glyph_end_turn = nullptr;
	SpellGraphicItem* wm_glyph_goto_unit = nullptr;
	SpellGraphicItem* wm_glyph_ground = nullptr;
	SpellGraphicItem* wm_glyph_map = nullptr;
	SpellGraphicItem* wm_glyph_heal = nullptr;
	SpellGraphicItem* wm_glyph_unit_info = nullptr;
	SpellGraphicItem* wm_glyph_next = nullptr;
	SpellGraphicItem* wm_glyph_options = nullptr;
	SpellGraphicItem* wm_glyph_next_unused = nullptr;
	SpellGraphicItem* wm_glyph_retreat = nullptr;
	SpellGraphicItem* wm_glyph_end_placement = nullptr;
	SpellGraphicItem* wm_glyph_info = nullptr;
	SpellGraphicItem* wm_glyph_place_unit = nullptr;

	SpellGraphicItem* wm_sel_tab = nullptr;
	SpellGraphicItem* wm_sel_attack = nullptr;
	SpellGraphicItem* wm_sel_move = nullptr;
	SpellGraphicItem* wm_sel_upper = nullptr;
	SpellGraphicItem* wm_sel_lower = nullptr;
	SpellGraphicItem* wm_sel_select = nullptr;

	SpellGraphicItem* wm_frame_horz = nullptr;
	SpellGraphicItem* wm_frame_vert = nullptr;
	SpellGraphicItem* wm_frame_corner = nullptr;

	SpellGraphicItem* wm_map_opt_frame = nullptr;
	SpellGraphicItem* wm_map_opt_btn_disabled = nullptr;
	SpellGraphicItem* wm_map_opt_btn_idle = nullptr;
	SpellGraphicItem* wm_map_opt_btn_hover = nullptr;
	SpellGraphicItem* wm_map_opt_btn_down = nullptr;	

	SpellGraphicItem* wm_map_units_list = nullptr;

	wxCursor* cur_pointer = nullptr;
	wxCursor* cur_wait = nullptr;
	wxCursor* cur_move = nullptr;
	wxCursor* cur_select = nullptr;
	wxCursor* cur_question = nullptr;
	wxCursor* cur_attack_down = nullptr;
	wxCursor* cur_attack_up = nullptr;
	wxCursor* cur_attack_up_down = nullptr;

	AnimPNM* pnm_sipka = nullptr;

	// projectile lists
	int SortProjectiles();
	SpellProjectile *GetProjectile(char *name);
};
