/*
 * Pakdefender3d – Cross‑platform (Windows, Linux, macOS, Android)
 * Real guns, Pakistani maps, gloo walls, emotes.
 *
 * Android: compiled via SDL2 for Android. Uses touch controls.
 */

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <GL/glu.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#ifdef __ANDROID__
  #include <jni.h>
  // Only on Android we use JNI to receive ad reward callbacks
#endif

/* ---------- constants ---------- */
#define MAX_PLAYERS      60
#define MAX_GUNS         20
#define MAX_EMOTES       20
#define MAX_CHARACTERS   20
#define MAX_GLOO_WALLS   200
#define MAP_HALF         250.0f
#define PI               3.14159265358979323846f
#define DEG2RAD          (PI / 180.0f)

/* ---------- maths ---------- */
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

/* ---------- guns ---------- */
typedef enum { GUN_MELEE, GUN_SHOTGUN, GUN_SMG, GUN_AR, GUN_PISTOL,
               GUN_SNIPER, GUN_LMG, GUN_DMR, GUN_LAUNCHER, GUN_GRENADE } GunType;

typedef struct {
    int id;
    const char *name;
    GunType type;
    float damage;
    float fire_rate;
    int mag_size;
    float max_range;
    float spread_deg;
    int attach_slots;
} GunDef;

static const GunDef guns_db[MAX_GUNS] = {
    {0,"Combat Knife",   GUN_MELEE,   50, 1.0f, -1, 1.5f,  0.0f,0},
    {1,"Mossberg 590",   GUN_SHOTGUN,120,1.5f,  6, 20.0f, 5.0f,1},
    {2,"MP5",            GUN_SMG,     32,0.08f, 35,80.0f, 2.5f,2},
    {3,"M4A1",           GUN_AR,      38,0.1f,  30,150.0f,1.2f,3},
    {4,"Glock 18",       GUN_PISTOL,  25,0.15f, 15,60.0f, 3.0f,1},
    {5,"AWP",            GUN_SNIPER, 150,1.8f,  5, 400.0f,0.2f,2},
    {6,"M16A4",          GUN_AR,      34,0.2f,  30,120.0f,1.0f,3},
    {7,"M249",           GUN_LMG,     40,0.12f,100,200.0f,3.5f,2},
    {8,"SKS",            GUN_DMR,     65,0.5f,  10,300.0f,0.5f,2},
    {9,"Desert Eagle",   GUN_PISTOL,  45,0.4f,  9, 50.0f, 2.0f,1},
    {10,"Crossbow",      GUN_SNIPER,  90,2.0f,  4, 100.0f,0.1f,1},
    {11,"RPG-7",         GUN_LAUNCHER,200,3.0f, 1, 250.0f,8.0f,0},
    {12,"UMP-45",        GUN_SMG,     28,0.07f, 40,90.0f, 2.8f,2},
    {13,"AK-47",         GUN_AR,      36,0.09f, 30,130.0f,1.5f,3},
    {14,"SPAS-12",       GUN_SHOTGUN,140,1.2f,  4, 15.0f, 7.0f,1},
    {15,"Revolver",      GUN_PISTOL,  55,0.6f,  6, 70.0f, 1.5f,1},
    {16,"Dragunov SVD",  GUN_SNIPER, 130,1.5f,  5, 450.0f,0.2f,3},
    {17,"SCAR-H",        GUN_AR,      36,0.09f, 35,130.0f,1.1f,3},
    {18,"Saiga-12",      GUN_SHOTGUN,110,1.0f,  8, 18.0f, 6.0f,1},
    {19,"Frag Grenade",  GUN_GRENADE,100,4.0f,  1, 30.0f, 0.0f,0}
};

/* ---------- characters ---------- */
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

/* ---------- emotes ---------- */
static const char *emote_names[MAX_EMOTES] = {
    "Bhangra","Attan","Dhamal","SwordSalute","KabaddiJump",
    "PeshawarClap","SindhiWave","BalochEagle","PunjabiStep","PakDefender",
    "Jhoomar","Luddi","Khattak","Sammi","Cholistan",
    "Hunar","Gidda","Tandoor","Mehndi","CamelDance"
};

