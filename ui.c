#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "hash.h"

#define PORT 3539
#define BACKLOG 4 

int main() {
    
    printf("Bienvenido.\n");
 
    //Declaracion de variables
    int fd, r;
    struct sockaddr_in server, cliente;
    char buffer[20];

    //Creacion del socket
    fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            perror("Error al crear el socket");
            exit(1);
        }

    //Configuracion del servidor
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = INADDR_ANY;
    bzero(&(server.sin_zero),8);
    
    //Coneccion al servidor
    r = connect(fd, (struct sockaddr *)&server, sizeof(struct sockaddr_in));
        if (r < 0) {
            perror("Error en connect");
            close(fd);
            exit(1);
        }
 
    return 0;
}