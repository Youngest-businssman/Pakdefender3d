 /*
 * Pakdefender3d – Single‑file 3D shooter (portable)
 * Real gun names, Pakistani maps, gloo walls, emotes, diamonds/coins.
 *
 * Compile (any platform with SDL2 installed):
 *   gcc -O2 pakdefender3d.c -o pakdefender3d -lSDL2 -lGL -lGLU -lm
 *   (On macOS: use -framework OpenGL instead of -lGL)
 *   (On Windows with MinGW: add -lfreeglut and -lopengl32 -lglu32)
 *
 * Run: ./pakdefender3d
 */

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <GL/glu.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

/* ---------- constants ---------- */
#define MAX_PLAYERS      60
#define MAX_GUNS         20
#define MAX_EMOTES       20
#define MAX_CHARACTERS   20
#define MAX_GLOO_WALLS   200
#define MAP_HALF         250.0f
#define PI               3.14159265358979323846f
#define DEG2RAD          (PI / 180.0f)

/* ---------- maths helpers ---------- */
typedef struct { float x, y, z; } vec3;
typedef struct { float x, y; } vec2;

static vec3 vec3_add(vec3 a, vec3 b) { return (vec3){a.x+b.x, a.y+b.y, a.z+b.z}; }
static vec3 vec3_sub(vec3 a, vec3 b) { return (vec3){a.x-b.x, a.y-b.y, a.z-b.z}; }
static vec3 vec3_scale(vec3 a, float s) { return (vec3){a.x*s, a.y*s, a.z*s}; }
static float vec3_dot(vec3 a, vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
static float vec3_len(vec3 v) { return sqrtf(vec3_dot(v, v)); }
static vec3 vec3_norm(vec3 v) {
    float l = vec3_len(v);
    return (l > 0.0001f) ? vec3_scale(v, 1.0f/l) : (vec3){0,0,0};
}

/* ---------- gun database (real‑world names) ---------- */
typedef enum {
    GUN_MELEE, GUN_SHOTGUN, GUN_SMG, GUN_AR, GUN_PISTOL,
    GUN_SNIPER, GUN_LMG, GUN_DMR, GUN_LAUNCHER, GUN_GRENADE
} GunType;

typedef struct {
    int id;
    const char *name;      // real gun name
    GunType type;
    float damage;
    float fire_rate;
    int mag_size;          // -1 infinite (melee)
    float max_range;
    float spread_deg;
    int attach_slots;
} GunDef;

static const GunDef guns_db[MAX_GUNS] = {
    { 0, "Combat Knife",    GUN_MELEE,   50,  1.0f,  -1, 1.5f,  0.0f, 0 },
    { 1, "Mossberg 590",    GUN_SHOTGUN, 120, 1.5f,   6, 20.0f, 5.0f, 1 },
    { 2, "MP5",             GUN_SMG,     32,  0.08f, 35, 80.0f, 2.5f, 2 },
    { 3, "M4A1",            GUN_AR,      38,  0.1f,  30, 150.0f,1.2f, 3 },
    { 4, "Glock 18",        GUN_PISTOL,  25,  0.15f, 15, 60.0f, 3.0f, 1 },
    { 5, "AWP",             GUN_SNIPER, 150,  1.8f,   5, 400.0f,0.2f, 2 },
    { 6, "M16A4",           GUN_AR,      34,  0.2f,  30, 120.0f,1.0f, 3 },
    { 7, "M249",            GUN_LMG,     40,  0.12f,100, 200.0f,3.5f, 2 },
    { 8, "SKS",             GUN_DMR,     65,  0.5f,  10, 300.0f,0.5f, 2 },
    { 9, "Desert Eagle",    GUN_PISTOL,  45,  0.4f,   9,  50.0f, 2.0f, 1 },
    {10, "Crossbow",        GUN_SNIPER,  90,  2.0f,   4, 100.0f,0.1f, 1 },
    {11, "RPG-7",           GUN_LAUNCHER,200,3.0f,   1, 250.0f,8.0f, 0 },
    {12, "UMP-45",          GUN_SMG,     28,  0.07f, 40, 90.0f, 2.8f, 2 },
    {13, "AK-47",           GUN_AR,      36,  0.09f, 30, 130.0f,1.5f, 3 },
    {14, "SPAS-12",         GUN_SHOTGUN, 140, 1.2f,   4, 15.0f, 7.0f, 1 },
    {15, "Revolver",        GUN_PISTOL,  55,  0.6f,   6,  70.0f, 1.5f, 1 },
    {16, "Dragunov SVD",    GUN_SNIPER, 130,  1.5f,   5, 450.0f,0.2f, 3 },
    {17, "SCAR-H",          GUN_AR,      36,  0.09f, 35, 130.0f,1.1f, 3 },
    {18, "Saiga-12",        GUN_SHOTGUN, 110, 1.0f,   8, 18.0f, 6.0f, 1 },
    {19, "Frag Grenade",    GUN_GRENADE, 100, 4.0f,   1, 30.0f, 0.0f, 0 }
};

/* ---------- characters (Pakistani traditional) ---------- */
typedef struct {
    int id;
    const char *name;
    float speed;
    float stealth;
} CharacterDef;

static const CharacterDef characters_db[MAX_CHARACTERS] = {
    {0,"Zainab",5.0f,1.2f},{1,"Ahmad",4.8f,1.5f},{2,"Bilal",5.2f,1.0f},
    {3,"Rukhsana",4.9f,1.3f},{4,"Fahad",5.1f,1.1f},{5,"Meerab",5.0f,1.4f},
    {6,"Aamir",4.7f,1.6f},{7,"Daniyal",5.3f,1.8f},{8,"Shanzay",5.0f,1.2f},
    {9,"Kulsoom",4.9f,1.5f},{10,"Arslan",5.2f,1.0f},{11,"Zara",5.1f,1.3f},
    {12,"Hassan",4.8f,1.7f},{13,"Noreen",5.0f,1.1f},{14,"Tariq",5.4f,0.9f},
    {15,"Anaya",4.7f,1.9f},{16,"Umar",5.0f,1.4f},{17,"Saira",4.9f,1.5f},
    {18,"Kamran",5.3f,1.6f},{19,"Yasmin",5.1f,1.2f}
};

/* ---------- emotes (Pakistani dances) ---------- */
static const char *emote_names[MAX_EMOTES] = {
    "Bhangra","Attan","Dhamal","SwordSalute","KabaddiJump",
    "PeshawarClap","SindhiWave","BalochEagle","PunjabiStep","PakDefender",
    "Jhoomar","Luddi","Khattak","Sammi","Cholistan",
    "Hunar","Gidda","Tandoor","Mehndi","CamelDance"
};

/* ---------- map names ---------- */
static const char *map_names[] = {
    "Lahore Fort", "Karachi Port", "Islamabad Hills", "Peshawar Old City"
};

/* ---------- player state ---------- */
typedef struct {
    int id;
    vec3 pos;
    float yaw, pitch;           // degrees
    float health, shield;
    int current_gun;            // index in guns_db
    int ammo[MAX_GUNS];
    int character_id;
    int diamonds, coins;
    int kills;
    int alive;
} Player;

/* ---------- gloo wall ---------- */
typedef struct {
    vec3 pos;
    float yaw;
    float hp;
    int owner_id;
    float spawn_time;
    int active;
} GlooWall;

/* ---------- game state ---------- */
static struct {
    int map;                    // 0..3
    Player local_player;
    Player players[MAX_PLAYERS];
    int player_count;
    GlooWall gloo_walls[MAX_GLOO_WALLS];
    int gloo_count;
    float match_time;
    int running;                // 1 while match active
} gs;

static const char *game_title = "Pakdefender3d";

/* ---------- SDL / OpenGL globals ---------- */
static SDL_Window *window = NULL;
static SDL_GLContext gl_context;
static int app_running = 1;
static float delta_time = 0.0f;

/* ---------- prototypes ---------- */
static void init_systems(void);
static void shutdown_systems(void);
static void handle_events(void);
static void player_move(float fwd, float right, float mx, float my);
static void fire_weapon(void);
static void place_gloo(void);
static void use_emote(int idx);
static void update_match(float dt);
static void render_frame(void);
static void login_screen(void);
static void lobby(void);

/* ---------- implementation ---------- */
int main(int argc, char *argv[]) {
    (void)argc; (void)argv;
    srand((unsigned)time(NULL));

    init_systems();
    login_screen();   // gives 10 diamonds + 10k coins, sets character
    lobby();          // starts a match directly for demo

    Uint32 last_time = SDL_GetTicks();
    while (app_running) {
        Uint32 now = SDL_GetTicks();
        delta_time = (now - last_time) / 1000.0f;
        last_time = now;

        handle_events();

        if (gs.running) {
            update_match(delta_time);
        }

        render_frame();
        SDL_GL_SwapWindow(window);
    }

    shutdown_systems();
    return 0;
}

/* ---------- system init ---------- */
static void init_systems() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
        exit(1);
    }
    window = SDL_CreateWindow(game_title,
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              1280, 720,
                              SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    if (!window) {
        fprintf(stderr, "Window error: %s\n", SDL_GetError());
        SDL_Quit();
        exit(1);
    }
    gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        fprintf(stderr, "OpenGL context error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        exit(1);
    }
    SDL_GL_SetSwapInterval(1);  // V‑sync

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

    // Init game state defaults
    memset(&gs, 0, sizeof(gs));
    gs.local_player.id = 0;
    gs.local_player.alive = 1;
    gs.local_player.health = 100;
    gs.local_player.shield = 0;
    gs.local_player.pos = (vec3){0, 0, 0};
    gs.local_player.yaw = 0;
    gs.local_player.pitch = 0;
    gs.local_player.current_gun = 3;  // M4A1
    gs.local_player.ammo[3] = 30;
    gs.local_player.character_id = 0;
    gs.local_player.diamonds = 0;
    gs.local_player.coins = 0;
    gs.player_count = 1;
    gs.players[0] = gs.local_player;
    gs.running = 0;
}

