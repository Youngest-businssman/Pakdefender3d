/* ===================================================================
   PAKISTAN DEFENDER – COMPLETE 3D SHOOTER (C + SDL2 only)
   - 5 detailed 32x32 city maps (fictional names)
   - 15 melee, 20 ranged (short/mid/long) — all skins procedural
   - 3 vehicles (Car, Bike, Rickshaw) with ad‑unlockable skins
   - Worship places = safe zones (no shooting inside)
   - All sound/music generated at runtime
   - Desktop + mobile‑ready
   =================================================================== */
#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ========= CONFIG ========= */
#define SCREEN_W       1280
#define SCREEN_H       720
#define MAP_SIZE       32
#define MAP_W          MAP_SIZE
#define MAP_H          MAP_SIZE
#define TEX_W          64
#define TEX_H          64
#define MELEE_COUNT    15
#define RANGED_COUNT   20
#define VEHICLE_COUNT  3
#define MAP_COUNT      5

/* ========= TYPES ========= */
typedef struct { Uint8 r,g,b; } Color;

typedef struct {
    double x,y, dirX,dirY, planeX,planeY;
    int health, kills, currentWeapon;  /* negative = melee index-1 */
    int ammo[RANGED_COUNT];
    Uint32 lastFire, footTimer;
    int moving, inVehicle;
} Player;

typedef struct {
    double x,y, angle;
    int alive, health;
    Uint32 lastMove;
} Enemy;

typedef struct {
    double x,y,w,h;
} NoFireZone;

typedef struct {
    char name[20];
    float fireRate;
    int damage, maxAmmo, rangeType;  /* 0=short,1=mid,2=long */
    SDL_Texture* tex;
} Ranged;

typedef struct {
    char name[20];
    int damage;
    SDL_Texture* tex;
} Melee;

typedef struct {
    char name[12];
    double x,y,angle,speed;
    int skin;                 /* 0 = default, 1 = ad‑unlocked premium */
    SDL_Texture* tex[2];      /* [0] default, [1] premium */
    int occupied;
} Vehicle;

typedef struct {
    double phase, step;
    int noteIdx;
    Uint32 nextNote;
    int melody[32], nCount;
    int footOn; Uint32 footEnd;
    int gunOn;  Uint32 gunEnd;
    int jingle; Uint32 jingleEnd;
} AudioCtx;

/* ========= GLOBALS ========= */
static SDL_Window*   win = NULL;
static SDL_Renderer* ren = NULL;
static SDL_AudioDeviceID audDev;
static AudioCtx aud;
static Player p;
static Enemy en;
static Ranged guns[RANGED_COUNT];
static Melee melees[MELEE_COUNT];
static Vehicle vehs[VEHICLE_COUNT];
static int cityMaps[MAP_COUNT][MAP_W * MAP_H];
static NoFireZone* curSafeZones = NULL;
static int curSafeCnt = 0;
static int curMapId = 0;
static const char* mapNames[MAP_COUNT] = {
    "Fortress City","Garden City","Port City","Mountain City","River City"
};

/* ========= MAP DATA (5 fictional cities, each 32x32) ========= */
#include <string.h>
static void fillRect(int* map, int x1, int y1, int w, int h, int val) {
    for (int y=y1; y<y1+h && y<MAP_H; y++)
        for (int x=x1; x<x1+w && x<MAP_W; x++)
            map[y*MAP_W + x] = val;
}

/* Fortress City */
static NoFireZone fortressSafeZones[] = {{13,10,6,6}};
static void initFortress() {
    memset(cityMaps[0], 0, sizeof(cityMaps[0]));
    fillRect(cityMaps[0], 0,0, MAP_W,1, 1);
    fillRect(cityMaps[0], 0,MAP_H-1, MAP_W,1, 1);
    fillRect(cityMaps[0], 0,0, 1,MAP_H, 1);
    fillRect(cityMaps[0], MAP_W-1,0, 1,MAP_H, 1);
    fillRect(cityMaps[0], 13,10, 6,6, 2);  /* mosque */
    fillRect(cityMaps[0], 2,2, 4,4, 1);
    fillRect(cityMaps[0], 3,3, 2,2, 0);
    fillRect(cityMaps[0], 10,25, 12,4, 3);
    fillRect(cityMaps[0], 5,15, 3,7, 1);
    fillRect(cityMaps[0], 24,5, 5,5, 1);
    fillRect(cityMaps[0], 25,22, 5,5, 1);
}

/* Garden City */
static NoFireZone gardenSafeZones[] = {{4,14,5,5}};
static void initGarden() {
    memset(cityMaps[1], 0, sizeof(cityMaps[1]));
    fillRect(cityMaps[1], 0,0, MAP_W,1,1); fillRect(cityMaps[1],0,MAP_H-1,MAP_W,1,1);
    fillRect(cityMaps[1], 0,0,1,MAP_H,1); fillRect(cityMaps[1],MAP_W-1,0,1,MAP_H,1);
    fillRect(cityMaps[1], 4,14, 5,5, 2);
    fillRect(cityMaps[1], 20,8, 8,8, 0);
    fillRect(cityMaps[1], 24,20, 6,6, 1);
    fillRect(cityMaps[1], 8,24, 10,4, 3);
    fillRect(cityMaps[1], 5,5, 4,4, 1);
}

