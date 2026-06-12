// HEADERS (Required Libraries)

#include <stdbool.h>  // Boolean values
#include <stdio.h>    // Standard inputs/outpus functions (printf, getline)
#include <stdlib.h>   // Memory allocation (mallc, free) and exit() functions
#include <string.h>   // String functions (strcmp, strncmp for comparison)
#include <stdint.h>   // Fixed-width integer types (uint32_t)


// ENUMS - Status codes for different operations

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
* into a Statement structure
*/
typedef enum {
  PREPARE_SUCCESS,
  PREPARE_SYNTAX_ERROR,
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
* ExecuteResult - Status codes for statement execution
*
* Indicates whether a prepared statement was executed successfully
* or if there was an error during execution
*/
typedef enum {
  EXECUTE_SUCCESS,
  EXECUTE_TABLE_FULL
} ExecuteResult;


// CONSTANTS - Database configuration

#define COLUMN_USERNAME_SIZE 32   // Maximum username length (characters)
#define COLUMN_EMAIL_SIZE 255     // Maximum email length (characters)

#define TABLE_MAX_PAGES 100       // Maximum number of pages in the table
#define PAGE_SIZE 4096         // Page size = 4KB (standard memory page)


// STRUCTURES - Data structures

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
* Row - A single record/row in the database
* 
* Represents one entry in the database with:
* - Unique identifier (id)
* - Username (max 32 characters)
* - Email address (max 255 characters)
* 
* Total size: 4 + 32 + 255 = 291 bytes
*/
typedef struct {
  uint32_t id;
  char username[COLUMN_USERNAME_SIZE];
  char email[COLUMN_EMAIL_SIZE];
} Row;

/**
* Statement - Represents a parsed SQL-like command
*
* Converts raw user input into a typed command that the database can execute
*/ 
typedef struct {
  StatementType type; // STATEMENT_INSERT, STATEMENT_SELECT, etc.
  Row row_to_insert;  // Data for Insert, Only used by insert statement
} Statement;

/**
* Table - In-memory database table
*
* Uses a paged memory system where rows are stored in fixed-size pages (4KB each)
* Each page can hold multiple rows (ROWS_PER_PAGE). Pages are allocated on-demand
*
* @num_rows: Total number of rows currently in the table
* @pages: Array of pointers to memory pages (each page is PAGE_SIZE bytes)
*/
typedef struct {
  uint32_t num_rows;
  void* pages[TABLE_MAX_PAGES];
} Table;


// ROW LAYOUT - Byte offsets for serialization

/**
* size_of_attribute - Macro to calculate the size of a struct field
*
* Example: size_of_attribute(Row, id) returns sizeof(uint32_t) = 4
*/
#define size_of_attribute(Struct, Attribute) sizeof(((Struct*)0)->Attribute)

// Individual field sizes
const uint32_t ID_SIZE = size_of_attribute(Row, id);              // 4 bytes
const uint32_t USERNAME_SIZE = size_of_attribute(Row, username);  // 32 bytes
const uint32_t EMAIL_SIZE = size_of_attribute(Row, email);        // 255 bytes

// Bytes offsets for each field within a Row
const uint32_t ID_OFFSET = 0;                                     // Start at byte 0
const uint32_t USERNAME_OFFSET = ID_OFFSET + ID_SIZE;             // Start at 4
const uint32_t EMAIL_OFFSET = USERNAME_OFFSET + USERNAME_SIZE;    // Start at 36

// Total row size
const uint32_t ROW_SIZE = ID_SIZE + USERNAME_SIZE + EMAIL_SIZE;   // 291 bytes

// Page and table capacity calculations
const uint32_t ROWS_PER_PAGE = PAGE_SIZE / ROW_SIZE;              // ~14 rows per page
const uint32_t TABLE_MAX_ROWS = ROWS_PER_PAGE * TABLE_MAX_PAGES;  // ~1400 rows max


// ROW SERIALIZATION FUNCTIONS

 /**
 * memcpy - Copies a block of memory from source to destination
 * 
 * void *memcpy(void *dest, const void *src, size_t n);
 * 
 * @dest: Pointer to the destination memory location (where to copy to)
 * @src:  Pointer to the source memory location (what to copy from)
 * @n:    Number of bytes to copy
 * 
 * Return: Pointer to dest (the destination address)
 * 
 * IMPORTANT:
 *   - Copies EXACTLY n bytes, no more, no less
 *   - Does NOT check for overlapping memory regions
 *   - Use memmove() if source and destination overlap
 *   - Source and destination should be at least n bytes in size
 *   - Much faster than copying byte-by-byte in a loop
 */

/**
* serialize_row - Convert a Row structure into raw bytes for storage
*
* Takes a structured Row and copies its fields to a destination memory location
* at the correct byte offsets. This allows the Row to be stored in pages
*
* @source: Pointer to the source Row structure
* @destination: Pointer to memory where the raw bytes will be written
*
* Memory layout:
*   destination[0-3]     → row.id
*   destination[4-35]    → row.username
*   destination[36-290]  → row.email
*/
void serialize_row(Row* source, void* destination) {
  // Copy ID_SIZE bytes (4 bytes) from source->id to destination + ID_OFFSET
  memcpy(destination + ID_OFFSET, &(source->id), ID_SIZE);
  memcpy(destination + USERNAME_OFFSET, &(source->username), USERNAME_SIZE);
  memcpy(destination + EMAIL_OFFSET, &(source->email), EMAIL_SIZE);
}

/**
* deserialize_row - Convert raw bytes back into a Row structure
*
* Reads raw bytes from memory and reconstructs a Row structure
* This is the inverse operation of serialize_row()
*
* @source: Pointer to the raw bytes in memory
* @destination: Pointer to the Row structure that will receive the data
*/
void deserialize_row(void* source, Row* destination) {
  memcpy(&(destination->id), source + ID_OFFSET, ID_SIZE);
  memcpy(&(destination->username), source + USERNAME_OFFSET, USERNAME_SIZE);
  memcpy(&(destination->email), source + EMAIL_OFFSET, EMAIL_SIZE);
}

/**
* row_slot - Calculate the memory address for a specific row number
*
* Given a row number, determines which page and which offset within that page
* the row should be stored at. Allocates a new page if it doesn't exist
*
* @table: Pointer to the Table structure
* @row_num: Row number (0-indexed) to locate
*
* Return: Pointer to the memory location where the row should be stored
*/
void* row_slot(Table* table, uint32_t row_num) {
  // Calculate which page contains this row
  uint32_t page_num = row_num / ROWS_PER_PAGE;

  // Get the page pointer (or allocate if NULL)
  void* page = table->pages[page_num];
  if (page == NULL) {
    // Lazy allocation: only allocate when first accessing a page
    page = table->pages[page_num] = malloc(PAGE_SIZE);
  }

  // Calculate position within the page
  uint32_t row_offset = row_num % ROWS_PER_PAGE;
  uint32_t byte_offset = row_offset * ROW_SIZE;

  return page + byte_offset;
}


// INPUT BUFFER FUNCTIONS

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


// TABLE FUNCTIONS

/**
* new_table - Creates and initializes a new Table structure
*
* Allocates memory for a Table object and initializes all pages to NULL
* The table starts empty with 0 rows
*
* Return: Pointer to the newly created Table structure
*/
Table* new_table() {
  Table* table = malloc(sizeof(Table));
  table->num_rows = 0;

  for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++){
    table->pages[i] = NULL;
  }

  return table;
}

