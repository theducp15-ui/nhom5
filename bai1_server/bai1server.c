/*
 * tcp_server.c - BAI 1: Chat rieng giua hai client qua server (Winsock)
 * Ten file phai co duoi .c (khong phai .cpp).
 * Nen luu file o dang UTF-8 with BOM hoac de comment khong dau
 * de tranh canh bao C4819 cua Visual Studio.
 */

#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <string.h>

#pragma comment(lib, "Ws2_32.lib")
#define PORT 9000

#define MSG_WAITING "*** Dang cho nguoi thu hai...\n"
#define MSG_START   "*** Bat dau chat.\n"
#define MSG_LEFT    "*** Nguoi kia da roi cuoc tro chuyen\n"

/* Gui dung "length" byte (send() co the chi gui duoc mot phan) */
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

/* Gui mot chuoi ket thuc bang '\0' */
static int send_text(SOCKET sock, const char* text)
{
    return send_all(sock, text, (int)strlen(text));
}

/* Vong lap chuyen tin: dung select() cho ca hai socket */
static void chat_loop(SOCKET c0, SOCKET c1)
{
    SOCKET clients[2] = { c0, c1 };
    fd_set readfds;
    char buf[1024];

    for (;;) {
        FD_ZERO(&readfds);
        FD_SET(clients[0], &readfds);
        FD_SET(clients[1], &readfds);

        if (select(0, &readfds, NULL, NULL, NULL) == SOCKET_ERROR) {
            printf("Loi select(): %d\n", WSAGetLastError());
            return;
        }

        for (int i = 0; i < 2; i++) {
            if (!FD_ISSET(clients[i], &readfds))
                continue;

            int n = recv(clients[i], buf, sizeof(buf), 0);

            /* n <= 0: client dong ket noi / tat ngang / loi mang */
            if (n <= 0) {
                send_text(clients[1 - i], MSG_LEFT);
                return;
            }

            /* kiem tra lenh /quit (bo \r \n o cuoi) */
            int len = n;
            while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
                len--;

            if (len == 5 && memcmp(buf, "/quit", 5) == 0) {
                send_text(clients[1 - i], MSG_LEFT);
                return;
            }

            /* chuyen nguyen n byte sang client con lai */
            if (!send_all(clients[1 - i], buf, n)) {
                /* ben kia da mat ket noi -> bao lai cho ben gui */
                send_text(clients[i], MSG_LEFT);
                return;
            }
        }
    }
}

int main(void)
{
    WSADATA wsa;
    SOCKET listener;
    struct sockaddr_in addr;
    SOCKET clients[2] = { INVALID_SOCKET, INVALID_SOCKET };

    /* 1. Khoi dong Winsock */
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("Loi WSAStartup\n");
        return 1;
    }

    /* 2. Tao socket TCP */
    listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET) {
        printf("Loi socket(): %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    /* 3. Dia chi server: moi card mang, cong PORT */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);

    /* 4. bind */
    if (bind(listener, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        printf("Loi bind(): %d (cong %d co the dang bi dung)\n", WSAGetLastError(), PORT);
        closesocket(listener);
        WSACleanup();
        return 1;
    }

    /* 5. listen */
    if (listen(listener, SOMAXCONN) == SOCKET_ERROR) {
        printf("Loi listen(): %d\n", WSAGetLastError());
        closesocket(listener);
        WSACleanup();
        return 1;
    }
    printf("Server dang lang nghe o cong %d\n", PORT);

    /* ===== Cau a: cho du hai client ===== */
    printf("Dang cho client thu nhat ket noi...\n");
    clients[0] = accept(listener, NULL, NULL);
    if (clients[0] == INVALID_SOCKET) {
        printf("Loi accept(): %d\n", WSAGetLastError());
        closesocket(listener);
        WSACleanup();
        return 1;
    }
    printf("Client thu nhat da ket noi.\n");

    if (!send_text(clients[0], MSG_WAITING)) {
        printf("Khong gui duoc thong bao cho client thu nhat.\n");
        closesocket(clients[0]);
        closesocket(listener);
        WSACleanup();
        return 1;
    }

    printf("Dang cho client thu hai ket noi...\n");
    clients[1] = accept(listener, NULL, NULL);
    if (clients[1] == INVALID_SOCKET) {
        printf("Loi accept client thu hai: %d\n", WSAGetLastError());
        closesocket(clients[0]);
        closesocket(listener);
        WSACleanup();
        return 1;
    }
    printf("Client thu hai da ket noi. Da du hai nguoi.\n");

    for (int i = 0; i < 2; i++) {
        if (!send_text(clients[i], MSG_START)) {
            printf("Khong gui duoc thong bao bat dau cho client %d.\n", i + 1);
            closesocket(clients[0]);
            closesocket(clients[1]);
            closesocket(listener);
            WSACleanup();
            return 1;
        }
    }
    printf("Da gui thong bao bat dau cho ca hai client.\n");

    /* ===== Cau b + c: chuyen tin, xu ly /quit va tat ngang ===== */
    chat_loop(clients[0], clients[1]);

    /* Don dep: shutdown de du lieu cuoi (thong bao roi phong) duoc gui het */
    printf("Phien chat ket thuc.\n");
    shutdown(clients[0], SD_SEND);
    shutdown(clients[1], SD_SEND);
    closesocket(clients[0]);
    closesocket(clients[1]);
    closesocket(listener);
    WSACleanup();
    return 0;
}