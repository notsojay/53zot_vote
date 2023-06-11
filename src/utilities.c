#include "utilities.h"

void
reportUnixError(char *msg, int state)
{
	perror(msg);
	exit(state);
}

FILE*
openFile(const char *filename, const char *flag)
{
    if(!filename)
    {
        reportUnixError("Invalid filename\n", 2);
    }

    FILE *fp = fopen(filename, flag);
    if(!fp)
    {
        reportUnixError("fopen failed\n", 2);
    }

    return fp;
}

int8_t
isEmptyFile(FILE *file) 
{
    if(file == NULL) return 1;

    int byte = fgetc(file);
    if(byte == EOF || feof(file)) return 1;
    ungetc(byte, file);

    return 0;
}

char* 
extractField(char **start, char **end, char delim) 
{
    size_t len = findChar(end, delim);
    char *field = (char*)malloc((len+1) * sizeof(char));

    strncpy(field, *start, len);
    field[len] = '\0';

    return field;
}

size_t
findChar(char **src, char delim) 
{
    if(!*src) return 0;
    
    size_t length = 0;
    while(**src && **src != delim)
    {
        (*src)++;
        ++length;
    }
    if(**src) ++(*src);
    
    return length;
}

uint32_t
getBit(uint32_t bitVector, int bitPosition)
{
    if(bitPosition < 0 || bitPosition > 31) return 0;

    uint32_t mask = 1 << bitPosition;
    uint32_t result = bitVector & mask;
    return result >> bitPosition;
}

void
setBit(uint32_t *bitVector, int bitPosition, int bitVal)
{
    if( (bitVal != 0 && bitVal != 1) || 
        bitPosition < 0 || bitPosition > 31 ) return;
    
    if(bitVal) *bitVector |= 1 << bitPosition;
    else *bitVector &= ~(1 << bitPosition);    
}