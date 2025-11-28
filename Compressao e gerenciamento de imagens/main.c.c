#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//defines
#define MAX_NOME 100
#define ARQUIVO_DADOS "meu_banco.dat"
#define ARQUIVO_INDICE "meu_banco.idx"
#define TAMANHO_BUFFER_COPIA 1024 // para compactacao o(1)


// guarda os metadados de cada imagem
typedef struct
{
    char nome[MAX_NOME];
    int limiar;
    int largura;
    int altura;
    long posicao;
    long tamanhoDados;
    int removido; // para remocao logica
} RegistroIndice;


//-------------------declaracoes de funcoes (prototipos)-------------------

//funcoes do bnco de dados (db)
void db_add_image();
void db_list_images();
void db_remove_image();
void db_extract_image();
void db_compact();
void db_rebuild_mean();


//compressoes
long compress_rle(FILE *arq_dados, int **matriz, int largura, int altura);
int** decompress_rle(FILE *arq_dados, int largura, int altura);

//trabalho com as imagens PGM
int** read_pgm(const char* nomeArquivo, int *largura, int *altura, int *maxVal);
void save_pgm(const char* nomeArquivo, int **matriz, int largura, int altura);
void binarize(int **matriz, int largura, int altura, int limiar);

//auxiliares
void free_matrix(int **matriz, int altura);
void skip_pgm_comments(FILE *arq);


//main
int main() {
    int opcao = -1;
    
    do {
        printf("\n--- Gerenciador de imagens ---\n");
        printf("1. Adicionar imagem\n");
        printf("2. Listar imagens\n");
        printf("3. Remover Imagem (Logico)\n");
        printf("4. Extrair Imagem (para PGM)\n");
        printf("5. Compactar Banco (Fisico O(1))\n");
        printf("6. Reconstruir Media (Bonus)\n");
        printf("0. Sair\n");
        printf("Escolha uma opcao: ");
        
        if (scanf("%d", &opcao) != 1) {
            // limpa o buffer se o usuario digitar algo errado
            while(getchar() != '\n'); 
            opcao = -1;
        }

        switch (opcao) {
            case 1:
                db_add_image();
                break;
            case 2:
                db_list_images();
                break;
            case 3:
                db_remove_image();
                break;
            case 4:
                db_extract_image(); 
                break;
            case 5:
                db_compact(); 
                break;
            case 6:
                db_rebuild_mean();
                break;
            case 0:
                printf("Encerrando...\n");
                break;
            default:
                printf("Opcao invalida!\n");
        }
        
    } while (opcao != 0);
    
    return 0;
}



// ----------------------------------------------------
// implementacao das funcoes
// ----------------------------------------------------


// le, binariza e salva a imagem
void db_add_image() {
    char arqPGM[MAX_NOME], nomeImg[MAX_NOME];
    int limiar;
    
    printf("Digite o caminho da imagem PGM: ");
    scanf("%s", arqPGM);
    printf("Digite um nome para o banco: ");
    scanf("%s", nomeImg);
    printf("Digite o limiar (0-255): ");
    scanf("%d", &limiar);
    
    int largura, altura, maxVal;
    
    int **matriz = read_pgm(arqPGM, &largura, &altura, &maxVal);
    if (!matriz) {
        fprintf(stderr, "ERRO: A imagem nao foi adicionada.\n"); 
        return;
    }
    
    binarize(matriz, largura, altura, limiar);
    
    FILE *arq_dados = fopen(ARQUIVO_DADOS, "ab"); 
    if (!arq_dados) {
        perror("Erro ao abrir banco de dados");
        free_matrix(matriz, altura);
        return;
    }
    
    // pega a posicao exata em bytes onde vamos escrever
    long posicaoAtual = ftell(arq_dados);
    
    long tamanhoEscrito = compress_rle(arq_dados, matriz, largura, altura);
    fclose(arq_dados);
    
    // agora salva a entrada no indice
    FILE *arq_indice = fopen(ARQUIVO_INDICE, "a");
    if (!arq_indice) {
        perror("Erro grave: dados salvos, mas o indice falhou");
        free_matrix(matriz, altura);
        return;
    }
    
    // o '0' no final e o flag 'removido'
    fprintf(arq_indice, "%s %d %d %d %ld %ld %d\n", 
            nomeImg, limiar, largura, altura, posicaoAtual, tamanhoEscrito, 0);
    fclose(arq_indice);

    printf("Imagem '%s' adicionada com sucesso!\n", nomeImg);
    free_matrix(matriz, altura);
}