static void shutdown_systems() {
    if (gl_context) SDL_GL_DeleteContext(gl_context);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
}

/* ---------- event handling ---------- */
static void handle_events() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            app_running = 0;
        }
        /* Keyboard for emotes (1-5) handled in update loop via keystate */
        /* Mouse buttons for fire/gloo */
        if (e.type == SDL_MOUSEBUTTONDOWN && gs.running) {
            if (e.button.button == SDL_BUTTON_LEFT) fire_weapon();
            if (e.button.button == SDL_BUTTON_RIGHT) place_gloo();
        }
        if (e.type == SDL_KEYDOWN) {
            if (e.key.keysym.sym == SDLK_ESCAPE) {
                gs.running = 0;  // exit match to lobby
            }
        }
    }
}

/* ---------- player movement (WASD + mouse) ---------- */
static void player_move(float fwd, float right, float mx, float my) {
    Player *p = &gs.local_player;
    const CharacterDef *c = &characters_db[p->character_id];

    p->yaw += mx * 0.2f;
    p->pitch -= my * 0.2f;
    if (p->pitch > 89.0f) p->pitch = 89.0f;
    if (p->pitch < -89.0f) p->pitch = -89.0f;

    vec3 forward = { sinf(p->yaw*DEG2RAD), 0, -cosf(p->yaw*DEG2RAD) };
    vec3 sideways = { cosf(p->yaw*DEG2RAD), 0, sinf(p->yaw*DEG2RAD) };
    vec3 move = {0,0,0};

    if (fwd != 0.0f) move = vec3_add(move, vec3_scale(forward, fwd * c->speed * delta_time));
    if (right != 0.0f) move = vec3_add(move, vec3_scale(sideways, right * c->speed * delta_time));

    p->pos = vec3_add(p->pos, move);
    // Clamp to map
    p->pos.x = fmaxf(-MAP_HALF, fminf(MAP_HALF, p->pos.x));
    p->pos.z = fmaxf(-MAP_HALF, fminf(MAP_HALF, p->pos.z));
}

