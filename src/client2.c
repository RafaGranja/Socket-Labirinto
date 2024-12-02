#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024     // Tamanho do buffer para mensagens
#define LABYRINTH_SIZE 10    // Tamanho máximo do labirinto

// Tipos de ações
#define ACTION_START 0
#define ACTION_MOVE 1
#define ACTION_MAP 2
#define ACTION_HINT 3
#define ACTION_UPDATE 4
#define ACTION_WIN 5
#define ACTION_RESET 6
#define ACTION_EXIT 7

// Representação do labirinto
#define WALL '#'
#define PATH '_'
#define ENTRY '>'
#define EXIT 'X'
#define UNKNOWN '?'
#define PLAYER '+'

// Estrutura da mensagem
struct action {
    int type;
    int moves[100];
    int board[LABYRINTH_SIZE][LABYRINTH_SIZE];
};


void display_moves(int moves[100]) {
    printf("Possible moves: ");
    int first = 1;

    for (int i = 0; i < 100 && moves[i] != 0; i++) {
        if (!first) printf(", ");
        first = 0;

        switch (moves[i]) {
            case 1: printf("up"); break;
            case 2: printf("right"); break;
            case 3: printf("down"); break;
            case 4: printf("left"); break;
        }
    }
    printf(".\n");
}

void display_board(int board[LABYRINTH_SIZE][LABYRINTH_SIZE]) {
    printf("Mapa do labirinto:\n");
    for (int i = 0; i < LABYRINTH_SIZE; i++) {
        for (int j = 0; j < LABYRINTH_SIZE; j++) {
            char symbol;
            switch (board[i][j]) {
                case 0: symbol = WALL; break;
                case 1: symbol = PATH; break;
                case 2: symbol = ENTRY; break;
                case 3: symbol = EXIT; break;
                case 4: symbol = UNKNOWN; break;
                case 5: symbol = PLAYER; break;
                default: symbol = '?'; // Caso inesperado
            }
            printf("%c\t", symbol);
        }
        printf("\n");
    }
}

void send_action(int socket, int action_type, int move) {
    struct action client_action = {0};
    client_action.type = action_type;
    if (move > 0) client_action.moves[0] = move;

    if (send(socket, &client_action, sizeof(client_action), 0) == -1) {
        perror("Erro ao enviar dados");
    }
}


void configure_client_address(const char *ip_version, const char *server_ip, int port, struct sockaddr_storage *server_addr, socklen_t *addr_len) {
    if (strcmp(ip_version, "v4") == 0) {
        struct sockaddr_in *addr = (struct sockaddr_in *)server_addr;
        *addr_len = sizeof(struct sockaddr_in);
        addr->sin_family = AF_INET;
        addr->sin_port = htons(port);
        if (inet_pton(AF_INET, server_ip, &addr->sin_addr) <= 0) {
            perror("Endereço IP inválido");
            exit(EXIT_FAILURE);
        }
    } else if (strcmp(ip_version, "v6") == 0) {
        struct sockaddr_in6 *addr = (struct sockaddr_in6 *)server_addr;
        *addr_len = sizeof(struct sockaddr_in6);
        addr->sin6_family = AF_INET6;
        addr->sin6_port = htons(port);
        if (inet_pton(AF_INET6, server_ip, &addr->sin6_addr) <= 0) {
            perror("Endereço IPv6 inválido");
            exit(EXIT_FAILURE);
        }
    } else {
        fprintf(stderr, "Versão de IP inválida: %s\n", ip_version);
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <v4/v6> <endereco_servidor> <porta>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *ip_version = argv[1];
    const char *server_ip = argv[2];
    int port = atoi(argv[3]);

    int client_socket;
    struct sockaddr_storage server_addr;
    socklen_t addr_len;

    // Configurar endereço do servidor
    configure_client_address(ip_version, server_ip, port, &server_addr, &addr_len);

    // Criar socket
    int domain = (strcmp(ip_version, "v4") == 0) ? AF_INET : AF_INET6;
    if ((client_socket = socket(domain, SOCK_STREAM, 0)) == -1) {
        perror("Erro ao criar o socket");
        return EXIT_FAILURE;
    }

    // Conectar ao servidor
    if (connect(client_socket, (struct sockaddr *)&server_addr, addr_len) == -1) {
        perror("Erro ao conectar ao servidor");
        close(client_socket);
        return EXIT_FAILURE;
    }

    printf("Conectado ao servidor\n");

    struct action server_response;
    char input[BUFFER_SIZE];

    // Loop principal
    while (1) {
        printf("> ");
        fgets(input, sizeof(input), stdin);
        input[strcspn(input, "\n")] = 0; // Remover o newline

        if (strcmp(input, "start") == 0) {
            send_action(client_socket, ACTION_START, 0);
        } else if (strncmp(input, "move", 4) == 0) {
            char *direction = input + 5;
            int move = 0;

            if (strcmp(direction, "up") == 0) move = 1;
            else if (strcmp(direction, "right") == 0) move = 2;
            else if (strcmp(direction, "down") == 0) move = 3;
            else if (strcmp(direction, "left") == 0) move = 4;
            else {
                printf("error: command not found\n");
                continue;
            }

            send_action(client_socket, ACTION_MOVE, move);
        } else if (strcmp(input, "map") == 0) {
            send_action(client_socket, ACTION_MAP, 0);
        } else if (strcmp(input, "hint") == 0) {
            send_action(client_socket, ACTION_HINT, 0);
        } else if (strcmp(input, "reset") == 0) {
            send_action(client_socket, ACTION_RESET, 0);
        } else if (strcmp(input, "exit") == 0) {
            send_action(client_socket, ACTION_EXIT, 0);
            break;
        } else {
            printf("error: command not found\n");
            continue;
        }

        // Receber resposta do servidor
        ssize_t bytes_received = recv(client_socket, &server_response, sizeof(server_response), 0);
        if (bytes_received <= 0) {
            printf("Conexão com o servidor encerrada\n");
            break;
        }

        // Processar resposta
        switch (server_response.type) {
            case ACTION_UPDATE:
                display_moves(server_response.moves);
                break;

            case ACTION_WIN:
                printf("You escaped!\n");
                display_board(server_response.board);
                break;

            default:
                printf("Resposta desconhecida do servidor\n");
        }
    }

    close(client_socket);
    return EXIT_SUCCESS;
}