// le o arquivo de indice e imprime na tela
void db_list_images() {
    FILE *arq_indice = fopen(ARQUIVO_INDICE, "r");
    if (!arq_indice) {
        printf("Banco de dados esta vazio.\n");
        return;
    }
    
    RegistroIndice reg;
    int id = 0;
    printf("\n--- LISTA DE IMAGENS ---\n");
    printf("ID | NOME | LIMIAR | TAMANHO | STATUS\n");
    
    while (fscanf(arq_indice, "%s %d %d %d %ld %ld %d", 
                  reg.nome, &reg.limiar, &reg.largura, &reg.altura, 
                  &reg.posicao, &reg.tamanhoDados, &reg.removido) == 7) {
        printf("%d | %s | %d | %dx%d | %s\n", 
               id, reg.nome, reg.limiar, reg.largura, reg.altura, 
               reg.removido ? "REMOVIDO" : "Ativo");
        id++;
    }
    fclose(arq_indice);
}

// faz a remocao logica (apenas marca o flag)
void db_remove_image() {
    db_list_images();
    int id;
    printf("Digite o ID da imagem para remover (logico): ");
    scanf("%d", &id);

    FILE *arq_indice = fopen(ARQUIVO_INDICE, "r");
    FILE *temp_indice = fopen("temp.idx", "w");
    if (!arq_indice || !temp_indice) {
        perror("Erro ao abrir arquivos para remocao");
        return;
    }
    
    RegistroIndice reg;
    int idAtual = 0;
    int encontrado = 0;

    while (fscanf(arq_indice, "%s %d %d %d %ld %ld %d", 
                  reg.nome, &reg.limiar, &reg.largura, &reg.altura, 
                  &reg.posicao, &reg.tamanhoDados, &reg.removido) == 7) {
        
        // marca a linha certa como removida
        if (idAtual == id) {
            reg.removido = 1;
            encontrado = 1;
        }
        
        fprintf(temp_indice, "%s %d %d %d %ld %ld %d\n", 
                reg.nome, reg.limiar, reg.largura, reg.altura, 
                reg.posicao, reg.tamanhoDados, reg.removido);
        idAtual++;
    }
    
    fclose(arq_indice);
    fclose(temp_indice);
    
    if (encontrado) {
        // substitui o arquivo antigo pelo novo
        remove(ARQUIVO_INDICE);
        rename("temp.idx", ARQUIVO_INDICE);
        printf("Imagem ID %d marcada como removida.\n", id);
    } else {
        remove("temp.idx"); 
        printf("ID %d nao encontrado.\n", id);
    }
}

// pega uma imagem do banco e salva como pgm
void db_extract_image() {
    db_list_images();
    int id;
    printf("Digite o ID da imagem para extrair: ");
    scanf("%d", &id);
    
    FILE *arq_indice = fopen(ARQUIVO_INDICE, "r");
    if (!arq_indice) {
        printf("Banco vazio.\n"); 
        return;
    }
    
    RegistroIndice reg;
    int idAtual = 0;
    int encontrado = 0;

    // acha o registro no indice
    while (fscanf(arq_indice, "%s %d %d %d %ld %ld %d", 
                  reg.nome, &reg.limiar, &reg.largura, &reg.altura, 
                  &reg.posicao, &reg.tamanhoDados, &reg.removido) == 7) {
        if (idAtual == id) {
            encontrado = 1;
            break;
        }
        idAtual++;
    }
    fclose(arq_indice);
    
    if (!encontrado) {
        printf("ID %d nao encontrado.\n", id);
        return;
    }
    if (reg.removido) {
        printf("Imagem ID %d esta removida e nao pode ser extraida.\n", id);
        return;
    }
    
    FILE *arq_dados = fopen(ARQUIVO_DADOS, "rb");
    if (!arq_dados) {
        perror("Erro ao abrir banco de dados");
        return;
    }
    
    // pula direto para a posicao da imagem no arquivo binario
    fseek(arq_dados, reg.posicao, SEEK_SET);
    
    int **matriz = decompress_rle(arq_dados, reg.largura, reg.altura);
    fclose(arq_dados);
    
    if (matriz) {
        char nomeSaida[MAX_NOME + 30]; 
        snprintf(nomeSaida, sizeof(nomeSaida), "extraida_%s_%d.pgm", reg.nome, reg.limiar);
        
        save_pgm(nomeSaida, matriz, reg.largura, reg.altura);
        free_matrix(matriz, reg.altura);
        printf("Imagem salva como '%s'\n", nomeSaida);
    }
}

