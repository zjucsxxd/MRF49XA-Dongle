//
//  generate-tables.c
//  
//
//  Created by William Dillon on 1/10/12.
//  Copyright (c) 2012. All rights reserved.
//

// This program can generate lookup tables for 8,4 hamming ECC.
// replace the following matricies with your generator and check
// matricies and run the program.  Also, you can tweak the speculative
// correction code for cases with more than 1 error.

#include <stdio.h>

uint8_t apply_gen_matrix(uint8_t input, uint8_t matrix[4]);
uint8_t apply_check_matrix(uint8_t input, uint8_t matrix[8]);
uint8_t get_hamming_distance(uint8_t a, uint8_t b);

// This is a binary matrix used to generate codewords
// These are the matricies from Wolfram Alpha. (systematic form)
uint8_t gen_matrix[4] = {
    0x8E,
    0x4D,
    0x2B,
    0x17
};

// This matrix is "rotated" 90 degrees clockwise.  This is because
// matrix semantics for this case are very inconvenient with binary
// operations.  Basically, we're treating the 'char' type as an 8
// bit wide SIMD type of binary values.  But, we only use the bottom 4
uint8_t check_matrix[8] = {
    0xE,
    0xD,
    0xB,
    0x7,
    0x8,
    0x4,
    0x2,
    0x1
};

// Even though the input is a char, only the low-order
// 4 bits are used for generating the codeword.
uint8_t apply_gen_matrix(uint8_t input, uint8_t matrix[4])
{
    uint8_t output = 0;
    
    // Scan across each bit, If the bit is a one,
    // apply the matrix row with an xor.
    // this successfully approximates binary matrix multiplication
    uint8_t mask = 0x08;
    uint8_t i;
    for (i = 0; i < 4; i++) {
        if (input & mask) {
            output ^= gen_matrix[i];
        }
        
        mask = mask >> 1;
    }
    
    return output;
}

// This function applies the check matrix to the 8 bit
// input.  The return value is the syndrome.  If the
// syndrome is '0000', then there were no errors.
uint8_t apply_check_matrix(uint8_t input, uint8_t matrix[8])
{
    uint8_t output = 0;
    
    // Scan across each bit, If the bit is a one,
    // apply the matrix row with an xor.
    // this successfully approximates binary matrix multiplication
    uint8_t mask = 0x80;
    uint8_t i;
    for (i = 0; i < 8; i++) {
        //        mask = (1 << (7 - i));
        if (input & mask) {
            output ^= check_matrix[i];
        }
        
        mask = mask >> 1;
    }
    
    return output;    
}

uint8_t get_hamming_distance(uint8_t a, uint8_t b)
{
    uint8_t output = 0;
    
    // Keep only the bit differences between arguments
    uint8_t difference = a ^ b;

    // Count the '1's in the difference
    uint8_t i, mask = 0x80;
    for (i = 0; i < 8; i++) {
        if (difference & mask) {
            output++;
        }
        
        mask = mask >> 1;
    }
    
    return output;
}

int main (int argc, const char * argv[])
{
    uint16_t counts[16] = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0};
    
    uint8_t i;
    
    uint8_t codewords[16];
    
    printf("Generator table:\n");
    for (i = 0; i < 16; i++) {
        uint8_t output = apply_gen_matrix(i, gen_matrix);
        printf("0x%02x\n", output);
        codewords[i] = output;
    }
    
    printf("\nCheck table:\n");
    uint8_t j;
    for (i = 0; i < 16; i++) {
        for (j = 0; j < 16; j++) {
            uint8_t codeword = (i << 4) | j;
            uint8_t output = apply_check_matrix(codeword, check_matrix);
            uint8_t value = 0;
            
            // If the syndrome is not zero, attempt to fix the error
            if (output != 0) {
                uint8_t k;
                
                // Calculate the hamming distance between
                // this message and the codewords.
                // Select the lowest distance codeword,
                // break ties with the data part of the codeword
                uint8_t distances[16];
                for (k = 0; k < 16; k++) {
                    distances[k] = get_hamming_distance(codeword, codewords[k]);
                }
                
                // Keep the symptom as the higher order bits (for diag)
                value = output << 4;
                
                // Check each column of the check matrix
                // if there is a matching column, its index
                // is the bit position that needs to be flipped
                // If there isn't one, then all is lost.
                for (k = 0; k < 8; k++) {
                    if (check_matrix[k] == output) {
                        codeword = codeword ^ (1 << (7-k));
                    }
                }
                
            }
            
            // Set the low-order bits to the value
            value |= (codeword >> 4) & 0x0F;
            
            // Count the number of times we get this value
            counts[(value & 0x0F)] += 1;
            
            // Print the output
            printf("0x%02x, ", value);
        }
        printf("\n");
    }    
    
    for (i = 0; i < 16; i++) {
        printf("Number of 0x%x's: %d\n", i, counts[i]);
    }
    
    return 0;
}

