#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <time.h>
#include "definitions.h"
#include <ctype.h>

#include <stdio.h>
#include <string.h>    // Para bzero
#include <sys/socket.h>
#include <netinet/in.h> // Para sockaddr_in
#include <arpa/inet.h>  // Para inet_addr




int main(){
    key_t key = SHM_KEY_DATA;
    int shmid;
    SharedData shared;

    // -------------------------------------------------------------------------------

     // Definir la estructura para la dirección del servidor
    struct sockaddr_in server_addr;

    // Crear el socket (TCP)
    // Usamos SOCK_STREAM para TCP
    int client_fd = socket(AF_INET, SOCK_STREAM, 0); 
    if (client_fd < 0) {
        perror("Error al crear el socket");
        return 1;
    }

    // Inicializar la estructura del servidor
    bzero(&server_addr, sizeof(server_addr)); // Limpiar la estructura

    server_addr.sin_family = AF_INET;
    // Asignar el puerto, convirtiendo a orden de bytes de red
    server_addr.sin_port = htons(PORT);
    // Asignar la IP, convirtiendo de string a formato numérico de red
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    // Conectarse al servidor 
    int r = connect(client_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (r < 0) {
        perror("Error en el connect");
        close(client_fd);
        return 1;
    }
    printf("Conectado al servidor en el puerto %d\n", PORT);

    // -------------------------------------------------------------------------------
    // Inicializamos el flag
    shared.flag = NOT_READY;

    int opcion = 0;
    char movie_name[MOVIE_NAME_SIZE];

    while (opcion != 4) {
        printf("\n=== Menú ===\n");
        printf("1. Ingresar nombre de película\n");
        printf("2. Buscar\n");
        printf("3. Escribir un registro\n");
        printf("4. Salir\n");
        printf("Seleccione opción: ");
        scanf("%d", &opcion);
        getchar(); // limpiar buffer

        switch (opcion) {
            case 1:
                printf("Ingrese el nombre de la película: ");
                fgets(movie_name, sizeof(movie_name), stdin);
                movie_name[strcspn(movie_name, "\r\n")] = 0;
                strncpy(shared.movie_name, movie_name, MOVIE_NAME_SIZE);
                printf("Película '%s' lista para buscar.\n", movie_name);
                break;

            case 2:

                struct timespec start, end;
                clock_gettime(CLOCK_MONOTONIC, &start); // inicio

                shared.flag = READY;

                //-------------------------------------------------------------------------------

                // Enviar un mensaje al servidor
                r = send(client_fd, &shared, sizeof(shared), 0);
                if (r < 0) {
                    perror("Error en el send");
                }
                printf("Esperando resultado...\n");
                // Recibir el mensaje del servidor
                r = recv(client_fd, &shared, sizeof(shared), 0);
                if (r < 0) {
                    perror("Error en el recv");
                } else {
                    printf("Mensaje recibido del servidor: \n");
                }

                printf("\n=== Resultado ===\n%s\n", shared.result);

                clock_gettime(CLOCK_MONOTONIC, &end); // fin
                double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

                printf("Tiempo total de búsqueda: %.6f segundos\n", elapsed);
                break;

            case 3:
                printf("Ingrese el nuevo registro deseado en el siguiente formato: \n");
                printf("ID,Movie_Name,Original_title,Tagline,Overview,Year,Director,Runtime,Genres,Vote_average,Vote_count\n");
                
                char new_record[RESULT_SIZE];
                fgets(new_record, sizeof(new_record), stdin);
                new_record[strcspn(new_record, "\r\n")] = 0; // eliminar salto de línea

                FILE *csv = fopen(DATASET_FILENAME, "a+");
                if(!csv) {
                    perror("Error abriendo CSV para escritura");
                    break;
                }
                // añadimos al final del archivo la nueva cadena;
                fseek(csv, 0, SEEK_END);
                uint64_t offset = ftell(csv);
                fprintf(csv, "%s\n", new_record);
                fclose(csv);


                // -----------------------------------------------------------|
                //ahora encarguemonos de actualizar el índice.
                char temp[8192];
                strcpy(temp, new_record);

                // Se extrae el nombre de la película (2ra columna) ---
                char *token = strtok(temp, ","); // id
                char *movie = strtok(NULL, ","); // title

                if (!movie) {
                    printf("Error: No se pudo extraer el nombre de la película del registro.\n");
                    break;
                }

                // Quitar salto de línea
                movie[strcspn(movie, "\r\n")] = 0;

                // Quitar comillas dobles si existen
                if (movie[0] == '"') {
                    // Desplazar una posición hacia la izquierda (remueve la comilla inicial)
                    memmove(movie, movie + 1, strlen(movie));
                }
                if (movie[strlen(movie) - 1] == '"') {
                    // Remover la comilla final
                    movie[strlen(movie) - 1] = '\0';
                }

                // Usamos solo 15 caracteres
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

                FILE *index = fopen(INDEX_FILENAME, "rb+");
                if (!index) {
                    perror("Error abriendo el archivo de índice");
                    break;
                }
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
                fclose(index);

                // -----------------------------------------------------------|
                break;

            case 4:
                printf("Saliendo...\n");
                shared.flag = EXIT;
                send(client_fd, &shared, sizeof(shared), 0);
                break;


            default:
                printf("Opción no válida.\n");
        }
    }

    close(client_fd);
    return 0;
}
