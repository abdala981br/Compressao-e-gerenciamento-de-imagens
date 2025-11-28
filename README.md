# Compressão e Gerenciamento de Imagens Binárias

Projeto desenvolvido para a disciplina **Estruturas de Dados 2 (ED2)**, com foco em:
- armazenamento de imagens PGM em formato binário,
- compressão utilizando **Run-Length Encoding (RLE)**,
- gerenciamento dos registros em um arquivo (inserção, remoção e compactação),
- reconstrução aproximada da imagem original a partir de múltiplos limiares.

---

## Funcionalidades

### 1. **Gerenciamento do banco de imagens**
- Armazenamento das imagens binarizadas em arquivo binário.
- Remoção lógica de registros.
- Compactação do arquivo para liberar espaço.
- Arquivo de índices para acesso eficiente via offsets.

### 2. **Compressão**
- Aplicação de limiarização para gerar imagens binárias.
- Compressão usando RLE (sequências de repetições).
- Salvamento da imagem comprimida com informações de cabeçalho e metadados.

### 3. **Recuperação**
- Decodificação dos registros para reconstruir imagens PGM.
- Diferentes versões da mesma imagem são diferenciadas pelo valor de limiar.

### 4. **Reconstrução (Bônus)**
- Combinação de múltiplas versões binárias para gerar uma imagem média.

---

## Como executar
1. Compile o código:
   ```bash
   gcc main.c -o programa

2. Execute:
   ```bash
    ./programa


4. Siga o menu para:
  - Adicionar imagens;
  - Remover;
  - Compactar;
  - Recuperar;
  - Reconstruir.
