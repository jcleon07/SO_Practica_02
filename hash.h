#ifndef HASH_H
#define HASH_H

#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>

#define TAM_TABLA 1000
#define CLAVE_MAX 512
#define RESP_MAX 10096

typedef struct Nodo {
    uint64_t hash;
    off_t offset;
    int32_t siguiente;
} Nodo;

typedef struct RegistroInfo {
    off_t offset;
    int32_t length;
} RegistroInfo;

unsigned long long XXH64(const void* data, size_t len, unsigned long long seed);

void init_tabla(void);
void reservar_pool_nodos(size_t expected);
unsigned long long calcular_hash64(const char *clave);
int indice_de_hash_from_u64(uint64_t h);
void insertar_indice(const char *clave_orig, off_t offset);
void construir_indice(FILE *f);
char* buscar_por_clave(FILE *f, const char *clave_orig, char *buffer_out);
int añadir_registro(FILE *f, const char *titulo, const char *ingredientes, const char *instrucciones, const char *enlace, const char *fuente, const char *entidades);
char* leer_por_numero_registro(FILE *f, int numero_registro, char *buffer_out);
void liberar_tabla(void);

extern int32_t tabla[TAM_TABLA];
extern Nodo *nodes;
extern int32_t nodes_capacity;
extern int32_t nodes_count;
extern RegistroInfo *registros_cache;
extern int total_registros;

#endif