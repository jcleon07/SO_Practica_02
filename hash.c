#include "hash.h"
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/file.h>

// ------------------- Control Estricto de Memoria -------------------
#include <sys/resource.h>

#define MEMORY_LIMIT_MB 10
#define MEMORY_LIMIT_KB (MEMORY_LIMIT_MB * 1000)
#define SAFETY_BUFFER_KB 500  // Buffer de seguridad de 500KB

size_t get_memory_usage_kb() {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return (size_t)usage.ru_maxrss;
}

void check_memory_limit() {
    size_t current_usage = get_memory_usage_kb();
    if (current_usage > (MEMORY_LIMIT_KB - SAFETY_BUFFER_KB)) {
        fprintf(stderr, "ERROR: Límite de memoria excedido: %zu KB > %d KB\n", 
                current_usage, MEMORY_LIMIT_KB);
        liberar_tabla();
        exit(-1);
    }
}

// --------------------------------------------------

// Función auxiliar para eliminar espacios en blanco al inicio y final
static void trim_whitespace(char *str) {
    if (!str || !*str) return;
    
    // Trim leading space
    char *start = str;
    while(isspace((unsigned char)*start)) start++;
    
    // Trim trailing space
    char *end = str + strlen(str) - 1;
    while(end > start && isspace((unsigned char)*end)) end--;
    
    // Write new null terminator
    *(end + 1) = '\0';
    
    // Move the string to the start if there were leading spaces
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
}

// Función auxiliar para comparar cadenas ignorando mayúsculas/minúsculas
static int strings_equal_ignore_case(const char *a, const char *b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == *b;  // Ambos deben terminar al mismo tiempo
}

//------------------------------------------------------------------

int32_t tabla[TAM_TABLA];
Nodo *nodes = NULL;
int32_t nodes_capacity = 0;
int32_t nodes_count = 0;

void init_tabla(void) {
    for (int i = 0; i < TAM_TABLA; ++i)
        tabla[i] = -1;
    nodes_count = 0;
}

static size_t max_nodes_for_limit(size_t limit_bytes) {
    size_t per = sizeof(Nodo);
    if (per == 0) 
        return 0;
    return limit_bytes / per;
}

void reservar_pool_nodos(size_t expected) {
    // CALCULAR MÁXIMO SEGÚN LÍMITE DE 10MB
    size_t max_nodes = max_nodes_for_limit((MEMORY_LIMIT_KB - SAFETY_BUFFER_KB) * 1000);
    
    // Usar el menor entre lo esperado y el máximo permitido
    size_t want = (expected > 0 && expected < max_nodes) ? expected : max_nodes;
    
    if (want < 100) want = 100; // Mínimo razonable
    
    printf("Reservando nodos: %zu (máximo permitido: %zu)\n", want, max_nodes);
    
    // Liberar memoria previa si existe
    if (nodes) {
        free(nodes);
        nodes = NULL;
    }
    
    nodes = (Nodo*) calloc(want, sizeof(Nodo));
    if (!nodes){
        perror("calloc nodes");
        exit(-1);
    }
    nodes_capacity = (int32_t) want;
    nodes_count = 0;
    
    check_memory_limit();
}

unsigned long long calcular_hash32(const char *clave) {
    return (unsigned long) XXH32(clave, strlen(clave), 0);
}

int indice_de_hash_from_u32(uint32_t h) {
    return (int) (h % TAM_TABLA);
}

static void to_lower_str(char *s) {
    for (; *s; ++s) *s = (char) tolower((unsigned char)*s);
}