/**
* free_table - Frees all memory used by the Table
*
* Iterates through all pages and frees each allocated page, then frees the
* Table structure itself.
*
* @table: Pointer to Table to be freed
*/
void free_table(Table* table) {
  // Free each allocated page
  for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++) {
    if (table->pages[i] != NULL) free(table->pages[i]);
  }
  free(table);
}


// USER INTERFACE FUNCTIONS

/*
* print_prompt - Displays the database prompt to the user
*/
void print_prompt() { printf("Ema's db > "); }

/**
* print_row - Display a Row's contents in human-readable format
*
* @row: Pointer to the Row structure to print
*/
void print_row(Row* row) {
  printf("(%d, %s, %s)\n", row->id, row->username, row->email);
}


// META-COMMAND HANDLING (Commands that start with '.')

/**
* do_meta_command - Handles dot-commands (.exit, .help, etc.)
*
* @input_buffer:  Contains the raw user input to check for meta-commands
* @table: Table pointer (needed for cleanup on .exit)
*
* Return: META_COMMAND_SUCCESS if command was handled,
*         META_COMMAND_UNRECOGNIZED otherwise
*/
MetaCommandResult do_meta_command(InputBuffer* input_buffer, Table* table) {
  if (strcmp(input_buffer->buffer, ".exit") == 0) {
    close_input_buffer(input_buffer); // Clean up input buffer
    free_table(table);                // Clean up table
    exit(EXIT_SUCCESS);
  } else {
    return META_COMMAND_UNRECOGNIZED;
  }
}


