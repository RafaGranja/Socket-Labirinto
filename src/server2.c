#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdbool.h>

#define PORT 51511 // Porta padrão
#define MAX_CLIENTS 1 // Número máximo de clientes suportados
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
#define WALL 0
#define PATH 1
#define ENTRY 2
#define EXIT 3
#define UNKNOWN 4
#define PLAYER 5

// Estrutura da mensagem
struct action {
    int type;
    int moves[100];
    int board[TAMANHO_LABIRINTO][TAMANHO_LABIRINTO];
};

typedef struct {
    int x, y;
    int path[100];
    int path_length;
} Node;

void enviaMapaCompleto(int client_socket, int board[TAMANHO_LABIRINTO][TAMANHO_LABIRINTO]) {
    struct action server_response = {0};
    server_response.type = ACTION_WIN;
    // Revelar todo o labirinto
    for (int i = 0; i < TAMANHO_LABIRINTO; i++) {
        for (int j = 0; j < TAMANHO_LABIRINTO; j++) {
            server_response.board[i][j] = board[i][j];
        }
    }
    // Enviar a resposta com o labirinto completo para o cliente
    send(client_socket, &server_response, sizeof(server_response), 0);
}

void enviaMapa(int client_socket, int board[TAMANHO_LABIRINTO][TAMANHO_LABIRINTO], int x, int y) {
    struct action server_response = {0};
    server_response.type = ACTION_UPDATE;

    // Percorrer o labirinto e decidir o que enviar
    for (int i = 0; i < TAMANHO_LABIRINTO; i++) {
        for (int j = 0; j < TAMANHO_LABIRINTO; j++) {
            // Revelar células dentro de um raio de 1 célula ao redor da posição do jogador
            if (abs(i - x) <= 1 && abs(j - y) <= 1) {
                server_response.board[i][j] = board[i][j]; // Revela o conteúdo da célula
            } else {
                server_response.board[i][j] = UNKNOWN; // Células fora do alcance permanecem ocultas
            }
        }
    }

    // Enviar a resposta com o labirinto parcial para o cliente
    send(client_socket, &server_response, sizeof(server_response), 0);
}

void carregaLabirinto(const char *filename, int board[TAMANHO_LABIRINTO][TAMANHO_LABIRINTO]) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Erro ao abrir o arquivo do labirinto");
        exit(EXIT_FAILURE);
    }

    int i = 0, j = 0;
    int ch;
    while ((ch = fgetc(file)) != EOF) {
        printf("%c",(ch));
        if (ch == '\n') {
            i++;
            j = 0; // Reiniciar a coluna para a nova linha
        } else if (ch >= 0 && ch <= 5) {
            printf("Vou inserir %d na posicao %d,%d",ch,i,j);
            board[i][j] = ch;
            j++;
        }
    }

    fclose(file);
}

void movimentosValidos(int board[TAMANHO_LABIRINTO][TAMANHO_LABIRINTO], int player_pos[2], int moves[100]) {
    int x = player_pos[0], y = player_pos[1];
    int idx = 0;

    if (x > 0 && board[x - 1][y] != WALL) moves[idx++] = 1; // Cima
    if (y < TAMANHO_LABIRINTO - 1 && board[x][y + 1] != WALL) moves[idx++] = 2; // Direita
    if (x < TAMANHO_LABIRINTO - 1 && board[x + 1][y] != WALL) moves[idx++] = 3; // Baixo
    if (y > 0 && board[x][y - 1] != WALL) moves[idx++] = 4; // Esquerda

    while (idx < 100) moves[idx++] = 0; // Preencher restante com 0
}

int atualizaPosicaoJogador(int board[TAMANHO_LABIRINTO][TAMANHO_LABIRINTO], int *x, int *y, int direction, int client_socket) {
    if(board[*x][*y]!=ENTRY && board[*x][*y]!=EXIT){
        board[*x][*y] = PATH; // Liberar posição atual
    }
    if (direction == 1 && *x > 0) {
        (*x)--; // Cima
    } else if (direction == 2 && *y < TAMANHO_LABIRINTO - 1) {
        (*y)++; // Direita
    } else if (direction == 3 && *x < TAMANHO_LABIRINTO - 1) {
        (*x)++; // Baixo
    } else if (direction == 4 && *y > 0) {
        (*y)--; // Esquerda
    }

    if (board[*x][*y] == EXIT) {
        enviaMapaCompleto(client_socket, board);
        return 0;
    }

    if(board[*x][*y]!=ENTRY && board[*x][*y]!=EXIT){
        board[*x][*y] = PLAYER;
    }

    return 1;
}

bool posicaoValida(int x, int y, int board[TAMANHO_LABIRINTO][TAMANHO_LABIRINTO], bool visited[TAMANHO_LABIRINTO][TAMANHO_LABIRINTO]) {
    return x >= 0 && x < TAMANHO_LABIRINTO && y >= 0 && y < TAMANHO_LABIRINTO && board[x][y] != WALL && !visited[x][y];
}

