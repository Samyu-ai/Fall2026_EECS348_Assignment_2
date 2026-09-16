/*
 * Program Name: EECS 348 Assignment 2 - CEO Email Priority Queue
 * Description: Reads EMAIL, NEXT, READ, and COUNT commands from a test file
 *              and prioritizes unread emails with a list-based MaxHeap.
 * Inputs: A command-file path supplied as the only command-line argument.
 * Output: Email counts and the next highest-priority email are printed to the terminal.
 * Author: Samyu Aitha
 * Creation Date: September 16, 2026
 * Revision Date: September 16, 2026
 * Revisions: Translated the selected Claude design from Python to C; added safe
 *            parsing, dynamic storage, empty-heap handling, and memory cleanup.
 * Collaborators and Sources:
 *   - Microsoft Copilot generated the first Python comparison program supplied
 *     with the assignment analysis. No Copilot code is copied directly here.
 *   - Anthropic Claude generated the second Python comparison program. Its
 *     MaxHeap organization and priority-key idea were used as a conceptual basis.
 *   - OpenAI ChatGPT helped translate, test, comment, and improve the final C
 *     implementation. All final code was reviewed and adapted by the author.
 *   - C standard library documentation: https://en.cppreference.com/w/c
 */

/* This block uses standard C headers; no pre-existing heap module is included. */
#include <stdio.h>   /* Provides file input and terminal output functions. */
#include <stdlib.h>  /* Provides dynamic memory allocation and conversion functions. */
#include <string.h>  /* Provides string length, copy, comparison, and search functions. */

/* This constant stores the initial number of email slots in the dynamic list. */
#define INITIAL_CAPACITY 8

/* This structure stores every field needed to represent and prioritize one email. */
typedef struct {
    char *category;          /* Stores the sender category as dynamically allocated text. */
    char *subject;           /* Stores the subject line as dynamically allocated text. */
    char *date_text;         /* Stores the original MM-DD-YYYY date for display. */
    int category_priority;   /* Stores the numeric priority assigned to the category. */
    int date_key;            /* Stores YYYYMMDD so newer dates compare as larger integers. */
    unsigned long sequence;  /* Preserves arrival order when category and date are identical. */
} Email;

/* This structure is a list-based MaxHeap whose list is a dynamic C array. */
typedef struct {
    Email *items;      /* Points to the list that stores Email structures. */
    size_t size;       /* Tracks the number of unread emails currently stored. */
    size_t capacity;   /* Tracks the number of allocated positions in the list. */
} MaxHeap;

/* This author-written helper reports a fatal error and ends the program safely. */
static void fail(const char *message) {
    fprintf(stderr, "%s\n", message); /* Prints the supplied error to standard error. */
    exit(EXIT_FAILURE);                 /* Stops execution with a failure status. */
}

/* This author-written helper allocates an independent copy of a string. */
static char *copy_string(const char *source) {
    size_t length = strlen(source) + 1;         /* Counts all characters plus the null terminator. */
    char *copy = malloc(length);                /* Allocates exactly enough memory for the copy. */
    if (copy == NULL) {                         /* Checks whether allocation failed. */
        fail("Error: unable to allocate memory."); /* Stops rather than using a null pointer. */
    }
    memcpy(copy, source, length);               /* Copies the text and its null terminator. */
    return copy;                                /* Returns ownership of the new string. */
}

/* This author-written helper removes surrounding spaces and line-ending characters. */
static char *trim(char *text) {
    char *end;                                  /* Will point at the last retained character. */
    while (*text == ' ' || *text == '\t') {     /* Skips spaces and tabs at the beginning. */
        text++;                                 /* Advances to the first non-whitespace character. */
    }
    end = text + strlen(text);                  /* Starts just after the final character. */
    while (end > text && (end[-1] == ' ' || end[-1] == '\t' ||
                          end[-1] == '\n' || end[-1] == '\r')) { /* Finds trailing whitespace. */
        end--;                                  /* Moves backward over one unwanted character. */
    }
    *end = '\0';                                /* Ends the string after the retained text. */
    return text;                                /* Returns the address of the trimmed beginning. */
}

