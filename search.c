#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include "hash.h"

int main (int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Inserte la ruta del dataset como argumento.\n");
        return 1;
    }

    const char *ruta_csv = argv[1];


    printf("Buscador finalizado.\n");
    return 0;
}