// STATEMENT PREPARATION (Parsing user input)

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

    // Parse the three arguments: id, username, email
    int args_assigned = sscanf(
      input_buffer->buffer, "insert %d %s %s", 
      &(statement->row_to_insert.id), 
      statement->row_to_insert.username, 
      statement->row_to_insert.email
    );

    if (args_assigned < 3) {
      return PREPARE_SYNTAX_ERROR;
    }
    return PREPARE_SUCCESS;
  }
  if (strcmp(input_buffer->buffer, "select") == 0) {
    statement->type = STATEMENT_SELECT;
    return PREPARE_SUCCESS;
  }

  return PREPARE_UNRECOGNIZED_STATEMENT;
}


// STATEMENT EXECUTION (Database operations)

/**
* execute_insert - Inserts a new row into the database
*
* Takes the row from the statement and stores it in the table's memory pages.
*
* @statement: Contains the row to insert
* @table: Target table for the insertion
*
* Return: EXECUTE_SUCCESS if insertion succeeded,
*         EXECUTE_TABLE_FULL if table has reached maximum capacity
*/
ExecuteResult execute_insert(Statement* statement, Table* table) {
  if (table->num_rows >= TABLE_MAX_ROWS) {
    return EXECUTE_TABLE_FULL;
  }

  // Get a pointer to the data to insert
  Row* row_to_insert = &(statement->row_to_insert);

  // Find the memory location for this row
  void* slot = row_slot(table, table->num_rows);

  // Convert the Row to raw bytes and store it
  serialize_row(row_to_insert, slot);
  table->num_rows += 1;

  return EXECUTE_SUCCESS;
}

/**
 * execute_select - Retrieves and displays all rows from the database
 * 
 * Iterates through all rows in the table, deserializes them from raw bytes
 * back into Row structures, and prints them.
 * 
 * @statement: Statement structure (not used, but kept for consistency)
 * @table: Source table to query
 * 
 * Return: Always returns EXECUTE_SUCCESS
 * 
 * Algorithm:
 *   1. Create a temporary Row structure
 *   2. For each row number from 0 to num_rows-1:
 *      a. Find the memory location using row_slot()
 *      b. Deserialize the raw bytes into the Row structure
 *      c. Print the Row contents
*/
ExecuteResult execute_select(Statement* statement, Table* table) {
  Row row;
  
  for (uint32_t i = 0; i < table->num_rows; i++) {
    // Find where the row is stored
    void* slot = row_slot(table, i);

    // Convert raw bytes back to a Row structure
    deserialize_row(slot, &row);

    print_row(&row);
  }

  return EXECUTE_SUCCESS;
}

/**
* execute_statement - Run a prepared statement
*
* Performs the actual database operation based on the statement type
*
* @statement: The parsed statement to execute
* @table: Database table to operate on
*/
ExecuteResult execute_statement(Statement* statement, Table* table) {
  switch (statement->type) {
    case (STATEMENT_INSERT):
      return execute_insert(statement, table);
    case (STATEMENT_SELECT):
      return execute_select(statement, table);
  }
  return EXECUTE_TABLE_FULL; // Should never happen
}


/**
* Main REPL (Read-Eval-Print Loop)
*/
int main(int argc, char* argv[]) {
  Table* table = new_table();
  InputBuffer* input_buffer = new_input_buffer();

  while (true) {
    print_prompt();
    read_input(input_buffer);

    if (input_buffer->buffer[0] == '.') {
      switch (do_meta_command(input_buffer, table)) {
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
      case (PREPARE_SYNTAX_ERROR):
        printf("Syntax error. Could not parse statement \n");
        continue;
      case (PREPARE_UNRECOGNIZED_STATEMENT):
        printf("Unrecognized keyword at start of '%s' \n", input_buffer->buffer);
        continue;
    }

    switch (execute_statement(&statement, table)) {
      case (EXECUTE_SUCCESS):
        printf("Executed \n");
        break;
      case (EXECUTE_TABLE_FULL):
        printf("Error: Table full \n");
        break;
    }
  }
}
