#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>

#pragma comment(lib, "ws2_32.lib")
#define server_port 8888


DWORD WINAPI dinleThread(LPVOID sock_ptr) {
    SOCKET sock = *(SOCKET *)sock_ptr;
    char buffer[1024];
    int len;

    while ((len = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[len] = '\0';
        printf("\n%s\n", buffer);
        fflush(stdout);
    }

    printf("\nBaglanti kesildi.\n");
    return 0;
}

int main() {
    WSADATA wsa;
    SOCKET sock;
    struct sockaddr_in server;
    char mesaj[1024];
    char userName[32];
    const char *server_ip = "127.0.0.1";
    

    printf("Connected to chat server at %s:%d\n", server_ip, server_port);

    printf("Commands: \n");
    printf("- Type '@nickname message' for private messages\n");
    printf("- Type any other message for broadcast\n");
    printf("- Type 'quit' to exit \n");
    printf("------------------------------- \n");

    while (1) {
        printf("Enter your nickname: ");
        fgets(userName, sizeof(userName), stdin);
        userName[strcspn(userName, "\n")] = 0; 

        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            printf("WSAStartup error.\n");
            return 1;
        }

        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == INVALID_SOCKET) {
            printf("Socket failed.\n");
            WSACleanup();
            return 1;
        }

        server.sin_family = AF_INET;
        server.sin_port = htons(server_port);
        server.sin_addr.s_addr = inet_addr(server_ip);

        if (connect(sock, (struct sockaddr *)&server, sizeof(server)) == SOCKET_ERROR) {
            printf("Baglanti failed: %d\n", WSAGetLastError());
            closesocket(sock);
            WSACleanup();
            return 1;
        }


        send(sock, userName, strlen(userName), 0);

        
        char server_msg[1024];
        int len = recv(sock, server_msg, sizeof(server_msg) - 1, 0);
        if (len <= 0) {
            printf("Server disconnected  or error.\n");
            closesocket(sock);
            WSACleanup();
            continue;
        }
        server_msg[len] = '\0';

        if (strcmp(server_msg, "Nickname already taken or invalid. Please choose another: ") == 0) {
            printf("%s", server_msg);
            closesocket(sock);
            WSACleanup();
            continue; 
        } else {

            HANDLE hThread = CreateThread(NULL, 0, dinleThread, &sock, 0, NULL);
            if (hThread == NULL) {
                printf("Thread error !.\n");
                closesocket(sock);
                WSACleanup();
                return 1;
            }
            CloseHandle(hThread);
            
            break; 
        }
    }

    while (1) {
        fgets(mesaj, sizeof(mesaj), stdin);
        mesaj[strcspn(mesaj, "\n")] = 0;
        if (strlen(mesaj) == 0) continue;
        send(sock, mesaj, strlen(mesaj), 0);
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}
