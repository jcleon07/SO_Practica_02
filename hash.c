#include "hash.h"
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/file.h>

int32_t tabla[TAM_TABLA];
Nodo *nodes = NULL;
int32_t nodes_capacity = 0;
int32_t nodes_count = 0;

RegistroInfo *registros_cache = NULL;
int total_registros = 0;

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
    const size_t MEM_LIM_MB = 9;
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

void insertar_indice(const char *clave_orig, off_t offset){
    char clave[CLAVE_MAX];
    strncpy(clave, clave_orig, CLAVE_MAX-1);
    clave[CLAVE_MAX-1] = '\0';
    to_lower_str(clave);

    uint64_t h = calcular_hash64(clave);
    int idx = indice_de_hash_from_u64(h);

    if(!nodes || nodes_count >= nodes_capacity) {
        size_t new_capacity = nodes_capacity * 2;
        if (new_capacity == 0) new_capacity = 16;
        size_t max_nodes = max_nodes_for_limit(9 * 1024 * 1024);
        if (new_capacity > max_nodes) {
            return;
        }
        Nodo *new_nodes = realloc(nodes, new_capacity * sizeof(Nodo));
        if (!new_nodes) {
            perror("realloc nodes");
            return;
        }
        nodes = new_nodes;
        nodes_capacity = new_capacity;
    }

    int32_t id = nodes_count++;
    nodes[id].hash = h;
    nodes[id].offset = offset;
    nodes[id].siguiente = tabla[idx];
    tabla[idx] = id; 
}

void construir_indice(FILE *f) {
    init_tabla();

    struct stat st;
    size_t expected = 0;
    if (fstat(fileno(f), &st) == 0 && st.st_size > 0) {
        size_t avg_line = 120;
        expected = (size_t)(st.st_size / avg_line);
    }
    reservar_pool_nodos(expected + 16);

    total_registros = 0;
    registros_cache = NULL;

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

        if (total_registros % 1000 == 0) {
            registros_cache = realloc(registros_cache, (total_registros + 1000) * sizeof(RegistroInfo));
        }
        registros_cache[total_registros].offset = pos;
        registros_cache[total_registros].length = nread;
        total_registros++;

        char clave[CLAVE_MAX];
        if (extract_nth_field(line, COL_A_INDEXAR, clave, sizeof(clave))) {
            if (strlen(clave) > 0) {
                insertar_indice(clave, pos);
            }
        }
    }

    free(line);
}

char* buscar_por_clave(FILE *f, const char *clave_orig, char *buffer_out) {
    if (!f || !clave_orig || !buffer_out) {
        printf("Error: Parámetros inválidos\n");
        return NULL;
    }

    char clave_norm[CLAVE_MAX];
    strncpy(clave_norm, clave_orig, CLAVE_MAX-1);
    clave_norm[CLAVE_MAX-1] = '\0';
    to_lower_str(clave_norm);
    
    printf("Buscando clave: '%s'\n", clave_norm);
    
    // Primero buscar en la caché
    for (int i = 0; i < total_registros; i++) {
        if (fseeko(f, registros_cache[i].offset, SEEK_SET) == 0) {
            if (fgets(buffer_out, RESP_MAX, f)) {
                char temp[RESP_MAX];
                strncpy(temp, buffer_out, RESP_MAX-1);
                temp[RESP_MAX-1] = '\0';
                
                // Limpiar saltos de línea
                size_t L = strlen(temp);
                while (L > 0 && (temp[L-1] == '\n' || temp[L-1] == '\r')) {
                    temp[--L] = '\0';
                }
                
                // Extraer el título (segundo campo)
                char titulo[CLAVE_MAX];
                if (extract_nth_field(temp, 2, titulo, sizeof(titulo))) {
                    to_lower_str(titulo);
                    if (strcmp(titulo, clave_norm) == 0) {
                        printf("Registro encontrado en caché\n");
                        return buffer_out;
                    }
                }
            }
        }
    }
    
    // Si no se encontró en caché, intentar con el hash
    uint64_t h = calcular_hash64(clave_norm);
    int idx = indice_de_hash_from_u64(h);
    int32_t cur = tabla[idx];
    
    while (cur != -1) {
        if (nodes[cur].hash == h) {
            if (fseeko(f, nodes[cur].offset, SEEK_SET) == 0) {
                if (fgets(buffer_out, RESP_MAX, f)) {
                    // Limpiar saltos de línea
                    size_t L = strlen(buffer_out);
                    while (L > 0 && (buffer_out[L-1] == '\n' || buffer_out[L-1] == '\r')) {
                        buffer_out[--L] = '\0';
                    }
                    
                    // Extraer el título (segundo campo)
                    char titulo[CLAVE_MAX];
                    if (extract_nth_field(buffer_out, 2, titulo, sizeof(titulo))) {
                        to_lower_str(titulo);
                        if (strcmp(titulo, clave_norm) == 0) {
                            printf("Registro encontrado en hash table\n");
                            return buffer_out;
                        }
                    }
                }
            }
        }
        cur = nodes[cur].siguiente;
    }
    
    printf("No se encontró el registro con título: %s\n", clave_norm);
    return NULL;
}

int añadir_registro(FILE *f, const char *titulo, const char *ingredientes, const char *instrucciones, const char *enlace, const char *fuente, const char *entidades) {
    if (!f || !titulo || !ingredientes || !instrucciones) {
        errno = EINVAL;
        return -1;
    }

    if (flock(fileno(f), LOCK_EX) == -1) {
        perror("flock");
        return -1;
    }

    int ultimo_id = 0;
    char *line = NULL;
    size_t len = 0;
    rewind(f);
    
    if (getline(&line, &len, f) == -1) {
        ultimo_id = 0;
    } else {
        off_t last_pos = 0;
        while (getline(&line, &len, f) != -1) {
            char id_str[20];
            if (extract_nth_field(line, 1, id_str, sizeof(id_str))) {
                int current_id = atoi(id_str);
                if (current_id > ultimo_id) {
                    ultimo_id = current_id;
                }
            }
            last_pos = ftello(f);
        }
    }
    
    free(line);
    line = NULL;

    int nuevo_id = ultimo_id + 1;

    if (fseeko(f, 0, SEEK_END) != 0) {
        perror("fseek SEEK_END");
        flock(fileno(f), LOCK_UN);
        return -1;
    }

    off_t offset = ftello(f);
    if (offset == -1) {
        perror("ftello");
        flock(fileno(f), LOCK_UN);
        return -1;
    }

    char registro_completo[RESP_MAX];
    snprintf(registro_completo, sizeof(registro_completo), 
             "%d,\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"\n",
             nuevo_id,
             titulo,
             ingredientes,
             instrucciones,
             enlace ? enlace : "",
             fuente ? fuente : "user",
             entidades ? entidades : "[]");

    if (fprintf(f, "%s", registro_completo) < 0) {
        perror("fprintf");
        flock(fileno(f), LOCK_UN);
        return -1;
    }
    fflush(f);

    if (total_registros % 1000 == 0) {
        registros_cache = realloc(registros_cache, (total_registros + 1000) * sizeof(RegistroInfo));
    }
    registros_cache[total_registros].offset = offset;
    registros_cache[total_registros].length = strlen(registro_completo);
    total_registros++;

    insertar_indice(titulo, offset);

    flock(fileno(f), LOCK_UN);
    
    printf("Nueva receta añadida con ID: %d\n", nuevo_id);
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

    if (registros_cache) {
        free(registros_cache);
        registros_cache = NULL;
    }
    total_registros = 0;
}