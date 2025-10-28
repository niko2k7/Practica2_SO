#include <ctype.h>
#include "definitions.h"

// Creación del índice hash
void build_index(const char *csv_filename, const char *index_filename){
    // Se abre el dataset, en modo lectura
    FILE *csv = fopen(csv_filename, "r");
    if(!csv){
        perror("Error abirendo el csv.");
        exit(1);
    }

    // Se abre (y crea si no existe) el hash_index, en modo escritura en binario
    FILE *index = fopen(index_filename, "wb+");
    if(!index){
        perror("Error abirendo el csv.");
        fclose(csv);
        exit(1);
    }

    // Llenar la tabla con structs IndexEntry vacías
    IndexEntry empty = {0, 0, -1};
    for(int i=0; i<TABLE_SIZE; i++){
        fwrite(&empty, sizeof(IndexEntry), 1, index);
    }

    char line[8192]; // Buffer para la linea completa
    char *movie; 
    uint64_t offset;

    //Encabezado
    fgets(line, sizeof(line), csv);
    long colisiones = 0;

    while ((offset = ftell(csv)), fgets(line, sizeof(line), csv)) {
        // Copiamos la línea para extraer el nombre
        char temp[8192];
        strcpy(temp, line);

        // Se extrae el nombre de la película (2ra columna) ---
        char *token = strtok(temp, ","); // id
        movie = strtok(NULL, ",");       // title

        if (!movie) continue;

        // Quitar salto de línea
        movie[strcspn(movie, "\r\n")] = 0;

        // 🔹 Quitar comillas dobles si existen
        if (movie[0] == '"') {
            // Desplazar una posición hacia la izquierda (remueve la comilla inicial)
            memmove(movie, movie + 1, strlen(movie));
        }
        if (movie[strlen(movie) - 1] == '"') {
            // Remover la comilla final
            movie[strlen(movie) - 1] = '\0';
        }

        // Usamos solo 10 caracteres
        int len = strlen(movie);
        if (len>CHAR_LENGTH) len=CHAR_LENGTH;

        // Convertir a minúsculas antes de hacer el hash
        for (int i = 0; movie[i]; i++) {
            movie[i] = tolower((unsigned char)movie[i]);
        }

        // Hacemos hash
        uint32_t hash = MurmurHash2(movie, len, SEED);
        // Calculamos la posicion en la tabla
        uint32_t index_pos = hash % TABLE_SIZE;

        printf("Movie:%s |Hash:%u |Index:%u \n", movie, hash, index_pos);
        
        // Leer la entrada existente del bucket principal
        IndexEntry current_bucket;
        fseek(index, index_pos * sizeof(IndexEntry), SEEK_SET);
        fread(&current_bucket, sizeof(IndexEntry), 1, index);

        // Si el bucket está vacío, se inserta directamente
        if (current_bucket.next == -1 && current_bucket.hash == 0 && current_bucket.offset == 0) {
            IndexEntry new_entry = {hash, offset, -1,};
            fseek(index, index_pos * sizeof(IndexEntry), SEEK_SET);
            fwrite(&new_entry, sizeof(IndexEntry), 1, index);
        } else {
            colisiones++;

            // Guardamos la posición actual del bucket principal
            int32_t old_next = current_bucket.next;

            // Creamos la nueva entrada, que apunta al anterior 'next'
            IndexEntry new_entry = {hash, offset, old_next};

            // Escribimos al final del archivo
            fseek(index, 0, SEEK_END);
            long new_pos = ftell(index) / sizeof(IndexEntry);
            fwrite(&new_entry, sizeof(IndexEntry), 1, index);

            // Actualizamos el bucket principal para que su next apunte al nuevo registro
            current_bucket.next = (int32_t)new_pos;
            fseek(index, index_pos * sizeof(IndexEntry), SEEK_SET);
            fwrite(&current_bucket, sizeof(IndexEntry), 1, index);
        }
    }

    fclose(csv);
    fclose(index);
    printf("Índice hash creado correctamente.\n");
    printf("Colisiones:%lu \n", colisiones);
}

int main(){
    build_index(DATASET_FILENAME, INDEX_FILENAME);
    return 0;
}