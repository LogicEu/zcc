#include <preproc.h>

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
    ptu.lines = array_create(sizeof(range_t));
    ptu.tokens = array_create(sizeof(range_t));  
    ptu.text = ppc_read(filename);
    if (ptu.text.size) {
        ptu.lines = strlines(&ptu.text);
        strtext(&ptu.text, &ptu.lines);
        ptu.tokens = strtoks(&ptu.text);
    }
    return ptu;
}