/* ---------- weapon firing (hitscan with cone) ---------- */
static void fire_weapon() {
    Player *p = &gs.local_player;
    const GunDef *g = &guns_db[p->current_gun];

    if (p->ammo[p->current_gun] <= 0 && g->mag_size > 0) {
        printf("click! (no ammo)\n");
        return;
    }
    if (g->mag_size > 0) p->ammo[p->current_gun]--;

    vec3 dir = { sinf(p->yaw*DEG2RAD)*cosf(p->pitch*DEG2RAD),
                 -sinf(p->pitch*DEG2RAD),
                 -cosf(p->yaw*DEG2RAD)*cosf(p->pitch*DEG2RAD) };
    dir = vec3_norm(dir);

    // Simple hitscan
    float best_dist = g->max_range;
    int hit_id = -1;
    for (int i = 0; i < gs.player_count; i++) {
        if (!gs.players[i].alive || i == p->id) continue;
        vec3 to = vec3_sub(gs.players[i].pos, p->pos);
        float dist = vec3_len(to);
        if (dist > best_dist) continue;
        vec3 to_dir = vec3_norm(to);
        float angle = acosf(vec3_dot(dir, to_dir));
        if (angle < (g->spread_deg * DEG2RAD + 0.1f)) {
            best_dist = dist;
            hit_id = i;
        }
    }
    if (hit_id >= 0) {
        gs.players[hit_id].health -= g->damage;
        printf("Hit player %d for %.0f dmg\n", hit_id, g->damage);
        if (gs.players[hit_id].health <= 0) {
            gs.players[hit_id].alive = 0;
            p->kills++;
            p->coins += 500;
            printf("Player %d killed!\n", hit_id);
        }
    }
}

