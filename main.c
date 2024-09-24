#include <stdio.h>
#include <freetype2/ft2build.h>
#include FT_FREETYPE_H
#include <stdlib.h>
#include <string.h>

void add_movement(float x, float y, int is_pen_down, char** gcode);
void generate_gcode(char *data, char** gcode, int startx, int starty, int font_size);
void add_character(char character, char** gcode, FT_Face face, int x, int y);
void gcode_from_outline(const FT_Outline* outline, int startx, int starty, char** gcode);
void add_command_to_gcode(char* command, char** gcode);
char *read_document(char *document_name);

char* ttf_file = "example_font_1.ttf";  // Nome del file .ttf da caricare

int main(int argc, char *argv[]) {
  char *document_name;
  
  if (argc == 2) {
    document_name = argv[1];
    printf("Loading document: %s\n", document_name);
  } else if (argc == 3) {
    document_name = argv[1];
    ttf_file = argv[2];
    printf("Loading document: %s using font: %s\n", document_name, ttf_file);
  } else {
    printf("Usage: %s <document_name> [ttf_file]\n", argv[0]);
    return 1;
  }

  char *document_data = read_document(document_name);  // Legge il testo dal file

  int font_size = 5;
  char *gcode_data = NULL;
  generate_gcode(document_data, &gcode_data, 0, 240, font_size);  // Genera il gcode a partire dalla stringa

  if (gcode_data == NULL) {
    printf("Error generating gcode!\n");
    return 1;
  }

  FILE *file = fopen("output.gcode", "w");
  
  // Controllare se il file è stato aperto correttamente
  if (file == NULL) {
    printf("Error opening file!\n");
    return 1;
  }

  // Scrivere del testo nel file
  if (fprintf(file, "%s", gcode_data) < 0) {
    printf("Errore writing to file!\n");
    fclose(file);
    free(gcode_data);
    return 1;
  }

  // Chiudere il file
  fclose(file);

  free(gcode_data);  // Libera la memoria
  return 0;
}

void generate_gcode(char *data, char** gcode, int startx, int starty, int font_size) {
  FT_Library library;
  FT_Face face;
  FT_Error error;

  // Inizializza FreeType
  error = FT_Init_FreeType(&library);
  if (error) {
    printf("Error initializing FreeType.\n");
    exit(1);
  }

  // Carica il font
  error = FT_New_Face(library, ttf_file, 0, &face);
  if (error) {
    printf("Error loading font.\n");
    exit(1);
  }

  // Imposta la dimensione del font
  FT_Set_Char_Size(face, 0, font_size * 100, 0, 0);

  *gcode = (char *) malloc(1);
  if (*gcode == NULL) {
    printf("Error allocating memory.\n");
    exit(1);
  }

  (*gcode)[0] = '\0';

  add_command_to_gcode("G28\n", gcode); // Porta tutte le assi (X, Y, Z) a home
  add_command_to_gcode("G90\n", gcode); // Imposta il posizionamento assoluto
  add_command_to_gcode("G21\n", gcode); // Imposta i millimetri come unità
  add_command_to_gcode("G0 Z2\n", gcode); // Alza la penna per non scrivere prima del primo carattere

  int x = startx, y = starty;
  int char_spaces = font_size;
  int new_line_spaces = char_spaces * 2;

  for (int i = 0; i < strlen(data); i++) {
    if (data[i] == '\n') {
      y -= new_line_spaces;
      x = 0;
      printf("New line at (%d, %d)\n", x, y);
      continue;
    }

    add_character(data[i], gcode, face, x, y);
    x += char_spaces;
  }

  add_command_to_gcode("G0 Z15\n", gcode);  // Alza la penna di 1.5 centimetri dopo aver finito

  // Libera la memoria
  FT_Done_Face(face);
  FT_Done_FreeType(library);
}

// Funzione per estrarre le coordinate di un contorno
void add_character(char character, char** gcode, FT_Face face, int x, int y) {
  // Carica il glifo del carattere
  FT_UInt glyph_index = FT_Get_Char_Index(face, character);
  FT_Error error = FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
  if (error) {
    printf("Error loading glyph.\n");
    exit(1);
  }

  // Estrai il contorno del glifo
  if (face->glyph->format == FT_GLYPH_FORMAT_OUTLINE){
    FT_Outline* outline = &face->glyph->outline;
    printf("Adding character '%c' at (%d, %d)\n", character, x, y);
    gcode_from_outline(outline, x, y, gcode);
  }
}

void gcode_from_outline(const FT_Outline* outline, int startx, int starty, char** gcode) {
  // Scorre i contorni del glifo
  for (int i = 0; i < outline->n_contours; i++) {
    int start_idx = (i == 0) ? 0 : outline->contours[i-1] + 1;
    int end_idx = outline->contours[i];
    
    // Scorre i punti del contorno
    for (int j = start_idx; j <= end_idx; j++) {
      FT_Vector point = outline->points[j];
      float x = point.x / 64.0;  // FreeType utilizza coordinate fisse a 26.6 bit
      float y = point.y / 64.0;
      
      // Se siamo al primo punto, si sposta a questo punto (penna alzata)
      if (j == start_idx) {
        add_movement(startx + x, starty + y, 0, gcode);
      } else {
        // Se siamo su un punto successivo, tracciamo verso questo punto (penna abbassata)
        add_movement(startx + x, starty + y, 1, gcode);
      }
    }
  }
}

void add_movement(float x, float y, int is_pen_down, char** gcode) {
  char command[1024];

  if (is_pen_down) {
    sprintf(command, "G0 Z0\nG1 X%.3f Y%.3f\n", x, y);  // Movimento con la penna abbassata
  } else {
    sprintf(command, "G0 Z2\nG0 X%.3f Y%.3f\n", x, y);  // Movimento rapido con la penna alzata
  }
  add_command_to_gcode(command, gcode);
}

void add_command_to_gcode(char* command, char** gcode) {
  // Rialloca la stringa
  size_t new_size = strlen(*gcode) + strlen(command) + 1;
  *gcode = (char *) realloc(*gcode, new_size);
  if (*gcode == NULL) {
    printf("Error allocating memory.\n");
    exit(1);
  }

  // Aggiunge il comando alla stringa
  strcat(*gcode, command);
}

char *read_document(char *document_name) {
  char *data;
  FILE *file = fopen(document_name, "r");

  // Controllare se il file è stato aperto correttamente
  if (file == NULL) {
    printf("Error opening file!\n");
    exit(1);
  }

  // Determina la dimensione del file
  fseek(file, 0, SEEK_END);
  long file_size = ftell(file);
  rewind(file);

  // Alloca memoria per il contenuto del file + terminatore null
  data = (char *) malloc(file_size + 1);
  if (data == NULL) {
    printf("Error allocating memory!\n");
    fclose(file);
    exit(1);
  }

  // Legge il contenuto del file
  size_t bytes_read = fread(data, 1, file_size, file);
  if (bytes_read != file_size) {
    printf("Error reading file!\n");
    free(data);
    fclose(file);
    exit(1);
  }

  // Aggiunge il terminatore null alla fine della stringa
  data[file_size] = '\0';

  // Chiude il file
  fclose(file);

  return data;
}