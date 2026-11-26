#pragma once
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

// Experimental EN DOS model. Evidence and known limitations:
// docs/original-projectile-geometry.md. No dependency on units.fsa.
namespace original_projectile {
struct Point { int x; int y; };
inline bool enabled() {
    static const bool result = [] {
        const char* mode = std::getenv("SPELLCROSS_PROJECTILE_MODEL");
        return !mode || std::strcmp(mode, "fsa") != 0;
    }();
    return result;
}
inline int air_height() { return enabled() ? 120 : 150; }
inline constexpr int heights[13] = {48,66,66,66,48,49,30,30,30,31,48,49,66};
// SPELCROS.EXE EN file offset 0x1894b8; exact signed LE int32 pairs.
inline constexpr Point offsets[13][16] = {
    {{0,-31},{13,-31},{26,-26},{34,-20},{36,-11},{33,-4},{25,2},{13,7},{0,9},{-13,7},{-25,2},{-33,-4},{-36,-11},{-34,-20},{-26,-26},{-13,-31}},
    {{0,-36},{14,-34},{25,-28},{33,-18},{36,-7},{34,5},{27,14},{14,20},{0,21},{-14,20},{-27,14},{-34,5},{-36,-7},{-33,-18},{-25,-28},{-14,-34}},
    {{0,-37},{14,-34},{27,-28},{34,-18},{36,-7},{34,5},{26,15},{14,20},{0,22},{-14,20},{-26,15},{-34,5},{-36,-7},{-34,-18},{-27,-28},{-14,-34}},
    {{6,-35},{20,-31},{31,-22},{38,-11},{39,0},{38,10},{29,18},{18,21},{4,23},{-10,17},{-21,8},{-27,-3},{-30,-13},{-27,-24},{-19,-31},{-7,-36}},
    {{7,-40},{20,-34},{32,-26},{39,-16},{39,-8},{38,0},{32,5},{21,7},{7,3},{-6,-1},{-17,-10},{-26,-19},{-27,-28},{-24,-36},{-17,-41},{-6,-42}},
    {{7,-23},{20,-17},{33,-9},{39,1},{39,9},{37,16},{32,22},{20,23},{7,21},{-6,16},{-17,7},{-25,-2},{-27,-12},{-25,-20},{-17,-24},{-6,-25}},
    {{4,-26},{18,-23},{30,-16},{37,-10},{39,-5},{38,0},{31,3},{21,3},{7,0},{-7,-3},{-19,-9},{-28,-15},{-30,-20},{-27,-25},{-21,-28},{-10,-27}},
    {{0,-24},{15,-23},{28,-21},{36,-18},{38,-14},{36,-10},{27,-7},{14,-5},{0,-5},{-13,-6},{-26,-7},{-35,-10},{-37,-14},{-34,-17},{-27,-21},{-15,-24}},
    {{1,-24},{15,-23},{27,-21},{36,-18},{39,-14},{36,-10},{27,-7},{15,-5},{0,-5},{-13,-5},{-26,-7},{-35,-10},{-38,-14},{-35,-18},{-27,-11},{-14,-23}},
    {{-3,-26},{10,-27},{23,-28},{30,-25},{33,-20},{30,-14},{20,-8},{9,-2},{-6,2},{-20,4},{-31,4},{-38,1},{-38,-4},{-37,-9},{-30,-15},{-16,-21}},
    {{-6,-44},{9,-44},{20,-40},{29,-32},{32,-22},{30,-10},{22,0},{11,9},{-3,14},{-17,14},{-29,10},{-37,2},{-38,-9},{-38,-20},{-31,-31},{-19,-39}},
    {{-6,-22},{7,-25},{18,-24},{26,-19},{29,-12},{27,-3},{18,7},{7,15},{-6,21},{-19,-23},{-30,22},{-37,17},{-38,9},{-38,1},{-31,-8},{-19,-16}},
    {{-6,-39},{8,-39},{20,-35},{29,-27},{32,-17},{30,-5},{22,5},{11,14},{-3,18},{-16,19},{-28,15},{-36,7},{-37,-4},{-37,-14},{-31,-25},{-19,-34}}
};
inline constexpr int slope_index(int slope) { return slope >= 0 && slope < 13 ? slope : 0; }
inline constexpr int wrap(int dir) { return ((dir % 16) + 16) % 16; }
inline constexpr int center_y(int slope) { return heights[slope_index(slope)] / 2; }
inline constexpr Point offset(int slope, bool air, int dir) {
    return offsets[air ? 0 : slope_index(slope)][wrap(dir)];
}
inline constexpr int tile_x(int x, int y) { return 80*x + (y%2 == 0 ? 40 : 0); }
inline constexpr int tile_y(int y, int elevation, int slope) {
    return 24*y - 18*elevation + center_y(slope);
}
inline constexpr Point tile_center(int x,int y,int elevation,int slope,int sprite_y,bool air) {
    return {tile_x(x,y)+40,tile_y(y,elevation,slope)+sprite_y-(air?120:0)};
}
inline constexpr Point difference(Point target, Point source) { return {target.x-source.x,target.y-source.y}; }
inline int bucket(float r) {
    return r < -0.6681 ? 0 : r < -0.1989 ? 1 : r < 0.1989 ? 2 : r < 0.6681 ? 3 : 4;
}
// EN 0x7712b: float ratio, double comparison constants. Y points down here.
inline int direction(Point delta, int fallback = 0) {
    if (!delta.x && !delta.y) return wrap(fallback);
    const float up=static_cast<float>(-static_cast<double>(delta.y)*1.66666666);
    const float x=static_cast<float>(delta.x);
    constexpr int east[]={6,5,4,3,2}, west[]={14,13,12,11,10};
    constexpr int north[]={14,15,0,1,2}, south[]={6,7,8,9,10};
    if (std::abs(up)<=std::abs(x)) return (x>0?east:west)[bucket(up/x)];
    return (up>0?north:south)[bucket(x/up)];
}
inline constexpr double angle(int dir) { return 90.0-22.5*wrap(dir); }
inline int from_angle(double value) { return wrap(static_cast<int>(std::lround((90.0-value)/22.5))); }
// DOS 0x77b5b: on a half-turn tie, follow the sign of target-current.
inline constexpr int turn_delta(int target,int current,int count) {
    if(count<=0) return 0;
    int d=(target%count)-(current%count);
    if(d>count/2) d-=count;
    if(d<-count/2) d+=count;
    return d;
}
inline constexpr int graphic_direction(int dir,bool mlrs) { return wrap(dir+(mlrs?4:0)); }
inline constexpr Point local_origin(int slope,bool air,int dir) {
    const auto p=offset(slope,air,dir);
    return {40+p.x,center_y(slope)+p.y};
}
struct Trajectory {
    Point launch, target, center_delta;
    bool horizontal;
    int dominant, remaining;
};
// The original slope and dominant axis come from the center delta BEFORE
// the muzzle offset is added. Current map centers replace DOS viewport clipping.
inline Trajectory trajectory(Point launch,Point target,Point center_delta) {
    const bool horizontal=std::abs(center_delta.x)>=std::abs(center_delta.y);
    const int dominant=horizontal?center_delta.x:center_delta.y;
    const int remaining=horizontal?target.x-launch.x:target.y-launch.y;
    return {launch,target,center_delta,horizontal,dominant,remaining};
}
inline int flight_ticks(const Trajectory& t) {
    if (!t.dominant || (t.dominant>0)!=(t.remaining>0)) return 1;
    return std::max(1,(std::abs(t.remaining)+9)/10-1);
}
inline Point position(const Trajectory& t,int tick) {
    if (!t.dominant || (t.dominant>0)!=(t.remaining>0)) return t.launch;
    const int steps=std::clamp(tick,0,flight_ticks(t)-1);
    const int d=(t.dominant>0?10:-10)*steps;
    if(t.horizontal) return {t.launch.x+d,t.launch.y+static_cast<int>(static_cast<float>(d)*(static_cast<float>(t.center_delta.y)/t.dominant))};
    return {t.launch.x+static_cast<int>(static_cast<float>(d)*(static_cast<float>(t.center_delta.x)/t.dominant)),t.launch.y+d};
}
} // namespace original_projectile
