/*
 * tcp_client.c - CLIENT dung chung cho Bai 1 va Bai 2 (Winsock)
 * - Luong chinh: doc ban phim (fgets) va gui di.
 * - Luong phu (CreateThread): chi nhan du lieu tu server va in ra man hinh.
 * Ten file phai co duoi .c (khong phai .cpp).
 */

#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "Ws2_32.lib")
#define PORT 9000

static SOCKET g_sock = INVALID_SOCKET;
static volatile int g_quitting = 0;   /* =1 khi nguoi dung chu dong thoat */

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

/* Luong nhan tin: in moi thu server gui ve */
static DWORD WINAPI recv_thread(LPVOID param)
{
    char buf[1024];
    int n;
    (void)param;

    for (;;) {
        n = recv(g_sock, buf, sizeof(buf), 0);
        if (n <= 0)
            break;
        fwrite(buf, 1, (size_t)n, stdout);
        fflush(stdout);
    }

    if (!g_quitting) {
        printf("\n*** Mat ket noi voi server.\n");
        fflush(stdout);
        exit(0);
    }
    return 0;
}

int main(void)
{
    WSADATA wsa;
    struct sockaddr_in addr;
    char ip[64];
    char line[1030];
    HANDLE hThread;

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("Loi WSAStartup\n");
        return 1;
    }

    /* Nhap IP server */
    printf("Nhap IP server (Enter = 127.0.0.1): ");
    if (fgets(ip, sizeof(ip), stdin) == NULL) {
        WSACleanup();
        return 1;
    }
    ip[strcspn(ip, "\r\n")] = '\0';
    if (ip[0] == '\0')
        strcpy(ip, "127.0.0.1");

    g_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_sock == INVALID_SOCKET) {
        printf("Loi socket(): %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
        printf("Dia chi IP khong hop le: %s\n", ip);
        closesocket(g_sock);
        WSACleanup();
        return 1;
    }

    if (connect(g_sock, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        printf("Khong ket noi duoc toi %s:%d (loi %d)\n", ip, PORT, WSAGetLastError());
        closesocket(g_sock);
        WSACleanup();
        return 1;
    }
    printf("Da ket noi toi server %s:%d. Go /quit de thoat.\n", ip, PORT);

    /* Tao luong chi de nhan tin */
    hThread = CreateThread(NULL, 0, recv_thread, NULL, 0, NULL);
    if (hThread == NULL) {
        printf("Loi CreateThread: %lu\n", GetLastError());
        closesocket(g_sock);
        WSACleanup();
        return 1;
    }
    CloseHandle(hThread);

    /* Luong chinh: doc ban phim va gui */
    while (fgets(line, 1024, stdin) != NULL) {
        int len = (int)strlen(line);

        /* bo \r \n o cuoi de so sanh /quit */
        int t = len;
        while (t > 0 && (line[t - 1] == '\n' || line[t - 1] == '\r'))
            t--;

        /* moi tin nhan la mot dong, ket thuc bang '\n' */
        line[t] = '\n';
        line[t + 1] = '\0';

        if (!send_all(g_sock, line, t + 1))
            break;

        if (t == 5 && strncmp(line, "/quit", 5) == 0)
            break;
    }

    g_quitting = 1;
    shutdown(g_sock, SD_SEND);
    Sleep(200);                 /* cho du lieu cuoi duoc gui di */
    closesocket(g_sock);
    WSACleanup();
    return 0;
}
