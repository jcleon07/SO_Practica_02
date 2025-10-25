#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "hash.h"

#define PORT 3535;
#define BACKLOG 4;

int main (int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Inserte la ruta del dataset como argumento.\n");
        return 1;
    }

    //Ruta del archivo CSV 
    const char *ruta_csv = argv[1];

    //Declaracion de variables
    int fd, fd2, r;
    struct sockaddr_in server, client;
    socklen_t size;
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
    
    //Asociar socket con direccion y puerto
    r = bind(fd, (struct sockaddr *)&server,sizeof(struct sockaddr));
        if (r < 0) {
            perror("Error en bind");
            close(fd);
            exit(1);
        }


    //Abrir el archivo CSV
    FILE *f = fopen(ruta_csv, "w+");
        if (f == NULL) {
            perror("Error al abrir el archivo CSV");
            exit(1);
        }

    //Construir el indice
    construir_indice(f);
    printf("Indice construido.\n");

    while(1) {

        //Poner el socket en modo escucha
        r = listen(fd, BACKLOG);
        if (r < 0) {
            perror("Error en listen");
            close(fd);
            exit(1);
        }

        printf("Buscador iniciado. Esperando conexion en el puerto %d...\n", PORT);

        //Aceptar conexiones entrantes
        size = sizeof(struct sockaddr_in);
        fd2 = accept(fd, (struct sockaddr_in *)&client, &size);
            if (fd2 < 0) {
                perror("Error en accept");
                close(fd);
                close(fd2);
                exit(1);
            
            }
    
        
    }

    printf("Buscador finalizado.\n");
    return 0;
}