static int extract_nth_field(const char* s, int n, char* out, size_t max) {
    if(!s || !out || max == 0 || n <= 0)
        return 0;

    const char *p = s;
    int field = 1;
    int in_quotes = 0;
    int in_array = 0;

    while (*p && field <= n) {
        size_t i = 0;

        while(*p == ' ' || *p == '\t') ++p;

        if(*p == '"') {
            in_quotes = 1;
            ++p;
        } else if (*p == '[') {
            in_array = 1;
            ++p;
        }

        while (*p && i < max - 1) {
            if (in_quotes) {
                if (*p == '"' && *(p+1) == '"') {
                    out[i++] = '"';
                    p += 2;
                } else if (*p == '"') {
                    ++p;
                    break;
                } else {
                    out[i++] = *p++;
                }
            } else if (in_array) {
                if (*p == ']') {
                    ++p;
                    break;
                } else {
                    out[i++] = *p++;
                }
            } else {
                if (*p == ',' || *p == '\n' || *p == '\r') break;
                out[i++] = *p++;
            }
        }

        out[i] = '\0';

        if(!in_quotes && !in_array) {
            while(i > 0 && (out[i-1] == ' ' || out[i-1] == '\t')) {
                out[--i] = '\0';
            }
        }

        if(field == n) return 1;

        if(*p == ',') {
            ++p;
            in_quotes = 0;
            in_array = 0;
        }
        ++field;
    }
    return 0;
}

void insertar_indice(const char *clave_orig, off_t offset) {
    // VERIFICAR MEMORIA ANTES DE INSERTAR
    check_memory_limit();

    char clave[CLAVE_MAX];
    strncpy(clave, clave_orig, CLAVE_MAX-1);
    clave[CLAVE_MAX-1] = '\0';
    to_lower_str(clave);

    uint32_t h = calcular_hash32(clave);
    int idx = indice_de_hash_from_u32(h);

    // VERIFICAR SI HAY ESPACIO - SIN REDIMENSIONAR
    if (nodes_count >= nodes_capacity) {
        fprintf(stderr, "ERROR: No hay espacio para más nodos en el índice (%d/%d)\n", 
                nodes_count, nodes_capacity);
        return;
    }

    // Crear nuevo nodo
    int32_t id = nodes_count++;
    nodes[id].hash = h;
    nodes[id].offset = offset;
    nodes[id].siguiente = tabla[idx];
    tabla[idx] = id;
}

void construir_indice(FILE *f) {
    init_tabla();

    // ESTIMAR MÁXIMO SEGÚN LÍMITE DE MEMORIA
    struct stat st;
    size_t expected = 0;
    if (fstat(fileno(f), &st) == 0 && st.st_size > 0) {
        size_t avg_line = 120;
        expected = (size_t)(st.st_size / avg_line);
        printf("Archivo de %ld bytes, estimando %zu registros\n", st.st_size, expected);
    }
    
    reservar_pool_nodos(expected);

    char *line = NULL;
    size_t len = 0;
    ssize_t nread;

    // Saltar cabecera
    if ((nread = getline(&line, &len, f)) == -1) {
        free(line);
        return;
    }

    const int COL_A_INDEXAR = 2;
    int registros_indexados = 0;
    int registros_omitidos = 0;

    while (1) {
        off_t pos = ftello(f);
        if (pos == -1) break;
        
        nread = getline(&line, &len, f);
        if (nread == -1) break;

        char clave[CLAVE_MAX];
        if (extract_nth_field(line, COL_A_INDEXAR, clave, sizeof(clave))) {
            if (strlen(clave) > 0) {
                if (nodes_count < nodes_capacity) {
                    insertar_indice(clave, pos);
                    registros_indexados++;
                } else {
                    registros_omitidos++;
                    // Solo mostrar mensaje cada 1000 registros omitidos para no saturar
                    if (registros_omitidos % 1000 == 0) {
                        printf("Advertencia: %d registros omitidos por límite de memoria\n", registros_omitidos);
                    }
                    // Romper el bucle si hemos omitido muchos registros
                    if (registros_omitidos > 10000) {
                        printf("Demasiados registros omitidos, deteniendo indexación...\n");
                        break;
                    }
                }
            }
        }
        
        // VERIFICAR MEMORIA PERIÓDICAMENTE
        if (registros_indexados % 1000 == 0) {
            check_memory_limit();
        }
    }

    free(line);
    printf("Índice construido: %d/%d nodos usados, %d registros omitidos, Memoria: %zu KB\n", 
           nodes_count, nodes_capacity, registros_omitidos, get_memory_usage_kb());
}