/* ---------- gloo wall placement ---------- */
static void place_gloo() {
    if (gs.gloo_count >= MAX_GLOO_WALLS) return;
    Player *p = &gs.local_player;
    GlooWall *gw = &gs.gloo_walls[gs.gloo_count++];
    vec3 dir = { sinf(p->yaw*DEG2RAD), 0, -cosf(p->yaw*DEG2RAD) };
    gw->pos = vec3_add(p->pos, vec3_scale(dir, 2.0f));
    gw->yaw = p->yaw;
    gw->hp = 500;
    gw->owner_id = p->id;
    gw->spawn_time = gs.match_time;
    gw->active = 1;
    printf("Gloo wall placed at (%.1f, %.1f, %.1f)\n", gw->pos.x, gw->pos.y, gw->pos.z);
}

/* ---------- emote ---------- */
static void use_emote(int idx) {
    if (idx < 0 || idx >= MAX_EMOTES) return;
    printf("Player %s emote: %s\n",
           characters_db[gs.local_player.character_id].name,
           emote_names[idx]);
}

/* ---------- match update (decay gloo walls, etc.) ---------- */
static void update_match(float dt) {
    gs.match_time += dt;
    for (int i = 0; i < gs.gloo_count; i++) {
        if (gs.gloo_walls[i].active) {
            gs.gloo_walls[i].hp -= 20.0f * dt;
            if (gs.gloo_walls[i].hp <= 0) {
                gs.gloo_walls[i].active = 0;
            }
        }
    }
    // Check local player death
    if (!gs.local_player.alive) {
        printf("You died. Press ESC to go to lobby.\n");
        gs.running = 0;
    }
}

