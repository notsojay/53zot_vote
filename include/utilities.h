#ifndef UTILITIES_H
#define UTILITIES_H

#include <stdio.h>
#include <stdlib.h> 
#include <string.h>
#include <stdint.h>

void reportUnixError(char *msg, int state);

FILE* openFile(const char *filename, const char *flag);

int8_t isEmptyFile(FILE *file);

char* extractField(char **start, char **end, char delim);

size_t findChar(char **src, char delim);

uint32_t getBit(uint32_t bitVector, int bitPosition);

void setBit(uint32_t *bitVector, int bitPosition, int bitVal);

#endif