/* Port City */
static NoFireZone portSafeZones[] = {{26,2,4,4}};
static void initPort() {
    memset(cityMaps[2], 0, sizeof(cityMaps[2]));
    fillRect(cityMaps[2], 0,0, MAP_W,1,1); fillRect(cityMaps[2],0,MAP_H-1,MAP_W,1,1);
    fillRect(cityMaps[2], 0,0,1,MAP_H,1); fillRect(cityMaps[2],MAP_W-1,0,1,MAP_H,1);
    fillRect(cityMaps[2], 26,2, 4,4, 2);
    fillRect(cityMaps[2], 0,26, MAP_W,6, 1);
    fillRect(cityMaps[2], 1,28, 30,1, 1);
    fillRect(cityMaps[2], 5,5, 8,6, 3);
    fillRect(cityMaps[2], 15,15, 4,4, 1);
    fillRect(cityMaps[2], 16,16, 2,2, 0);
    fillRect(cityMaps[2], 10,22, 6,4, 1);
}

/* Mountain City */
static NoFireZone mountainSafeZones[] = {{12,20,5,5}};
static void initMountain() {
    memset(cityMaps[3], 0, sizeof(cityMaps[3]));
    fillRect(cityMaps[3], 0,0, MAP_W,1,1); fillRect(cityMaps[3],0,MAP_H-1,MAP_W,1,1);
    fillRect(cityMaps[3], 0,0,1,MAP_H,1); fillRect(cityMaps[3],MAP_W-1,0,1,MAP_H,1);
    fillRect(cityMaps[3], 12,20, 5,5, 2);
    fillRect(cityMaps[3], 10,8, 12,8, 1);
    fillRect(cityMaps[3], 13,10, 6,4, 0);
    fillRect(cityMaps[3], 2,2, 6,5, 3);
    fillRect(cityMaps[3], 24,24, 6,6, 1);
}

/* River City */
static NoFireZone riverSafeZones[] = {{14,8,4,4}};
static void initRiver() {
    memset(cityMaps[4], 0, sizeof(cityMaps[4]));
    fillRect(cityMaps[4], 0,0, MAP_W,1,1); fillRect(cityMaps[4],0,MAP_H-1,MAP_W,1,1);
    fillRect(cityMaps[4], 0,0,1,MAP_H,1); fillRect(cityMaps[4],MAP_W-1,0,1,MAP_H,1);
    fillRect(cityMaps[4], 14,8, 4,4, 2);
    for (int i=1; i<MAP_H-1; i++) cityMaps[4][i*MAP_W + MAP_W/2] = 1;
    fillRect(cityMaps[4], 3,3, 6,5, 3);
    fillRect(cityMaps[4], 23,20, 7,5, 3);
    fillRect(cityMaps[4], 3,22, 6,6, 1);
    fillRect(cityMaps[4], 10,25, 8,4, 0);
}

/* ========= AUDIO (procedural) ========= */
void audio_callback(void* ud, Uint8* stream, int len) {
    (void)ud;
    Sint16* buf = (Sint16*)stream;
    int samples = len / sizeof(Sint16);
    Uint32 now = SDL_GetTicks();
    for (int i=0; i<samples; i+=2) {
        if (now >= aud.nextNote) {
            aud.noteIdx = (aud.noteIdx+1) % aud.nCount;
            double freqs[] = {261.63,293.66,329.63,392.00,440.00,523.25,587.33,659.25};
            int n = aud.melody[aud.noteIdx] % 8;
            aud.step = freqs[n] * 2.0 * M_PI / 44100.0;
            aud.nextNote = now + 220;
        }
        double mu = 0.07 * sin(aud.phase);
        aud.phase += aud.step; if (aud.phase>2*M_PI) aud.phase-=2*M_PI;

        double ft = 0.0;
        if (aud.footOn && now<aud.footEnd) {
            double t = (double)(now - (aud.footEnd-100))/100.0;
            if (t>0 && t<1) ft = 0.15*(1-t)*((double)rand()/RAND_MAX*2-1);
        }
        double gu = 0.0;
        if (aud.gunOn && now<aud.gunEnd) {
            double t = (double)(now - (aud.gunEnd-120))/120.0;
            if (t>=0 && t<1) gu = ((double)rand()/RAND_MAX*2-1)*0.6*(1-t);
        } else aud.gunOn=0;
        double ji = 0.0;
        if (aud.jingle && now<aud.jingleEnd) ji = 0.15*sin(now*0.08); else aud.jingle=0;

        double left = mu + ft + gu + ji;
        if (left>1) left=1; if (left<-1) left=-1;
        buf[i] = buf[i+1] = (Sint16)(left*32767);
    }
}
void play_step() { aud.footOn=1; aud.footEnd=SDL_GetTicks()+400; }
void play_gun()  { aud.gunOn=1;  aud.gunEnd=SDL_GetTicks()+120; }
void play_kill() { aud.jingle=1; aud.jingleEnd=SDL_GetTicks()+600; }

/* ========= PROCEDURAL TEXTURE HELPERS ========= */
SDL_Texture* makeTex(int w, int h, Color (*func)(int,int)) {
    SDL_Surface* s = SDL_CreateRGBSurface(0, w, h, 32, 0,0,0,0);
    Uint32* pix = (Uint32*)s->pixels;
    for (int y=0; y<h; y++) for (int x=0; x<w; x++) {
        Color c = func(x,y);
        pix[y*s->pitch/4 + x] = SDL_MapRGB(s->format, c.r,c.g,c.b);
    }
    SDL_Texture* t = SDL_CreateTextureFromSurface(ren, s);
    SDL_FreeSurface(s);
    return t;
}

