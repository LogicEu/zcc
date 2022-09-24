#include <utopia/utopia.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct range_t {
    size_t start, end;
} range_t;

typedef struct ptu_t {
    char* name;
    string_t text;
    array_t lines;
    array_t tokens;
} ptu_t;

static inline size_t eatblock(const char* str, const char ch, const size_t end, size_t i)
{
    if (str[i] == ch) {
        while (i + 1 < end && str[i + 1] != ch) { ++i; } 
    }
    return i;
}

static void ptu_print_lines(ptu_t* ptu)
{
    range_t* r = ptu->lines.data;
    const size_t linecount = ptu->lines.size;
    for (size_t i = 0; i < linecount; ++i) {
        const size_t end = r[i].end;
        printf("'");
        for (size_t j = r[i].start; j < end; j++) {
            printf("%c", ptu->text.data[j]);
        }
        printf("'\n");
    }
}

static void ptu_splice_lines(ptu_t* ptu)
{
    size_t i;
    range_t r = {0, 0};
    for (i = 0; ptu->text.data[i]; ++i) {
        if (ptu->text.data[i] == '\n') {
            if (i > 0 && ptu->text.data[i - 1] == '\\') {
                string_remove_index(&ptu->text, i--);
                string_remove_index(&ptu->text, i--);
            } else {
                r.end = i;
                array_push(&ptu->lines, &r);
                r.start = i + 1;
            }
        }
    }
    
    r.end = i;
    array_push(&ptu->lines, &r);
}

static void ptu_preprocess_text(ptu_t* ptu)
{
    ptu_splice_lines(ptu);

    range_t* ranges = ptu->lines.data, r;
    for (size_t i = 0, dif = 0; i < ptu->lines.size; ++i) {
        
        ranges[i].start -= dif;
        ranges[i].end -= dif;
        
        for (size_t j = ranges[i].start; j < ranges[i].end; ++j) {
            
            j = eatblock(ptu->text.data, '\'', ranges[i].end, j);
            j = eatblock(ptu->text.data, '"', ranges[i].end, j);
            
            if (ptu->text.data[j] == '/') {
                if (ptu->text.data[j + 1] == '/') {
                    string_remove_range(&ptu->text, j, ranges[i].end);
                    dif += ranges[i].end - j;
                    ranges[i].end = j;
                }
                else if (ptu->text.data[j + 1] == '*') {
                    char* s = strstr(ptu->text.data + j + 2, "*/");
                    if (!s) {
                        printf("'' comment is not closed at line %zu\n", i + 1);
                        return;
                    }
                    
                    const size_t n = s - (ptu->text.data + j) + 2;
                    string_remove_range(&ptu->text, j + 1, j + n);
                    ptu->text.data[j] = ' ';
                    dif += n - 1;

                    r = ranges[i];
                    for (size_t k = i; k < ptu->lines.size; ++k) {
                        if (ranges[k].end >= j + n) {
                            ranges[i].end = ranges[k].end - dif;
                            ranges[i].start = r.start;
                            break;
                        } 
                        else array_remove(&ptu->lines, k--);
                    }
                    
                    break;
                }
            }
        }
    }
}

static ptu_t ptu_read(const char* filename)
{
    ptu_t ptu;
    ptu.name = (char*)(size_t)filename;
    ptu.text = string_empty();
    ptu.lines = array_create(sizeof(range_t));
    ptu.tokens = array_create(sizeof(range_t));
    
    FILE* file = fopen(filename, "rb");
    if (!file) {
        printf("Could not open file '%s'.\n", filename);
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
    ptu_preprocess_text(&ptu);
    return ptu;
}

static void ptu_free(ptu_t* ptu)
{
    string_free(&ptu->text);
    array_free(&ptu->lines);
    array_free(&ptu->tokens);
}

int main(const int argc, const char** argv)
{
    int status = EXIT_SUCCESS;
    array_t infiles = array_create(sizeof(char*));
    array_t includes = array_create(sizeof(char*));

    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == 45) {
            if (argv[i][1] == 'I') {
                const char* ptr = argv[i] + 2;
                array_push(&includes, &ptr);
            }
        } 
        else array_push(&infiles, &argv[i]);
    }

    if (!infiles.size) {
        printf("Missing input file.\n");
        status = EXIT_FAILURE;
        goto exit;
    }

    char** filepaths = infiles.data;
    const size_t filecount = infiles.size;
    for (size_t i = 0; i < filecount; ++i) {
        ptu_t ptu = ptu_read(filepaths[i]);
        if (ptu.text.size) {
            ptu_print_lines(&ptu);
        }
        ptu_free(&ptu);
    }

exit:
    array_free(&infiles);
    array_free(&includes);
    return status;
}
