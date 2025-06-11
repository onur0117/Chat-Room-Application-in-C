#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

#pragma comment(lib, "ws2_32.lib")



typedef struct {
    SOCKET socket;
    char username[32];
    int isActive;
} Client;

#define MAX_CLIENTS 50
#define port 8888
Client clients[MAX_CLIENTS];
int client_sayisi = 0;
CRITICAL_SECTION lock;  

//write console and file at the same time
void write_log(const char *textString, ...) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    char timeText[64];
    strftime(timeText, sizeof(timeText), "[%a %b %d %H:%M:%S %Y]", t);

    FILE *fp = fopen("SERVER_LOGS.log", "a");
  

    printf("%s ", timeText);

    va_list args;
    va_start(args, textString);

    vprintf(textString, args);

    va_end(args);
    va_start(args, textString);

    fprintf(fp, "%s ", timeText);
    vfprintf(fp, textString, args);
    fflush(fp);

    va_end(args);
//8888
    fclose(fp);
}

void broadcast(const char *gonderen, const char *mesaj, const char *target) {
    EnterCriticalSection(&lock);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].isActive && clients[i].socket != INVALID_SOCKET) {
            char tam_mesaj[1024];
            if (target != NULL) {
                if (strcmp(clients[i].username, target) == 0) {
                    snprintf(tam_mesaj, sizeof(tam_mesaj), "[private from %s]: %s", gonderen, mesaj);
                    send(clients[i].socket, tam_mesaj, strlen(tam_mesaj), 0);
                    break; // send it only to target user 
                }
            } else {

                 if (strcmp(clients[i].username, gonderen) == 0)
                {
                    continue; //  do not show my own message to me
                }

                snprintf(tam_mesaj, sizeof(tam_mesaj), "[%s]: %s", gonderen, mesaj);
                send(clients[i].socket, tam_mesaj, strlen(tam_mesaj), 0);
            }
        }
    }

    LeaveCriticalSection(&lock);
}

void broadcast_system_message(const char *mesaj) {
    EnterCriticalSection(&lock);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].isActive && clients[i].socket != INVALID_SOCKET) {
            send(clients[i].socket, mesaj, strlen(mesaj), 0);
        }
    }
    LeaveCriticalSection(&lock);
}

DWORD WINAPI clientThread(LPVOID lpParam) {  
    int index = *(int *)lpParam; 
    free(lpParam);  
    SOCKET client_sock = clients[index].socket; 
    char buffer[1024];
    int len;

    char username[32];
    while (1) {
        len = recv(client_sock, username, sizeof(username) - 1, 0);   
        if (len <= 0) {           
            closesocket(client_sock);
            clients[index].isActive = 0;
            return 0;
        }
        username[len] = '\0';

        if (strlen(username) == 0) {
            char *msg = "nickname already taken or invalid try again\n";
            send(client_sock, msg, strlen(msg), 0);
            continue;
        }

        int duplicate = 0;
        EnterCriticalSection(&lock);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].isActive && strcmp(clients[i].username, username) == 0) {
                duplicate = 1;
                break;
            }
        }
        LeaveCriticalSection(&lock);

        if (duplicate) {
            char *msg = "nickname already taken or invalid try again\n";
            send(client_sock, msg, strlen(msg), 0);
        } else {
            
            strcpy(clients[index].username, username);
            clients[index].isActive = 1;
            break;
        }
    }

   
    struct sockaddr_in addr;
    int addr_len = sizeof(addr);
    getpeername(client_sock, (struct sockaddr*)&addr, &addr_len);
    char *client_ip = inet_ntoa(addr.sin_addr);  

    write_log("Client %s connected from %s\n", clients[index].username, client_ip);

    char join_msg[128];
    snprintf(join_msg, sizeof(join_msg), " %s has joined the chat\n", clients[index].username);
    broadcast_system_message(join_msg);

    while ((len = recv(client_sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[len] = '\0';

       if (strcmp(buffer, "quit") == 0) {
        printf(" Client %s disconnected.\n", clients[index].username);
        break;  
    }
        write_log("Message from %s: %s\n", clients[index].username, buffer);

        char *etiket = strstr(buffer, "@");
        if (etiket) {
            char hedef[32];
            char asilMesaj[1024];
            sscanf(etiket, "@%31s %[^\n]", hedef, asilMesaj);
            broadcast(clients[index].username, asilMesaj, hedef);
        } else {
            broadcast(clients[index].username, buffer, NULL);
        }
    }

    write_log("Client %s disconnected\n", clients[index].username);

    char leave_msg[128];
    snprintf(leave_msg, sizeof(leave_msg), " %s has left the chat\n", clients[index].username);
    broadcast_system_message(leave_msg);

    closesocket(client_sock);
    clients[index].isActive = 0;
    return 0;
}

void adjustServer(struct sockaddr_in *serverName){

//server settings ve port ayarlama

    serverName->sin_family = AF_INET;
    serverName->sin_port = htons(port);
    serverName->sin_addr.s_addr = INADDR_ANY;

}

int main() {
    WSADATA wsa;
    SOCKET server_sock, client_sock;
    struct sockaddr_in server; 
    struct sockaddr_in client; 
    int c = sizeof(client);
    InitializeCriticalSection(&lock);

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("WSAStartup error\n");
        return 1;
    }

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == INVALID_SOCKET) {
        printf("Socket failed.\n");
        WSACleanup();
        return 1;
    }


    adjustServer(&server);
 

 
    if (bind(server_sock, (struct sockaddr *)&server, sizeof(server)) == SOCKET_ERROR) {
        printf("Bind failed.\n");
        closesocket(server_sock);
        WSACleanup();
        return 1;
    }

    if (listen(server_sock, 5) == SOCKET_ERROR) {
        printf("Listen failed.\n");
        closesocket(server_sock);
        WSACleanup();
        return 1;
    }

    write_log("Chat server started\n");

    printf("Chat server started on port %d \n",port);

    while (1) {
        client_sock = accept(server_sock, (struct sockaddr *)&client, &c);
        if (client_sock == INVALID_SOCKET) {
            printf("Accept error.\n");
            continue;
        }

        if (client_sayisi < MAX_CLIENTS) {
            clients[client_sayisi].socket = client_sock;
            clients[client_sayisi].isActive = 0;
            int *index = malloc(sizeof(int));
            *index = client_sayisi;
            CreateThread(NULL, 0, clientThread, index, 0, NULL);
            client_sayisi++;
        } else {
            printf("User count reached max. you cant join now\n");
            closesocket(client_sock);
        }
    }

    closesocket(server_sock);
    WSACleanup();
    DeleteCriticalSection(&lock);
    return 0;
}