/* This author-written function converts an allowed category into its required rank. */
static int category_rank(const char *category) {
    if (strcmp(category, "Boss") == 0) return 5;            /* Gives Boss the highest rank. */
    if (strcmp(category, "Subordinate") == 0) return 4;     /* Gives Subordinate the next rank. */
    if (strcmp(category, "Peer") == 0) return 3;            /* Gives Peer the middle rank. */
    if (strcmp(category, "ImportantPerson") == 0) return 2; /* Gives ImportantPerson rank two. */
    if (strcmp(category, "OtherPerson") == 0) return 1;     /* Gives OtherPerson the lowest rank. */
    return 0;                                                /* Marks any unexpected category invalid. */
}

/* This author-written function converts MM-DD-YYYY into sortable YYYYMMDD form. */
static int parse_date_key(const char *date_text) {
    int month;                                                        /* Receives the parsed month. */
    int day;                                                          /* Receives the parsed day. */
    int year;                                                         /* Receives the parsed year. */
    char extra;                                                       /* Detects unwanted trailing input. */
    if (sscanf(date_text, "%2d-%2d-%4d%c", &month, &day, &year, &extra) != 3 ||
        month < 1 || month > 12 || day < 1 || day > 31) {             /* Validates the required basic format. */
        return -1;                                                    /* Signals that the date cannot be used. */
    }
    return year * 10000 + month * 100 + day;                          /* Builds a chronologically sortable key. */
}

/* This author-written comparison returns true when first belongs above second. */
static int is_higher_priority(const Email *first, const Email *second) {
    if (first->category_priority != second->category_priority) {      /* Compares categories first. */
        return first->category_priority > second->category_priority;  /* Higher category rank wins. */
    }
    if (first->date_key != second->date_key) {                        /* Breaks a category tie by date. */
        return first->date_key > second->date_key;                    /* Newer email wins the tie. */
    }
    return first->sequence < second->sequence;                        /* Earlier arrival wins an exact tie. */
}

/* This author-written function initializes an empty list-based MaxHeap. */
static void heap_init(MaxHeap *heap) {
    heap->items = malloc(INITIAL_CAPACITY * sizeof(*heap->items)); /* Allocates the initial email list. */
    if (heap->items == NULL) {                                    /* Checks whether the allocation succeeded. */
        fail("Error: unable to allocate heap storage.");          /* Stops safely if no list can be created. */
    }
    heap->size = 0;                                                /* Records that the inbox starts empty. */
    heap->capacity = INITIAL_CAPACITY;                             /* Records the number of allocated slots. */
}

/* This author-written helper exchanges two complete Email structures. */
static void swap_emails(Email *first, Email *second) {
    Email temporary = *first; /* Saves the first structure before overwriting it. */
    *first = *second;         /* Moves the second structure into the first position. */
    *second = temporary;      /* Moves the saved structure into the second position. */
}

/* This author-written operation inserts one email and restores the MaxHeap property. */
static void heap_push(MaxHeap *heap, Email email) {
    size_t index;                                                        /* Tracks the new email while it rises. */
    if (heap->size == heap->capacity) {                                 /* Checks whether the list is full. */
        size_t new_capacity = heap->capacity * 2;                        /* Doubles capacity for amortized efficiency. */
        Email *resized = realloc(heap->items, new_capacity * sizeof(*resized)); /* Requests the larger list. */
        if (resized == NULL) {                                          /* Checks whether resizing failed. */
            fail("Error: unable to expand heap storage.");             /* Stops before losing valid heap data. */
        }
        heap->items = resized;                                          /* Uses the successfully resized list. */
        heap->capacity = new_capacity;                                  /* Saves the updated capacity. */
    }
    index = heap->size;                                                  /* Selects the next unused list position. */
    heap->items[index] = email;                                         /* Places the new email at the end. */
    heap->size++;                                                        /* Includes the new email in the heap size. */
    while (index > 0) {                                                  /* Continues until the root or correct position. */
        size_t parent = (index - 1) / 2;                                /* Computes the parent list index. */
        if (!is_higher_priority(&heap->items[index], &heap->items[parent])) break; /* Stops when order is valid. */
        swap_emails(&heap->items[index], &heap->items[parent]);          /* Moves the higher-priority child upward. */
        index = parent;                                                  /* Continues checking from its new position. */
    }
}

