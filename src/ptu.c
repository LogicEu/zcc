#include <preproc.h>
#include <stdio.h>

extern void ptu_preptext(ptu_t* ptu);

void ptu_free(ptu_t* ptu)
{
    string_free(&ptu->text);
    array_free(&ptu->lines);
    array_free(&ptu->tokens);
}

ptu_t ptu_read(const char* filename)
{
    ptu_t ptu;
    ptu.name = (char*)(size_t)filename;
    ptu.text = string_empty();
    ptu.lines = array_create(sizeof(range_t));
    ptu.tokens = array_create(sizeof(range_t));
    
    FILE* file = fopen(filename, "rb");
    if (!file) {
        return ptu;
    }
    
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);
    ptu.text = string_reserve(size + 1);
    fread(ptu.text.data, 1, size, file);
    fclose(file);
    ptu.text.data[size] = 0;
    ptu.text.size = size;
    ptu.lines = strlines(&ptu.text);
    strtext(&ptu.text, &ptu.lines);
    ptu.tokens = strtoks(&ptu.text);
    return ptu;
}