// remove fisicamente os dados marcados
void db_compact() {
    printf("Iniciando compactacao (Espaco O(1))...\n");
    
    FILE *arq_indice = fopen(ARQUIVO_INDICE, "r");
    FILE *arq_dados = fopen(ARQUIVO_DADOS, "rb");
    FILE *novo_indice = fopen("novo.idx", "w");
    FILE *novo_dados = fopen("novo.dat", "wb");
    
    if (!arq_indice || !arq_dados || !novo_indice || !novo_dados) {
        perror("Erro ao criar arquivos temporarios para compactacao");
        if(arq_indice) fclose(arq_indice);
        if(arq_dados) fclose(arq_dados);
        if(novo_indice) fclose(novo_indice);
        if(novo_dados) fclose(novo_dados);
        return;
    }
    
    RegistroIndice reg;
    char buffer[TAMANHO_BUFFER_COPIA]; // buffer de 1k (espaco o(1))
    
    while (fscanf(arq_indice, "%s %d %d %d %ld %ld %d", 
                  reg.nome, &reg.limiar, &reg.largura, &reg.altura, 
                  &reg.posicao, &reg.tamanhoDados, &reg.removido) == 7) {
        
        if (!reg.removido) {
            long novaPosicao = ftell(novo_dados);
            fseek(arq_dados, reg.posicao, SEEK_SET);
            
            // copia os bytes brutos, em blocos, sem descomprimir
            // isso garante o requisito de espaco o(1)
            long bytesParaCopiar = reg.tamanhoDados;
            while (bytesParaCopiar > 0) {
                size_t bytesLidos = (bytesParaCopiar > TAMANHO_BUFFER_COPIA) ? 
                                    TAMANHO_BUFFER_COPIA : bytesParaCopiar;
                
                fread(buffer, 1, bytesLidos, arq_dados);
                fwrite(buffer, 1, bytesLidos, novo_dados);
                bytesParaCopiar -= bytesLidos;
            }
            
            // salva a entrada atualizada no novo indice
            fprintf(novo_indice, "%s %d %d %d %ld %ld %d\n", 
                    reg.nome, reg.limiar, reg.largura, reg.altura, 
                    novaPosicao, reg.tamanhoDados, 0);
        } else {
            printf("Removendo fisicamente: %s (Limiar %d)\n", reg.nome, reg.limiar);
        }
    }

    fclose(arq_indice);
    fclose(arq_dados);
    fclose(novo_indice);
    fclose(novo_dados);
    
    remove(ARQUIVO_DADOS);
    remove(ARQUIVO_INDICE);
    rename("novo.dat", ARQUIVO_DADOS);
    rename("novo.idx", ARQUIVO_INDICE);
    
    printf("Compactacao concluida!\n");
}

