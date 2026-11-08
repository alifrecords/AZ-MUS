// =============================================================================
//  AZ BEAT — ritm kutubxonasi: Latino, Africano, Tresillo, Fors / Sharq, O'zbek,
//  6/8, 5/4, 7/8, 9/8, 12/8 va zamonaviy uslublar (1 takt; DRUM-SEQ'da tanlanadi)
//  Belgilar: X = urg'u (125), x = 105, o = 80, . = sharpa (45), 2/3 = ratchet, - = jim
//  Padlar: 0 Kick, 1 Snare, 2 Clap, 3 Closed Hat, 4 Open Hat, 5 Rim, 6 Tom Low, 7 Tom High,
//          8 808, 9 Shaker, 10 Crash, 11 Perc, 12 Snap, 13 Kick 2, 14 Snare 2, 15 Ride,
//          16 Conga, 17 Cowbell, 18 Tambourine, 19 FX
// =============================================================================
#pragma once

namespace az { namespace beat {

struct StyleRow { int pad; const char* pat; };
struct Style { const char* cat; const char* name; int meter; int bpm; int groove; float grooveAmt; StyleRow rows[8]; };

// meter indekslari: 0 4/4 · 1 3/4 · 2 6/8 · 3 5/4 · 4 7/8 · 5 9/8 · 6 12/8 · 7 7/4 · 8 4/4 1/8T · 9 4/4 1/16T
inline const Style styles[] = {
    // ---------------- Zamonaviy
    { "Zamonaviy", "House 124",            0, 124, 0, 0.0f, { { 0, "X---x---X---x---" }, { 2, "----X-------X---" }, { 3, "--x---x---x---x-" }, { 9, "o.o.o.o.o.o.o.o." }, { 4, "------o-------o-" } } },
    { "Zamonaviy", "Hip-Hop Boom Bap 90",  0,  90, 0, 0.35f, { { 0, "X------x-x------" }, { 1, "----X-------X---" }, { 3, "x-o-x-o-x-o-x-o-" }, { 4, "--------------o-" } } },
    { "Zamonaviy", "Trap 140",             0, 140, 0, 0.0f, { { 0, "X-------x-x-----" }, { 8, "X-------x-x-----" }, { 2, "--------X-------" }, { 3, "x-x-x-x-x3x-x-2x" }, { 4, "------o---------" } } },
    { "Zamonaviy", "Reggaeton (Dembow) 95", 0, 95, 3, 0.3f, { { 0, "X---x---X---x---" }, { 1, "---x--x----x--x-" }, { 3, "x-o-x-o-x-o-x-o-" }, { 18, "--o---o---o---o-" } } },
    { "Zamonaviy", "Amapiano 113",         0, 113, 0, 0.25f, { { 0, "X---x---X---x---" }, { 8, "x-----x---x---x-" }, { 5, "---x--x----x--x-" }, { 9, "o.o.o.o.o.o.o.o." }, { 12, "----x-------x---" } } },
    // ---------------- Latino
    { "Latino", "Son clave 3-2",           0, 100, 3, 0.4f, { { 5, "X--X--X---X-X---" }, { 0, "x-----x-x-----x-" }, { 16, "--o-..x---o-..x-" }, { 17, "x---x---x---x---" }, { 9, "o.o.o.o.o.o.o.o." } } },
    { "Latino", "Rumba clave 3-2",         0, 105, 3, 0.4f, { { 5, "X--X---X--X-X---" }, { 0, "x-----x-x-----x-" }, { 16, "----x-x-----x-x-" }, { 9, "o.o.o.o.o.o.o.o." } } },
    { "Latino", "Bossa nova",              0, 130, 3, 0.3f, { { 5, "x--x--x---x--x--" }, { 0, "x-----x-x-----x-" }, { 3, "x-x-x-x-x-x-x-x-" }, { 9, "o.o.o.o.o.o.o.o." } } },
    { "Latino", "Salsa (cascara)",         0, 180, 3, 0.3f, { { 15, "x-xx-x-xx-x-x-x-" }, { 5, "X--X--X---X-X---" }, { 16, "------x-----x-x-" }, { 0, "---x-------x----" } } },
    { "Latino", "Cumbia",                  0,  95, 3, 0.3f, { { 0, "X-------x-------" }, { 5, "----x-------x---" }, { 9, "x.x.x.x.x.x.x.x." }, { 16, "--x---x---x---x-" }, { 17, "x---x---x---x---" } } },
    { "Latino", "Tresillo (Habanera)",     0, 100, 3, 0.35f, { { 0, "X--x--x-X--x--x-" }, { 12, "----x-------x---" }, { 9, "o.o.o.o.o.o.o.o." }, { 16, "--o-----..o-----" } } },
    // ---------------- Africano
    { "Africano", "Afrobeat",              0, 110, 0, 0.3f, { { 0, "X-----x---x-----" }, { 1, "----X--o----X---" }, { 3, "x-xxx-xxx-xxx-xx" }, { 9, "o.o.o.o.o.o.o.o." }, { 16, "--x-.-x---x-.-x-" } } },
    { "Africano", "Afro 6/8 (bell)",       2, 110, 4, 0.5f, { { 17, "x-x-xx-x-x-x" }, { 0, "X-----x-----" }, { 16, "---o-x---o-x" }, { 9, "o.o.o.o.o.o." } } },
    { "Africano", "Bembe 12/8",            6, 120, 4, 0.5f, { { 17, "x---x---xx--x---x---x---" }, { 0, "X-----------x-----------" }, { 16, "------o-x---------o-x---" }, { 9, "o-o-o-o-o-o-o-o-o-o-o-o-" } } },
    // ---------------- Fors / Sharq
    { "Fors / Sharq", "Fors 6/8 (Reng)",   2, 130, 4, 0.25f, { { 0, "X-----x-----" }, { 16, "--.-x---x-x-" }, { 18, "x-o-o-x-o-o-" } } },
    { "Fors / Sharq", "Maqsum 4/4",        0, 100, 3, 0.2f, { { 0, "X-------X-------" }, { 16, "--x---x-----x---" }, { 18, "x-o-x-o-x-o-x-o-" } } },
    { "Fors / Sharq", "Saidi 4/4",         0, 100, 3, 0.2f, { { 0, "X-----x-X-------" }, { 16, "--x---------x---" }, { 18, "x-o-x-o-x-o-x-o-" } } },
    { "Fors / Sharq", "Karsilama 9/8",     5, 120, 0, 0.0f, { { 0, "X-------x---------" }, { 16, "----x-------x-x-x-" }, { 18, "x-o-o-o-x-o-o-o-o-" } } },
    // ---------------- O'zbek
    { "O'zbek", "Lazgi 6/8",               2, 150, 4, 0.3f, { { 0, "X-----x-----" }, { 16, "--x-x---x-x-" }, { 18, "x-x-x-x-x-x-" } } },
    { "O'zbek", "Doira 6/8 (usul)",        2, 120, 4, 0.25f, { { 0, "X-----X-----" }, { 16, "--o-x---o-x-" }, { 5, "----x-----x-" }, { 18, "x.o.x.x.o.x." } } },
    // ---------------- Notoq metrlar
    { "Notoq metr", "5/4 (3+2) jazz",      3, 170, 2, 0.4f, { { 15, "x--ox--ox--ox--ox--o" }, { 0, "X-----------x-------" }, { 1, "--------x-------x---" } } },
    { "Notoq metr", "7/8 Bolqon (2+2+3)",  4, 140, 0, 0.0f, { { 0, "X---x---x-----" }, { 1, "--o---o---x---" }, { 3, "x-x-x-x-x-x-x-" } } },
    { "Notoq metr", "7/4 groove",          7, 110, 0, 0.0f, { { 0, "X-------x-------x-------x---" }, { 1, "----x-------x-------x-----x-" }, { 3, "x-x-x-x-x-x-x-x-x-x-x-x-x-x-" } } },
    { "Notoq metr", "Shuffle / Blues (1/8T)", 8, 90, 0, 0.0f, { { 0, "X-----x-----" }, { 1, "---x-----x--" }, { 3, "x-xx-xx-xx-x" } } },
};
constexpr int numStyles = (int) std::size (styles);

inline int patChar (char c) { return c == 'X' ? 125 : c == 'x' ? 105 : c == 'o' ? 80 : c == '.' ? 45 : (c == '2' || c == '3') ? 95 : 0; }

}} // namespace az::beat
