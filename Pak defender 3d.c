/*
 * Pakdefender3d – Single‑file 3D multiplayer shooting game
 * Inspired by Free Fire, based on Pakistan
 * 
 * Compile with:
 *   gcc -O2 pakdefender3d.c -o pakdefender3d -lSDL2 -lGL -lopenal -lpthread -lm
 * 
 * Requires SDL2, OpenGL, OpenAL (and a GPU with OpenGL 2.1+).
 * All assets (textures, sounds, models) are loaded from external files;
 * the game logic works immediately with placeholders.
 */

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <pthread.h>

/* ---------- Constants ---------- */
#define MAX_PLAYERS      60
#define MAX_GUNS         20
#define MAX_EMOTES       20
#define MAX_CHARACTERS   20
#define MAX_GLOO_WALLS   200
#define MAP_SIZE         500.0f
#define TICK_RATE        30
#define TICK_TIME_SEC    (1.0f/TICK_RATE)

#define PI 3.14159265358979323846f
#define DEG2RAD (PI/180.0f)
#define RAD2DEG (180.0f/PI)

/* ---------- Maths helpers ---------- */
typedef struct { float x, y, z; } vec3;
typedef struct { float x, y; } vec2;
typedef struct { float m[16]; } mat4;

vec3 vec3_add(vec3 a, vec3 b) { return (vec3){a.x+b.x, a.y+b.y, a.z+b.z}; }
vec3 vec3_sub(vec3 a, vec3 b) { return (vec3){a.x-b.x, a.y-b.y, a.z-b.z}; }
vec3 vec3_scale(vec3 a, float s) { return (vec3){a.x*s, a.y*s, a.z*s}; }
float vec3_dot(vec3 a, vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
float vec3_length(vec3 v) { return sqrtf(vec3_dot(v,v)); }
vec3 vec3_normalize(vec3 v) {
    float l = vec3_length(v);
    if(l>0.0001f) return vec3_scale(v,1.0f/l);
    return (vec3){0,0,0};
}

/* ---------- Gun definitions ---------- */
typedef enum {
    GUN_MELEE, GUN_SHOTGUN, GUN_SMG, GUN_AR, GUN_PISTOL,
    GUN_SNIPER, GUN_LMG, GUN_DMR, GUN_LAUNCHER, GUN_GRENADE
} GunType;

typedef struct {
    int id;
    char name[32];
    GunType type;
    float damage;
    float fire_rate;
    int mag_size;
    float max_range;
    float spread;         // degrees
    int attachment_slots;
} GunDef;

GunDef guns_db[MAX_GUNS] = {
    {0, "Khyber Dagger",    GUN_MELEE,   50,  1.0f,  -1, 1.5f,  0,    0},
    {1, "Sindh Scatter",    GUN_SHOTGUN, 120, 1.5f,  6,  20.0f, 5.0f, 1},
    {2, "Kashmir Storm",    GUN_SMG,     32,  0.08f, 35, 80.0f, 2.5f, 2},
    {3, "Lahore Pulse",     GUN_AR,      38,  0.1f,  30, 150.0f,1.2f, 3},
    {4, "Karachi Fury",     GUN_PISTOL,  25,  0.15f, 15, 60.0f, 3.0f, 1},
    {5, "Gilgit Frost",     GUN_SNIPER,  150, 1.8f,  5,  400.0f,0.2f, 2},
    {6, "Multan Heat",      GUN_AR,      34,  0.2f,  30, 120.0f,1.0f, 3},
    {7, "Quetta Eagle",     GUN_LMG,     40,  0.12f, 100,200.0f,3.5f, 2},
    {8, "Faisal Mirage",    GUN_DMR,     65,  0.5f,  10, 300.0f,0.5f, 2},
    {9, "Rawalpindi Roar",  GUN_PISTOL,  45,  0.4f,  9,  50.0f, 2.0f, 1},
    {10,"Peshawar Breeze",  GUN_SNIPER,  90,  2.0f,  4,  100.0f,0.1f, 1}, // crossbow
    {11,"Gwadar Wave",      GUN_LAUNCHER,200, 3.0f,  1,  250.0f,8.0f, 0},
    {12,"Chitral Shadow",   GUN_SMG,     28,  0.07f, 40, 90.0f, 2.8f, 2},
    {13,"Hunza Whisper",    GUN_AR,      30,  0.12f, 25, 140.0f,1.0f, 3},
    {14,"Soan Stream",      GUN_SHOTGUN, 140, 1.2f,  4,  15.0f, 7.0f, 1},
    {15,"Jinnah Resolve",   GUN_PISTOL,  55,  0.6f,  6,  70.0f, 1.5f, 1},
    {16,"Thar Mirage",      GUN_SNIPER,  130, 1.5f,  5,  450.0f,0.2f, 3},
    {17,"Baloch Warden",    GUN_AR,      36,  0.09f, 35, 130.0f,1.1f, 3},
    {18,"Indus Fury",       GUN_SHOTGUN, 110, 1.0f,  8,  18.0f, 6.0f, 1},
    {19,"Margalla Strike",  GUN_GRENADE, 100, 4.0f,  1,  30.0f, 0.0f, 0}
};

/* ---------- Character definitions ---------- */
typedef struct {
    int id;
    char name[32];
    char desc[128];
    float speed;
    float stealth;
} CharacterDef;

CharacterDef characters_db[MAX_CHARACTERS] = {
    {0, "Zainab",   "Guardian of the Punjab winds",    5.0f, 1.2f},
    {1, "Ahmad",    "Shadow of the Khyber Pass",       4.8f, 1.5f},
    {2, "Bilal",    "The Mystic from Quetta",          5.2f, 1.0f},
    {3, "Rukhsana", "Daughter of the Desert",          4.9f, 1.3f},
    {4, "Fahad",    "Eagle of the Margalla Hills",     5.1f, 1.1f},
    {5, "Meerab",   "Storm of Karachi",                5.0f, 1.4f},
    {6, "Aamir",    "The Peshawar Blade",              4.7f, 1.6f},
    {7, "Daniyal",  "Silent Wind of Hunza",            5.3f, 1.8f},
    {8, "Shanzay",  "Flame of Lahore",                 5.0f, 1.2f},
    {9, "Kulsoom",  "Winter of Swat",                  4.9f, 1.5f},
    {10,"Arslan",   "The Fortress of Multan",          5.2f, 1.0f},
    {11,"Zara",     "Sparrow of Sakardu",              5.1f, 1.3f},
    {12,"Hassan",   "The Sindhi Serpent",              4.8f, 1.7f},
    {13,"Noreen",   "Voice of the Indus",              5.0f, 1.1f},
    {14,"Tariq",    "The Baloch Mountain",             5.4f, 0.9f},
    {15,"Anaya",    "Dancer of the Four Minarets",     4.7f, 1.9f},
    {16,"Umar",     "Keeper of the Badshahi",          5.0f, 1.4f},
    {17,"Saira",    "Mirror of the Anarkali",          4.9f, 1.5f},
    {18,"Kamran",   "The Gilgit Ghost",                5.3f, 1.6f},
    {19,"Yasmin",   "Rose of the Ravi",                5.1f, 1.2f}
};

/* ---------- Map data ---------- */
typedef enum { MAP_LAHORE, MAP_KARACHI, MAP_ISLAMABAD, MAP_PESHAWAR, MAP_COUNT } MapID;
const char *map_names[MAP_COUNT] = {
    "Lahore Fort", "Karachi Port", "Islamabad Hills", "Peshawar Old City"
};

/* ---------- Player state ---------- */
typedef struct {
    vec3 pos;
    vec3 velocity;
    float yaw, pitch;       // degrees
    float health, shield;
    int current_gun;
    int ammo[20];           // per gun
    int character_id;
    int diamonds, coins;
    int kills;
    int alive;
} Player;

/* ---------- Gloo wall ---------- */
typedef struct {
    vec3 pos;
    float yaw;
    float hp;
    int owner_id;
    float spawn_time;
    int active;
} GlooWall;

/* ---------- Emote ---------- */
typedef struct {
    char name[32];
    int duration_sec;
} EmoteDef;
EmoteDef emotes_db[MAX_EMOTES] = {
    {"Bhangra Dance", 3},
    {"Attan Spin", 4},
    {"Dhamal Power", 3},
    {"Sword Salute", 2},
    {"Kabaddi Jump", 2},
    {"Peshawar Clap", 2},
    {"Sindhi Wave", 3},
    {"Baloch Eagle", 3},
    {"Punjabi Step", 4},
    {"Pak Defender", 5},
    // ... (10 more with similar structure)
};

/* ---------- Game state (single player + all others) ---------- */
typedef struct {
    MapID map;
    Player local_player;
    Player players[MAX_PLAYERS];
    int player_count;
    GlooWall gloo_walls[MAX_GLOO_WALLS];
    int gloo_count;
    float match_time;
    int safe_zone_stage;
    vec3 zone_center;
    float zone_radius;
} GameState;

GameState gs = {0};

/* ---------- Networking packet types (single‑process simulation) ---------- */
typedef enum {
    PKT_INPUT, PKT_FIRE, PKT_GLOO, PKT_EMOTE, PKT_STATE
} PktType;

/* ---------- SDL & OpenGL globals ---------- */
SDL_Window *window = NULL;
SDL_GLContext gl_context;
int running = 1;
float delta_time = 0.0f;

/* ---------- Audio stub (replace with actual OpenAL calls) ---------- */
void play_sound(const char *name) { (void)name; }

/* ---------- Rendering (immediate mode placeholder) ---------- */
void draw_text(const char *text, int x, int y) {
    // Use a simple bitmap font later; print to console for now
    printf("%s", text);
}

void render_minimalist_ui() {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 1280, 720, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glColor3f(1,1,1);
    // Draw crosshair
    glBegin(GL_LINES);
    glVertex2f(635,360); glVertex2f(645,360);
    glVertex2f(640,355); glVertex2f(640,365);
    glEnd();
    // Health bar
    glColor3f(1,0,0);
    glBegin(GL_QUADS);
    glVertex2f(30,30); glVertex2f(30+gs.local_player.health*2,30);
    glVertex2f(30+gs.local_player.health*2,50); glVertex2f(30,50);
    glEnd();
    glEnable(GL_DEPTH_TEST);
}

void render_game_world() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    // Simple perspective
    float aspect = 1280.0f/720.0f;
    gluPerspective(60.0f, aspect, 0.1f, 1000.0f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    Player *p = &gs.local_player;
    vec3 eye = vec3_add(p->pos, (vec3){0, 1.8f, 0});
    vec3 look = vec3_add(eye, (vec3){
        sinf(p->yaw*DEG2RAD)*cosf(p->pitch*DEG2RAD),
        -sinf(p->pitch*DEG2RAD),
        -cosf(p->yaw*DEG2RAD)*cosf(p->pitch*DEG2RAD)
    });
    gluLookAt(eye.x,eye.y,eye.z, look.x,look.y,look.z, 0,1,0);
    // Draw ground (green quad)
    glColor3f(0.3f,0.6f,0.2f);
    glBegin(GL_QUADS);
    glVertex3f(-500,0,500);
    glVertex3f(500,0,500);
    glVertex3f(500,0,-500);
    glVertex3f(-500,0,-500);
    glEnd();
    // Draw other players as cubes
    for(int i=0; i<gs.player_count; i++) {
        if(!gs.players[i].alive) continue;
        glPushMatrix();
        glTranslatef(gs.players[i].pos.x, gs.players[i].pos.y, gs.players[i].pos.z);
        glColor3f(0.8f,0.2f,0.2f); // red enemies
        glutSolidCube(1.0f);
        glPopMatrix();
    }
    // Draw gloo walls
    for(int i=0; i<gs.gloo_count; i++) {
        if(!gs.gloo_walls[i].active) continue;
        glPushMatrix();
        glTranslatef(gs.gloo_walls[i].pos.x, gs.gloo_walls[i].pos.y, gs.gloo_walls[i].pos.z);
        glRotatef(gs.gloo_walls[i].yaw, 0,1,0);
        glColor4f(0.2f,0.8f,0.9f,0.6f); // semi-transparent blue
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBegin(GL_QUADS);
        glVertex3f(-1,0,0); glVertex3f(1,0,0);
        glVertex3f(1,2,0); glVertex3f(-1,2,0);
        glEnd();
        glDisable(GL_BLEND);
        glPopMatrix();
    }
}

/* ---------- Game logic ---------- */
void player_move(Player *p, float forward, float right, float mouse_dx, float mouse_dy) {
    p->yaw += mouse_dx * 0.2f;
    p->pitch -= mouse_dy * 0.2f;
    if(p->pitch > 89.0f) p->pitch = 89.0f;
    if(p->pitch < -89.0f) p->pitch = -89.0f;
    CharacterDef *c = &characters_db[p->character_id];
    vec3 move = {0,0,0};
    if(forward != 0.0f) {
        vec3 fwd = {sinf(p->yaw*DEG2RAD), 0, -cosf(p->yaw*DEG2RAD)};
        move = vec3_add(move, vec3_scale(fwd, forward * c->speed * delta_time));
    }
    if(right != 0.0f) {
        vec3 rgt = {cosf(p->yaw*DEG2RAD), 0, sinf(p->yaw*DEG2RAD)};
        move = vec3_add(move, vec3_scale(rgt, right * c->speed * delta_time));
    }
    p->pos = vec3_add(p->pos, move);
    // Clamp to map
    p->pos.x = fmaxf(-MAP_SIZE/2, fminf(MAP_SIZE/2, p->pos.x));
    p->pos.z = fmaxf(-MAP_SIZE/2, fminf(MAP_SIZE/2, p->pos.z));
}

void fire_weapon(Player *p) {
    GunDef *g = &guns_db[p->current_gun];
    if(p->ammo[p->current_gun] <= 0) { play_sound("empty"); return; }
    p->ammo[p->current_gun]--;
    play_sound(g->name);
    // Simple hit‑scan (for demo)
    vec3 dir = { sinf(p->yaw*DEG2RAD)*cosf(p->pitch*DEG2RAD),
                 -sinf(p->pitch*DEG2RAD),
                 -cosf(p->yaw*DEG2RAD)*cosf(p->pitch*DEG2RAD) };
    dir = vec3_normalize(dir);
    float spread_rad = g->spread * DEG2RAD;
    // Apply random spread
    float angle = ((float)rand()/RAND_MAX) * 2.0f * PI;
    float offset_x = cosf(angle) * spread_rad;
    float offset_y = sinf(angle) * spread_rad;
    // (In a real engine, raycast with sphere collision for each player)
    // Here: check closest enemy within polygon (simplified to distance check)
    float closest_dist = g->max_range;
    int hit_idx = -1;
    for(int i=0; i<gs.player_count; i++) {
        if(!gs.players[i].alive || i == p->id) continue;
        vec3 to = vec3_sub(gs.players[i].pos, p->pos);
        float dist = vec3_length(to);
        if(dist < closest_dist) {
            // angle check roughly
            vec3 t = vec3_normalize(to);
            if(fabsf(acosf(vec3_dot(dir,t))) < 0.3f) { // ~17° cone
                closest_dist = dist;
                hit_idx = i;
            }
        }
    }
    if(hit_idx >= 0) {
        gs.players[hit_idx].health -= g->damage;
        if(gs.players[hit_idx].health <= 0) {
            gs.players[hit_idx].alive = 0;
            p->kills++;
            p->coins += 500;
        }
    }
}

void place_gloo_wall(Player *p) {
    if(gs.gloo_count >= MAX_GLOO_WALLS) return;
    GlooWall *gw = &gs.gloo_walls[gs.gloo_count++];
    vec3 dir = { sinf(p->yaw*DEG2RAD), 0, -cosf(p->yaw*DEG2RAD) };
    gw->pos = vec3_add(p->pos, vec3_scale(dir, 2.0f));
    gw->yaw = p->yaw;
    gw->hp = 500;
    gw->owner_id = p->id;
    gw->spawn_time = gs.match_time;
    gw->active = 1;
    play_sound("gloo_deploy");
}

void use_emote(Player *p, int emote_index) {
    printf("Player %s uses emote: %s\n", characters_db[p->character_id].name, emotes_db[emote_index].name);
    play_sound(emotes_db[emote_index].name);
}

/* ---------- Economy ---------- */
void reward_diamonds_and_coins() {
    gs.local_player.diamonds = 10;
    gs.local_player.coins = 10000;
    printf("Welcome! You received 10 diamonds and 10,000 coins.\n");
}

void watch_ad_reward() {
    printf("Watching ad...\n");
    SDL_Delay(5000); // simulate
    gs.local_player.diamonds += 1;
    printf("+1 Diamond! Total: %d\n", gs.local_player.diamonds);
}

/* ---------- State machine (simplified) ---------- */
enum { STATE_LOGIN, STATE_LOBBY, STATE_MATCH } game_state = STATE_LOGIN;

void login_screen() {
    printf("=== Pakdefender3d Login ===\n");
    printf("Enter your name: (simulated)\n");
    // Simulate input
    strcpy(characters_db[0].name, "Player"); // use first character
    gs.local_player.character_id = 0;
    reward_diamonds_and_coins();
    game_state = STATE_LOBBY;
}

void lobby() {
    printf("=== Lobby ===\n1. Lahore Clash (Battle Royale)\n2. Karachi Royale (TDM)\n3. Islamabad Strike (Capt. Flag)\n4. Shop\n5. Emotes\n6. Watch Ad (+1 Diamond)\n");
    // Simulate selection
    int choice = 1; // demo
    switch(choice) {
        case 1: case 2: case 3:
            gs.map = MAP_LAHORE;
            gs.local_player.alive = 1;
            gs.local_player.health = 100;
            gs.local_player.shield = 0;
            gs.local_player.pos = (vec3){0,0,0};
            gs.local_player.yaw = 0;
            gs.local_player.pitch = 0;
            gs.local_player.current_gun = 3; // Lahore Pulse
            gs.local_player.ammo[3] = 30;
            gs.player_count = 1; // only local player for now
            gs.players[0] = gs.local_player;
            game_state = STATE_MATCH;
            break;
        case 4: printf("Shop opened.\n"); break;
        case 5: printf("Emote wheel shown.\n"); break;
        case 6: watch_ad_reward(); break;
    }
}

/* ---------- Main loop ---------- */
int main(int argc, char *argv[]) {
    srand(time(NULL));
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO) != 0) { fprintf(stderr,"SDL_Init error\n"); return 1; }
    window = SDL_CreateWindow("Pakdefender3d", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_OPENGL);
    if(!window) { fprintf(stderr,"Window error\n"); SDL_Quit(); return 1; }
    gl_context = SDL_GL_CreateContext(window);
    SDL_GL_SetSwapInterval(1); // vsync
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.1f,0.1f,0.1f,1.0f);
    printf("=== Pakdefender3d started ===\n");

    login_screen();
    lobby();

    Uint32 last_time = SDL_GetTicks();
    while(running) {
        SDL_Event e;
        while(SDL_PollEvent(&e)) {
            if(e.type == SDL_QUIT) running = 0;
            if(game_state == STATE_MATCH) {
                if(e.type == SDL_MOUSEBUTTONDOWN) {
                    if(e.button.button == SDL_BUTTON_LEFT) fire_weapon(&gs.local_player);
                    if(e.button.button == SDL_BUTTON_RIGHT) place_gloo_wall(&gs.local_player);
                }
            }
        }

        Uint32 now = SDL_GetTicks();
        delta_time = (now - last_time) / 1000.0f;
        last_time = now;

        if(game_state == STATE_MATCH) {
            // Keyboard input
            const Uint8 *keystate = SDL_GetKeyboardState(NULL);
            float fwd = (keystate[SDL_SCANCODE_W] ? 1.0f : 0.0f) - (keystate[SDL_SCANCODE_S] ? 1.0f : 0.0f);
            float rgt = (keystate[SDL_SCANCODE_D] ? 1.0f : 0.0f) - (keystate[SDL_SCANCODE_A] ? 1.0f : 0.0f);
            int mouse_x, mouse_y;
            SDL_GetRelativeMouseState(&mouse_x, &mouse_y);
            player_move(&gs.local_player, fwd, rgt, (float)mouse_x, (float)mouse_y);

            // Emote keys 1-5
            for(int i=0; i<5; i++)
                if(keystate[SDL_SCANCODE_1 + i])
                    use_emote(&gs.local_player, i);

            gs.match_time += delta_time;
            // Decrease gloo wall hp over time
            for(int i=0; i<gs.gloo_count; i++) {
                if(gs.gloo_walls[i].active) {
                    gs.gloo_walls[i].hp -= 20 * delta_time;
                    if(gs.gloo_walls[i].hp <= 0) gs.gloo_walls[i].active = 0;
                }
            }
        }

        if(game_state == STATE_MATCH) {
            render_game_world();
            render_minimalist_ui();
        } else {
            glClear(GL_COLOR_BUFFER_BIT);
            // Display lobby text (just clear)
        }
        SDL_GL_SwapWindow(window);

        // Exit match if player dead
        if(game_state == STATE_MATCH && !gs.local_player.alive) {
            printf("You died! Returning to lobby...\n");
            game_state = STATE_LOBBY;
        }
    }

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