void buscaCaminho(int board[TAMANHO_LABIRINTO][TAMANHO_LABIRINTO], int start_x, int start_y, int moves[100]) {
    bool visited[TAMANHO_LABIRINTO][TAMANHO_LABIRINTO] = {false};
    Node queue[1000];
    int front = 0, rear = 0;

    Node start = {start_x, start_y, {0}, 0};
    queue[rear++] = start;
    visited[start_x][start_y] = true;

    int directions[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    int move_codes[4] = {1, 3, 4, 2};

    while (front < rear) {
        Node current = queue[front++];
        if (board[current.x][current.y] == EXIT) {
            memcpy(moves, current.path, sizeof(current.path));
            return;
        }

        for (int i = 0; i < 4; i++) {
            int new_x = current.x + directions[i][0];
            int new_y = current.y + directions[i][1];

            if (posicaoValida(new_x, new_y, board, visited)) {
                visited[new_x][new_y] = true;
                Node next = {new_x, new_y, {0}, current.path_length + 1};
                memcpy(next.path, current.path, sizeof(current.path));
                next.path[current.path_length] = move_codes[i];
                queue[rear++] = next;
            }
        }
    }

    moves[0] = 0; // No path found
}

void configuraServidor(const char *version, int port, struct sockaddr_storage *server_addr, socklen_t *addr_len) {
    if (strcmp(version, "v4") == 0) {
        struct sockaddr_in *addr = (struct sockaddr_in *)server_addr;
        *addr_len = sizeof(struct sockaddr_in);
        addr->sin_family = AF_INET;
        addr->sin_port = htons(port);
        addr->sin_addr.s_addr = INADDR_ANY;
    } else if (strcmp(version, "v6") == 0) {
        struct sockaddr_in6 *addr = (struct sockaddr_in6 *)server_addr;
        *addr_len = sizeof(struct sockaddr_in6);
        addr->sin6_family = AF_INET6;
        addr->sin6_port = htons(port);
        addr->sin6_addr = in6addr_any;
    } else {
        fprintf(stderr, "Versão de IP inválida: %s\n", version);
        exit(EXIT_FAILURE);
    }
}

void loopCliente(int client_socket, int labyrinth[TAMANHO_LABIRINTO][TAMANHO_LABIRINTO]) {
    struct action client_action, server_response; 
    int player_pos[2] = {0, 0}; // Posição inicial do jogador

    // Loop principal de comunicação
    while (1) {
        ssize_t bytes_received = recv(client_socket, &client_action, sizeof(client_action), 0);
        if (bytes_received <= 0) {
            printf("client desconnected\n");
            break;
        }

        memset(&server_response, 0, sizeof(server_response));
        switch (client_action.type) {
            case ACTION_START:
                printf("starting new game\n");
                server_response.type = ACTION_UPDATE;
                movimentosValidos(labyrinth, player_pos, server_response.moves);
                send(client_socket, &server_response, sizeof(server_response), 0);
                break;

            case ACTION_MOVE:
                if (client_action.moves[0] >= 1 && client_action.moves[0] <= 4) {
                    int verifica = atualizaPosicaoJogador(labyrinth, &player_pos[0], &player_pos[1], client_action.moves[0], client_socket);
                    if(verifica==1){
                        server_response.type = ACTION_UPDATE;
                        movimentosValidos(labyrinth, player_pos, server_response.moves);
                    }
                }
                send(client_socket, &server_response, sizeof(server_response), 0);
                break;

            case ACTION_MAP:
                server_response.type = ACTION_UPDATE;
                enviaMapa(client_socket, labyrinth, player_pos[0], player_pos[1]);
                break;

            case ACTION_HINT:
                server_response.type = ACTION_UPDATE;
                buscaCaminho(labyrinth, player_pos[0], player_pos[1], server_response.moves);
                send(client_socket, &server_response, sizeof(server_response), 0);
                break;

            case ACTION_RESET:
                printf("starting new game\n");
                for (int i = 0; i < TAMANHO_LABIRINTO; i++) {
                    for (int j = 0; j < TAMANHO_LABIRINTO; j++) {
                        if (labyrinth[i][j] == ENTRY) {
                            player_pos[0] = i;
                            player_pos[1] = j;
                        }
                        else if (labyrinth[i][j] == PLAYER){
                            labyrinth[i][j] = PATH;
                        }
                    }
                }
                server_response.type = ACTION_UPDATE;
                movimentosValidos(labyrinth, player_pos, server_response.moves);
                send(client_socket, &server_response, sizeof(server_response), 0);
                break;

            case ACTION_EXIT:
                printf("client disconnected\n");
                close(client_socket);
                return;

            default:
                break;
        }
    }

    close(client_socket);
}

int main(int argc, char *argv[]) {
    if (argc != 5 || strcmp(argv[3], "-i") != 0) {
        fprintf(stderr, "Uso: %s <v4/v6> <porta> -i <arquivo_labirinto>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *ip_version = argv[1];
    int port = atoi(argv[2]);
    char *labyrinth_file = argv[4];

    int labyrinth[TAMANHO_LABIRINTO][TAMANHO_LABIRINTO] = {WALL};

    // Carregar o labirinto do arquivo
    carregaLabirinto(labyrinth_file, labyrinth);

    int server_socket;
    struct sockaddr_storage server_addr;
    socklen_t addr_len;

    // Configurar endereço do servidor
    configuraServidor(ip_version, port, &server_addr, &addr_len);

    // Criar socket
    int domain = (strcmp(ip_version, "v4") == 0) ? AF_INET : AF_INET6;
    if ((server_socket = socket(domain, SOCK_STREAM, 0)) == -1) {
        perror("Erro ao criar o socket");
        return EXIT_FAILURE;
    }

    // Vincular o socket
    if (bind(server_socket, (struct sockaddr *)&server_addr, addr_len) == -1) {
        perror("Erro ao vincular o socket");
        close(server_socket);
        return EXIT_FAILURE;
    }

    // Iniciar escuta
    if (listen(server_socket, MAX_CLIENTS) == -1) {
        perror("Erro ao escutar");
        close(server_socket);
        return EXIT_FAILURE;
    }

    while (1) {
        struct sockaddr_storage client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_socket == -1) {
            perror("Erro ao aceitar conexão");
            continue;
        }
        printf("client connected\n");
        loopCliente(client_socket, labyrinth);
    }

    close(server_socket);
    return EXIT_SUCCESS;
}