void db_rebuild_mean() {
    char nome[MAX_NOME], saida[MAX_NOME];
    
    printf("Digite o nome da imagem para reconstruir: ");
    scanf("%s", nome);
    
    printf("Digite o nome do arquivo de saida (ex: reconstruida.pgm): ");
    scanf("%s", saida);

    FILE *idx = fopen(ARQUIVO_INDICE, "r");
    if (!idx) { 
        printf("Nenhum indice encontrado.\n"); 
        return; 
    }
    
    RegistroIndice reg;
    int **soma = NULL; // matriz para somar os pixels
    int largura = 0, altura = 0, count = 0;
    
    //varre o indice procurando imagens com o nome certo
    while (fscanf(idx, "%s %d %d %d %ld %ld %d",
                  reg.nome, &reg.limiar, &reg.largura, &reg.altura, 
                  &reg.posicao, &reg.tamanhoDados, &reg.removido) == 7) {
        
        // se o nome bate e nao esta removido
        if (strcmp(reg.nome, nome) == 0 && !reg.removido) {
            FILE *dados = fopen(ARQUIVO_DADOS, "rb");
            if (!dados) { 
                perror("Erro ao abrir banco.dat para reconstrucao"); 
                continue; 
            }

            fseek(dados, reg.posicao, SEEK_SET);
            
            int **img = decompress_rle(dados, reg.largura, reg.altura);
            fclose(dados);

            if (!img) {
                fprintf(stderr, "Falha na descompressao de uma versao da imagem %s. Pulando.\n", reg.nome);
                continue;
            }
            
            // se for a primeira imagem encontrada, aloca a matriz 'soma'
            if (!soma) {
                largura = reg.largura;
                altura = reg.altura;
                soma = malloc(altura * sizeof(int *));
                if (!soma) { 
                    free_matrix(img, altura); 
                    perror("Alocacao inicial falhou"); 
                    break; 
                }
                
                for (int i = 0; i < altura; i++) {
                    soma[i] = malloc(largura * sizeof(int));
                    if (!soma[i]) { 
                        free_matrix(soma, i); 
                        free_matrix(img, altura); 
                        perror("Alocacao inicial falhou"); 
                        soma = NULL;
                        break; 
                    }
                    for (int j = 0; j < largura; j++)
                        soma[i][j] = 0;
                }
                if (!soma) break;
            }
            
            //verifica se as outras imagens tem o mesmo tamanho
            if (reg.largura != largura || reg.altura != altura) {
                fprintf(stderr, "Aviso: Imagem %s (Limiar %d) tem dimensoes diferentes. Ignorada.\n", 
                        reg.nome, reg.limiar);
                free_matrix(img, altura);
                continue;
            }
            
            //soma os pixels da imagem na matriz 'soma'
            for (int i = 0; i < altura; i++)
                for (int j = 0; j < largura; j++)
                    soma[i][j] += img[i][j];
            
            free_matrix(img, altura);
            count++;
        }
    }
    
    fclose(idx);

    if (count == 0 || !soma) { 
        printf("Nenhuma versao valida e nao removida encontrada para a imagem '%s'.\n", nome); 
        if (soma) free_matrix(soma, altura);
        return; 
    }
    
    // aloca a matriz 'media' e calcula a media
    int **media = malloc(altura * sizeof(int *));
    if (!media) { 
        free_matrix(soma, altura); 
        perror("Alocacao da media falhou"); 
        return; 
    }
    
    printf("(Info: Calculando media de %d versoes...)\n", count);
    
    for (int i = 0; i < altura; i++) {
        media[i] = malloc(largura * sizeof(int));
        if (!media[i]) { 
            free_matrix(media, i); 
            free_matrix(soma, altura); 
            perror("Alocacao da media falhou"); 
            return; 
        }
        
        for (int j = 0; j < largura; j++)
            media[i][j] = (soma[i][j] * 2 + count) / (count * 2); 
            
        free(soma[i]);
    }
    free(soma);

    //salva a imagem media em PGM
    save_pgm(saida, media, largura, altura);

    free_matrix(media, altura); 
    
    printf("Reconstrucao concluida (%d versoes usadas) e salva em '%s'.\n", count, saida);
}


// --- Funcoes de Compressao ---

// algoritmo run-length encoding (rle)
long compress_rle(FILE *arq_dados, int **matriz, int largura, int altura) {
    long bytesEscritos = 0;
    int pixelAtual = matriz[0][0]; // pega o primeiro pixel
    long contagem = 0;

    fwrite(&pixelAtual, sizeof(int), 1, arq_dados);
    bytesEscritos += sizeof(int);

    for (int i = 0; i < altura; i++) {
        for (int j = 0; j < largura; j++) {
            if (matriz[i][j] == pixelAtual) {
                contagem++;
            } else {
                // se trocou o pixel, salva a contagem anterior
                fwrite(&contagem, sizeof(long), 1, arq_dados);
                bytesEscritos += sizeof(long);
                pixelAtual = matriz[i][j];
                contagem = 1;
            }
        }
    }
    
    // salva a ultima contagem
    fwrite(&contagem, sizeof(long), 1, arq_dados);
    bytesEscritos += sizeof(long);

    return bytesEscritos;
}

// le os dados rle e recria a matriz
int** decompress_rle(FILE *arq_dados, int largura, int altura) {
    int pixelAtual;
    long contagem;
    
    int **matriz = malloc(altura * sizeof(int *));
    for (int i = 0; i < altura; i++) {
        matriz[i] = malloc(largura * sizeof(int));
    }
    
    if (fread(&pixelAtual, sizeof(int), 1, arq_dados) != 1) {
        fprintf(stderr, "Erro ao ler pixel inicial na descompressao.\n");
        free_matrix(matriz, altura);
        return NULL;
    }

    int x = 0, y = 0;
    
    while (fread(&contagem, sizeof(long), 1, arq_dados) == 1) {
        for (long i = 0; i < contagem; i++) {
            matriz[x][y] = pixelAtual;
            
            // avanca o 'cursor' da matriz
            y++;
            if (y == largura) {
                y = 0;
                x++;
            }
            if (x == altura) break;
        }
        if (x == altura) break;
        
        pixelAtual = 1 - pixelAtual; // inverte o pixel
    }
    
    return matriz;
}