/* ---------- maps ---------- */
static const char *map_names[] = {
    "Lahore Fort", "Karachi Port", "Islamabad Hills", "Peshawar Old City"
};

/* ---------- player state ---------- */
typedef struct {
    int id;
    vec3 pos;
    float yaw, pitch;
    float health, shield;
    int current_gun;
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

/* ---------- global game state ---------- */
static struct {
    int map;
    Player local_player;
    Player players[MAX_PLAYERS];
    int player_count;
    GlooWall gloo_walls[MAX_GLOO_WALLS];
    int gloo_count;
    float match_time;
    int running;
} gs;

static SDL_Window *window = NULL;
static SDL_GLContext gl_context;
static int app_running = 1;
static float delta_time = 0.0f;

// Touch control state (Android)
static int touch_active = 0;
static int touch_move_finger = -1;
static int touch_look_finger = -1;
static float touch_look_last_x = 0, touch_look_last_y = 0;
static int touch_fire = 0;
static int touch_gloo = 0;

/* ---------- prototypes ---------- */
static void init_game(void);
static void update_match(float dt);
static void render_frame(void);
static void fire_weapon(void);
static void place_gloo(void);
static void use_emote(int idx);

#ifdef __ANDROID__
// JNI functions for Adsterra rewarded ads
JNIEXPORT void JNICALL Java_com_pakdefender3d_MainActivity_addDiamonds(JNIEnv *env, jobject thiz, jint amount) {
    (void)env; (void)thiz;
    gs.local_player.diamonds += amount;
    printf("Ad reward: +%d diamonds (now %d)\n", amount, gs.local_player.diamonds);
}

JNIEXPORT void JNICALL Java_com_pakdefender3d_MainActivity_showInterstitialAd(JNIEnv *env, jobject thiz) {
    // Called from Java when we want to show an interstitial
}
#endif

/* ---------- init ---------- */
static void init_game(void) {
    srand((unsigned)time(NULL));
    memset(&gs, 0, sizeof(gs));
    gs.local_player.id = 0;
    gs.local_player.alive = 1;
    gs.local_player.health = 100;
    gs.local_player.shield = 0;
    gs.local_player.pos = (vec3){0,0,0};
    gs.local_player.yaw = 0;
    gs.local_player.pitch = 0;
    gs.local_player.current_gun = 3; // M4A1
    gs.local_player.ammo[3] = 30;
    gs.local_player.character_id = 0;
    gs.local_player.diamonds = 10;
    gs.local_player.coins = 10000;
    gs.player_count = 1;
    gs.players[0] = gs.local_player;
    gs.running = 0;
}

/* ---------- touch to game input conversion (Android) ---------- */
static void handle_touch_input(SDL_Event *e) {
    if (e->type == SDL_FINGERDOWN) {
        if (e->tfinger.x < 0.3f) {  // left side: move joystick
            touch_move_finger = (int)e->tfinger.fingerId;
        } else if (e->tfinger.x > 0.7f) {  // right side: look / buttons
            if (e->tfinger.y < 0.7f) {      // upper right: look
                touch_look_finger = (int)e->tfinger.fingerId;
                touch_look_last_x = e->tfinger.x;
                touch_look_last_y = e->tfinger.y;
            }
        }
    } else if (e->type == SDL_FINGERUP) {
        if ((int)e->tfinger.fingerId == touch_move_finger) touch_move_finger = -1;
        if ((int)e->tfinger.fingerId == touch_look_finger) touch_look_finger = -1;
    } else if (e->type == SDL_FINGERMOTION) {
        if ((int)e->tfinger.fingerId == touch_look_finger) {
            float dx = e->tfinger.x - touch_look_last_x;
            float dy = e->tfinger.y - touch_look_last_y;
            gs.local_player.yaw += dx * 200.0f;   // adjust sensitivity
            gs.local_player.pitch -= dy * 200.0f;
            if (gs.local_player.pitch > 89.0f) gs.local_player.pitch = 89.0f;
            if (gs.local_player.pitch < -89.0f) gs.local_player.pitch = -89.0f;
            touch_look_last_x = e->tfinger.x;
            touch_look_last_y = e->tfinger.y;
        }
    }
    // Fire & gloo via right-side buttons simulated: on screen we don't have real buttons, but we keep mouse alternatives
}

/* ---------- player movement (WASD or virtual joystick) ---------- */
static void player_move(float fwd, float right, float mx, float my) {
    Player *p = &gs.local_player;
    CharacterDef *c = &characters_db[p->character_id];
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
    p->pos.x = fmaxf(-MAP_HALF, fminf(MAP_HALF, p->pos.x));
    p->pos.z = fmaxf(-MAP_HALF, fminf(MAP_HALF, p->pos.z));
}

/* ... (fire_weapon, place_gloo, use_emote, update_match, render_frame unchanged) ... */

// Fire
static void fire_weapon(void) {
    Player *p = &gs.local_player;
    const GunDef *g = &guns_db[p->current_gun];
    if (p->ammo[p->current_gun] <= 0 && g->mag_size > 0) return;
    if (g->mag_size > 0) p->ammo[p->current_gun]--;

    vec3 dir = { sinf(p->yaw*DEG2RAD)*cosf(p->pitch*DEG2RAD),
                 -sinf(p->pitch*DEG2RAD),
                 -cosf(p->yaw*DEG2RAD)*cosf(p->pitch*DEG2RAD) };
    dir = vec3_norm(dir);
    float best_dist = g->max_range;
    int hit_id = -1;
    for (int i=0; i<gs.player_count; i++) {
        if (!gs.players[i].alive || i==p->id) continue;
        vec3 to = vec3_sub(gs.players[i].pos, p->pos);
        float dist = vec3_len(to);
        if (dist > best_dist) continue;
        vec3 to_dir = vec3_norm(to);
        float angle = acosf(vec3_dot(dir, to_dir));
        if (angle < g->spread_deg*DEG2RAD + 0.1f) { best_dist=dist; hit_id=i; }
    }
    if (hit_id>=0) {
        gs.players[hit_id].health -= g->damage;
        if (gs.players[hit_id].health <= 0) {
            gs.players[hit_id].alive=0;
            p->kills++; p->coins+=500;
        }
    }
}

static void place_gloo(void) {
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
}

static void use_emote(int idx) {
    if (idx<0||idx>=MAX_EMOTES) return;
    printf("Emote: %s\n", emote_names[idx]);
}

static void update_match(float dt) {
    gs.match_time += dt;
    for (int i=0; i<gs.gloo_count; i++) {
        if (gs.gloo_walls[i].active) {
            gs.gloo_walls[i].hp -= 20.0f*dt;
            if (gs.gloo_walls[i].hp <= 0) gs.gloo_walls[i].active = 0;
        }
    }
    if (!gs.local_player.alive) gs.running = 0;
}

/* Rendering (same as before, using OpenGL) */
static void render_frame(void) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!gs.running) return;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0, 1280.0/720.0, 0.1, 1000.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    Player *p = &gs.local_player;
    vec3 eye = vec3_add(p->pos, (vec3){0,1.7f,0});
    vec3 look_dir = { sinf(p->yaw*DEG2RAD)*cosf(p->pitch*DEG2RAD),
                      -sinf(p->pitch*DEG2RAD),
                      -cosf(p->yaw*DEG2RAD)*cosf(p->pitch*DEG2RAD) };
    vec3 look = vec3_add(eye, look_dir);
    gluLookAt(eye.x,eye.y,eye.z, look.x,look.y,look.z, 0,1,0);

    // ground
    glColor3f(0.2f,0.5f,0.2f);
    glBegin(GL_QUADS);
    glVertex3f(-MAP_HALF,0, MAP_HALF);
    glVertex3f( MAP_HALF,0, MAP_HALF);
    glVertex3f( MAP_HALF,0,-MAP_HALF);
    glVertex3f(-MAP_HALF,0,-MAP_HALF);
    glEnd();

    // other players
    for (int i=0; i<gs.player_count; i++) {
        if (!gs.players[i].alive || i==p->id) continue;
        glPushMatrix();
        glTranslatef(gs.players[i].pos.x, gs.players[i].pos.y, gs.players[i].pos.z);
        glColor3f(0.9f,0.2f,0.2f);
        glutSolidCube(1.0f);
        glPopMatrix();
    }

    // gloo walls
    for (int i=0; i<gs.gloo_count; i++) {
        if (!gs.gloo_walls[i].active) continue;
        GlooWall *w = &gs.gloo_walls[i];
        glPushMatrix();
        glTranslatef(w->pos.x, w->pos.y, w->pos.z);
        glRotatef(w->yaw, 0,1,0);
        glColor4f(0.2f,0.8f,1.0f,0.5f);
        glBegin(GL_QUADS);
        glVertex3f(-1,0,0); glVertex3f(1,0,0);
        glVertex3f(1,2,0); glVertex3f(-1,2,0);
        glEnd();
        glPopMatrix();
    }

    // HUD (simplified)
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0,1280,720,0,-1,1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glColor3f(1,1,1);
    glBegin(GL_LINES);
    glVertex2f(635,360); glVertex2f(645,360);
    glVertex2f(640,355); glVertex2f(640,365);
    glEnd();
    glEnable(GL_DEPTH_TEST);
}

