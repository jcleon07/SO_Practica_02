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
    char buffer[200];

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
        
        // Leer input del usuario
        char opcion[3];
        fgets(opcion, sizeof(opcion), stdin);
        op = atoi(opcion);

        // Salir
        if (op == 3) {
            r = send(fd, "<<SALIR>>", strlen("<<SALIR>>"), 0);
                if (r < 0) {
                    perror("Error en send");
                    close(fd);
                    break;
                }
        }

        if (op == 2){
            printf("Ingrese el registro a añadir (separado por comas): ");
            
            //Campos del registro
            char titulo[RESP_MAX];
            char ingredientes[1500];
            char descripcion[2000];
            char links[500];
            char source[150];
            char NER[1500];
            
            printf("Titulo: ");
            fgets(titulo, sizeof(titulo), stdin);
            titulo[strcspn(titulo, "\n")] = 0; // Eliminar salto de linea

            printf("Ingredientes: ");
            fgets(ingredientes, sizeof(ingredientes), stdin);
            ingredientes[strcspn(ingredientes, "\n")] = 0; 

            printf("Descripcion: ");
            fgets(descripcion, sizeof(descripcion), stdin);
            descripcion[strcspn(descripcion, "\n")] = 0;

            printf("Links: ");
            fgets(links, sizeof(links), stdin);
            links[strcspn(links, "\n")] = 0;

            printf("Source: ");
            fgets(source, sizeof(source), stdin);
            source[strcspn(source, "\n")] = 0;

            printf("NER: ");
            fgets(NER, sizeof(NER), stdin); 
            NER[strcspn(NER, "\n")] = 0;
           

            //Registro completo
            char registro[6000];
            snprintf(registro, sizeof(registro), "OP2|%s|%s|%s|%s|%s|%s", titulo, ingredientes, 
                    descripcion, links, source, NER);    

            r = send(fd, registro, strlen(registro), 0);
                if (r < 0) {
                    perror("Error en send");
                    close(fd);
                    exit(1);
                }
        }

        // Buscar registro
        if (op == 1) {
            printf("Ingrese el nombre del titulo (igual a como aparece en el CSV): ");
            fgets(buffer, sizeof(buffer), stdin);
            buffer[strcspn(buffer, "\n")] = 0; // Eliminar salto de linea

            char registro[200];
            snprintf(registro, sizeof(registro), "OP1|%s", buffer);

            r = send(fd, registro, strlen(registro), 0);
                if (r < 0) {
                    perror("Error en send");
                    close(fd);
                    exit(1);
                }
        }
        
    }

    return 0;
}