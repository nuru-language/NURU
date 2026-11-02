#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../lexer/nuru_lexer.h"
#include "../parser/nuru_parser.h"
#include "../ast/nuru_ast.h"
#include "../semantic/nuru_semantic.h"
#include "../nir/nuru_nir.h"
#include "../nir/nuru_irgen.h"
#include "../nir/nuru_codegen.h"
#include "../optimizer/nuru_optimizer.h"
#include "../object/nuru_object.h"
#include "nuru_import.h"

typedef struct {
    int show_tokens;
    int show_ast;
    int show_semantic;
    int show_ir;
    int show_help;
    int disable_opt;
    const char* input_file;
    const char* output_file;
    const char* stdlib_dir;
} CompilerOptions;

static void print_help(const char* program) {
    printf("NURU Compiler v0.1\n");
    printf("Usage: %s [options] <input file>\n", program);
    printf("\nOptions:\n");
    printf("  -t, --tokens    Show tokens\n");
    printf("  -a, --ast       Show AST\n");
    printf("  -s, --semantic  Show semantic analysis\n");
    printf("  -i, --ir        Show IR\n");
    printf("  -o <file>       Output file\n");
    printf("  -O0             Disable NIR optimization (default: on)\n");
    printf("  -S, --stdlib DIR  Module search directory (default: stdlib)\n");
    printf("  -h, --help      Show this help\n");
}

static int parse_args(CompilerOptions* opts, int argc, char** argv) {
    memset(opts, 0, sizeof(*opts));
    opts->stdlib_dir = "stdlib";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--tokens") == 0) {
            opts->show_tokens = 1;
        } else if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--ast") == 0) {
            opts->show_ast = 1;
        } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--semantic") == 0) {
            opts->show_semantic = 1;
        } else if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--ir") == 0) {
            opts->show_ir = 1;
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            opts->output_file = argv[++i];
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            opts->show_help = 1;
        } else if (strcmp(argv[i], "-O0") == 0) {
            opts->disable_opt = 1;
        } else if ((strcmp(argv[i], "--stdlib") == 0 || strcmp(argv[i], "-S") == 0) && i + 1 < argc) {
            opts->stdlib_dir = argv[++i];
        } else if (argv[i][0] != '-') {
            opts->input_file = argv[i];
        }
    }

    return 1;
}

static char* read_file(const char* path, size_t* size) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Error: Cannot open file '%s'\n", path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* buffer = (char*)malloc(len + 1);
    if (!buffer) {
        fclose(f);
        return NULL;
    }

    size_t read_len = fread(buffer, 1, len, f);
    buffer[read_len] = '\0';
    fclose(f);

    if (size) *size = read_len;
    return buffer;
}

static char* derive_output_name(const char* input) {
    const char* dot = strrchr(input, '.');
    size_t len = dot ? (size_t)(dot - input) : strlen(input);
    char* out = malloc(len + 5);
    snprintf(out, len + 5, "%.*s", (int)len, input);
    return out;
}

