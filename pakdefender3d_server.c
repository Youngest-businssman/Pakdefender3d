/*
 * Pakdefender3d – Dedicated Server (UDP)
 * Compile: gcc -O2 pakdefender3d_server.c -o pakdefender3d_server.exe -lws2_32 -lm
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdint.h>

#ifdef _WIN32
  #include <winsock2.h>
  #pragma comment(lib, "ws2_32.lib")
  typedef int socklen_t;
  #define close closesocket
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #define SOCKET int
  #define INVALID_SOCKET -1
  #define SOCKET_ERROR -1
#endif

#define PORT 7777
#define MAX_PLAYERS 60
#define MAP_HALF 250.0f
#define TICK_RATE 30
#define TICK_TIME (1.0f / TICK_RATE)

typedef struct {
    float x, y, z;
} vec3;

typedef struct {
    int id;
    vec3 pos;
    float yaw, pitch;
    float health;
    int current_gun;
    int alive;
    struct sockaddr_in addr;
    int addr_len;
} Player;

static Player players[MAX_PLAYERS];
static int player_count = 0;
static SOCKET server_sock;

static void init_network() {
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);
#endif
    server_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_sock == INVALID_SOCKET) {
        printf("Socket creation failed\n");
        exit(1);
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(server_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        printf("Bind failed\n");
        exit(1);
    }
    // set non-blocking
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(server_sock, FIONBIO, &mode);
#else
    int flags = fcntl(server_sock, F_GETFL, 0);
    fcntl(server_sock, F_SETFL, flags | O_NONBLOCK);
#endif
    printf("Pakdefender3d Server listening on UDP port %d\n", PORT);
}

static int find_player(struct sockaddr_in *addr) {
    for (int i = 0; i < player_count; i++) {
        if (players[i].addr.sin_addr.s_addr == addr->sin_addr.s_addr &&
            players[i].addr.sin_port == addr->sin_port)
            return i;
    }
    return -1;
}

static void broadcast_state() {
    char buf[2048];
    int offset = 0;
    int count = player_count;
    memcpy(buf + offset, &count, sizeof(int)); offset += sizeof(int);
    for (int i = 0; i < player_count; i++) {
        memcpy(buf + offset, &players[i].id, sizeof(int)); offset += sizeof(int);
        memcpy(buf + offset, &players[i].pos.x, sizeof(float)); offset += sizeof(float);
        memcpy(buf + offset, &players[i].pos.y, sizeof(float)); offset += sizeof(float);
        memcpy(buf + offset, &players[i].pos.z, sizeof(float)); offset += sizeof(float);
        memcpy(buf + offset, &players[i].yaw, sizeof(float)); offset += sizeof(float);
        memcpy(buf + offset, &players[i].health, sizeof(float)); offset += sizeof(float);
        memcpy(buf + offset, &players[i].alive, sizeof(int)); offset += sizeof(int);
        memcpy(buf + offset, &players[i].current_gun, sizeof(int)); offset += sizeof(int);
        float pitch = players[i].pitch;
        memcpy(buf + offset, &pitch, sizeof(float)); offset += sizeof(float);
    }
    for (int i = 0; i < player_count; i++) {
        sendto(server_sock, buf, offset, 0, (struct sockaddr*)&players[i].addr, players[i].addr_len);
    }
}

static void handle_packet(char *data, int len, struct sockaddr_in *sender, socklen_t sender_len) {
    if (len < 1) return;
    uint8_t type = data[0];
    int idx = find_player(sender);

    if (type == 0) { // JOIN
        if (idx == -1 && player_count < MAX_PLAYERS) {
            idx = player_count++;
            players[idx].id = idx;
            players[idx].pos.x = 0;
            players[idx].pos.y = 1.7f;
            players[idx].pos.z = 0;
            players[idx].yaw = 0;
            players[idx].pitch = 0;
            players[idx].health = 100;
            players[idx].alive = 1;
            players[idx].current_gun = 13; // AK-47
            players[idx].addr = *sender;
            players[idx].addr_len = sender_len;
            printf("Player %d joined\n", idx);
        }
    } else if (type == 1 && idx != -1 && players[idx].alive) { // INPUT
        float fwd, rgt, mx, my;
        if (len < 1 + sizeof(float)*4) return;
        memcpy(&fwd, data+1, sizeof(float));
        memcpy(&rgt, data+5, sizeof(float));
        memcpy(&mx, data+9, sizeof(float));
        memcpy(&my, data+13, sizeof(float));

        players[idx].yaw += mx * 0.2f;
        players[idx].pitch -= my * 0.2f;
        if (players[idx].pitch > 89) players[idx].pitch = 89;
        if (players[idx].pitch < -89) players[idx].pitch = -89;

        float speed = 5.0f * TICK_TIME;
        float rad = players[idx].yaw * (3.14159f/180.0f);
        players[idx].pos.x += fwd * speed * sinf(rad);
        players[idx].pos.z -= fwd * speed * cosf(rad);
        players[idx].pos.x += rgt * speed * cosf(rad);
        players[idx].pos.z += rgt * speed * sinf(rad);

        if (players[idx].pos.x < -MAP_HALF) players[idx].pos.x = -MAP_HALF;
        if (players[idx].pos.x > MAP_HALF) players[idx].pos.x = MAP_HALF;
        if (players[idx].pos.z < -MAP_HALF) players[idx].pos.z = -MAP_HALF;
        if (players[idx].pos.z > MAP_HALF) players[idx].pos.z = MAP_HALF;

    } else if (type == 2 && idx != -1 && players[idx].alive) { // FIRE
        for (int t = 0; t < player_count; t++) {
            if (t == idx || !players[t].alive) continue;
            float dx = players[t].pos.x - players[idx].pos.x;
            float dz = players[t].pos.z - players[idx].pos.z;
            float dist = sqrtf(dx*dx + dz*dz);
            if (dist < 80.0f) {
                float angle_to_target = atan2f(dz, dx) * 180.0f/3.14159f;
                float diff = fabsf(players[idx].yaw - angle_to_target);
                if (diff > 180) diff = 360 - diff;
                if (diff < 5.0f) {
                    players[t].health -= 36.0f;
                    printf("Hit! Player %d -> %d, health %.0f\n", idx, t, players[t].health);
                    if (players[t].health <= 0) {
                        players[t].alive = 0;
                        printf("Player %d died\n", t);
                    }
                    break;
                }
            }
        }
    }
}

int main() {
    init_network();
    while (1) {
        char buf[1024];
        struct sockaddr_in sender;
        socklen_t sender_len = sizeof(sender);
        int recv_len = recvfrom(server_sock, buf, sizeof(buf), 0, (struct sockaddr*)&sender, &sender_len);
        if (recv_len > 0) {
            handle_packet(buf, recv_len, &sender, sender_len);
        }
        static double last_tick = 0;
        double now = (double)clock() / CLOCKS_PER_SEC;
        if (now - last_tick >= TICK_TIME) {
            broadcast_state();
            last_tick = now;
        }
    }
    close(server_sock);
    return 0;
}