/* This author-written operation returns the maximum email without deleting it. */
static const Email *heap_peek(const MaxHeap *heap) {
    return heap->size == 0 ? NULL : &heap->items[0]; /* Returns null for empty or the root for nonempty. */
}

/* This author-written helper releases every dynamic string owned by one email. */
static void free_email(Email *email) {
    free(email->category);  /* Releases the copied category string. */
    free(email->subject);   /* Releases the copied subject string. */
    free(email->date_text); /* Releases the copied display-date string. */
}

/* This author-written operation removes the maximum email and restores heap order. */
static void heap_pop(MaxHeap *heap) {
    size_t index = 0;                                                   /* Starts heap repair at the root. */
    if (heap->size == 0) return;                                        /* Makes READ on an empty inbox harmless. */
    free_email(&heap->items[0]);                                        /* Releases the removed email's strings. */
    heap->size--;                                                       /* Removes one item from the logical list. */
    if (heap->size == 0) return;                                        /* Finishes if the removed item was alone. */
    heap->items[0] = heap->items[heap->size];                            /* Moves the last email to the root. */
    while (1) {                                                         /* Repeats until the MaxHeap property is restored. */
        size_t left = 2 * index + 1;                                    /* Computes the left-child index. */
        size_t right = 2 * index + 2;                                   /* Computes the right-child index. */
        size_t largest = index;                                         /* Initially assumes the current item is largest. */
        if (left < heap->size && is_higher_priority(&heap->items[left], &heap->items[largest])) largest = left; /* Tests left child. */
        if (right < heap->size && is_higher_priority(&heap->items[right], &heap->items[largest])) largest = right; /* Tests right child. */
        if (largest == index) break;                                    /* Stops when neither child should move up. */
        swap_emails(&heap->items[index], &heap->items[largest]);         /* Moves the best child toward the root. */
        index = largest;                                                 /* Continues repair at the child's old position. */
    }
}

/* This author-written function releases all storage still owned by the heap. */
static void heap_destroy(MaxHeap *heap) {
    size_t index;                                                       /* Visits each unread email still stored. */
    for (index = 0; index < heap->size; index++) free_email(&heap->items[index]); /* Frees every email's strings. */
    free(heap->items);                                                  /* Releases the list itself. */
    heap->items = NULL;                                                 /* Prevents accidental reuse of freed storage. */
    heap->size = 0;                                                     /* Resets the heap's logical size. */
    heap->capacity = 0;                                                 /* Resets the recorded allocation size. */
}

/* This author-written function parses one EMAIL command and inserts valid data. */
static void process_email_command(MaxHeap *heap, char *fields, unsigned long sequence) {
    char *first_comma = strchr(fields, ',');                            /* Finds the category delimiter. */
    char *second_comma;                                                 /* Will locate the subject delimiter. */
    char *category;                                                     /* Will point to the parsed category. */
    char *subject;                                                      /* Will point to the parsed subject. */
    char *date_text;                                                    /* Will point to the parsed date. */
    Email email;                                                        /* Holds the completed email before insertion. */
    if (first_comma == NULL) fail("Error: malformed EMAIL command.");   /* Rejects a command missing its first comma. */
    *first_comma = '\0';                                                /* Ends the category field. */
    second_comma = strchr(first_comma + 1, ',');                        /* Finds the next comma after the category. */
    if (second_comma == NULL) fail("Error: malformed EMAIL command.");  /* Rejects a command missing its second comma. */
    *second_comma = '\0';                                               /* Ends the subject field. */
    category = trim(fields);                                            /* Removes optional spaces around category. */
    subject = trim(first_comma + 1);                                    /* Removes optional spaces around subject. */
    date_text = trim(second_comma + 1);                                 /* Removes spaces and the line ending from date. */
    email.category_priority = category_rank(category);                  /* Converts the category into its priority. */
    email.date_key = parse_date_key(date_text);                         /* Converts the date into comparison form. */
    if (email.category_priority == 0 || email.date_key < 0) fail("Error: invalid EMAIL data."); /* Validates fields. */
    email.category = copy_string(category);                             /* Saves an owned copy of category. */
    email.subject = copy_string(subject);                               /* Saves an owned copy of subject. */
    email.date_text = copy_string(date_text);                           /* Saves an owned copy of the original date. */
    email.sequence = sequence;                                          /* Saves arrival order for deterministic ties. */
    heap_push(heap, email);                                             /* Adds the completed email to the MaxHeap. */
}