int main(int argc, char** argv) {
    CompilerOptions opts;

    if (!parse_args(&opts, argc, argv)) {
        fprintf(stderr, "Error parsing arguments\n");
        return 1;
    }

    if (opts.show_help) {
        print_help(argv[0]);
        return 0;
    }

    if (!opts.input_file) {
        fprintf(stderr, "Error: No input file specified\n");
        print_help(argv[0]);
        return 1;
    }

    size_t source_size;
    char* source = read_file(opts.input_file, &source_size);
    if (!source) {
        return 1;
    }

    {
        char errmsg[256];
        size_t expanded_size;
        char* expanded = nuru_expand_imports(source, source_size, &expanded_size,
                                             opts.stdlib_dir, errmsg, sizeof(errmsg));
        if (!expanded) {
            fprintf(stderr, "Import error: %s\n", errmsg);
            free(source);
            return 1;
        }
        free(source);
        source = expanded;
        source_size = expanded_size;
    }

    Lexer lexer;
    lexer_init(&lexer, source, source_size);

    if (opts.show_tokens) {
        printf("=== TOKENS ===\n");
        while (1) {
            Token token = lexer_next_token(&lexer);
            printf("[%3d:%3d] %-20s",
                   token.line, token.column, token_type_name(token.type));

            if (token.type == TOKEN_INTEGER) {
                printf(" (%" PRIu64 ")", token.integer_value);
            } else if (token.type == TOKEN_NUMBER) {
                printf(" (%f)", token.number_value);
            } else if (token.type == TOKEN_STRING) {
                printf(" (\"%.*s\")", (int)token.string_length, token.string_value);
            } else if (token.type == TOKEN_IDENTIFIER) {
                printf(" (%.*s)", (int)token.length, token.start);
            } else if (token.type == TOKEN_ERROR) {
                printf(" ERROR: %s", token.start);
            }

            printf("\n");

            if (token.type == TOKEN_EOF || token.type == TOKEN_ERROR) {
                break;
            }
        }
    }

    Parser parser;
    parser_init(&parser, source, source_size);

    ASTNode* ast = parser_parse(&parser);

    if (opts.show_ast && ast) {
        printf("\n=== AST ===\n");
        for (size_t i = 0; i < ast->program.statement_count; i++) {
            ast_print(ast->program.statements[i], 0);
        }
    }

    if (parser.had_error) {
        fprintf(stderr, "Parse error: %s\n", parser_get_error(&parser));
        free(source);
        parser_free(&parser);
        if (ast) ast_free(ast);
        return 1;
    }

    if (!ast) {
        fprintf(stderr, "Parse error: failed to parse program (unsupported syntax)\n");
        free(source);
        parser_free(&parser);
        return 1;
    }

    SemanticAnalyzer analyzer;
    semantic_init(&analyzer);

    int semantic_ok = semantic_analyze(&analyzer, ast);

    if (opts.show_semantic) {
        printf("\n=== SEMANTIC ANALYSIS ===\n");
        if (semantic_ok != 0) {
            printf("Semantic analysis failed with %d error(s):\n", semantic_get_error_count(&analyzer));
            for (int i = 0; i < semantic_get_error_count(&analyzer); i++) {
                printf("  %s\n", semantic_get_error(&analyzer, i));
            }
        } else {
            printf("Semantic analysis passed.\n");
            if (analyzer.has_main) {
                printf("  - 'main' function found\n");
            }
        }
    }

    if (semantic_ok != 0) {
        fprintf(stderr, "Semantic error: %s\n", semantic_get_error(&analyzer, 0));
        if (semantic_get_error_count(&analyzer) > 1) {
            fprintf(stderr, "  ... and %d more error(s)\n", semantic_get_error_count(&analyzer) - 1);
        }
        free(source);
        parser_free(&parser);
        ast_free(ast);
        semantic_free(&analyzer);
        return 1;
    }

    // Generate IR
    IRGen* irgen = irgen_create();
    NIRProgram* ir = irgen_lower(irgen, ast);
    if (!opts.disable_opt) {
        nir_optimize(ir);
    }

    if (opts.show_ir) {
        nir_print_program(ir);
        irgen_free(irgen);
        printf("Compilation successful!\n");
        free(source);
        parser_free(&parser);
        if (ast) ast_free(ast);
        semantic_free(&analyzer);
        return 0;
    }

    // Generate assembly, assemble and link into an executable
    char* outfile = opts.output_file ? strdup(opts.output_file) : derive_output_name(opts.input_file);
    char* asmfile = malloc(strlen(outfile) + 5);
    sprintf(asmfile, "%s.s", outfile);
    char* objfile = malloc(strlen(outfile) + 5);
    sprintf(objfile, "%s.o", outfile);
    char* tmpasm = malloc(strlen(outfile) + 6);
    sprintf(tmpasm, "%s.tmp", outfile);

    CodeGen* cg = codegen_create(tmpasm, ir);
    if (!cg) {
        fprintf(stderr, "Error: cannot create assembly file '%s'\n", tmpasm);
        free(outfile);
        free(asmfile);
        free(objfile);
        free(tmpasm);
        irgen_free(irgen);
        return 1;
    }
    codegen_emit(cg);
    codegen_free(cg);

    if (nuru_assemble(tmpasm, objfile) != 0) {
        fprintf(stderr, "Assembly failed\n");
        /* keep asm */
        unlink(objfile);
        return 1;
    }
    if (nuru_link(objfile, outfile) != 0) {
        fprintf(stderr, "Linking failed\n");
        /* keep asm */
        unlink(objfile);
        return 1;
    }

    /* keep asm */
    unlink(objfile);
    free(outfile);
    free(asmfile);
    free(objfile);
    free(tmpasm);

    irgen_free(irgen);
    printf("Compilation successful!\n");

    free(source);
    parser_free(&parser);
    if (ast) ast_free(ast);
    semantic_free(&analyzer);

    return 0;
}