char* buscar_por_clave(FILE *f, const char *clave_orig, char *buffer_out) {
    if (!f || !clave_orig || !buffer_out) {
        printf("Error: Parámetros inválidos\n");
        return NULL;
    }

    // Buffers estáticos reutilizables
    static char clave_norm[CLAVE_MAX];
    static char temp_buffer[RESP_MAX];
    static char titulo_temp[CLAVE_MAX];

    // Normalización rápida 
    strncpy(clave_norm, clave_orig, CLAVE_MAX-1);
    clave_norm[CLAVE_MAX-1] = '\0';
    trim_whitespace(clave_norm);
    to_lower_str(clave_norm);
    
    // Calcular el hash
    uint32_t h = calcular_hash32(clave_norm);
    int idx = indice_de_hash_from_u32(h);
    
    int32_t cur = tabla[idx];
    
    while (cur != -1) {
        if (nodes[cur].hash == h) {
            if (fseeko(f, nodes[cur].offset, SEEK_SET) == 0 &&
                fgets(buffer_out, RESP_MAX, f)) {

            // Procesar registro
            strncpy(temp_buffer, buffer_out, RESP_MAX-1);
            temp_buffer[RESP_MAX-1] = '\0';
            
            // Limpiar newlines
            char *end = temp_buffer + strlen(temp_buffer) - 1;
            while (end > temp_buffer && (*end == '\n' || *end == '\r')) {
                *end-- = '\0';
            }
            
            // Extraer y comparar título
            if (extract_nth_field(temp_buffer, 2, titulo_temp, CLAVE_MAX-1)) {
                trim_whitespace(titulo_temp);
                to_lower_str(titulo_temp);
                
                if (strcmp(titulo_temp, clave_norm) == 0) {
                    return buffer_out; // ¡Éxito!
                }
            }
        }
    }
        cur = nodes[cur].siguiente;
    }

    printf("No se encontró el registro con título: '%s'\n", clave_norm);
    return NULL;
}

int añadir_registro(FILE *f, const char *titulo, const char *ingredientes, 
                   const char *descripcion, const char *link, 
                   const char *source, const char *ner) {
    // VERIFICAR MEMORIA ANTES DE AÑADIR
    check_memory_limit();
    
    if (!f) {
        fprintf(stderr, "Error: Archivo no abierto\n");
        return -1;
    }
    
    // VERIFICAR SI HAY ESPACIO EN EL ÍNDICE
    if (nodes_count >= nodes_capacity) {
        fprintf(stderr, "ERROR: No hay espacio en el índice para más registros (%d/%d)\n", 
                nodes_count, nodes_capacity);
        return -1;
    }
    
    // Hacer copias de los parámetros para normalizarlos
    char titulo_norm[CLAVE_MAX] = {0};
    strncpy(titulo_norm, titulo, sizeof(titulo_norm) - 1);
    trim_whitespace(titulo_norm);
    to_lower_str(titulo_norm);
    
    // Calcular el hash del título normalizado
    uint32_t h = calcular_hash32(titulo_norm);
    uint32_t idx = indice_de_hash_from_u32(h);
    
    // Buscar el último ID utilizado
    int nuevo_id = 1;
    char line[RESP_MAX];
    
    // Ir al final del archivo para buscar el último registro
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    
    if (file_size > 0) {
        // Buscar el inicio de la última línea
        long pos = file_size - 2;
        while (pos > 0) {
            fseek(f, pos, SEEK_SET);
            if (fgetc(f) == '\n') {
                fgets(line, sizeof(line), f);
                if (strlen(line) > 0) {
                    char *token = strtok(line, ",");
                    if (token) {
                        nuevo_id = atoi(token) + 1;
                    }
                    break;
                }
            }
            pos--;
        }
    }

    // Construir la línea del nuevo registro
    char registro[RESP_MAX];
    snprintf(registro, sizeof(registro), "%d,%s,%s,%s,%s,%s,%s\n", 
             nuevo_id, titulo, ingredientes, descripcion, link, source, ner);
    
    // Escribir el nuevo registro al final del archivo
    fseek(f, 0, SEEK_END);
    long offset = ftell(f);
    
    size_t bytes_escritos = fwrite(registro, 1, strlen(registro), f);
    fflush(f);
    
    if (bytes_escritos != strlen(registro)) {
        perror("Error al escribir en el archivo");
        return -1;
    }
    
    // Insertar en el índice usando el título normalizado
    insertar_indice(titulo_norm, offset);
    
    printf("Registro añadido con éxito (ID: %d), Nodos usados: %d/%d\n", 
           nuevo_id, nodes_count, nodes_capacity);
    
    return nuevo_id;
}