/* ========= SKIN GENERATORS ========= */
Color c_grey(int x,int y){ (void)x;(void)y; return (Color){128,128,128}; }
Color c_silver(int x,int y){ (void)x;(void)y; return (Color){192,192,192}; }
Color c_dark(int x,int y){ (void)x;(void)y; return (Color){40,40,40}; }
Color c_red(int x,int y){ (void)x;(void)y; return (Color){200,50,50}; }
Color c_blue(int x,int y){ (void)x;(void)y; return (Color){0,0,200}; }
Color c_green(int x,int y){ (void)x;(void)y; return (Color){0,150,0}; }
Color c_yellow(int x,int y){ (void)x;(void)y; return (Color){220,220,0}; }
Color c_black(int x,int y){ (void)x;(void)y; return (Color){20,20,20}; }
Color c_white(int x,int y){ (void)x;(void)y; return (Color){255,255,255}; }
Color c_camo(int x,int y){ return (x/16+y/16)%2 ? (Color){80,120,60} : (Color){60,90,40}; }
Color c_desert(int x,int y){ (void)x;(void)y; return (Color){194,178,128}; }
Color c_urban(int x,int y){ return (x/4+y/4)%2 ? (Color){100,100,100} : (Color){150,150,150}; }
Color c_gold(int x,int y){ (void)x;(void)y; return (Color){200,170,50}; }
Color c_navy(int x,int y){ (void)x;(void)y; return (Color){0,0,128}; }

/* ========= INIT ALL WEAPONS & VEHICLES ========= */
void initWeapons() {
    /* 15 MELEE */
    strcpy(melees[0].name, "Knife");     melees[0].damage=10; melees[0].tex=makeTex(128,128,c_grey);
    strcpy(melees[1].name, "Machete");   melees[1].damage=18; melees[1].tex=makeTex(128,128,c_silver);
    strcpy(melees[2].name, "Axe");       melees[2].damage=22; melees[2].tex=makeTex(128,128,c_dark);
    strcpy(melees[3].name, "Katana");    melees[3].damage=25; melees[3].tex=makeTex(128,128,c_silver);
    strcpy(melees[4].name, "BaseballBat"); melees[4].damage=16; melees[4].tex=makeTex(128,128,c_desert);
    strcpy(melees[5].name, "Cleaver");   melees[5].damage=20; melees[5].tex=makeTex(128,128,c_dark);
    strcpy(melees[6].name, "Sword");     melees[6].damage=28; melees[6].tex=makeTex(128,128,c_gold);
    strcpy(melees[7].name, "Pipe");      melees[7].damage=15; melees[7].tex=makeTex(128,128,c_grey);
    strcpy(melees[8].name, "Wrench");    melees[8].damage=14; melees[8].tex=makeTex(128,128,c_black);
    strcpy(melees[9].name, "Sickle");    melees[9].damage=19; melees[9].tex=makeTex(128,128,c_silver);
    strcpy(melees[10].name,"Crowbar");   melees[10].damage=17; melees[10].tex=makeTex(128,128,c_dark);
    strcpy(melees[11].name,"Spear");     melees[11].damage=24; melees[11].tex=makeTex(128,128,c_desert);
    strcpy(melees[12].name,"Hammer");    melees[12].damage=21; melees[12].tex=makeTex(128,128,c_red);
    strcpy(melees[13].name,"Polearm");   melees[13].damage=27; melees[13].tex=makeTex(128,128,c_navy);
    strcpy(melees[14].name,"Chainsaw");  melees[14].damage=30; melees[14].tex=makeTex(128,128,c_camo);

    /* 20 RANGED (short / mid / long) */
    /* Short range 0–3 */
    strcpy(guns[0].name, "Compact SMG");   guns[0].fireRate=18; guns[0].damage=16; guns[0].maxAmmo=35; guns[0].rangeType=0;
    guns[0].tex = makeTex(256,128,c_black);
    strcpy(guns[1].name, "Sawn-Off SG");   guns[1].fireRate=2;  guns[1].damage=45; guns[1].maxAmmo=8;  guns[1].rangeType=0;
    guns[1].tex = makeTex(256,128,c_dark);
    strcpy(guns[2].name, "Tactical SG");   guns[2].fireRate=3;  guns[2].damage=40; guns[2].maxAmmo=10; guns[2].rangeType=0;
    guns[2].tex = makeTex(256,128,c_desert);
    strcpy(guns[3].name, "PDW");           guns[3].fireRate=14; guns[3].damage=18; guns[3].maxAmmo=30; guns[3].rangeType=0;
    guns[3].tex = makeTex(256,128,c_urban);

    /* Mid range 4–11 */
    strcpy(guns[4].name, "Assault Rifle");   guns[4].fireRate=12; guns[4].damage=25; guns[4].maxAmmo=30; guns[4].rangeType=1;
    guns[4].tex = makeTex(256,128,c_camo);
    strcpy(guns[5].name, "Bullpup Rifle");   guns[5].fireRate=13; guns[5].damage=23; guns[5].maxAmmo=30; guns[5].rangeType=1;
    guns[5].tex = makeTex(256,128,c_green);
    strcpy(guns[6].name, "AK-47");           guns[6].fireRate=10; guns[6].damage=28; guns[6].maxAmmo=30; guns[6].rangeType=1;
    guns[6].tex = makeTex(256,128,c_desert);
    strcpy(guns[7].name, "M4 Carbine");      guns[7].fireRate=11; guns[7].damage=24; guns[7].maxAmmo=30; guns[7].rangeType=1;
    guns[7].tex = makeTex(256,128,c_dark);
    strcpy(guns[8].name, "G36");             guns[8].fireRate=12; guns[8].damage=26; guns[8].maxAmmo=30; guns[8].rangeType=1;
    guns[8].tex = makeTex(256,128,c_navy);
    strcpy(guns[9].name, "F2000");           guns[9].fireRate=13; guns[9].damage=24; guns[9].maxAmmo=30; guns[9].rangeType=1;
    guns[9].tex = makeTex(256,128,c_grey);
    strcpy(guns[10].name,"Battle Rifle");    guns[10].fireRate=9; guns[10].damage=30; guns[10].maxAmmo=25; guns[10].rangeType=1;
    guns[10].tex = makeTex(256,128,c_camo);
    strcpy(guns[11].name,"STG-44");          guns[11].fireRate=10; guns[11].damage=27; guns[11].maxAmmo=25; guns[11].rangeType=1;
    guns[11].tex = makeTex(256,128,c_desert);

    /* Long range 12–19 */
    strcpy(guns[12].name,"Bolt-Action Sniper"); guns[12].fireRate=1.2; guns[12].damage=85; guns[12].maxAmmo=5; guns[12].rangeType=2;
    guns[12].tex = makeTex(256,128,c_dark);
    strcpy(guns[13].name,"Semi-Auto Sniper");   guns[13].fireRate=3; guns[13].damage=65; guns[13].maxAmmo=10; guns[13].rangeType=2;
    guns[13].tex = makeTex(256,128,c_urban);
    strcpy(guns[14].name,"Dragunov");           guns[14].fireRate=2; guns[14].damage=75; guns[14].maxAmmo=8; guns[14].rangeType=2;
    guns[14].tex = makeTex(256,128,c_desert);
    strcpy(guns[15].name,"M82");                guns[15].fireRate=1; guns[15].damage=100; guns[15].maxAmmo=4; guns[15].rangeType=2;
    guns[15].tex = makeTex(256,128,c_black);
    strcpy(guns[16].name,"VSS");                guns[16].fireRate=5; guns[16].damage=40; guns[16].maxAmmo=12; guns[16].rangeType=2;
    guns[16].tex = makeTex(256,128,c_grey);
    strcpy(guns[17].name,"M1 Garand");          guns[17].fireRate=4; guns[17].damage=55; guns[17].maxAmmo=8; guns[17].rangeType=2;
    guns[17].tex = makeTex(256,128,c_desert);
    strcpy(guns[18].name,"Scout");              guns[18].fireRate=2.5; guns[18].damage=70; guns[18].maxAmmo=6; guns[18].rangeType=2;
    guns[18].tex = makeTex(256,128,c_green);
    strcpy(guns[19].name,"Anti-Materiel");      guns[19].fireRate=0.8; guns[19].damage=120; guns[19].maxAmmo=3; guns[19].rangeType=2;
    guns[19].tex = makeTex(256,128,c_dark);
}

