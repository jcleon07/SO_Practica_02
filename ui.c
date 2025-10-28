#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "hash.h"

#define PORT 3540
#define BACKLOG 4 

int main() {
    
    printf("Bienvenido.\n");
 
    //Declaracion de variables
    int fd, r, op;
    struct sockaddr_in server;
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
    
    //Coneccion al servidor
    r = connect(fd, (struct sockaddr *)&server, sizeof(struct sockaddr_in));
        if (r < 0) {
            perror("Error en connect");
            close(fd);
            exit(1);
        }

    //Verificar conexion
    r = send(fd, "Servidor conectado...", sizeof("Servidor conectado..."), 0);
    if (r == -1){
        perror("Error al enviar");
        exit(-1);
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
                    exit(1);
                }

            r = recv(fd, buffer, sizeof(buffer) - 1, 0);
                if (r < 0) {
                    perror("Error en recv");
                    exit(1);
                }

            printf("%s\n", buffer);
            close(fd);
            break;
        }

        // Añadir registro
        if (op == 2){
            printf("Ingrese el registro a añadir (separado por comas): ");
            
            //Campos del registro
            char titulo[200];
            char ingredientes[1500];
            char descripcion[2000];
            char links[500];
            char source[150];
            char NER[1500];
            
            printf("Titulo: ");
            fgets(titulo, sizeof(titulo), stdin);
            titulo[strcspn(titulo, "\n")] = '\0'; // Eliminar salto de linea

            printf("Ingredientes: ");
            fgets(ingredientes, sizeof(ingredientes), stdin);
            ingredientes[strcspn(ingredientes, "\n")] = '\0'; 

            printf("Descripcion: ");
            fgets(descripcion, sizeof(descripcion), stdin);
            descripcion[strcspn(descripcion, "\n")] = '\0';

            printf("Links: ");
            fgets(links, sizeof(links), stdin);
            links[strcspn(links, "\n")] = '\0';

            printf("Source: ");
            fgets(source, sizeof(source), stdin);
            source[strcspn(source, "\n")] = '\0';

            printf("NER: ");
            fgets(NER, sizeof(NER), stdin); 
            NER[strcspn(NER, "\n")] = '\0';
           

            //Registro completo
            char registro[8000];
            snprintf(registro, sizeof(registro), "OP2|%s|%s|%s|%s|%s|%s", titulo, ingredientes, 
                    descripcion, links, source, NER);    

            //Enviar registro al servidor
            r = send(fd, registro, strlen(registro), 0);
                if (r < 0) {
                    perror("Error en send");
                    close(fd);
                    exit(1);
                }
            //Esperar respuesta del servidor
            r = recv(fd, buffer, sizeof(buffer) - 1, 0);
                if (r < 0) {
                    perror("Error en recv");
                    close(fd);
                    exit(1);
                }

            buffer[r] = '\0';
            printf("%s\n", buffer);
        }

        // Buscar registro
        if (op == 1) {
            printf("Ingrese el nombre del titulo (igual a como aparece en el CSV): ");
            fgets(buffer, sizeof(buffer), stdin);
            buffer[strcspn(buffer, "\r\n")] = '\0'; // Eliminar salto de linea

            char registro[1006];
            snprintf(registro, sizeof(registro), "OP1|%s", buffer);

            //Enviar titulo al servidor
            r = send(fd, registro, strlen(registro), 0);
                if (r < 0) {
                    perror("Error en send");
                    close(fd);
                    exit(1);
                }
            
            // Limpiar el búfer antes de recibir la respuesta
            memset(buffer, 0, sizeof(buffer));
            
            // Esperar respuesta del servidor con consulta
            r = recv(fd, buffer, sizeof(buffer) - 1, 0);
                if (r < 0) {
                    perror("Error en recv");
                    close(fd);
                    exit(1);
                }
            
            // Asegurarse de que el buffer esté terminado en nulo
            buffer[r] = '\0';
    
            //Imprime el registro encontrado
            printf("Resultado: %s\n", buffer);

        }
     
        memset(buffer, 0, sizeof(buffer));
    }
    close(fd);

    return 0;
}