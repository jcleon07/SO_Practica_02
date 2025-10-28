#include "hash.h"
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/file.h>

// ------------------- Uso de Memoria -------------------

#include <sys/resource.h>

size_t get_memory_usage_kb() {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return (size_t)usage.ru_maxrss; // KB en Linux
}

void check_memory_limit() {
    size_t current_usage = get_memory_usage_kb();
    if (current_usage > 9000) { // 9 MB
        fprintf(stderr, "Advertencia: Uso de memoria acercándose al límite: %zu KB\n", current_usage);
    }
}


//--------------------------------------------------


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
    for (int i = 0; i < TAM_TABLA; ++i) {
        tabla[i] = -1;
    }
    nodes_count = 0;
}

static size_t max_nodes_for_limit(size_t limit_bytes) {
    size_t per = sizeof(Nodo);
    if (per == 0) 
        return 0;
    return limit_bytes / per;
}

void reservar_pool_nodos(size_t expected) {
    const size_t MEM_LIM_MB = 6;
    size_t max_nodes = max_nodes_for_limit(MEM_LIM_MB * 1024 * 1024);
    size_t want = expected ? expected : 1024; 
    if (want > max_nodes)
        want = max_nodes;
    if (want < 16)
        want = 16;
    nodes = (Nodo*) calloc(want, sizeof(Nodo));
    if (!nodes){
        perror("calloc nodes");
        exit(-1);
    }
    nodes_capacity = (int32_t) want;
    nodes_count = 0;
}

unsigned long long calcular_hash64(const char *clave) {
    return (unsigned long long) XXH64(clave, strlen(clave), 0);
}

int indice_de_hash_from_u64(uint64_t h) {
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
    // Asegurar que la tabla esté inicializada
    static int initialized = 0;
    if (!initialized) {
        init_tabla();
        initialized = 1;
        // No mostrar depuración durante la inicialización
        return;
    }

    char clave[CLAVE_MAX];
    strncpy(clave, clave_orig, CLAVE_MAX-1);
    clave[CLAVE_MAX-1] = '\0';
    to_lower_str(clave);

    uint64_t h = calcular_hash64(clave);
    int idx = indice_de_hash_from_u64(h);

    // Asegurar que tenemos espacio para más nodos
    if (!nodes || nodes_count >= nodes_capacity) {
        size_t new_capacity = nodes_capacity ? nodes_capacity * 2 : 16;
        
        Nodo *new_nodes = realloc(nodes, new_capacity * sizeof(Nodo));
        if (!new_nodes) {
            perror("Error en realloc de nodos");
            return;
        }
        nodes = new_nodes;
        nodes_capacity = new_capacity;
        }

        // Crear nuevo nodo
    int32_t id = nodes_count++;
    nodes[id].hash = h;
    nodes[id].offset = offset;
    nodes[id].siguiente = tabla[idx];
    tabla[idx] = id;

    }