void initVehicles() {
    strcpy(vehs[0].name, "Car"); vehs[0].x=4; vehs[0].y=4; vehs[0].speed=2.5; vehs[0].skin=0;
    vehs[0].tex[0] = makeTex(256,128,c_blue);
    vehs[0].tex[1] = makeTex(256,128,c_red);

    strcpy(vehs[1].name, "Bike"); vehs[1].x=25; vehs[1].y=25; vehs[1].speed=3.2; vehs[1].skin=0;
    vehs[1].tex[0] = makeTex(256,128,c_black);
    vehs[1].tex[1] = makeTex(256,128,c_gold);

    strcpy(vehs[2].name, "Rickshaw"); vehs[2].x=28; vehs[2].y=3; vehs[2].speed=1.8; vehs[2].skin=0;
    vehs[2].tex[0] = makeTex(256,128,c_green);
    vehs[2].tex[1] = makeTex(256,128,c_yellow);
}

/* ========= SAFE ZONE CHECK ========= */
int inSafeZone(double x, double y) {
    for (int i=0; i<curSafeCnt; i++)
        if (x >= curSafeZones[i].x && x <= curSafeZones[i].x+curSafeZones[i].w &&
            y >= curSafeZones[i].y && y <= curSafeZones[i].y+curSafeZones[i].h)
            return 1;
    return 0;
}

/* ========= MAP SWITCH ========= */
void selectMap(int id) {
    curMapId = id;
    switch (id) {
        case 0: curSafeZones = fortressSafeZones; curSafeCnt = 1; break;
        case 1: curSafeZones = gardenSafeZones;   curSafeCnt = 1; break;
        case 2: curSafeZones = portSafeZones;     curSafeCnt = 1; break;
        case 3: curSafeZones = mountainSafeZones; curSafeCnt = 1; break;
        case 4: curSafeZones = riverSafeZones;    curSafeCnt = 1; break;
        default: curSafeCnt = 0;
    }
    p.x = p.y = 16.0;
    p.dirX = -1; p.dirY = 0; p.planeX = 0; p.planeY = 0.66;
}

