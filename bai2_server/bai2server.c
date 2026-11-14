/*
 * bai2_server.c - BAI 2: Phong chat chung toi da 10 client (Winsock)
 * Ten file phai co duoi .c (khong phai .cpp).
 *
 * - Dung select() de cho listener va tat ca client cung luc.
 * - Client thu 11 bi bao "*** Phong da day" roi ngat ket noi.
 * - Vao phong phai nhap ten; tin hien o client khac dang "[Ten] noi dung".
 * - Server bao ca phong khi co nguoi vao / roi phong.
 * - Client thoat (/quit) hoac tat ngang: server khong dung, chi xoa client do.
 */

#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <string.h>

#pragma comment(lib, "Ws2_32.lib")
#define PORT        9000
#define MAX_CLIENTS 10
#define NAME_LEN    32
#define LINE_LEN    1024

typedef struct {
    SOCKET sock;               /* INVALID_SOCKET = o trong */
    char   name[NAME_LEN];
    int    has_name;           /* da nhap ten chua */
    char   buf[LINE_LEN];      /* bo dem ghep dong (TCP la dong byte) */
    int    len;
} Client;

static Client clients[MAX_CLIENTS];

/* Gui dung "length" byte */
static int send_all(SOCKET sock, const char* data, int length)
{
    int total = 0;

    while (total < length) {
        int n = send(sock, data + total, length - total, 0);
        if (n == SOCKET_ERROR || n == 0)
            return 0;
        total += n;
    }
    return 1;
}

static int send_text(SOCKET sock, const char* text)
{
    return send_all(sock, text, (int)strlen(text));
}

/* Gui cho moi nguoi da co ten, tru client thu "except" (-1 = gui tat ca) */
static void broadcast(const char* text, int except)
{
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (i == except) continue;
        if (clients[i].sock == INVALID_SOCKET || !clients[i].has_name) continue;
        send_text(clients[i].sock, text);
    }
}

/* Xoa client khoi danh sach va bao cho ca phong */
static void remove_client(int i)
{
    char msg[NAME_LEN + 64];
    int had_name = clients[i].has_name;

    closesocket(clients[i].sock);
    clients[i].sock = INVALID_SOCKET;
    clients[i].has_name = 0;
    clients[i].len = 0;

    if (had_name) {
        sprintf(msg, "*** %s da roi phong\n", clients[i].name);
        printf("%s", msg);
        broadcast(msg, -1);
    }
    else {
        printf("Mot client chua nhap ten da ngat ket noi.\n");
    }
}

static int name_in_use(const char* name)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
        if (clients[i].sock != INVALID_SOCKET && clients[i].has_name &&
            strcmp(clients[i].name, name) == 0)
            return 1;
    return 0;
}

/* Xu ly mot dong day du. Tra ve 0 neu client can bi xoa (/quit). */
static int handle_line(int i, const char* line)
{
    Client* c = &clients[i];
    char msg[LINE_LEN + NAME_LEN + 16];

    if (strcmp(line, "/quit") == 0)
        return 0;

    /* Chua co ten: dong dau tien chinh la ten */
    if (!c->has_name) {
        if (line[0] == '\0') {
            send_text(c->sock, "Nhap ten cua ban: ");
            return 1;
        }
        if (name_in_use(line)) {
            send_text(c->sock, "*** Ten da co nguoi dung. Nhap ten khac: ");
            return 1;
        }
        strncpy(c->name, line, NAME_LEN - 1);
        c->name[NAME_LEN - 1] = '\0';
        c->has_name = 1;

        sprintf(msg, "*** Chao %s! Ban da vao phong. Go /quit de thoat.\n", c->name);
        send_text(c->sock, msg);

        sprintf(msg, "*** %s vua vao phong\n", c->name);
        printf("%s", msg);
        broadcast(msg, i);
        return 1;
    }

    /* Tin nhan thuong: gui cho tat ca nguoi con lai */
    if (line[0] == '\0')
        return 1;
    sprintf(msg, "[%s] %s\n", c->name, line);
    broadcast(msg, i);
    return 1;
}

