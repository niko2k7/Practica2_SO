#ifndef DEFINITIONS_H
#define DEFINITIONS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/wait.h> 

// -------------- Relacionado a hash y a búsqueda
#define DATASET_FILENAME "../Dataset/all_movies_expanded.csv"
#define INDEX_FILENAME "../Dataset/hash_index.bin"
#define TABLE_SIZE 5000003     // número primo grande (número de buckets)
#define SEED 0xDEADBEEF        // semilla del hash
#define CHAR_LENGTH 15

#define MOVIE_NAME_SIZE 100
#define RESULT_SIZE 8192
#define NOT_READY 0
#define READY 1
#define EXIT 2

typedef struct {
    uint32_t hash;     // hash del Movie_Name
    uint64_t offset;   // posición (en bytes) en el CSV
    int32_t next;      // siguiente colisión (-1 si no hay)
} IndexEntry;

    // Funcion pública
uint32_t MurmurHash2(const void *key, int len, uint32_t seed);

// -------------- Relacionado a memoria compartida
#define SHM_KEY_DATA 12345  // Clave para la memoria compartida de mensajes

// Estructura de comunicación entre procesos (UI, Worker)
typedef struct {
    int flag;
    char movie_name[MOVIE_NAME_SIZE];
    char result[RESULT_SIZE];
} SharedData;


#endif
