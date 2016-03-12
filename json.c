#include <jansson.h>

/*
 * get_string_contents reads the file into a
 * null terminated string.
 *
 * side effects: allocates a string which
 * must be freed with 'free'
 */
static char * get_string_contents(char *filename) {
    FILE *file;
    long file_size;
    char *contents;

    file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Fatal: could not open %s\n", filename);
        exit(1);
    }
    fseek(file, 0, SEEK_END);
    file_size = ftell(file);
    rewind(file);
    contents = malloc((file_size + 1) * (sizeof(char)));
    fread(contents, sizeof(char), file_size, file);
    fclose(file);
    contents[file_size] = 0; /* add null termination */

    return contents;
}

/*
 * read_json parses and returns
 * the root json object of the file
 *
 * side effects: allocates a json context 
 * which must be freed with 'json_decref'
 *
 * Actual memory management is managed by the
 * jansson library, for more information
 * see https://jansson.readthedocs.org/en/
 */
json_t * read_json(char *filename) {
    char *jsonString;
    json_t *root;
    json_error_t error;

    jsonString = get_string_contents(filename);
    root = json_loads(jsonString, 0, &error);
    free(jsonString);
    if (!root) {
        fprintf(stderr, "Fatal: %s parse error on line %d: %s\n", filename, error.line, error.text);
        exit(1);
    }

    return root;
}
