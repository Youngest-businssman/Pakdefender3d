/*
 * Pakdefender3d – Online Multiplayer Client
 * Pakistani cities, traditional characters, real guns, gloo walls, emotes,
 * Gumroad monetization, redeem screen, sounds, networking.
 * Compile: see above.
 */

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <GL/glu.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#ifdef _WIN32
  #include <winsock2.h>
  #pragma comment(lib, "ws2_32.lib")
  typedef int socklen_t;
  #define close closesocket
  #include <GL/freeglut.h>
  static void open_url(const char *url) { ShellExecute(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL); }
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #define SOCKET int
  #define INVALID_SOCKET -1
  #define SOCKET_ERROR -1
  #ifdef __APPLE__
    static void open_url(const char *url) { char cmd[512]; snprintf(cmd, sizeof(cmd), "open '%s' &", url); system(cmd); }
    #include <GLUT/glut.h>
  #elif __linux__
    static void open_url(const char *url) { char cmd[512]; snprintf(cmd, sizeof(cmd), "xdg-open '%s' &", url); system(cmd); }
    #include <GL/freeglut.h>
  #endif
#endif

/* ---------- Constants ---------- */
#define MAX_PLAYERS       60
#define MAX_GUNS          20
#define MAX_EMOTES        20
#define MAX_CHARACTERS    20
#define MAX_GLOO_WALLS    200
#define MAP_HALF          250.0f
#define PI                3.14159265358979323846f
#define DEG2RAD           (PI/180.0)

#define SERVER_PORT 7777

