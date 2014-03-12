//
//  enc_dec_test.c
//  MRF49XA-Dongle
//
//  Created by William Dillon on 3/11/14.
//  Copyright (c) 2014 Oregon State University (COAS). All rights reserved.
//

#include <stdio.h>
#include "../hamming.h"

int
main(int argc, char **argv)
{
    int casesTested  = 0;
    int casesSucceed = 0;

    // Test basic encoding and decoding
//    for (int i = 0; i < 255; i++) {
//        printf("Encoding 0x%x: 0x%x, decoded as: 0x%x\n",
//               i,
//               hamming_encode(i),
//               hamming_decode(hamming_encode(i)));
//    }
    
    // Test decoding with single bit errors
    // Input byte
    for (int i = 0; i < 255; i++) {
        uint16_t symbol = hamming_encode(i);

        // Flipped bit position
        for (int j = 0; j < 8; j++) {
            casesTested++;
            
            if (hamming_decode(symbol ^ (1 << j)) != i) {
                printf("FAILED TO CORRECT SINGLE BIT ERROR (bit %d), 0x%x: 0x%x != 0x%x\n",
                       j, symbol, hamming_decode(symbol ^ (1 << j)), i);
            } else {
                casesSucceed++;
            }
        }
    }

    printf("Tested %d single-bit error conditions, %f%% corrected.\n",
           casesTested, (float)casesSucceed / (float)casesTested * 100.);
    
    // Test decoding with double bit errors
    // Input byte
    casesTested  = 0;
    casesSucceed = 0;
    for (int i = 0; i < 255; i++) {
        uint16_t symbol = hamming_encode(i);
        
        // Flipped bit position 1
        for (int j = 0; j < 16; j++) {
            
            // Flipped bit position 2
            for (int k = 0; k < 16; k++) {
                // If they're the same bit, skip it.
                if (k == j) k++;
                casesTested++;

                uint16_t symptom = (1 << j) | (1 << k);
                
                if (hamming_decode(symbol ^ symptom) != i) {
//                    printf("FAILED TO CORRECT DOUBLE BIT ERROR (bit %d and %d), 0x%x: 0x%x != 0x%x\n",
//                           j, k, symbol, hamming_decode(symbol ^ symptom), i);
                } else {
                    casesSucceed++;
                }
            }
        }
    }
    
    printf("Tested %d double-bit error conditions, %f%% corrected.\n",
           casesTested, (float)casesSucceed / (float)casesTested * 100.);
    

}