#include <stdbool.h>  // Boolean values
#include <stdio.h>    // Standard inputs/outpus functions (printf, getline)
#include <stdlib.h>   // Memory allocation (mallc, free) and exit() functions
#include <string.h>   // String functions (strcmp, strncmp for comparison)

/**
* MetaCommandResult - Status codes for meta-command processing
*
* Meta-commands are special commands that start with a dot (.)
*/
typedef enum {
  META_COMMAND_SUCCESS,
  META_COMMAND_UNRECOGNIZED
} MetaCommandResult;

/**
* PrepareResult - Status codes for statement parsing
* 
* Indicates whether a SQL-like command was successfully parsed
* into a Statement structure.
*/
typedef enum {
  PREPARE_SUCCESS,
  PREPARE_UNRECOGNIZED_STATEMENT
} PrepareResult;

/**
* StatementType - Available database operations
*
* SQL operations:
* - INSERT: Add a new row to the database
* - SELECT: Retrieve rows from the database
*/
typedef enum {
  STATEMENT_INSERT,
  STATEMENT_SELECT
} StatementType;


/** 
* InputBuffer - A structure that manages user input dynamically
*
* @buffer: Pointer to dynamically allocated string containing user input
* @buffer_length: Total allocated size of the buffer (in bytes)
* @input_length: Actual length of user input (withouth the newline character "\n")
*/
typedef struct {
  char* buffer;           // Pointer to the actual string data in heap memory 
  size_t buffer_length;   // Total capacity of the buffer (How much memory is reserved)
  ssize_t input_length;   // Real input length (getline returns this, minus the newline)
} InputBuffer;

/**
 * Statement - Represents a parsed SQL-like command
*/ 
typedef struct {
  StatementType type; // STATEMENT_INSERT, STATEMENT_SELECT, etc.
} Statement;


/**
* new_input_buffer - Creates and initializes a new InputBuffer structure
*
* Allocates memory for an InputBuffer object on the heap and initializes all fields
* to safe default values (NULL, 0, 0)
*
* Return: Pointer to the newly created InputBuffer structure
*/
InputBuffer* new_input_buffer() {
  // Allocate memory on the heap
  // sizeof(InputBuffer) -> Calculate the bytes occupied by the structure (8 char pointer + 8 size_t + 8 ssize_t)
  // malloc -> Reserves that block of memory in the heap (dynamic memory) and returns a pointer (the memory address) to the first byte 
  InputBuffer* input_buffer = malloc(sizeof(InputBuffer));
  // initialize input_buffer to NULL (no memory allocated yet)
  input_buffer->buffer = NULL;
  // getline will allocate when needed
  input_buffer->buffer_length = 0;
  input_buffer->input_length = 0;

  return input_buffer;
}

/**
* read_input - Reads a line of user input form stdin
*
* Uses getline() which automatically allocates memory as needed. The fuction stores
* the input in input_buffer->buffer and updates buffer_length accordingly
*
* @input_buffer: Pointer to InputBuffer structure that will store the input
*
* Note: getline() includes the newline character "\n" in its count
*   we remove it and store the actual length in input_length
*/ 
void read_input(InputBuffer* input_buffer) {
  // getline() reads an entire line from stdin
  // Parameters:
  //  &(input_buffer->buffer) - Address of the buffer pointer (allows reallocation)
  //  &(input_buffer->buffer_length) - Address of size variable (gets updated)
  //  stdin - Standard input (keyboard)
  // Returns: number of bytes read (inluding newline), or -1 on error
  ssize_t bytes_read =
      getline(&(input_buffer->buffer), &(input_buffer->buffer_length), stdin);

  // Check for read error on EOF (end of file) (Ctrl + D)
  if (bytes_read <= 0) {
    printf("Error reading input\n");
    exit(EXIT_FAILURE);   // Terminate program with error code
  }

  // Remove trailing newline character
  input_buffer->input_length = bytes_read - 1;
  // Replace newline with null terminator (makes it a proper C string)
  input_buffer->buffer[bytes_read - 1] = 0;
}

/*
* close_input_buffer - Frees all memory associated with InputBuffer
*
* Releases the heap memory used by both the buffer string and the InputBuffer
* structure itself (thiss prevents memory leaks) 
*/
void close_input_buffer(InputBuffer* input_buffer) {
    free(input_buffer->buffer); // Free the actual string data first
    free(input_buffer);         // Then free the structure itself
}

/* 
* print_prompt - Displays the database prompt to the user
*/
void print_prompt() { printf("Ema's db > "); }

/**
* do_meta_command - Handles dot-commands (.exit, .help, etc.)
*
* @input_buffer:  Contains the raw user input to check for meta-commands
*
* Return: META_COMMAND_SUCCESS if command was handled,
*         META_COMMAND_UNRECOGNIZED otherwise
*/
MetaCommandResult do_meta_command(InputBuffer* input_buffer) {
  if (strcmp(input_buffer->buffer, ".exit") == 0) {
    close_input_buffer(input_buffer); // Clean up before exiting
    exit(EXIT_SUCCESS);
  } else {
    return META_COMMAND_UNRECOGNIZED;
  }
}

/**
* prepare_statement - Parses user input into a Statement struct
*
* Converts a raw command string like "insert" or "select" into a typed
* Statement object that can be executed
*
* @input_buffer: Contains the raw user input (e.g., "insert into users...")
* @statement: Pointer to Statement struct that will be filled with parsed data
*
* Return: PREPARE_SUCCESS if command is recognized,
*         PREPARE_UNRECOGNIZED_STATEMENT otherwise
*/
PrepareResult prepare_statement(InputBuffer* input_buffer, Statement* statement) {
  if (strncmp(input_buffer->buffer, "insert", 6) == 0){
    statement->type = STATEMENT_INSERT;
    return PREPARE_SUCCESS;
  }
  if (strcmp(input_buffer->buffer, "select") == 0) {
    statement->type = STATEMENT_SELECT;
    return PREPARE_SUCCESS;
  }

  return PREPARE_UNRECOGNIZED_STATEMENT;
}

/**
* execute_statement - Run a prepared statement
* 
* Performs the actual database operation based on the statement type
* 
* @statement: The parsed statement to execute
*/
void execute_statement(Statement* statement) {
  switch (statement->type) {
    case (STATEMENT_INSERT):
      printf("Insert do here \n");
      break;
    case (STATEMENT_SELECT):
      printf("Select do here \n");
      break;
  }
}

/**
* Main REPL (Read-Eval-Print Loop)
*/
int main(int argc, char* argv[]) {
  InputBuffer* input_buffer = new_input_buffer();
  while (true) {
    print_prompt();
    read_input(input_buffer);

    if (input_buffer->buffer[0] == '.') {
      switch (do_meta_command(input_buffer)) {
        case (META_COMMAND_SUCCESS):
          continue;
        case (META_COMMAND_UNRECOGNIZED):
          printf("Unrecognized command '%s' \n", input_buffer->buffer);
          continue;
      }
    }

    Statement statement;
    switch (prepare_statement(input_buffer, &statement)) {
      case (PREPARE_SUCCESS):
        break;
      case (PREPARE_UNRECOGNIZED_STATEMENT):
        printf("Unrecognized keyword at start of '%s' \n", input_buffer->buffer);
        continue;
    }
    
    execute_statement(&statement);
    printf("Executed \n");
  }
}