/* This author-written function reads and executes every command in the test file. */
static void process_file(FILE *input) {
    MaxHeap heap;                                                       /* Stores the CEO's unread emails. */
    char line[4096];                                                    /* Stores one command line, including long subjects. */
    unsigned long sequence = 0;                                        /* Counts EMAIL commands in arrival order. */
    heap_init(&heap);                                                   /* Creates the empty list-based MaxHeap. */
    while (fgets(line, sizeof(line), input) != NULL) {                  /* Reads commands until the end of the file. */
        char *command = trim(line);                                     /* Removes surrounding whitespace from the command. */
        if (*command == '\0') continue;                                 /* Ignores blank lines. */
        if (strncmp(command, "EMAIL ", 6) == 0) {                       /* Detects an EMAIL insertion command. */
            process_email_command(&heap, command + 6, sequence++);      /* Parses and inserts the new email. */
        } else if (strcmp(command, "COUNT") == 0) {                    /* Detects a request for unread count. */
            printf("There are %zu emails to read.\n\n", heap.size);    /* Prints count using the sample format. */
        } else if (strcmp(command, "NEXT") == 0) {                     /* Detects a request to display the next email. */
            const Email *next = heap_peek(&heap);                       /* Looks at the root without removing it. */
            if (next == NULL) {                                        /* Handles NEXT when the inbox is empty. */
                printf("There are no emails to read.\n\n");           /* Reports the empty inbox clearly. */
            } else {                                                    /* Handles NEXT when an email is available. */
                printf("Next email:\n");                              /* Prints the sample heading. */
                printf("      Sender: %s\n", next->category);          /* Prints the selected sender category. */
                printf("      Subject: %s\n", next->subject);          /* Prints the selected subject. */
                printf("      Date: %s\n\n", next->date_text);         /* Prints the selected date and a blank line. */
            }
        } else if (strcmp(command, "READ") == 0) {                     /* Detects removal of the highest-priority email. */
            heap_pop(&heap);                                            /* Removes it silently, including on an empty heap. */
        } else {                                                        /* Handles commands outside the required format. */
            fprintf(stderr, "Warning: ignored command: %s\n", command); /* Reports the unexpected line without crashing. */
        }
    }
    heap_destroy(&heap);                                                /* Releases all memory before returning. */
}

/* This author-written main function validates arguments, opens the file, and runs it. */
int main(int argc, char *argv[]) {
    FILE *input;                                                        /* Will refer to the requested test file. */
    if (argc != 2) {                                                    /* Requires exactly one command-file argument. */
        fprintf(stderr, "Usage: %s <test_file>\n", argv[0]);           /* Shows the correct command format. */
        return EXIT_FAILURE;                                            /* Returns failure without opening a file. */
    }
    input = fopen(argv[1], "r");                                       /* Opens the command file for reading. */
    if (input == NULL) {                                                /* Checks whether the file could be opened. */
        perror("Error opening test file");                             /* Prints the operating system's error detail. */
        return EXIT_FAILURE;                                            /* Returns failure because processing cannot begin. */
    }
    process_file(input);                                                /* Processes every command with the custom MaxHeap. */
    fclose(input);                                                      /* Closes the test file after processing. */
    return EXIT_SUCCESS;                                                /* Reports successful program completion. */
}
