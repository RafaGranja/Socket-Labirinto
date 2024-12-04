#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>

#define BUFFER_SIZE 1024 // Tamanho do buffer para mensagens
#define TAMANHO_LABIRINTO 10 // Tamanho máximo do labirinto

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
    int board[TAMANHO_LABIRINTO][TAMANHO_LABIRINTO];
};

void mostrarDica(int moves[100]) {
    printf("Hint: ");
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
    printf("\n");
}

void mostrarMovimentos(int moves[100]) {
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

void mostrarMapa(int board[TAMANHO_LABIRINTO][TAMANHO_LABIRINTO]) {
    printf("Mapa do labirinto:\n");
    for (int i = 0; i < TAMANHO_LABIRINTO; i++) {
        for (int j = 0; j < TAMANHO_LABIRINTO; j++) {
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

void enviaAction(int socket, int action_type, int move) {
    struct action client_action = {0};
    client_action.type = action_type;
    if (move > 0) client_action.moves[0] = move;

    if (send(socket, &client_action, sizeof(client_action), 0) == -1) {
        perror("Erro ao enviar dados");
    }
}

void configuraCliente(const char *server_ip, int port, struct sockaddr_storage *server_addr, socklen_t *addr_len) {
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; // Permite IPv4 ou IPv6
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(server_ip, NULL, &hints, &res) != 0) {
        perror("Erro ao resolver endereço");
        exit(EXIT_FAILURE);
    }

    if (res->ai_family == AF_INET) { // IPv4
        struct sockaddr_in *addr = (struct sockaddr_in *)server_addr;
        *addr_len = sizeof(struct sockaddr_in);
        addr->sin_family = AF_INET;
        addr->sin_port = htons(port);
        addr->sin_addr = ((struct sockaddr_in *)res->ai_addr)->sin_addr;
    } else if (res->ai_family == AF_INET6) { // IPv6
        struct sockaddr_in6 *addr = (struct sockaddr_in6 *)server_addr;
        *addr_len = sizeof(struct sockaddr_in6);
        addr->sin6_family = AF_INET6;
        addr->sin6_port = htons(port);
        addr->sin6_addr = ((struct sockaddr_in6 *)res->ai_addr)->sin6_addr;
    } else {
        fprintf(stderr, "Versão de IP não suportada\n");
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(res);
}

int movimentosValidos(int move, int valid_moves[100]) {
    for (int i = 0; i < 100 && valid_moves[i] != 0; i++) {
        if (valid_moves[i] == move) {
            return 1;
        }
    }
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <endereco_servidor> <porta>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *server_ip = argv[1];
    int port = atoi(argv[2]);

    int client_socket;
    struct sockaddr_storage server_addr;
    socklen_t addr_len;

    // Configurar endereço do servidor
    configuraCliente(server_ip, port, &server_addr, &addr_len);

    // Criar socket
    int domain = (server_addr.ss_family == AF_INET) ? AF_INET : AF_INET6;
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
    int game_started = 0; // Variável de controle para verificar se o jogo foi iniciado
    int valid_moves[100] = {0}; // Armazenar movimentos válidos

    // Loop principal
    while (1) {
        printf("> ");
        fgets(input, sizeof(input), stdin);
        input[strcspn(input, "\n")] = 0; // Remover o newline

        if (strcmp(input, "start") == 0) {
            enviaAction(client_socket, ACTION_START, 0);
            game_started = 1; // Marcar que o jogo foi iniciado
        } else if (!game_started) {
            printf("error: start the game first\n");
            continue;
        } else if (strcmp(input, "map") == 0) {
            enviaAction(client_socket, ACTION_MAP, 0);
        } else if (strcmp(input, "hint") == 0) {
            enviaAction(client_socket, ACTION_HINT, 0);
        } else if (strcmp(input, "reset") == 0) {
            enviaAction(client_socket, ACTION_RESET, 0);
        } else if (strcmp(input, "exit") == 0) {
            enviaAction(client_socket, ACTION_EXIT, 0);
            break;
        } else {
            int move = 0;
            if (strcmp(input, "up") == 0) move = 1;
            else if (strcmp(input, "right") == 0) move = 2;
            else if (strcmp(input, "down") == 0) move = 3;
            else if (strcmp(input, "left") == 0) move = 4;

            if (move > 0) {
                if (movimentosValidos(move, valid_moves)) {
                    enviaAction(client_socket, ACTION_MOVE, move);
                } else {
                    printf("error: you cannot go this way\n");
                    continue;
                }
            } else {
                printf("error: command not found\n");
                continue;
            }
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
                if(strcmp(input, "map") == 0){
                    mostrarMapa(server_response.board);
                }
                else if(strcmp(input, "hint") == 0){
                    mostrarDica(server_response.moves);
                }
                else{
                    mostrarMovimentos(server_response.moves);
                    memcpy(valid_moves, server_response.moves, sizeof(valid_moves)); // Atualizar movimentos válidos
                }
                break;

            case ACTION_WIN:
                printf("You escaped!\n");
                mostrarMapa(server_response.board);
                break;

            default:
                continue;
        }

    }

    close(client_socket);
    return EXIT_SUCCESS;
}