/* ---------- main ---------- */
int main(int argc, char *argv[]) {
    init_game();

    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
    window = SDL_CreateWindow("Pakdefender3d", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    gl_context = SDL_GL_CreateContext(window);
    SDL_GL_SetSwapInterval(1);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.1f,0.1f,0.1f,1.0f);

    // Start a match immediately for demo
    gs.map = 0;
    gs.local_player.alive = 1;
    gs.running = 1;
    gs.player_count = 4;
    for (int i=1; i<4; i++) {
        Player *bot = &gs.players[i];
        bot->id = i;
        bot->alive = 1;
        bot->health = 100;
        bot->pos = (vec3){ rand()%50-25, 0, rand()%50-25 };
        bot->current_gun = rand()%20;
        for (int j=0; j<MAX_GUNS; j++) bot->ammo[j] = (guns_db[j].mag_size>0)?guns_db[j].mag_size:1;
        bot->character_id = i % MAX_CHARACTERS;
    }

    Uint32 last_time = SDL_GetTicks();
    while (app_running) {
        Uint32 now = SDL_GetTicks();
        delta_time = (now - last_time) / 1000.0f;
        last_time = now;

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) app_running = 0;
            handle_touch_input(&e);

            if (gs.running) {
                if (e.type == SDL_MOUSEBUTTONDOWN) {
                    if (e.button.button == SDL_BUTTON_LEFT) fire_weapon();
                    if (e.button.button == SDL_BUTTON_RIGHT) place_gloo();
                }
                if (e.type == SDL_KEYDOWN) {
                    if (e.key.keysym.sym == SDLK_ESCAPE) gs.running = 0;
                    if (e.key.keysym.sym >= SDLK_1 && e.key.keysym.sym <= SDLK_5)
                        use_emote(e.key.keysym.sym - SDLK_1);
                }
            }
        }

        if (gs.running) {
            const Uint8 *keystate = SDL_GetKeyboardState(NULL);
            float fwd = (keystate[SDL_SCANCODE_W]?1:0)-(keystate[SDL_SCANCODE_S]?1:0);
            float rgt = (keystate[SDL_SCANCODE_D]?1:0)-(keystate[SDL_SCANCODE_A]?1:0);
            int mx=0, my=0;
            SDL_GetRelativeMouseState(&mx, &my);
            player_move(fwd, rgt, (float)mx, (float)my);
            update_match(delta_time);
        }
        render_frame();
        SDL_GL_SwapWindow(window);
    }

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
