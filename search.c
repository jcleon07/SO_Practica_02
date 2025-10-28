#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "hash.h"
#include <time.h>   

#define PORT 3540
#define BACKLOG 4

int main (int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Inserte la ruta del dataset como argumento.\n");
        return 1;
    }

    //Ruta del archivo CSV 
    const char *ruta_csv = argv[1];

    //Declaracion de variables
    int fd, fd2, r;
    struct sockaddr_in server;
    socklen_t size;
    char buffer[8000];

    //Creacion del socket
    fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            perror("Error al crear el socket");
            exit(1);
        }

    //Configuracion del servidor
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    bzero(&(server.sin_zero),8);
    
    //Asociar socket con direccion y puerto
    r = bind(fd, (struct sockaddr *)&server,sizeof(struct sockaddr));
        if (r < 0) {
            perror("Error en bind");
            close(fd);
            exit(1);
        }


    //Abrir el archivo CSV
    FILE *f = fopen(ruta_csv, "a+");
        if (f == NULL) {
            perror("Error al abrir el archivo CSV");
            exit(1);
        }

    //Construir el indice
    construir_indice(f);
    printf("Indice construido.\n");

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
    fd2 = accept(fd, (struct sockaddr *)&server, &size);
        if (fd2 < 0) {
            perror("Error en accept");
            close(fd);
            close(fd2);
            exit(1);
        
        }

    //Confirmar conexion
    r = recv(fd2, buffer, sizeof(buffer) - 1, 0);
        if (r < 0) {
            perror("Error en recv");
            close(fd);
            close(fd2);
            exit(1);
        }

    buffer[r] = '\0';
    printf("%s\n", buffer);

    //Bucle principal del servidor
    while(1) {

        memset(buffer, 0, sizeof(buffer));
        
        //Recibir datos del cliente
        r = recv(fd2, buffer, sizeof(buffer) - 1, 0);
            if (r < 0) {
                perror("Error en recv");
                close(fd);
                close(fd2);
                exit(1);
            }

        buffer[r] = '\0'; 

            // Validar SALIR
            if (strcmp(buffer, "<<SALIR>>") == 0) {
                
                r = send(fd2, "Conexion cerrada", strlen("Conexion cerrada"), 0);
                    if (r < 0) {
                        perror("Error en send");
                        close(fd);
                        close(fd2);
                        exit(1);
                    }
              break;
            }

            //Agregar registro
            if (strncmp(buffer, "OP2|",4) == 0) {
                    char *token;
                    char *campos[6];
                    int i = 0;
                    
                    // Saltar el "OP2|"
                    token = strtok(buffer + 4, "|");
                    
                    while (token != NULL && i < 6) {
                        campos[i++] = token;
                        token = strtok(NULL, "|");
                    }

                        if (i == 6) {
                        añadir_registro(f, campos[0], campos[1], campos[2], campos[3], campos[4], campos[5]);
                        r = send(fd2, "Registro añadido", strlen("Registro añadido"), 0);
                    } else {
                        r = send(fd2, "Error: Formato incorrecto", strlen("Error: Formato incorrecto"), 0);
                    }
                    
                    if (r < 0) {
                        perror("Error en send");
                        close(fd);
                        close(fd2);
                        exit(1);
                    }

            }

            // Buscar registro por titulo
            if (strncmp(buffer, "OP1|",4) == 0) {

                char *titulo = buffer + 4;
                char *res = NULL;
                char temp[RESP_MAX];

                memset(temp, 0, sizeof(temp));      

                struct timespec start, end;
                clock_gettime(CLOCK_MONOTONIC, &start);

                res = buscar_por_clave(f, titulo, temp);

                clock_gettime(CLOCK_MONOTONIC, &end);
                double tiempo = (end.tv_sec - start.tv_sec) +
                                (end.tv_nsec - start.tv_nsec) / 1e9;
        
                printf("Tiempo de busqueda: %.6f segundos\n",tiempo);

                if (res) {
                    r = send(fd2, res, strlen(res), 0);
                        if (r < 0) {
                            perror("Error en send");
                            close(fd);
                            close(fd2);
                            exit(1);
                        }
                } else {
                    r = send(fd2, "N/A", strlen("N/A"), 0);
                        if (r < 0) {
                            perror("Error en send");
                            close(fd);
                            close(fd2);
                            exit(1);
                        }     
                }
            }
    }
    liberar_tabla();
    fclose(f);
    close(fd);
    close(fd2);

    printf("Buscador finalizado.\n");
    return 0;
}