// --- Funcoes de manipulacao de PGM ---

// funcao recursiva para pular comentarios em pgm
void skip_pgm_comments(FILE *arq) {
    int ch;
    ch = fgetc(arq);

    while (ch == ' ' || ch == '\t' || ch == '\n') {
        ch = fgetc(arq);
    }

    if (ch == '#') { 
        while (ch != '\n' && ch != EOF) {
            ch = fgetc(arq);
        }
        skip_pgm_comments(arq);
    } else {
        ungetc(ch, arq); // devolve o caractere
    }
}

int** read_pgm(const char* nomeArquivo, int *largura, int *altura, int *maxVal) {
    FILE *arq = fopen(nomeArquivo, "r");
    if (!arq) {
        perror("ERRO DETALHADO (read_pgm -> fopen)");
        return NULL;
    }

    char tipo[3];
    if (fscanf(arq, "%2s", tipo) != 1) {
        fprintf(stderr, "ERRO DETALHADO (read_pgm -> fscanf tipo): Arquivo vazio ou corrompido.\n");
        fclose(arq);
        return NULL;
    }

    // verifica o 'magic number' p2
    if (strcmp(tipo, "P2") != 0) {
        fprintf(stderr, "ERRO DETALHADO (read_pgm): Formato PGM esperado e 'P2', mas o arquivo e '%s'.\n", tipo);
        fclose(arq);
        return NULL;
    }

    skip_pgm_comments(arq);

    if (fscanf(arq, "%d %d %d", largura, altura, maxVal) != 3) {
        fprintf(stderr, "ERRO DETALHADO (read_pgm -> fscanf dimensoes): Erro ao ler largura, altura ou maxVal.\n");
        fclose(arq);
        return NULL;
    }

    skip_pgm_comments(arq);

    int **matriz = malloc((*altura) * sizeof(int *));
    if (!matriz) {
        perror("ERRO DETALHADO (read_pgm -> malloc linhas)");
        fclose(arq);
        return NULL;
    }

    for (int i = 0; i < *altura; i++) {
        matriz[i] = malloc((*largura) * sizeof(int));
        if (!matriz[i]) {
            perror("ERRO DETALHADO (read_pgm -> malloc colunas)");
            free_matrix(matriz, i);
            fclose(arq);
            return NULL;
        }

        for (int j = 0; j < *largura; j++) { 
            if (fscanf(arq, "%d", &matriz[i][j]) != 1) {
                fprintf(stderr, "ERRO DETALHADO (read_pgm -> fscanf pixel): Erro na linha %d, coluna %d.\n", i, j);
                free_matrix(matriz, *altura);
                fclose(arq);
                return NULL;
            }
        }
    }
    
    fclose(arq);
    printf("(Info: Imagem PGM '%s' lida com sucesso.)\n", nomeArquivo);
    return matriz;
}


// escreve uma matriz no formato pgm p2
void save_pgm(const char* nomeArquivo, int **matriz, int largura, int altura) {
    FILE *arq = fopen(nomeArquivo, "w");
    if (!arq) {
        perror("Erro ao criar arquivo PGM de saida");
        return;
    }
    
    fprintf(arq, "P2\n%d %d\n255\n", largura, altura);
    
    for (int i = 0; i < altura; i++) {
        for (int j = 0; j < largura; j++) {
            fprintf(arq, "%d ", matriz[i][j] * 255); 
        }
        fprintf(arq, "\n");
    }
    
    fclose(arq);
}

// converte a matriz para 0 ou 1
void binarize(int **matriz, int largura, int altura, int limiar) {
    for (int i = 0; i < altura; i++) {
        for (int j = 0; j < largura; j++) {
            matriz[i][j] = (matriz[i][j] > limiar) ? 1 : 0;
        }
    }
}


// --- funcoes auxiliares ---

//libera a memoria da matriz (linhas e ponteiro principal)
void free_matrix(int **matriz, int altura) {
    if (matriz == NULL) return;
    for (int i = 0; i < altura; i++) {
        if (matriz[i] != NULL) {
            free(matriz[i]);
        }
    }
    free(matriz);
}