#include <stdbool.h>  // Boolean values
#include <stdio.h>    // Standard inputs/outpus functions (printf, getline)
#include <stdlib.h>   // Memory allocation (mallc, free) and exit() functions
#include <string.h>   // String functions (strcmp for comparison)


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
  //  &(input_length->buffer_length) - Address of size variable (gets updated)
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

int main(int argc, char* argv[]) {
  InputBuffer* input_buffer = new_input_buffer();
  while (true) {
    print_prompt();
    read_input(input_buffer);

    if (strcmp(input_buffer->buffer, ".exit") == 0) {
      close_input_buffer(input_buffer);
      exit(EXIT_SUCCESS);
    } else {
      printf("Unrecognized command '%s'.\n", input_buffer->buffer);
    }
  }
}
