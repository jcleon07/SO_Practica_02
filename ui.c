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
    int fd, r, op;
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
 
    while (1) {
        printf("\n--- MENU ---\n");
        printf("1) Buscar registro por título (segunda columna)\n");
        printf("2) Añadir registro\n");
        printf("3) Salir\n> ");
        
        char opcion[3];
        fgets(opcion, sizeof(opcion), stdin);
        op = atoi(opcion);

        if (op == 3) {
            r = send(fd, "<<SALIR>>", strlen("<<SALIR>>"), 0);
                if (r < 0) {
                    perror("Error en send");
                    close(fd);
                    exit(1);
                }

        if (op == 2){

        }

        if (op == 1) {
            printf("Ingrese el nombre del titulo (igual a como aparece en el CSV): ");
            fgets(buffer, sizeof(buffer), stdin);
            buffer[strcspn(buffer, "\n")] = 0; // Eliminar salto de linea

            r = send(fd, buffer, strlen(buffer), 0);
                if (r < 0) {
                    perror("Error en send");
                    close(fd);
                    exit(1);
                }
        }
        }
    }

    return 0;
}