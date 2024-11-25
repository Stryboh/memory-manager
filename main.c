#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PAGE_SIZE 512
#define NUM_PAGES 256
#define MEMORY_SIZE (PAGE_SIZE * NUM_PAGES)
#define SEGMENT_COUNT 1

typedef struct {
    unsigned int page_number;
    unsigned int valid;
} PageTableEntry;

typedef struct {
    PageTableEntry *page_table;
    size_t size;
} Segment;

typedef struct {
    Segment *segments[SEGMENT_COUNT];
    char *physical_memory;
    unsigned int *page_frames;
} MemoryManager;

int allocate_page(MemoryManager *manager) {
    for (int i = 0; i < NUM_PAGES; i++) {
        if (manager->page_frames[i] == 0) {
            manager->page_frames[i] = 1;
            return i;
        }
    }
    return -1;
}

MemoryManager *init_memory_manager() {
    MemoryManager *manager = (MemoryManager *) malloc(sizeof(MemoryManager));
    manager->physical_memory = (char *) malloc(MEMORY_SIZE);
    manager->page_frames = (unsigned int *) calloc(NUM_PAGES, sizeof(unsigned int));

    for (int i = 0; i < SEGMENT_COUNT; i++) {
        manager->segments[i] = NULL;
    }
    return manager;
}

int allocate_segment(MemoryManager *manager, size_t size) {
    for (int i = 0; i < SEGMENT_COUNT; i++) {
        if (manager->segments[i] == NULL) {
            size_t num_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
            Segment *segment = (Segment *) malloc(sizeof(Segment));
            segment->size = size;
            segment->page_table = (PageTableEntry *) calloc(num_pages, sizeof(PageTableEntry));

            for (size_t j = 0; j < num_pages; j++) {
                segment->page_table[j].valid = 0;
            }
            manager->segments[i] = segment;
            return i;
        }
    }
    return -1;
}

void free_segment(MemoryManager *manager, int segment_index) {
    if (segment_index >= 0 && segment_index < SEGMENT_COUNT && manager->segments[segment_index] != NULL) {
        Segment *segment = manager->segments[segment_index];
        size_t num_pages = (segment->size + PAGE_SIZE - 1) / PAGE_SIZE;

        for (size_t i = 0; i < num_pages; i++) {
            if (segment->page_table[i].valid) {
                manager->page_frames[segment->page_table[i].page_number] = 0;
            }
        }
        free(segment->page_table);
        free(segment);
        manager->segments[segment_index] = NULL;
    }
}

void *resolve_address(MemoryManager *manager, int segment_index, size_t offset) {
    Segment *segment = manager->segments[segment_index];
    if (segment == NULL || offset >= segment->size) {
        return NULL;
    }

    size_t page_index = offset / PAGE_SIZE;
    size_t page_offset = offset % PAGE_SIZE;

    PageTableEntry *entry = &segment->page_table[page_index];
    if (!entry->valid) {
        int frame_number = allocate_page(manager);
        if (frame_number == -1) {
            printf("Error: No free pages!\n");

            return NULL;
        }
        entry->page_number = frame_number;
        entry->valid = 1;
    }

    printf("| Page Id | Physical frame | Offset |\n", page_index, entry->page_number, page_offset);
    printf("| %4zu    | %8u       | %4zu   |\n", page_index, entry->page_number, page_offset);

    return manager->physical_memory + (entry->page_number * PAGE_SIZE) + page_offset;
}

void write_memory(MemoryManager *manager, int segment_index, void *data, size_t size, size_t offset) {
    printf("\033[5;33mWriting\033[0m\n");
    printf("_____________________________________\n");
    Segment *segment = manager->segments[segment_index];
    if (segment == NULL) {
        printf("Error: Segment not found!\n");
        return;
    }
    void *first_addr = NULL;
    void *second_addr = NULL;
    for (size_t i = 0; i < size; i++) {
        void *addr = resolve_address(manager, segment_index, offset + i);

        if (first_addr == NULL) {
            first_addr = addr;
        }
        second_addr = addr;
        if (addr) {
            memcpy(addr, (char *) data + i, 1);
        } else {
            printf("Error: Inaccessable address!\n");
            return;
        }
    }
    printf("‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾\n");
    printf("Addresses:\n");
    printf("%p - %p\n", first_addr, second_addr);
}