void liberar_tabla(void) {
    if (nodes) {
        free(nodes); 
        nodes = NULL;
    }
    nodes_capacity = nodes_count = 0;
    for (int i = 0; i < TAM_TABLA; ++i) 
        tabla[i] = -1;
}

// Implementación simple de XXH32 para evitar dependencias externas
unsigned long long XXH32(const void* data, size_t len, unsigned long long seed) {
    const uint8_t* p = (const uint8_t*)data;
    const uint8_t* const end = p + len;
    uint32_t h32;

    if (len >= 16) {
        const uint8_t* const limit = end - 16;
        uint32_t v1 = (uint32_t)seed + 0x9E3779B1 + 0x85EBCA77;
        uint32_t v2 = (uint32_t)seed + 0x85EBCA77;
        uint32_t v3 = (uint32_t)seed + 0;
        uint32_t v4 = (uint32_t)seed - 0x9E3779B1;

        do {
            v1 += *(uint32_t*)p * 0x85EBCA77;
            v1 = (v1 << 13) | (v1 >> 19);
            v1 *= 0x9E3779B1;
            p += 4;
            v2 += *(uint32_t*)p * 0x85EBCA77;
            v2 = (v2 << 13) | (v2 >> 19);
            v2 *= 0x9E3779B1;
            p += 4;
            v3 += *(uint32_t*)p * 0x85EBCA77;
            v3 = (v3 << 13) | (v3 >> 19);
            v3 *= 0x9E3779B1;
            p += 4;
            v4 += *(uint32_t*)p * 0x85EBCA77;
            v4 = (v4 << 13) | (v4 >> 19);
            v4 *= 0x9E3779B1;
            p += 4;
        } while (p <= limit);

        h32 = (v1 << 1) | (v1 >> 31);
        h32 += (v2 << 7) | (v2 >> 25);
        h32 += (v3 << 12) | (v3 >> 20);
        h32 += (v4 << 18) | (v4 >> 14);
    } else {
        h32 = (uint32_t)seed + 0x165667B1;
    }

    h32 += (uint32_t)len;

    while (p + 4 <= end) {
        h32 += *(uint32_t*)p * 0xC2B2AE3D;
        h32 = (h32 << 17) | (h32 >> 15);
        h32 *= 0x27D4EB2F;
        p += 4;
    }

    while (p < end) {
        h32 += *p * 0x165667B1;
        h32 = (h32 << 11) | (h32 >> 21);
        h32 *= 0x9E3779B1;
        p++;
    }

    h32 ^= h32 >> 15;
    h32 *= 0x85EBCA77;
    h32 ^= h32 >> 13;
    h32 *= 0xC2B2AE3D;
    h32 ^= h32 >> 16;

    return h32;
}