void construir_indice(FILE *f) {

    printf("Memoria inicial: %zu KB\n", get_memory_usage_kb());

    init_tabla();

    struct stat st;
    size_t expected = 5000;
    if (fstat(fileno(f), &st) == 0 && st.st_size > 0) {
        size_t avg_line = 200;
        expected = (size_t)(st.st_size / avg_line);
        if (expected > 100000) expected = 100000; // Límite máximo
    }
    reservar_pool_nodos(expected);


    char *line = NULL;
    size_t len = 0;
    ssize_t nread;

    if ((nread = getline(&line, &len, f)) == -1) {
        free(line);
        return;
    }

    const int COL_A_INDEXAR = 2;  // Título es la columna 2 (ID es columna 1)

    while (1) {
        off_t pos = ftello(f);
        if (pos == -1)
            break;
        nread = getline(&line, &len, f);
        if (nread == -1)
            break;

        char clave[CLAVE_MAX];
        if (extract_nth_field(line, COL_A_INDEXAR, clave, sizeof(clave))) {
            if (strlen(clave) > 0) {
                insertar_indice(clave, pos);
            }
        }
    }

    free(line);
    printf("Memoria final: %zu KB, Nodos: %d\n", get_memory_usage_kb(), nodes_count);
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

    printf("\n=== BÚSQUEDA INICIADA ===\n");

    // Normalización rápida 
    strncpy(clave_norm, clave_orig, CLAVE_MAX-1);
    clave_norm[CLAVE_MAX-1] = '\0';
    trim_whitespace(clave_norm);
    to_lower_str(clave_norm);
    

    
    // Calcular el hash
    uint64_t h = calcular_hash64(clave_norm);
    int idx = indice_de_hash_from_u64(h);
    
    printf("Hash calculado: %llu\n", (unsigned long long)h);
    printf("Índice en tabla hash: %d\n", idx);
    
    int32_t cur = tabla[idx];

    
    while (cur != -1) {

        
        if (nodes[cur].hash == h) {
            printf("¡Hash coincidente encontrado!\n");
            
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
    
    
    printf("Hash calculado: %llu\n", (unsigned long long)h);
    printf("Índice en tabla hash: %d\n", idx);

    printf("\n=== BÚSQUEDA FINALIZADA ===\n");
    printf("No se encontró el registro con título: '%s'\n", clave_norm);
    return NULL;
}



int añadir_registro(FILE *f, const char *titulo, const char *ingredientes, 
                   const char *descripcion, const char *link, 
                   const char *source, const char *ner) {
    if (!f) {
        fprintf(stderr, "Error: Archivo no abierto\n");
        return -1;
    }
    
    printf("\n=== AÑADIENDO NUEVO REGISTRO ===\n");

    // Hacer copias de los parámetros para normalizarlos
    char titulo_norm[CLAVE_MAX] = {0};
    strncpy(titulo_norm, titulo, sizeof(titulo_norm) - 1);
    trim_whitespace(titulo_norm);
    to_lower_str(titulo_norm);
    
    
    // Calcular el hash del título normalizado
    uint64_t h = calcular_hash64(titulo_norm);
    printf("Hash calculado para el título: %llu\n", (unsigned long long)h);
    uint32_t idx = indice_de_hash_from_u64(h);
    printf("Índice en tabla hash: %d\n", idx);
    
    // Buscar el último ID utilizado
    int nuevo_id = 1; // Valor por defecto si no hay registros
    char line[RESP_MAX];
    
    // Ir al final del archivo para buscar el último registro
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    
    printf("Tamaño actual del archivo: %ld bytes\n", file_size);
    
    if (file_size > 0) {
        // Buscar el inicio de la última línea
        long pos = file_size - 2; // Empezar desde el final-2 para evitar el último \n
        while (pos > 0) {
            fseek(f, pos, SEEK_SET);
            if (fgetc(f) == '\n') {
                // Leer la línea completa
                fgets(line, sizeof(line), f);
                if (strlen(line) > 0) {
                    // Extraer el ID del último registro
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
    
    // Mostrar información de depuración
    printf("\nRegistro a insertar:\n%s\n", registro);
    
    // Escribir el nuevo registro al final del archivo
    fseek(f, 0, SEEK_END);
    long offset = ftell(f);
    printf("Escribiendo en offset: %ld\n", offset);
    
    size_t bytes_escritos = fwrite(registro, 1, strlen(registro), f);
    fflush(f);
    
    if (bytes_escritos != strlen(registro)) {
        perror("Error al escribir en el archivo");
        return -1;
    }
    
    printf("Registro escrito correctamente (%zu bytes)\n", bytes_escritos);
    
    // Insertar en el índice usando el título normalizado
    printf("\nInsertando en el índice:\n");
    printf("  Clave: '%s'\n", titulo_norm);
    printf("  Hash: %llu\n", (unsigned long long)h);
    printf("  Offset: %ld\n", offset);
    
    insertar_indice(titulo_norm, offset);
    
    printf("=== REGISTRO AÑADIDO CON ÉXITO (ID: %d) ===\n\n", nuevo_id);
    
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