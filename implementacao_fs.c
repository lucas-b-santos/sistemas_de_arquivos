#include <stdio.h>
#include <stdint.h>

#define WORD_SIZE 64

#define TOTAL_BLOCKS 1024
#define WORDS (TOTAL_BLOCKS / WORD_SIZE)

uint64_t bitmap[WORDS] = {0}; 

void set_block_used(int block_id) {
    bitmap[block_id / WORD_SIZE] |= (1ULL << (block_id % WORD_SIZE));
}

int find_first_free_fast() {
    for (int i = 0; i < WORDS; i++) {
        // Se a palavra é toda 1s (0xFFFFFFFF), está tudo cheio. Pula.
        if (bitmap[i] == ~0U) {
            continue;
        }

        // Truque de Hardware:
        // Invertemos a palavra. Os blocos livres (0) viram (1).
        // Exemplo: Bitmap é ...11110111 (ocupado, ocupado, livre, ocupado...)
        // Invertido:       ...00001000
        uint64_t inverted = ~bitmap[i];

        int bit_offset = __builtin_ctzll(inverted);

        return (i * 64) + bit_offset;
    }
    return -1;
}

int main() {
    bitmap[0] = ~0U; 
    bitmap[1] = ~0U; 
    
    // Enchemos alguns da segunda word
    // set_block_used(64); 
    // set_block_used(33); 
    // set_block_used(34); 
    
    printf("Bloco livre (Scan Rápido): %d\n", find_first_free_fast());
    
    return 0;
}