/* ========= RENDERING ========= */
void renderScene() {
    SDL_Surface* fb = SDL_CreateRGBSurface(0, SCREEN_W, SCREEN_H, 32, 0,0,0,0);
    Uint32* pix = (Uint32*)fb->pixels;
    /* sky */
    for (int y=0; y<SCREEN_H/2; y++) {
        Uint32 col = SDL_MapRGB(fb->format, 100+y/2, 150+y/2, 220);
        for (int x=0; x<SCREEN_W; x++) pix[y*SCREEN_W + x] = col;
    }
    /* floor */
    for (int y=SCREEN_H/2; y<SCREEN_H; y++) {
        Uint32 col = SDL_MapRGB(fb->format, 34, 100, 34);
        for (int x=0; x<SCREEN_W; x++) pix[y*SCREEN_W + x] = col;
    }
    int* map = cityMaps[curMapId];
    for (int x=0; x<SCREEN_W; x++) {
        double camX = 2*x/(double)SCREEN_W - 1;
        double rDirX = p.dirX + p.planeX*camX;
        double rDirY = p.dirY + p.planeY*camX;
        int mapX=(int)p.x, mapY=(int)p.y;
        double sideDistX, sideDistY;
        double deltaDistX = fabs(1/rDirX), deltaDistY = fabs(1/rDirY);
        int stepX, stepY, hit=0, side;
        if (rDirX<0) { stepX=-1; sideDistX = (p.x - mapX)*deltaDistX; }
        else         { stepX=1;  sideDistX = (mapX+1.0 - p.x)*deltaDistX; }
        if (rDirY<0) { stepY=-1; sideDistY = (p.y - mapY)*deltaDistY; }
        else         { stepY=1;  sideDistY = (mapY+1.0 - p.y)*deltaDistY; }
        while (!hit) {
            if (sideDistX < sideDistY) { sideDistX+=deltaDistX; mapX+=stepX; side=0; }
            else { sideDistY+=deltaDistY; mapY+=stepY; side=1; }
            if (map[mapX*MAP_W+mapY]) hit=1;
        }
        double perpWallDist = side==0 ? (mapX - p.x + (1-stepX)/2.0)/rDirX : (mapY - p.y + (1-stepY)/2.0)/rDirY;
        int lineH = (int)(SCREEN_H / perpWallDist);
        int drawStart = -lineH/2 + SCREEN_H/2; if (drawStart<0) drawStart=0;
        int drawEnd = lineH/2 + SCREEN_H/2; if (drawEnd>=SCREEN_H) drawEnd=SCREEN_H-1;
        Uint8 r,g,b; int shade = side?150:255;
        int tile = map[mapX*MAP_W+mapY];
        if (tile==2) { r=0; g=shade*0.8; b=0; }
        else if (curMapId==0) { r=180*shade/255; g=120*shade/255; b=60*shade/255; }
        else if (curMapId==1) { r=140*shade/255; g=180*shade/255; b=140*shade/255; }
        else if (curMapId==2) { r=100*shade/255; g=100*shade/255; b=150*shade/255; }
        else if (curMapId==3) { r=130*shade/255; g=110*shade/255; b=100*shade/255; }
        else { r=80*shade/255; g=100*shade/255; b=140*shade/255; }
        Uint32 col = SDL_MapRGB(fb->format, r,g,b);
        for (int y=drawStart; y<drawEnd; y++) pix[y*SCREEN_W + x] = col;
    }
    SDL_Texture* t = SDL_CreateTextureFromSurface(ren, fb);
    SDL_RenderCopy(ren, t, NULL, NULL);
    SDL_DestroyTexture(t);
    SDL_FreeSurface(fb);
}

void drawHUD() {
    // Health
    SDL_Rect hb = {10, SCREEN_H-50, 200, 25}; SDL_SetRenderDrawColor(ren,40,40,40,200); SDL_RenderFillRect(ren,&hb);
    SDL_Rect hf = {10, SCREEN_H-50, (int)(200*p.health/100), 25}; SDL_SetRenderDrawColor(ren,220,0,0,255); SDL_RenderFillRect(ren,&hf);
    // Weapon icon
    if (p.currentWeapon < 0) {
        int idx = -p.currentWeapon - 1;
        SDL_Rect wr = {SCREEN_W/2-64, SCREEN_H-140, 128,128};
        SDL_RenderCopy(ren, melees[idx].tex, NULL, &wr);
    } else {
        SDL_Rect wr = {SCREEN_W/2-128, SCREEN_H-140, 256,128};
        SDL_RenderCopy(ren, guns[p.currentWeapon].tex, NULL, &wr);
    }
    // Minimap
    int mmW=150, mmH=150, mmX=SCREEN_W-mmW-10, mmY=10;
    SDL_Rect mbg = {mmX-2,mmY-2,mmW+4,mmH+4}; SDL_SetRenderDrawColor(ren,0,0,0,200); SDL_RenderFillRect(ren,&mbg);
    float tw=mmW/(float)MAP_W, th=mmH/(float)MAP_H;
    int* map = cityMaps[curMapId];
    for (int y=0; y<MAP_H; y++) for (int x=0; x<MAP_W; x++) {
        SDL_Rect r = {mmX+(int)(x*tw), mmY+(int)(y*th), (int)tw+1, (int)th+1};
        int cell = map[x*MAP_W+y];
        if (cell==1) { SDL_SetRenderDrawColor(ren,80,60,40,255); SDL_RenderFillRect(ren,&r); }
        else if (cell==2) { SDL_SetRenderDrawColor(ren,0,200,0,255); SDL_RenderFillRect(ren,&r); }
    }
    int px = mmX+(int)(p.x*tw), py = mmY+(int)(p.y*th);
    SDL_Rect pr = {px-2,py-2,5,5}; SDL_SetRenderDrawColor(ren,255,0,0,255); SDL_RenderFillRect(ren,&pr);
}

