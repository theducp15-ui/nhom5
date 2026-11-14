/*
 * tcp_server.c - CODE GỐC: KHỞI TẠO SOCKET TCP PHÍA SERVER
 * Môn Mạng máy tính - Visual Studio trên Windows (Winsock)
 * LƯU Ý: tên file phải có đuôi .c (KHÔNG phải .cpp).
 */

#define _CRT_SECURE_NO_WARNINGS        /* tắt cảnh báo C4996 của Visual Studio */
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <string.h>
#include <winsock2.h>

#pragma comment(lib, "Ws2_32.lib")     /* tự liên kết thư viện Winsock */
#define PORT 9000

 /* Gui du toan bo noi dung chuoi */
int send_text(SOCKET sock, const char* text)
{
    int length = (int)strlen(text);
    int total = 0;

    while (total < length) {
        int n = send(sock, text + total, length - total, 0);

        if (n == SOCKET_ERROR || n == 0) {
            return 0;
        }

        total += n;
    }

    return 1;
}
void chat_loop(SOCKET c0, SOCKET c1)
{
    SOCKET clients[2] = { c0, c1 };
    fd_set readfds;
    char buf[1024];

    for (;;) {
        FD_ZERO(&readfds);
        FD_SET(clients[0], &readfds);
        FD_SET(clients[1], &readfds);

        if (select(0, &readfds, NULL, NULL, NULL) == SOCKET_ERROR)
            return;

        for (int i = 0; i < 2; i++) {
            if (!FD_ISSET(clients[i], &readfds))
                continue;

            int n = recv(clients[i], buf, sizeof(buf) - 1, 0);

            /* kiem tra /quit (bo \r \n o cuoi) */
            int len = n;
            while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
                len--;
            int is_quit = (n > 0 && len == 5 && strncmp(buf, "/quit", 5) == 0);

            /* n <= 0: tat ngang / loi;  is_quit: go /quit */
            if (n <= 0 || is_quit) {
                send_text(clients[1 - i], "*** Nguoi kia da roi cuoc tro chuyen\n");
                return;
            }

            buf[n] = '\0';
            send_text(clients[1 - i], buf);
        }
    }
}

int main(void)
{
    WSADATA wsa;
    SOCKET listener;
    struct sockaddr_in addr;
    SOCKET clients[2] = { INVALID_SOCKET, INVALID_SOCKET };

    /* 1. Khởi động Winsock (bắt buộc trên Windows, gọi một lần đầu chương trình) */
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("Loi WSAStartup\n");
        return 1;
    }

    /* 2. Tạo socket TCP: AF_INET = IPv4, SOCK_STREAM = TCP */
    listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET) {
        printf("Loi socket(): %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    /* 3. Địa chỉ của server: mọi card mạng của máy (INADDR_ANY), cổng PORT */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);   /* htons: đổi sang thứ tự byte của mạng */

    /* 4. Gắn socket vào địa chỉ trên */
    if (bind(listener, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        printf("Loi bind(): %d (cong %d co the dang bi dung)\n", WSAGetLastError(), PORT);
        closesocket(listener);
        WSACleanup();
        return 1;
    }

    /* 5. Chuyển socket sang trạng thái lắng nghe kết nối */
    listen(listener, SOMAXCONN);
    printf("Server dang lang nghe o cong %d\n", PORT);
    printf("Dang cho client thu nhat ket noi...\n");

    clients[0] = accept(listener, NULL, NULL);

    if (clients[0] == INVALID_SOCKET) {
        printf("Loi accept(): %d\n", WSAGetLastError());
        closesocket(listener);
        WSACleanup();
        return 1;
    }

    printf("Client thu nhat da ket noi.\n");
    const char waiting[] = "*** Dang cho nguoi thu hai...\n";

    if (!send_text(clients[0], waiting)) {
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
    const char start_message[] = "*** Bat dau chat.\n";

    for (int i = 0; i < 2; i++) {
        if (!send_text(clients[i], start_message)) {
            printf("Khong gui duoc thong bao bat dau cho client %d.\n", i + 1);
            closesocket(clients[0]);
            closesocket(clients[1]);
            closesocket(listener);
            WSACleanup();
            return 1;
        }
    }

    printf("Da gui thong bao bat dau cho ca hai client.\n");

    /* ==========================================
       CODE CÂU B (Vòng lặp select)
       ========================================== */
    fd_set readfds;
    char buf[1024];
    int ret;

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(clients[0], &readfds);
        FD_SET(clients[1], &readfds);

        ret = select(0, &readfds, NULL, NULL, NULL);
        if (ret == SOCKET_ERROR) {
            printf("Loi select()\n");
            break;
        }

        /* Kiểm tra Client 1 gửi tin -> chuyển cho Client 2 */
        if (FD_ISSET(clients[0], &readfds)) {
            ret = recv(clients[0], buf, sizeof(buf) - 1, 0);
            if (ret <= 0) break;

            buf[ret] = '\0';
            send_text(clients[1], buf);
        }

        /* Kiểm tra Client 2 gửi tin -> chuyển cho Client 1 */
        if (FD_ISSET(clients[1], &readfds)) {
            ret = recv(clients[1], buf, sizeof(buf) - 1, 0);
            if (ret <= 0) break;

            buf[ret] = '\0';
            send_text(clients[0], buf);
        }
    }

    /* Don dep truoc khi thoat */
    
    chat_loop(clients[0], clients[1]);
    closesocket(clients[0]);
    closesocket(clients[1]);
    closesocket(listener);
    WSACleanup();
    return 0;
}