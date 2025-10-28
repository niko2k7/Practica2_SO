#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "definitions.h"

#define PORT 3535
#define BACKLOG 4

// Función de búsqueda (idéntica a la tuya)
void search_movie(const char *csv_filename, const char *index_filename, const char *movie_name, char *output) {
    FILE *csv = fopen(csv_filename, "r");
    if (!csv) { perror("Error abriendo CSV"); return; }

    FILE *index = fopen(index_filename, "rb");
    if (!index) { perror("Error abriendo índice"); fclose(csv); return; }

    char lower_name[256];
    strncpy(lower_name, movie_name, sizeof(lower_name));
    lower_name[sizeof(lower_name) - 1] = '\0';
    for (int i = 0; lower_name[i]; i++)
        lower_name[i] = tolower((unsigned char)lower_name[i]);

    int len = strlen(lower_name);
    if (len > CHAR_LENGTH) len = CHAR_LENGTH;

    uint32_t hash = MurmurHash2(lower_name, len, SEED);
    uint32_t index_pos = hash % TABLE_SIZE;

    IndexEntry entry;
    fseek(index, index_pos * sizeof(IndexEntry), SEEK_SET);
    fread(&entry, sizeof(IndexEntry), 1, index);

    while (1) {
        if (entry.hash == hash) {
            char line[8192];
            fseek(csv, entry.offset, SEEK_SET);
            if (fgets(line, sizeof(line), csv)) {
                snprintf(output, RESULT_SIZE, "%s", line);
                printf("\nPelícula encontrada: %s", line);
            } else {
                snprintf(output, RESULT_SIZE, "Error al leer línea en CSV.\n");
            }
            fclose(csv); fclose(index);
            return;
        }

        if (entry.next == -1) break;
        fseek(index, entry.next * sizeof(IndexEntry), SEEK_SET);
        fread(&entry, sizeof(IndexEntry), 1, index);
    }

    snprintf(output, RESULT_SIZE, "NA (Película No Encontrada)");
    fclose(csv);
    fclose(index);
}

int main() {
    int server_fd, client_fd, r;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_size = sizeof(client_addr);
    SharedData shared;

    // Crear socket TCP
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    // Configurar estructura del servidor
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");  // LOCALHOST

    // Asociar socket a puerto
    r = bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (r < 0) { perror("bind"); close(server_fd); return 1; }

    // Escuchar conexiones
    r = listen(server_fd, BACKLOG);
    if (r < 0) { perror("listen"); close(server_fd); return 1; }
    printf("Servidor TCP escuchando en el puerto %d...\n", PORT);

    // Esperar conexión
    client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_size);
    if (client_fd < 0) { perror("accept"); close(server_fd); return 1; }
    printf("Cliente conectado.\n");

    // Bucle principal
    while (1) {
        r = recv(client_fd, &shared, sizeof(shared), 0);
        if (r <= 0) {
            printf("Cliente desconectado.\n");
            break;
        }

        // Procesar solicitud
        if (shared.flag == EXIT) {
            printf("Cliente solicitó salida.\n");
            break;
        }

        printf("Buscando: %s\n", shared.movie_name);
        search_movie(DATASET_FILENAME, INDEX_FILENAME, shared.movie_name, shared.result);
        shared.flag = NOT_READY;

        // Enviar resultado
        r = send(client_fd, &shared, sizeof(shared), 0);
        if (r < 0) perror("send");
    }

    close(client_fd);
    close(server_fd);
    printf("Servidor cerrado.\n");
    return 0;
}