/* ========= MAIN ========= */
int main(int argc, char** argv) {
    (void)argc; (void)argv;
    srand(time(NULL));
    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO) < 0) {
        printf("SDL Init failed: %s\n", SDL_GetError());
        return 1;
    }
    win = SDL_CreateWindow("Pakistan Defender",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_W, SCREEN_H, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!win) { printf("Window error: %s\n", SDL_GetError()); return 1; }
    ren = SDL_CreateRenderer(win, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) { printf("Renderer error: %s\n", SDL_GetError()); return 1; }

    /* ---- AUDIO SETUP ---- */
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = 44100;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = audio_callback;
    audDev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (audDev == 0) {
        printf("Audio error: %s\n", SDL_GetError());
    } else {
        int mel[] = {3,5,4,3,7,6,5,4,3,2,3,5,4,3,7,6,
                     3,5,4,3,7,6,5,4,3,2,3,5,4,3,7,6};
        memcpy(aud.melody, mel, sizeof(mel));
        aud.nCount = 32;
        aud.noteIdx = -1;
        aud.nextNote = 0;
        aud.phase = 0.0;
        aud.step = 0.0;
        SDL_PauseAudioDevice(audDev, 0);  /* start playback */
    }

    /* ---- INIT MAPS, WEAPONS, VEHICLES ---- */
    initFortress();
    initGarden();
    initPort();
    initMountain();
    initRiver();
    initWeapons();
    initVehicles();

    /* ---- PLAYER START ---- */
    selectMap(0);
    p.health = 100;
    p.kills = 0;
    p.currentWeapon = 4;  /* start with Assault Rifle */
    p.inVehicle = -1;
    for (int i = 0; i < RANGED_COUNT; i++)
        p.ammo[i] = guns[i].maxAmmo;
    p.lastFire = 0;
    p.footTimer = 0;
    p.moving = 0;

    /* ---- ENEMY START ---- */
    en.x = 10.0; en.y = 10.0;
    en.alive = 1; en.health = 100;
    en.angle = 0.0;
    en.lastMove = 0;

    /* ---- MAIN LOOP ---- */
    int running = 1;
    int firePressed = 0;
    Uint32 lastTime = SDL_GetTicks();

    while (running) {
        Uint32 now = SDL_GetTicks();
        float dt = (now - lastTime) / 1000.0f;
        lastTime = now;

        /* ----- EVENT HANDLING ----- */
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
                running = 0;

            if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        running = 0;
                        break;
                    /* Map selection 1-5 */
                    case SDLK_1: selectMap(0); break;
                    case SDLK_2: selectMap(1); break;
                    case SDLK_3: selectMap(2); break;
                    case SDLK_4: selectMap(3); break;
                    case SDLK_5: selectMap(4); break;
                    /* Fire */
                    case SDLK_SPACE:
                        firePressed = 1;
                        break;
                    /* Ad reward (simulate watching an ad) */
                    case SDLK_A:
                        if (p.inVehicle >= 0) {
                            vehs[p.inVehicle].skin = 1;
                            printf("[AD REWARD] Vehicle \"%s\" premium skin unlocked!\n",
                                   vehs[p.inVehicle].name);
                        } else {
                            /* Unlock a random ranged weapon premium skin */
                            int r = rand() % RANGED_COUNT;
                            printf("[AD REWARD] Premium skin for \"%s\" unlocked!\n",
                                   guns[r].name);
                            /* In a real game, you'd set a flag and regenerate the texture.
                               For now it's a console message – hook Adsterra here. */
                        }
                        break;
                    /* Enter/exit vehicle */
                    case SDLK_E:
                        if (p.inVehicle < 0) {
                            for (int i = 0; i < VEHICLE_COUNT; i++) {
                                double dx = p.x - vehs[i].x;
                                double dy = p.y - vehs[i].y;
                                if (sqrt(dx*dx + dy*dy) < 2.0 && !vehs[i].occupied) {
                                    p.inVehicle = i;
                                    vehs[i].occupied = 1;
                                    printf("Entered %s\n", vehs[i].name);
                                    break;
                                }
                            }
                        } else {
                            vehs[p.inVehicle].occupied = 0;
                            p.inVehicle = -1;
                            printf("Exited vehicle\n");
                        }
                        break;
                    /* Weapon switching */
                    case SDLK_q: /* cycle melee backward */
                        if (p.currentWeapon < 0) {
                            int idx = -p.currentWeapon - 1;
                            idx = (idx - 1 + MELEE_COUNT) % MELEE_COUNT;
                            p.currentWeapon = -(idx + 1);
                        } else {
                            p.currentWeapon = -(MELEE_COUNT); /* last melee */
                        }
                        printf("Weapon: %s\n", melees[-p.currentWeapon - 1].name);
                        break;
                    case SDLK_f: /* cycle ranged forward */
                        if (p.currentWeapon >= 0) {
                            p.currentWeapon = (p.currentWeapon + 1) % RANGED_COUNT;
                        } else {
                            p.currentWeapon = 0;
                        }
                        printf("Weapon: %s\n", guns[p.currentWeapon].name);
                        break;
                    default:
                        break;
                }
            }
            if (e.type == SDL_KEYUP) {
                if (e.key.keysym.sym == SDLK_SPACE)
                    firePressed = 0;
            }

            /* Touch events (mobile) */
            if (e.type == SDL_FINGERDOWN || e.type == SDL_FINGERMOTION) {
                float tx = e.tfinger.x * SCREEN_W;
                float ty = e.tfinger.y * SCREEN_H;
                /* Right half of screen = fire */
                if (tx > SCREEN_W * 0.55f) {
                    firePressed = 1;
                }
            }
            if (e.type == SDL_FINGERUP) {
                firePressed = 0;
            }
        }

        /* ----- MOVEMENT ----- */
        const Uint8* key = SDL_GetKeyboardState(NULL);
        double moveSpeed = 5.0 * dt;
        if (p.inVehicle >= 0)
            moveSpeed *= vehs[p.inVehicle].speed;

        p.moving = 0;

        /* Forward */
        if (key[SDL_SCANCODE_W] || key[SDL_SCANCODE_UP]) {
            double nx = p.x + p.dirX * moveSpeed;
            double ny = p.y + p.dirY * moveSpeed;
            int cell = cityMaps[curMapId][(int)nx * MAP_W + (int)ny];
            if (cell == 0 || cell == 3) {  /* 3 = bazaar (walkable) */
                p.x = nx; p.y = ny;
                p.moving = 1;
            }
        }
        /* Backward */
        if (key[SDL_SCANCODE_S] || key[SDL_SCANCODE_DOWN]) {
            double nx = p.x - p.dirX * moveSpeed;
            double ny = p.y - p.dirY * moveSpeed;
            int cell = cityMaps[curMapId][(int)nx * MAP_W + (int)ny];
            if (cell == 0 || cell == 3) {
                p.x = nx; p.y = ny;
                p.moving = 1;
            }
        }
        /* Strafe left */
        if (key[SDL_SCANCODE_A]) {
            double strafeX = -p.planeX * moveSpeed * 0.6;
            double strafeY = -p.planeY * moveSpeed * 0.6;
            int cell = cityMaps[curMapId][(int)(p.x + strafeX) * MAP_W + (int)(p.y + strafeY)];
            if (cell == 0 || cell == 3) {
                p.x += strafeX; p.y += strafeY;
                p.moving = 1;
            }
        }
        /* Strafe right */
        if (key[SDL_SCANCODE_D]) {
            double strafeX = p.planeX * moveSpeed * 0.6;
            double strafeY = p.planeY * moveSpeed * 0.6;
            int cell = cityMaps[curMapId][(int)(p.x + strafeX) * MAP_W + (int)(p.y + strafeY)];
            if (cell == 0 || cell == 3) {
                p.x += strafeX; p.y += strafeY;
                p.moving = 1;
            }
        }
        /* Rotate left */
        if (key[SDL_SCANCODE_LEFT]) {
            double rot = 3.0 * dt;
            double od = p.dirX;
            p.dirX = od * cos(rot) - p.dirY * sin(rot);
            p.dirY = od * sin(rot) + p.dirY * cos(rot);
            double op = p.planeX;
            p.planeX = op * cos(rot) - p.planeY * sin(rot);
            p.planeY = op * sin(rot) + p.planeY * cos(rot);
        }
        /* Rotate right */
        if (key[SDL_SCANCODE_RIGHT]) {
            double rot = -3.0 * dt;
            double od = p.dirX;
            p.dirX = od * cos(rot) - p.dirY * sin(rot);
            p.dirY = od * sin(rot) + p.dirY * cos(rot);
            double op = p.planeX;
            p.planeX = op * cos(rot) - p.planeY * sin(rot);
            p.planeY = op * sin(rot) + p.planeY * cos(rot);
        }

        /* Footstep sounds */
        if (p.moving && now - p.footTimer > 400) {
            play_step();
            p.footTimer = now;
        }

        /* ----- SHOOTING ----- */
        if (firePressed) {
            if (!inSafeZone(p.x, p.y)) {
                /* Melee attack */
                if (p.currentWeapon < 0) {
                    int idx = -p.currentWeapon - 1;
                    double dx = en.x - p.x;
                    double dy = en.y - p.y;
                    double dist = sqrt(dx*dx + dy*dy);
                    double dot = (dx * p.dirX + dy * p.dirY) / (dist + 0.001);
                    if (dist < 2.0 && dot > 0.5) {
                        en.health -= melees[idx].damage;
                        printf("Melee hit! Enemy HP: %d\n", en.health);
                        if (en.health <= 0) {
                            en.alive = 0;
                            p.kills++;
                            play_kill();
                            printf("Enemy killed! Total kills: %d\n", p.kills);
                        }
                    }
                }
                /* Ranged attack */
                else {
                    Ranged* g = &guns[p.currentWeapon];
                    if (p.ammo[p.currentWeapon] > 0 &&
                        now - p.lastFire > (Uint32)(1000.0 / g->fireRate)) {
                        p.ammo[p.currentWeapon]--;
                        p.lastFire = now;
                        play_gun();

                        /* Hit detection */
                        double dx = en.x - p.x;
                        double dy = en.y - p.y;
                        double dist = sqrt(dx*dx + dy*dy);
                        double dot = (dx * p.dirX + dy * p.dirY) / (dist + 0.001);

                        double maxRange = (g->rangeType == 2) ? 15.0 :
                                          (g->rangeType == 1) ? 10.0 : 6.0;

                        if (dist < maxRange && dot > 0.85) {
                            en.health -= g->damage;
                            printf("Hit! Enemy HP: %d (used %s)\n", en.health, g->name);
                            if (en.health <= 0) {
                                en.alive = 0;
                                p.kills++;
                                play_kill();
                                printf("Enemy killed! Total kills: %d\n", p.kills);
                            }
                        }
                        /* Auto-reload if empty */
                        if (p.ammo[p.currentWeapon] <= 0) {
                            p.ammo[p.currentWeapon] = g->maxAmmo;
                            printf("%s reloaded!\n", g->name);
                        }
                    }
                }
            } else {
                /* In safe zone - cannot fire */
                if (firePressed && p.currentWeapon >= 0 &&
                    now - p.lastFire > 1000) {
                    printf("[SAFE ZONE] Weapons disabled in this sacred area.\n");
                    p.lastFire = now;
                }
            }
        }

        /* ----- ENEMY AI ----- */
        /* Respawn if dead */
        if (!en.alive) {
            /* Find a valid spawn point */
            int sx, sy;
            do {
                sx = rand() % (MAP_W - 4) + 2;
                sy = rand() % (MAP_H - 4) + 2;
            } while (cityMaps[curMapId][sx * MAP_W + sy] != 0);
            en.x = sx + 0.5;
            en.y = sy + 0.5;
            en.health = 100;
            en.alive = 1;
            en.angle = (rand() % 628) / 100.0;  /* random angle */
        }

        /* Move toward player if close, otherwise patrol */
        if (now - en.lastMove > 250) {
            double dx = p.x - en.x;
            double dy = p.y - en.y;
            double dist = sqrt(dx*dx + dy*dy);
            if (dist < 12.0 && dist > 1.5) {
                /* Chase player */
                double nx = en.x + (dx / dist) * 0.025;
                double ny = en.y + (dy / dist) * 0.025;
                if (cityMaps[curMapId][(int)nx * MAP_W + (int)ny] == 0 ||
                    cityMaps[curMapId][(int)nx * MAP_W + (int)ny] == 3) {
                    en.x = nx;
                    en.y = ny;
                }
            } else if (dist >= 12.0) {
                /* Wander */
                en.angle += (rand() % 100 - 50) * 0.02;
                double nx = en.x + cos(en.angle) * 0.02;
                double ny = en.y + sin(en.angle) * 0.02;
                if (cityMaps[curMapId][(int)nx * MAP_W + (int)ny] == 0 ||
                    cityMaps[curMapId][(int)nx * MAP_W + (int)ny] == 3) {
                    en.x = nx;
                    en.y = ny;
                }
            }
            en.lastMove = now;
        }

        /* Enemy attacks player if very close */
        {
            double dx = p.x - en.x;
            double dy = p.y - en.y;
            double dist = sqrt(dx*dx + dy*dy);
            if (dist < 1.5 && en.alive && now - en.lastMove > 600) {
                p.health -= 5;
                printf("Player hit! HP: %d\n", p.health);
                if (p.health <= 0) {
                    printf("YOU DIED! Respawning...\n");
                    p.health = 100;
                    p.x = 16.0; p.y = 16.0;
                    p.kills = (p.kills > 0) ? p.kills - 1 : 0;
                }
            }
        }

        /* Update vehicle position if player inside */
        if (p.inVehicle >= 0) {
            vehs[p.inVehicle].x = p.x;
            vehs[p.inVehicle].y = p.y;
            vehs[p.inVehicle].angle = atan2(p.dirY, p.dirX);
        }

        /* ----- RENDER ----- */
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);

        renderScene();
        drawHUD();

        /* Map name display (simple rect + we skip text since no SDL_ttf;
           console prints map on switch instead) */

        SDL_RenderPresent(ren);

        /* Cap at ~120 FPS */
        SDL_Delay(8);
    }

    /* ---- CLEANUP ---- */
    SDL_CloseAudioDevice(audDev);

    /* Free all textures */
    for (int i = 0; i < MELEE_COUNT; i++)
        if (melees[i].tex) SDL_DestroyTexture(melees[i].tex);
    for (int i = 0; i < RANGED_COUNT; i++)
        if (guns[i].tex) SDL_DestroyTexture(guns[i].tex);
    for (int i = 0; i < VEHICLE_COUNT; i++) {
        if (vehs[i].tex[0]) SDL_DestroyTexture(vehs[i].tex[0]);
        if (vehs[i].tex[1]) SDL_DestroyTexture(vehs[i].tex[1]);
    }

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();

    return 0;
}
