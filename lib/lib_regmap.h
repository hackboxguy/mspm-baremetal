#ifndef LIB_REGMAP_H
#define LIB_REGMAP_H

#include <stdbool.h>
#include <stdatomic.h>
#include <stdint.h>

#define LIB_REGMAP_UNMAPPED_VALUE UINT8_C(0xff)

typedef enum {
    LIB_REGMAP_ACCESS_READ_ONLY = 0,
    LIB_REGMAP_ACCESS_READ_WRITE,
} lib_regmap_access_t;

typedef enum {
    LIB_REGMAP_WRITE_IGNORED = 0,
    LIB_REGMAP_WRITE_QUEUED,
    LIB_REGMAP_WRITE_REJECTED,
} lib_regmap_write_result_t;

typedef struct {
    uint16_t address;
    uint8_t value;
} lib_regmap_command_t;

/* Fixed-storage SPSC queue: I2C producer, main-loop consumer. */
typedef struct {
    lib_regmap_command_t *storage;
    uint32_t capacity;
    volatile uint32_t write_index;
    volatile uint32_t read_index;
    volatile uint32_t dropped_count;
} lib_regmap_command_queue_t;

/*
 * The active-buffer bit and both reader counts are updated as one atomic word.
 * This lets a publisher reject a buffer still held by an I2C read transaction.
 */
typedef struct {
    uint8_t *buffers[2];
    uint16_t length;
    atomic_uint_least32_t state;
    volatile uint32_t publish_rejected_count;
} lib_regmap_snapshot_t;

/* Initialize and publish from one main-loop context; publishers do not race. */

typedef bool (*lib_regmap_queue_write_t)(void *context, uint16_t address,
                                         uint8_t value);

typedef struct {
    uint16_t first_address;
    uint16_t length;
    lib_regmap_access_t access;
    lib_regmap_snapshot_t *snapshot;
    lib_regmap_queue_write_t queue_write;
    void *context;
} lib_regmap_page_t;

typedef struct {
    const lib_regmap_page_t *pages;
    uint16_t page_count;
    uint16_t current_address;
    const lib_regmap_page_t *latched_page;
    uint8_t latched_buffer;
} lib_regmap_t;

bool lib_regmap_command_queue_init(lib_regmap_command_queue_t *queue,
                                   lib_regmap_command_t *storage, uint32_t capacity);
bool lib_regmap_command_queue_enqueue(void *context, uint16_t address, uint8_t value);
bool lib_regmap_command_queue_try_pop(lib_regmap_command_queue_t *queue,
                                      lib_regmap_command_t *command);
uint32_t
lib_regmap_command_queue_dropped_count(const lib_regmap_command_queue_t *queue);

bool lib_regmap_snapshot_init(lib_regmap_snapshot_t *snapshot, uint8_t *buffer_zero,
                              uint8_t *buffer_one, uint16_t length,
                              const uint8_t *initial_data);
bool lib_regmap_snapshot_publish(lib_regmap_snapshot_t *snapshot, const uint8_t *data,
                                 uint16_t length);
uint32_t
lib_regmap_snapshot_publish_rejected_count(const lib_regmap_snapshot_t *snapshot);

bool lib_regmap_init(lib_regmap_t *regmap, const lib_regmap_page_t *pages,
                     uint16_t page_count);
void lib_regmap_set_address(lib_regmap_t *regmap, uint16_t address);
uint16_t lib_regmap_current_address(const lib_regmap_t *regmap);
void lib_regmap_begin_read(lib_regmap_t *regmap);
void lib_regmap_end_read(lib_regmap_t *regmap);
uint8_t lib_regmap_read_current(lib_regmap_t *regmap);
lib_regmap_write_result_t lib_regmap_write_current(lib_regmap_t *regmap, uint8_t value);

#endif