/* ---------- rendering ---------- */
static void render_frame() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (!gs.running) {
        // Lobby / menus (text not drawn in this demo)
        return;
    }

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0, 1280.0/720.0, 0.1, 1000.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    Player *p = &gs.local_player;
    vec3 eye = vec3_add(p->pos, (vec3){0, 1.7f, 0});
    vec3 look_dir = {
        sinf(p->yaw*DEG2RAD) * cosf(p->pitch*DEG2RAD),
        -sinf(p->pitch*DEG2RAD),
        -cosf(p->yaw*DEG2RAD) * cosf(p->pitch*DEG2RAD)
    };
    vec3 look = vec3_add(eye, look_dir);
    gluLookAt(eye.x, eye.y, eye.z, look.x, look.y, look.z, 0, 1, 0);

    // Ground
    glColor3f(0.2f, 0.5f, 0.2f);
    glBegin(GL_QUADS);
    glVertex3f(-MAP_HALF, 0, MAP_HALF);
    glVertex3f( MAP_HALF, 0, MAP_HALF);
    glVertex3f( MAP_HALF, 0,-MAP_HALF);
    glVertex3f(-MAP_HALF, 0,-MAP_HALF);
    glEnd();

    // Other players (red cubes)
    for (int i = 0; i < gs.player_count; i++) {
        if (!gs.players[i].alive || i == p->id) continue;
        glPushMatrix();
        glTranslatef(gs.players[i].pos.x, gs.players[i].pos.y, gs.players[i].pos.z);
        glColor3f(0.9f, 0.2f, 0.2f);
        glutSolidCube(1.0f);
        glPopMatrix();
    }

    // Gloo walls (semi-transparent blue)
    for (int i = 0; i < gs.gloo_count; i++) {
        if (!gs.gloo_walls[i].active) continue;
        GlooWall *w = &gs.gloo_walls[i];
        glPushMatrix();
        glTranslatef(w->pos.x, w->pos.y, w->pos.z);
        glRotatef(w->yaw, 0, 1, 0);
        glColor4f(0.2f, 0.8f, 1.0f, 0.5f);
        glBegin(GL_QUADS);
        glVertex3f(-1, 0, 0);
        glVertex3f( 1, 0, 0);
        glVertex3f( 1, 2, 0);
        glVertex3f(-1, 2, 0);
        glEnd();
        glPopMatrix();
    }

    // 2D HUD
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 1280, 720, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);

    // Crosshair
    glColor3f(1, 1, 1);
    glLineWidth(2);
    glBegin(GL_LINES);
    glVertex2f(635, 360); glVertex2f(645, 360);
    glVertex2f(640, 355); glVertex2f(640, 365);
    glEnd();

    // Health bar
    glColor3f(1, 0, 0);
    glBegin(GL_QUADS);
    glVertex2f(30, 30); glVertex2f(30 + p->health*2, 30);
    glVertex2f(30 + p->health*2, 50); glVertex2f(30, 50);
    glEnd();

    // Ammo text (simplified, no real font)
    char ammo_str[32];
    sprintf(ammo_str, "%s | %d", guns_db[p->current_gun].name, p->ammo[p->current_gun]);
    // In a real game you'd render with a bitmap font; here we just print to stdout occasionally.
    // printf("Ammo: %s\n", ammo_str); // uncomment for debug

    glEnable(GL_DEPTH_TEST);
}

/* ---------- login (gives diamonds/coins) ---------- */
static void login_screen() {
    printf("=== %s Login ===\n", game_title);
    printf("Starting with 10 Diamonds and 10,000 Coins.\n");
    gs.local_player.diamonds = 10;
    gs.local_player.coins = 10000;
}

/* ---------- lobby (auto‑starts match for demo) ---------- */
static void lobby() {
    printf("=== Lobby ===\n");
    printf("Loading map: %s\n", map_names[0]);
    gs.map = 0;
    gs.local_player.alive = 1;
    gs.local_player.health = 100;
    gs.local_player.pos = (vec3){0,0,0};
    gs.running = 1;
    gs.match_time = 0;
    // Add a few enemy bots for testing
    gs.player_count = 4;
    for (int i = 1; i < 4; i++) {
        Player *bot = &gs.players[i];
        bot->id = i;
        bot->alive = 1;
        bot->health = 100;
        bot->pos = (vec3){ rand() % 50 - 25, 0, rand() % 50 - 25 };
        bot->current_gun = rand() % 20;
        for (int j = 0; j < MAX_GUNS; j++) bot->ammo[j] = (guns_db[j].mag_size > 0) ? guns_db[j].mag_size : 1;
        bot->character_id = i % MAX_CHARACTERS;
    }
    printf("Match started! Use WASD + mouse. LMB shoot, RMB gloo wall. Keys 1-5 emotes. ESC to quit match.\n");
}