/* ---------- Maths ---------- */
typedef struct { float x,y,z; } vec3;
typedef struct { float r,g,b; } Color;
static vec3 vec3_add(vec3 a,vec3 b){return (vec3){a.x+b.x,a.y+b.y,a.z+b.z};}
static vec3 vec3_sub(vec3 a,vec3 b){return (vec3){a.x-b.x,a.y-b.y,a.z-b.z};}
static vec3 vec3_scale(vec3 a,float s){return (vec3){a.x*s,a.y*s,a.z*s};}
static float vec3_dot(vec3 a,vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static float vec3_len(vec3 v){return sqrtf(vec3_dot(v,v));}
static vec3 vec3_norm(vec3 v){float l=vec3_len(v);return l>0.0001f?vec3_scale(v,1.0f/l):(vec3){0,0,0};}

/* ---------- Gun database ---------- */
typedef enum { GUN_MELEE,GUN_SHOTGUN,GUN_SMG,GUN_AR,GUN_PISTOL,GUN_SNIPER,GUN_LMG,GUN_DMR,GUN_LAUNCHER,GUN_GRENADE } GunType;
typedef struct { int id; const char *name; GunType type; float damage,fire_rate; int mag_size; float max_range,spread_deg; int attach_slots; } GunDef;
static const GunDef guns_db[MAX_GUNS] = {
    {0,"Combat Knife",   GUN_MELEE,   50,1.0f,-1,1.5f, 0.0f,0},
    {1,"Mossberg 590",   GUN_SHOTGUN,120,1.5f, 6,20.0f,5.0f,1},
    {2,"MP5",            GUN_SMG,     32,0.08f,35,80.0f,2.5f,2},
    {3,"M4A1",           GUN_AR,      38,0.1f, 30,150.0f,1.2f,3},
    {4,"Glock 18",       GUN_PISTOL,  25,0.15f,15,60.0f,3.0f,1},
    {5,"AWP",            GUN_SNIPER, 150,1.8f, 5,400.0f,0.2f,2},
    {6,"M16A4",          GUN_AR,      34,0.2f, 30,120.0f,1.0f,3},
    {7,"M249",           GUN_LMG,     40,0.12f,100,200.0f,3.5f,2},
    {8,"SKS",            GUN_DMR,     65,0.5f, 10,300.0f,0.5f,2},
    {9,"Desert Eagle",   GUN_PISTOL,  45,0.4f, 9,50.0f,2.0f,1},
    {10,"Crossbow",      GUN_SNIPER,  90,2.0f, 4,100.0f,0.1f,1},
    {11,"RPG-7",         GUN_LAUNCHER,200,3.0f,1,250.0f,8.0f,0},
    {12,"UMP-45",        GUN_SMG,     28,0.07f,40,90.0f,2.8f,2},
    {13,"AK-47",         GUN_AR,      36,0.09f,30,130.0f,1.5f,3},
    {14,"SPAS-12",       GUN_SHOTGUN,140,1.2f, 4,15.0f,7.0f,1},
    {15,"Revolver",      GUN_PISTOL,  55,0.6f, 6,70.0f,1.5f,1},
    {16,"Dragunov SVD",  GUN_SNIPER, 130,1.5f, 5,450.0f,0.2f,3},
    {17,"SCAR-H",        GUN_AR,      36,0.09f,35,130.0f,1.1f,3},
    {18,"Saiga-12",      GUN_SHOTGUN,110,1.0f, 8,18.0f,6.0f,1},
    {19,"Frag Grenade",  GUN_GRENADE,100,4.0f, 1,30.0f,0.0f,0}
};

/* ---------- Characters ---------- */
typedef struct { int id; const char *name; float speed,stealth; Color outfit,skin; } CharacterDef;
static const CharacterDef characters_db[MAX_CHARACTERS] = {
    {0,"Zainab",5.0f,1.2f,{0.1f,0.5f,0.2f},{0.8f,0.6f,0.5f}},{1,"Ahmad",4.8f,1.5f,{0.9f,0.9f,0.9f},{0.9f,0.7f,0.6f}},
    {2,"Bilal",5.2f,1.0f,{0.6f,0.4f,0.2f},{0.8f,0.6f,0.5f}},{3,"Rukhsana",4.9f,1.3f,{0.8f,0.2f,0.3f},{0.9f,0.7f,0.6f}},
    {4,"Fahad",5.1f,1.1f,{0.2f,0.3f,0.8f},{0.8f,0.6f,0.5f}},{5,"Meerab",5.0f,1.4f,{0.9f,0.7f,0.1f},{0.9f,0.7f,0.6f}},
    {6,"Aamir",4.7f,1.6f,{0.5f,0.5f,0.5f},{0.8f,0.6f,0.5f}},{7,"Daniyal",5.3f,1.8f,{0.7f,0.4f,0.2f},{0.9f,0.7f,0.6f}},
    {8,"Shanzay",5.0f,1.2f,{0.1f,0.6f,0.4f},{0.9f,0.7f,0.6f}},{9,"Kulsoom",4.9f,1.5f,{0.8f,0.1f,0.5f},{0.8f,0.6f,0.5f}},
    {10,"Arslan",5.2f,1.0f,{0.3f,0.3f,0.3f},{0.8f,0.6f,0.5f}},{11,"Zara",5.1f,1.3f,{0.9f,0.6f,0.2f},{0.9f,0.7f,0.6f}},
    {12,"Hassan",4.8f,1.7f,{0.2f,0.2f,0.5f},{0.8f,0.6f,0.5f}},{13,"Noreen",5.0f,1.1f,{0.5f,0.8f,0.5f},{0.9f,0.7f,0.6f}},
    {14,"Tariq",5.4f,0.9f,{0.7f,0.7f,0.3f},{0.8f,0.6f,0.5f}},{15,"Anaya",4.7f,1.9f,{0.9f,0.3f,0.3f},{0.9f,0.7f,0.6f}},
    {16,"Umar",5.0f,1.4f,{0.5f,0.2f,0.6f},{0.8f,0.6f,0.5f}},{17,"Saira",4.9f,1.5f,{0.3f,0.7f,0.9f},{0.9f,0.7f,0.6f}},
    {18,"Kamran",5.3f,1.6f,{0.6f,0.5f,0.2f},{0.8f,0.6f,0.5f}},{19,"Yasmin",5.1f,1.2f,{0.1f,0.5f,0.5f},{0.9f,0.7f,0.6f}}
};

/* ---------- Emotes ---------- */
static const char *emote_names[MAX_EMOTES] = {
    "Bhangra","Attan","Dhamal","SwordSalute","KabaddiJump","PeshawarClap","SindhiWave","BalochEagle",
    "PunjabiStep","PakDefender","Jhoomar","Luddi","Khattak","Sammi","Cholistan","Hunar","Gidda",
    "Tandoor","Mehndi","CamelDance"
};

/* ---------- Maps ---------- */
static const char *map_names[] = {"Lahore Fort","Karachi Port","Islamabad Hills","Peshawar Old City"};

/* ---------- Player ---------- */
typedef struct {
    int id; vec3 pos; float yaw,pitch,health,shield; int current_gun; int ammo[MAX_GUNS];
    int character_id; int diamonds,coins; int kills; int alive; float anim_time;
} Player;

/* ---------- Gloo wall ---------- */
typedef struct { vec3 pos; float yaw,hp; int owner_id; float spawn_time; int active; } GlooWall;

/* ---------- Building ---------- */
typedef struct { vec3 pos; float size,height; Color color; int type; } Building;
#define MAX_BUILDINGS 100
static Building city[MAX_BUILDINGS];
static int num_buildings=0;

/* ---------- Game State ---------- */
static struct {
    int map; Player local_player; Player players[MAX_PLAYERS]; int player_count;
    GlooWall gloo_walls[MAX_GLOO_WALLS]; int gloo_count; float match_time; int running;
} gs;

static SDL_Window *window=NULL;
static SDL_GLContext gl_context;
static int app_running=1;
static float delta_time=0.0f;

/* ---------- Audio ---------- */
#define SAMPLE_RATE 44100
#define AUDIO_CHANNELS 32
typedef struct { float freq, volume, duration; float phase, sample_time; int active; int type; } SoundChannel;
static SoundChannel channels[AUDIO_CHANNELS];
static SDL_AudioDeviceID audio_dev=0;
static void audio_callback(void *udata, Uint8 *stream, int len) {
    (void)udata; float *buf = (float*)stream; int samples = len / sizeof(float);
    for (int i=0; i<samples; i++) { float sample=0; for (int ch=0; ch<AUDIO_CHANNELS; ch++) { if (!channels[ch].active) continue; float t=channels[ch].sample_time; float val=0; if (channels[ch].type==0) val=sinf(2.0f*PI*channels[ch].freq*t); else if (channels[ch].type==1) val=(sinf(2.0f*PI*channels[ch].freq*t)>0)?1.0f:-1.0f; else val=((rand()%10000)/5000.0f-1.0f); sample+=val*channels[ch].volume; channels[ch].sample_time+=1.0f/SAMPLE_RATE; if (channels[ch].sample_time>=channels[ch].duration) channels[ch].active=0; } buf[i]=sample*0.3f; } }
static void init_audio(){ SDL_AudioSpec want,have; SDL_zero(want); want.freq=SAMPLE_RATE; want.format=AUDIO_F32SYS; want.channels=1; want.samples=1024; want.callback=audio_callback; audio_dev=SDL_OpenAudioDevice(NULL,0,&want,&have,0); if(audio_dev) SDL_PauseAudioDevice(audio_dev,0); }
static void play_sound(float freq,float duration,float volume,int type){ for(int i=0;i<AUDIO_CHANNELS;i++) if(!channels[i].active){ channels[i].freq=freq; channels[i].duration=duration; channels[i].volume=volume; channels[i].sample_time=0; channels[i].active=1; channels[i].type=type; return; } }
static void sfx_gun(int id){ play_sound(200+id*50,0.15f,0.8f,1); }
static void sfx_hit(){ play_sound(300,0.1f,0.7f,0); }
static void sfx_death(){ play_sound(100,0.5f,0.9f,1); }
static void sfx_gloo(){ play_sound(800,0.2f,0.6f,0); }
static void sfx_emote(){ play_sound(500,0.3f,0.5f,0); }
static void sfx_footstep(){ play_sound(60,0.05f,0.2f,2); }
static float bgm_time=0; static void update_bgm(float dt){ bgm_time+=dt; float notes[]={261.6f,293.7f,329.6f,349.2f,392.0f,440.0f,493.9f,523.3f}; int note_idx=((int)(bgm_time*2.0f))%8; if(fmodf(bgm_time,0.5f)<dt) play_sound(notes[note_idx],0.4f,0.15f,0); }

/* ---------- Networking ---------- */
static SOCKET network_sock = INVALID_SOCKET;
static struct sockaddr_in server_addr;
static int network_connected = 0;
static char server_ip[32] = "127.0.0.1";

static void network_init(const char *ip) {
    if (ip) strncpy(server_ip, ip, 31);
#ifdef _WIN32
    WSADATA wsa; WSAStartup(MAKEWORD(2,2), &wsa);
#endif
    network_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (network_sock == INVALID_SOCKET) { printf("Network error\n"); return; }
#ifdef _WIN32
    u_long mode = 1; ioctlsocket(network_sock, FIONBIO, &mode);
#else
    int flags = fcntl(network_sock, F_GETFL, 0); fcntl(network_sock, F_SETFL, flags | O_NONBLOCK);
#endif
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(server_ip);
    char join = 0;
    sendto(network_sock, &join, 1, 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
    network_connected = 1;
    printf("Connected to server %s\n", server_ip);
}

static void network_send_input(float fwd, float rgt, float mx, float my) {
    if (!network_connected) return;
    char buf[17]; buf[0]=1; memcpy(buf+1,&fwd,4); memcpy(buf+5,&rgt,4); memcpy(buf+9,&mx,4); memcpy(buf+13,&my,4);
    sendto(network_sock, buf, sizeof(buf), 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
}

static void network_send_fire() {
    if (!network_connected) return;
    char fire = 2;
    sendto(network_sock, &fire, 1, 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
}

static void network_receive() {
    if (!network_connected) return;
    char buf[2048]; struct sockaddr_in from; socklen_t fromlen = sizeof(from);
    int len = recvfrom(network_sock, buf, sizeof(buf), 0, (struct sockaddr*)&from, &fromlen);
    if (len <= 0) return;
    int offset=0, count; memcpy(&count, buf+offset, sizeof(int)); offset+=sizeof(int);
    gs.player_count = count;
    for (int i=0; i<count; i++) {
        memcpy(&gs.players[i].id, buf+offset, sizeof(int)); offset+=sizeof(int);
        memcpy(&gs.players[i].pos.x, buf+offset, sizeof(float)); offset+=sizeof(float);
        memcpy(&gs.players[i].pos.y, buf+offset, sizeof(float)); offset+=sizeof(float);
        memcpy(&gs.players[i].pos.z, buf+offset, sizeof(float)); offset+=sizeof(float);
        memcpy(&gs.players[i].yaw, buf+offset, sizeof(float)); offset+=sizeof(float);
        memcpy(&gs.players[i].health, buf+offset, sizeof(float)); offset+=sizeof(float);
        memcpy(&gs.players[i].alive, buf+offset, sizeof(int)); offset+=sizeof(int);
        memcpy(&gs.players[i].current_gun, buf+offset, sizeof(int)); offset+=sizeof(int);
        float pitch; memcpy(&pitch, buf+offset, sizeof(float)); offset+=sizeof(float);
        gs.players[i].pitch = pitch;
        if (gs.players[i].id == gs.local_player.id) {
            gs.local_player.health = gs.players[i].health;
            gs.local_player.alive = gs.players[i].alive;
        }
    }
}

/* ---------- Gumroad ---------- */
static const char *gumroad_url_100 = "https://hassantemplatespk.gumroad.com/l/100diamonds";
static const char *gumroad_url_500 = "https://hassantemplatespk.gumroad.com/l/pakdefender3d500diamonds";
static void buy_diamonds(int amount) {
    const char *url = (amount == 500) ? gumroad_url_500 : gumroad_url_100;
    if (url && strlen(url) > 0) open_url(url);
}

/* ---------- Redeem Screen ---------- */
static char redeem_text[32] = {0};
static int redeem_len = 0;
static int show_redeem = 0;
static int redeem_success = 0;
static float redeem_success_time = 0.0f;

static void draw_button(float x, float y, float w, float h, const char *label, int pressed) {
    if (pressed) glColor3f(0.3f,0.7f,0.3f); else glColor3f(0.2f,0.5f,0.2f);
    glBegin(GL_QUADS); glVertex2f(x,y); glVertex2f(x+w,y); glVertex2f(x+w,y+h); glVertex2f(x,y+h); glEnd();
    glColor3f(1,1,1); glBegin(GL_LINES); glVertex2f(x+10,y+h/2); glVertex2f(x+w-10,y+h/2); glEnd();
}

static int point_in_rect(float px, float py, float x, float y, float w, float h) {
    return (px >= x && px <= x+w && py >= y && py <= y+h);
}

static void redeem_handle_click(float mx, float my) {
    float kw=80, kh=60, sx=1280/2 - (4*kw)/2, sy=300;
    struct { float x,y,w,h; const char *l; char ch; } btns[] = {
        {sx+0*kw,sy+0*kh,kw,kh,"1",'1'},{sx+1*kw,sy+0*kh,kw,kh,"2",'2'},{sx+2*kw,sy+0*kh,kw,kh,"3",'3'},{sx+3*kw,sy+0*kh,kw,kh,"<",'<'},
        {sx+0*kw,sy+1*kh,kw,kh,"4",'4'},{sx+1*kw,sy+1*kh,kw,kh,"5",'5'},{sx+2*kw,sy+1*kh,kw,kh,"6",'6'},{sx+3*kw,sy+1*kh,kw,kh,"C",'-'},
        {sx+0*kw,sy+2*kh,kw,kh,"7",'7'},{sx+1*kw,sy+2*kh,kw,kh,"8",'8'},{sx+2*kw,sy+2*kh,kw,kh,"9",'9'},{sx+3*kw,sy+2*kh,kw,kh,"Done",'E'},
        {sx+1*kw,sy+3*kh,kw,kh,"0",'0'},{sx+2*kw,sy+3*kh,kw,kh,"Del",'D'}
    };
    int n = sizeof(btns)/sizeof(btns[0]);
    for (int i=0; i<n; i++) {
        if (point_in_rect(mx, my, btns[i].x, btns[i].y, btns[i].w, btns[i].h)) {
            char c = btns[i].ch;
            if (c>='0'&&c<='9') { if(redeem_len<30){ redeem_text[redeem_len++]=c; redeem_text[redeem_len]='\0'; } }
            else if (c=='<'||c=='D') { if(redeem_len>0) redeem_text[--redeem_len]='\0'; }
            else if (c=='-') { redeem_len=0; redeem_text[0]='\0'; }
            else if (c=='E') { if(strlen(redeem_text)==8){ gs.local_player.diamonds+=100; redeem_success=1; redeem_success_time=0.0f; } show_redeem=0; }
        }
    }
}

static void render_redeem_screen() {
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0,1280,720,0,-1,1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity(); glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0,0,0,0.7f); glBegin(GL_QUADS); glVertex2f(0,0); glVertex2f(1280,0); glVertex2f(1280,720); glVertex2f(0,720); glEnd();
    glColor3f(0.2f,0.2f,0.2f); glBegin(GL_QUADS); glVertex2f(400,200); glVertex2f(880,200); glVertex2f(880,260); glVertex2f(400,260); glEnd();
    glColor3f(1,1,1);
    for (int i=0; i<redeem_len; i++) { float x=440+i*30; glBegin(GL_LINES); glVertex2f(x,230); glVertex2f(x+20,230); glEnd(); }
    float kw=80, kh=60, sx=1280/2 - (4*kw)/2, sy=300;
    struct { float x,y,w,h; const char *l; } btns[] = {
        {sx+0*kw,sy+0*kh,kw,kh,"1"},{sx+1*kw,sy+0*kh,kw,kh,"2"},{sx+2*kw,sy+0*kh,kw,kh,"3"},{sx+3*kw,sy+0*kh,kw,kh,"<"},
        {sx+0*kw,sy+1*kh,kw,kh,"4"},{sx+1*kw,sy+1*kh,kw,kh,"5"},{sx+2*kw,sy+1*kh,kw,kh,"6"},{sx+3*kw,sy+1*kh,kw,kh,"C"},
        {sx+0*kw,sy+2*kh,kw,kh,"7"},{sx+1*kw,sy+2*kh,kw,kh,"8"},{sx+2*kw,sy+2*kh,kw,kh,"9"},{sx+3*kw,sy+2*kh,kw,kh,"Done"},
        {sx+1*kw,sy+3*kh,kw,kh,"0"},{sx+2*kw,sy+3*kh,kw,kh,"Del"}
    };
    for (int i=0; i<14; i++) draw_button(btns[i].x, btns[i].y, btns[i].w, btns[i].h, btns[i].l, 0);
    if (redeem_success) { glColor3f(0,1,0); glBegin(GL_QUADS); glVertex2f(400,500); glVertex2f(880,500); glVertex2f(880,540); glVertex2f(400,540); glEnd(); }
    glDisable(GL_BLEND); glEnable(GL_DEPTH_TEST);
}

/* ---------- Rendering and Game Logic (abbreviated but complete) ---------- */
static void init_game() {
    srand((unsigned)time(NULL)); memset(&gs,0,sizeof(gs));
    gs.local_player.id=0; gs.local_player.alive=1; gs.local_player.health=100;
    gs.local_player.pos=(vec3){0,0,0}; gs.local_player.yaw=0; gs.local_player.pitch=0;
    gs.local_player.current_gun=13; gs.local_player.ammo[13]=30;
    gs.local_player.character_id=0; gs.local_player.diamonds=10; gs.local_player.coins=10000;
    gs.player_count=1; gs.players[0]=gs.local_player;
}

static void generate_map(int map) {
    num_buildings=0; srand(map*12345);
    for(int x=-200;x<=200;x+=50) for(int z=-200;z<=200;z+=50) {
        if(rand()%3==0)continue;
        Building b; b.pos=(vec3){x+(rand()%30-15),0,z+(rand()%30-15)};
        b.size=10+rand()%20; b.height=5+rand()%30;
        b.color=(Color){rand()%100/100.0f,rand()%100/100.0f,rand()%100/100.0f}; b.type=rand()%3;
        if(num_buildings<MAX_BUILDINGS) city[num_buildings++]=b;
    }
    for(int i=0;i<5;i++) {
        Building b; b.pos=(vec3){rand()%400-200,0,rand()%400-200};
        b.size=20; b.height=40+rand()%60; b.color=(Color){0.9f,0.8f,0.6f}; b.type=1;
        if(num_buildings<MAX_BUILDINGS) city[num_buildings++]=b;
    }
}

static void draw_building(Building *b) {
    glColor3f(b->color.r,b->color.g,b->color.b); glPushMatrix(); glTranslatef(b->pos.x,0,b->pos.z);
    if(b->type==0){glPushMatrix();glScalef(b->size,b->height,b->size);glutSolidCube(1.0);glPopMatrix();}
    else if(b->type==1){
        glPushMatrix();glScalef(b->size,b->height*0.7f,b->size);glutSolidCube(1.0);glPopMatrix();
        glTranslatef(0,b->height*0.7f,0);glColor3f(0.8f,0.8f,0.8f);glutSolidSphere(b->size*0.5f,16,16);
    }else{
        glColor3f(0.9f,0.9f,0.8f);glPushMatrix();glRotatef(90,1,0,0);
        gluCylinder(gluNewQuadric(),b->size*0.1f,b->size*0.1f,b->height,8,1);glPopMatrix();
        glTranslatef(0,b->height,0);glColor3f(0.7f,0.7f,0.7f);glutSolidSphere(b->size*0.15f,8,8);
    }
    glPopMatrix();
}

static void draw_character(Player *p,int local) {
    const CharacterDef *c=&characters_db[p->character_id];
    glPushMatrix(); glTranslatef(p->pos.x,p->pos.y,p->pos.z); glRotatef(p->yaw,0,1,0);
    float walk=sinf(p->anim_time*10.0f)*0.3f; if(!local) p->anim_time+=delta_time*2.0f;
    glColor3f(c->skin.r,c->skin.g,c->skin.b); glPushMatrix(); glTranslatef(0,1.7f,0); glutSolidSphere(0.25,16,16); glPopMatrix();
    glColor3f(c->skin.r*0.8f,c->skin.g*0.8f,c->skin.b*0.8f); glPushMatrix(); glTranslatef(0,1.55f,0); glRotatef(90,1,0,0); gluCylinder(gluNewQuadric(),0.1,0.1,0.15,8,1); glPopMatrix();
    glColor3f(c->outfit.r,c->outfit.g,c->outfit.b); glPushMatrix(); glTranslatef(0,1.05f,0); glRotatef(90,1,0,0); gluCylinder(gluNewQuadric(),0.3,0.35,0.9,16,1); glPopMatrix();
    glColor3f(c->outfit.r*0.9f,c->outfit.g*0.9f,c->outfit.b*0.9f);
    glPushMatrix(); glTranslatef(-0.35f,1.45f,0); glRotatef(90,0,1,0); glRotatef(-30+walk*20,0,0,1); gluCylinder(gluNewQuadric(),0.08,0.08,0.7,8,1); glPopMatrix();
    glPushMatrix(); glTranslatef(0.35f,1.45f,0); glRotatef(90,0,1,0); glRotatef(30-walk*20,0,0,1); gluCylinder(gluNewQuadric(),0.08,0.08,0.7,8,1); glPopMatrix();
    glColor3f(c->outfit.r,c->outfit.g,c->outfit.b);
    glPushMatrix(); glTranslatef(-0.15f,0.45f,0); glRotatef(90,1,0,0); glRotatef(walk*20,0,0,1); gluCylinder(gluNewQuadric(),0.12,0.12,0.8,8,1); glPopMatrix();
    glPushMatrix(); glTranslatef(0.15f,0.45f,0); glRotatef(90,1,0,0); glRotatef(-walk*20,0,0,1); gluCylinder(gluNewQuadric(),0.12,0.12,0.8,8,1); glPopMatrix();
    glColor3f(0.2f,0.2f,0.2f);
    glPushMatrix(); glTranslatef(-0.15f,0.1f,0.1f); glScalef(0.3f,0.1f,0.4f); glutSolidCube(1.0); glPopMatrix();
    glPushMatrix(); glTranslatef(0.15f,0.1f,0.1f); glScalef(0.3f,0.1f,0.4f); glutSolidCube(1.0); glPopMatrix();
    if(local){ glColor3f(0.2f,0.2f,0.2f); glPushMatrix(); glTranslatef(0.25f,1.2f,0.1f); glRotatef(90,0,1,0); gluCylinder(gluNewQuadric(),0.03,0.03,0.5,8,1); glTranslatef(0,0,0.5f); glScalef(0.08f,0.08f,0.2f); glutSolidCube(1.0); glPopMatrix(); }
    glPopMatrix();
}

static void player_move(float fwd,float right,float mx,float my) {
    Player *p=&gs.local_player; const CharacterDef *c=&characters_db[p->character_id];
    p->yaw+=mx*0.2f; p->pitch-=my*0.2f;
    if(p->pitch>89.0f)p->pitch=89.0f; if(p->pitch<-89.0f)p->pitch=-89.0f;
    vec3 forward={sinf(p->yaw*DEG2RAD),0,-cosf(p->yaw*DEG2RAD)};
    vec3 sideways={cosf(p->yaw*DEG2RAD),0,sinf(p->yaw*DEG2RAD)};
    vec3 move={0,0,0};
    if(fwd!=0.0f)move=vec3_add(move,vec3_scale(forward,fwd*c->speed*delta_time));
    if(right!=0.0f)move=vec3_add(move,vec3_scale(sideways,right*c->speed*delta_time));
    if(vec3_len(move)>0.01f){ p->anim_time+=delta_time; if(rand()%20==0) sfx_footstep(); }
    p->pos=vec3_add(p->pos,move);
    p->pos.x=fmaxf(-MAP_HALF,fminf(MAP_HALF,p->pos.x));
    p->pos.z=fmaxf(-MAP_HALF,fminf(MAP_HALF,p->pos.z));
}

static void fire_weapon() {
    Player *p=&gs.local_player; const GunDef *g=&guns_db[p->current_gun];
    if(p->ammo[p->current_gun]<=0&&g->mag_size>0)return;
    if(g->mag_size>0)p->ammo[p->current_gun]--;
    sfx_gun(p->current_gun);
    // local hit detection disabled when online – server handles it
}

static void place_gloo() {
    if(gs.gloo_count>=MAX_GLOO_WALLS)return;
    Player *p=&gs.local_player; GlooWall *gw=&gs.gloo_walls[gs.gloo_count++];
    vec3 dir={sinf(p->yaw*DEG2RAD),0,-cosf(p->yaw*DEG2RAD)};
    gw->pos=vec3_add(p->pos,vec3_scale(dir,2.0f)); gw->yaw=p->yaw; gw->hp=500;
    gw->owner_id=p->id; gw->spawn_time=gs.match_time; gw->active=1; sfx_gloo();
}

static void use_emote(int idx){ if(idx<0||idx>=MAX_EMOTES)return; printf("Emote: %s\n",emote_names[idx]); sfx_emote(); }

static void update_match(float dt){
    gs.match_time+=dt;
    for(int i=0;i<gs.gloo_count;i++){
        if(gs.gloo_walls[i].active){ gs.gloo_walls[i].hp-=20.0f*dt; if(gs.gloo_walls[i].hp<=0)gs.gloo_walls[i].active=0; }
    }
    if(!gs.local_player.alive)gs.running=0;
}

static void draw_logo() {
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0,1280,720,0,-1,1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity(); glDisable(GL_DEPTH_TEST);
    glColor3f(0.01f,0.25f,0.06f); glBegin(GL_QUADS); glVertex2f(0,0); glVertex2f(1280,0); glVertex2f(1280,720); glVertex2f(0,720); glEnd();
    glColor3f(0.83f,0.69f,0.22f);
    int x=120,y=250,w=18,gap=6,h=80;
    #define BAR(x,y,w,h) glBegin(GL_QUADS); glVertex2f(x,y); glVertex2f(x+w,y); glVertex2f(x+w,y+h); glVertex2f(x,y+h); glEnd()
    BAR(x,y,w,h); BAR(x+w+gap,y,w*2,w); BAR(x+w+gap,y+h/2-w,w*2,w); BAR(x+w*3+gap*2,y,w,h/2); x+=w*4+gap*4;
    BAR(x,y,w,h); BAR(x+w+gap,y,w*2,w); BAR(x+w*3+gap*2,y,w,h); BAR(x+w+gap,y+h/2,w*2,w); x+=w*4+gap*4;
    BAR(x,y,w,h); for(int i=0;i<h/2;i+=6){glBegin(GL_LINES);glVertex2f(x+w,y+h/2-i);glVertex2f(x+w*3,y+i);glEnd();} for(int i=0;i<h/2;i+=6){glBegin(GL_LINES);glVertex2f(x+w,y+h/2+i);glVertex2f(x+w*3,y+h-i);glEnd();} x+=w*4+gap*2;
    BAR(x,y,w,h); BAR(x+w+gap,y,w*2,w); BAR(x+w+gap,y+h-w,w*2,w); BAR(x+w*3+gap*2,y+w,w,h-w*2); x+=w*4+gap*4;
    BAR(x,y,w,h); BAR(x+w+gap,y,w*3,w); BAR(x+w+gap,y+h/2-w,w*2,w); BAR(x+w+gap,y+h-w,w*3,w); x+=w*4+gap*4;
    BAR(x,y,w,h); BAR(x+w+gap,y,w*3,w); BAR(x+w+gap,y+h/2-w,w*2,w); x+=w*4+gap*4;
    BAR(x,y,w,h); BAR(x+w+gap,y,w*3,w); BAR(x+w+gap,y+h/2-w,w*2,w); BAR(x+w+gap,y+h-w,w*3,w); x+=w*4+gap*4;
    BAR(x,y,w,h); BAR(x+w*3+gap,y,w,h); for(int i=0;i<h;i+=6){glBegin(GL_LINES);glVertex2f(x+w,y+i);glVertex2f(x+w*3,y+h-i);glEnd();} x+=w*4+gap*4;
    BAR(x,y,w,h); BAR(x+w+gap,y,w*2,w); BAR(x+w+gap,y+h-w,w*2,w); BAR(x+w*3+gap*2,y+w,w,h-w*2); x+=w*4+gap*4;
    BAR(x,y,w,h); BAR(x+w+gap,y,w*3,w); BAR(x+w+gap,y+h/2-w,w*2,w); BAR(x+w+gap,y+h-w,w*3,w); x+=w*4+gap*4;
    BAR(x,y,w,h); BAR(x+w+gap,y,w*2,w); BAR(x+w+gap,y+h/2-w,w*2,w); BAR(x+w*3+gap*2,y,w,h/2); for(int i=0;i<h/2;i+=6){glBegin(GL_LINES);glVertex2f(x+w*2,y+h/2+i);glVertex2f(x+w*3,y+h-i);glEnd();} x+=w*4+gap*4;
    BAR(x,y,w*3,w); BAR(x+w*2,y,w,h); BAR(x,y+h/2,w*3,w); BAR(x,y+h-w,w*3,w); x+=w*4+gap*4;
    BAR(x,y,w,h); BAR(x+w+gap,y,w*2,w); BAR(x+w+gap,y+h-w,w*2,w); BAR(x+w*3+gap*2,y+w,w,h-w*2);
    glColor3f(1,1,1); glLineWidth(2); glBegin(GL_LINES); glVertex2f(350,550); glVertex2f(930,550); glEnd();
    glBegin(GL_LINES); glVertex2f(450,580); glVertex2f(830,580); glEnd();
    glEnable(GL_DEPTH_TEST);
}

static void render_frame() {
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    if(!gs.running){ draw_logo(); return; }
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); gluPerspective(60.0,1280.0/720.0,0.1,1000.0);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    Player *p=&gs.local_player; vec3 eye=vec3_add(p->pos,(vec3){0,1.7f,0});
    vec3 look_dir={sinf(p->yaw*DEG2RAD)*cosf(p->pitch*DEG2RAD),-sinf(p->pitch*DEG2RAD),-cosf(p->yaw*DEG2RAD)*cosf(p->pitch*DEG2RAD)};
    gluLookAt(eye.x,eye.y,eye.z, eye.x+look_dir.x,eye.y+look_dir.y,eye.z+look_dir.z, 0,1,0);
    glColor3f(0.2f,0.5f,0.2f); glBegin(GL_QUADS); glVertex3f(-MAP_HALF,0,MAP_HALF); glVertex3f(MAP_HALF,0,MAP_HALF); glVertex3f(MAP_HALF,0,-MAP_HALF); glVertex3f(-MAP_HALF,0,-MAP_HALF); glEnd();
    for(int i=0;i<num_buildings;i++) draw_building(&city[i]);
    for(int i=0;i<gs.player_count;i++){ if(!gs.players[i].alive||i==p->id)continue; draw_character(&gs.players[i],0); }
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    for(int i=0;i<gs.gloo_count;i++){ if(!gs.gloo_walls[i].active)continue; GlooWall *w=&gs.gloo_walls[i]; glPushMatrix(); glTranslatef(w->pos.x,w->pos.y,w->pos.z); glRotatef(w->yaw,0,1,0); glColor4f(0.2f,0.8f,1.0f,0.5f); glBegin(GL_QUADS); glVertex3f(-1,0,0); glVertex3f(1,0,0); glVertex3f(1,2,0); glVertex3f(-1,2,0); glEnd(); glPopMatrix(); }
    glDisable(GL_BLEND);
    // HUD
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0,1280,720,0,-1,1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity(); glDisable(GL_DEPTH_TEST);
    glColor3f(1,1,1); glBegin(GL_LINES); glVertex2f(635,360); glVertex2f(645,360); glVertex2f(640,355); glVertex2f(640,365); glEnd();
    glColor3f(1,0,0); glBegin(GL_QUADS); glVertex2f(30,30); glVertex2f(30+p->health*2,30); glVertex2f(30+p->health*2,50); glVertex2f(30,50); glEnd();
    float ammo_pct = (guns_db[p->current_gun].mag_size>0)?p->ammo[p->current_gun]/30.0f:1.0f;
    glColor3f(0.8f,0.8f,0.8f); glBegin(GL_QUADS); glVertex2f(30,680); glVertex2f(30+200*ammo_pct,680); glVertex2f(30+200*ammo_pct,700); glVertex2f(30,700); glEnd();
    glColor3f(1.0f,0.84f,0.0f); glBegin(GL_QUADS); glVertex2f(30,60); glVertex2f(80,60); glVertex2f(80,75); glVertex2f(30,75); glEnd();
    // Redeem button
    float bx=1080,by=20,bw=160,bh=40;
    glColor3f(0.2f,0.6f,0.2f); glBegin(GL_QUADS); glVertex2f(bx,by); glVertex2f(bx+bw,by); glVertex2f(bx+bw,by+bh); glVertex2f(bx,by+bh); glEnd();
    glColor3f(1,1,1); glBegin(GL_LINES); glVertex2f(bx+30,by+20); glVertex2f(bx+130,by+20); glEnd();
    glEnable(GL_DEPTH_TEST);
}

static void lobby() {
    printf("=== PAKDEFENDER3d - Developed by Urban Studios Official ===\n");
    for(int i=0;i<4;i++) printf("%d. %s\n",i+1,map_names[i]);
    int choice=1; gs.map=choice-1; generate_map(gs.map);
    gs.local_player.alive=1; gs.local_player.health=100; gs.local_player.pos=(vec3){0,0,0}; gs.running=1; gs.match_time=0;
    gs.player_count=0; // filled by server
    printf("Map %s loaded. Press B to buy diamonds, R to redeem.\n",map_names[gs.map]);
}

int main(int argc, char *argv[]) {
    init_game();
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO)!=0) return 1;
    init_audio();
    window=SDL_CreateWindow("Pakdefender3d - by Urban Studios Official",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,1280,720,SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE);
    if(!window){SDL_Quit();return 1;}
    gl_context=SDL_GL_CreateContext(window); SDL_GL_SetSwapInterval(1);
    glEnable(GL_DEPTH_TEST); glClearColor(0.2f,0.3f,0.4f,1.0f);
    network_init(argc>1 ? argv[1] : NULL);
    lobby();
    Uint32 last_time=SDL_GetTicks();
    while(app_running){
        Uint32 now=SDL_GetTicks(); delta_time=(now-last_time)/1000.0f; last_time=now;
        SDL_Event e; while(SDL_PollEvent(&e)){
            if(e.type==SDL_QUIT)app_running=0;
            if(show_redeem){
                if(e.type==SDL_MOUSEBUTTONDOWN) redeem_handle_click((float)e.button.x, (float)e.button.y);
                if(e.type==SDL_KEYDOWN){
                    if(e.key.keysym.sym==SDLK_ESCAPE) show_redeem=0;
                    else if(e.key.keysym.sym>=SDLK_0&&e.key.keysym.sym<=SDLK_9){ if(redeem_len<30){ redeem_text[redeem_len++]='0'+(e.key.keysym.sym-SDLK_0); redeem_text[redeem_len]='\0'; } }
                    else if(e.key.keysym.sym==SDLK_BACKSPACE){ if(redeem_len>0) redeem_text[--redeem_len]='\0'; }
                    else if(e.key.keysym.sym==SDLK_RETURN){ if(strlen(redeem_text)==8){ gs.local_player.diamonds+=100; redeem_success=1; redeem_success_time=0.0f; } show_redeem=0; }
                }
                continue;
            }
            if(gs.running){
                if(e.type==SDL_MOUSEBUTTONDOWN){
                    int mx=e.button.x, my=e.button.y;
                    float bx=1080,by=20,bw=160,bh=40;
                    if(point_in_rect((float)mx,(float)my,bx,by,bw,bh)){ show_redeem=1; redeem_len=0; redeem_text[0]='\0'; redeem_success=0; }
                    else{
                        if(e.button.button==SDL_BUTTON_LEFT){ fire_weapon(); network_send_fire(); }
                        if(e.button.button==SDL_BUTTON_RIGHT) place_gloo();
                    }
                }
                if(e.type==SDL_KEYDOWN){
                    if(e.key.keysym.sym==SDLK_ESCAPE) gs.running=0;
                    if(e.key.keysym.sym>=SDLK_1&&e.key.keysym.sym<=SDLK_5) use_emote(e.key.keysym.sym-SDLK_1);
                    if(e.key.keysym.sym==SDLK_b) buy_diamonds(100);
                    if(e.key.keysym.sym==SDLK_r){ show_redeem=1; redeem_len=0; redeem_text[0]='\0'; redeem_success=0; }
                }
            }
        }
        if(gs.running){
            const Uint8 *keystate=SDL_GetKeyboardState(NULL);
            float fwd=(keystate[SDL_SCANCODE_W]?1:0)-(keystate[SDL_SCANCODE_S]?1:0);
            float rgt=(keystate[SDL_SCANCODE_D]?1:0)-(keystate[SDL_SCANCODE_A]?1:0);
            int mx=0,my=0; SDL_GetRelativeMouseState(&mx,&my);
            network_send_input(fwd,rgt,(float)mx,(float)my);
            player_move(fwd,rgt,(float)mx,(float)my);
            update_match(delta_time);
        }
        network_receive();
        update_bgm(delta_time);
        render_frame();
        if(show_redeem) render_redeem_screen();
        SDL_GL_SwapWindow(window);
    }
    if(audio_dev)SDL_CloseAudioDevice(audio_dev);
    SDL_GL_DeleteContext(gl_context); SDL_DestroyWindow(window); SDL_Quit();
    close(network_sock);
    return 0;
}