/* Nhan "n" byte moi tu client i, tach thanh tung dong va xu ly.
   Tra ve 0 neu client da bi xoa. */
static int feed_data(int i, const char* data, int n)
{
    Client* c = &clients[i];

    for (int k = 0; k < n; k++) {
        char ch = data[k];

        if (ch != '\n' && c->len < LINE_LEN - 1) {
            c->buf[c->len++] = ch;
            continue;
        }

        /* gap '\n' (hoac dong qua dai): cat \r cuoi roi xu ly */
        while (c->len > 0 && c->buf[c->len - 1] == '\r')
            c->len--;
        c->buf[c->len] = '\0';
        c->len = 0;

        if (!handle_line(i, c->buf)) {
            remove_client(i);
            return 0;
        }
        if (ch != '\n')            /* dong qua dai: giu lai ky tu hien tai */
            c->buf[c->len++] = ch;
    }
    return 1;
}

int main(void)
{
    WSADATA wsa;
    SOCKET listener;
    struct sockaddr_in addr;
    fd_set readfds;
    char buf[1024];

    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].sock = INVALID_SOCKET;
        clients[i].has_name = 0;
        clients[i].len = 0;
    }

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("Loi WSAStartup\n");
        return 1;
    }

    listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET) {
        printf("Loi socket(): %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);

    if (bind(listener, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        printf("Loi bind(): %d (cong %d co the dang bi dung)\n", WSAGetLastError(), PORT);
        closesocket(listener);
        WSACleanup();
        return 1;
    }

    if (listen(listener, SOMAXCONN) == SOCKET_ERROR) {
        printf("Loi listen(): %d\n", WSAGetLastError());
        closesocket(listener);
        WSACleanup();
        return 1;
    }
    printf("Phong chat dang lang nghe o cong %d (toi da %d client)\n", PORT, MAX_CLIENTS);

    for (;;) {
        FD_ZERO(&readfds);
        FD_SET(listener, &readfds);
        for (int i = 0; i < MAX_CLIENTS; i++)
            if (clients[i].sock != INVALID_SOCKET)
                FD_SET(clients[i].sock, &readfds);

        if (select(0, &readfds, NULL, NULL, NULL) == SOCKET_ERROR) {
            printf("Loi select(): %d\n", WSAGetLastError());
            break;
        }

        /* 1. Co client moi ket noi */
        if (FD_ISSET(listener, &readfds)) {
            SOCKET s = accept(listener, NULL, NULL);
            if (s != INVALID_SOCKET) {
                int slot = -1;
                for (int i = 0; i < MAX_CLIENTS; i++)
                    if (clients[i].sock == INVALID_SOCKET) { slot = i; break; }

                if (slot < 0) {
                    send_text(s, "*** Phong da day\n");
                    shutdown(s, SD_SEND);
                    closesocket(s);
                    printf("Tu choi mot client: phong da day.\n");
                }
                else {
                    clients[slot].sock = s;
                    clients[slot].has_name = 0;
                    clients[slot].len = 0;
                    clients[slot].name[0] = '\0';
                    send_text(s, "Nhap ten cua ban: ");
                    printf("Client moi ket noi (slot %d).\n", slot);
                }
            }
        }

        /* 2. Co client gui du lieu / ngat ket noi */
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].sock == INVALID_SOCKET) continue;
            if (!FD_ISSET(clients[i].sock, &readfds)) continue;

            int n = recv(clients[i].sock, buf, sizeof(buf), 0);
            if (n <= 0) {                 /* tat ngang hoac loi mang */
                remove_client(i);
                continue;
            }
            feed_data(i, buf, n);
        }
    }

    for (int i = 0; i < MAX_CLIENTS; i++)
        if (clients[i].sock != INVALID_SOCKET)
            closesocket(clients[i].sock);
    closesocket(listener);
    WSACleanup();
    return 0;
}