void free_page(MemoryManager *manager, int segment_index, size_t offset) {
    Segment *segment = manager->segments[segment_index];
    if (segment == NULL || offset >= segment->size) {
        printf("Error: Segment not found or offset is outside the segment range");
        return;
    }

    size_t page_index = offset / PAGE_SIZE;
    PageTableEntry *entry = &segment->page_table[page_index];

    if (!entry->valid) {
        printf("Error: Page invalid, can't free\n");
        return;
    }

    manager->page_frames[entry->page_number] = 0;

    entry->valid = 0;
    memset(manager->physical_memory + entry->page_number * PAGE_SIZE, 0, PAGE_SIZE);

    printf("Page deleted. Physical frame %u is now free\n", entry->page_number);
}

void read_memory(MemoryManager *manager, int segment_index, void *buffer, size_t size, size_t offset) {
    printf("\033[5;32mReading\033[0m\n");
    printf("_____________________________________\n");
    Segment *segment = manager->segments[segment_index];
    size_t page_index = offset / PAGE_SIZE;
    PageTableEntry *entry = &segment->page_table[page_index];
    if (segment == NULL) {
        printf("Error: Segment not found!\n");
        return;
    }
    if (!entry->valid) {
        printf("\033[0;41mError: Page is invalid. Unable to read data\033[0m\n");
        return;
    }

    void *first_addr = NULL;
    void *second_addr = NULL;
    for (size_t i = 0; i < size; i++) {
        void *addr = resolve_address(manager, segment_index, offset + i);

        if (first_addr == NULL) {
            first_addr = addr;
        }
        second_addr = addr;

        if (addr) {
            memcpy((char *) buffer + i, addr, 1);
        } else {
            printf("Error: Inaccessable address!\n");
            return;
        }
    }
    printf("‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾\n");
    printf("Addresses:\n");
    printf("%p - %p\n", first_addr, second_addr);
}

int main() {
    MemoryManager *manager = init_memory_manager();

    int segment_index = allocate_segment(manager, 8192);
    if (segment_index == -1) {
        printf("Error allocating segment memory!\n");
        return 1;
    }

    printf("================Char=================\n");
    char data_char[] = "Hello, Memory!";
    printf("Input:%s\n", data_char);
    write_memory(manager, segment_index, data_char, sizeof(data_char) - 1, 0);
    char buffer_char[50];
    read_memory(manager, segment_index, buffer_char, sizeof(data_char) - 1, 0);
    printf("Output:%s\n", buffer_char);

    printf("=================INT=================\n");
    int data_int = 12345;
    printf("Input:%d\n", data_int);
    write_memory(manager, segment_index, &data_int, sizeof(data_int), 100);
    int buffer_int;
    read_memory(manager, segment_index, &buffer_int, sizeof(data_int), 100);
    printf("Output: %d\n", buffer_int);

    printf("================Float================\n");
    float data_float = 3.14;
    printf("Input:%f\n", data_float);
    write_memory(manager, segment_index, &data_float, sizeof(data_float), 200);
    float buffer_float;
    read_memory(manager, segment_index, &buffer_float, sizeof(data_float), 200);
    printf("Output: %f\n", buffer_float);

    printf("\033[5;31mRemoving page\033[0m\n");
    free_page(manager, segment_index, 200);
    buffer_float = 0.0;
    printf("Input:%f\n", data_float);
    read_memory(manager, segment_index, &buffer_float, sizeof(data_float), 200);
    printf("Output: %f\n", buffer_float);

    free_segment(manager, segment_index);
    free(manager->physical_memory);
    free(manager->page_frames);
    free(manager);
    return 0;
}
