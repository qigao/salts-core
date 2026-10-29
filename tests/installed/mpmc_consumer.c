#include <bucket_priority_queue_mpmc.h>
#include <salts/thread.h>

#include <stdio.h>
#include <string.h>

#define REQUIRE(condition) do { \
  if (!(condition)) { \
    fprintf(stderr, "MPMC contract failed at line %d: %s\n", __LINE__, #condition); \
    return 1; \
  } \
} while (0)

enum { PRODUCERS = 4, CONSUMERS = 3, ITEMS_PER_PRODUCER = 1024,
       TOTAL_ITEMS = PRODUCERS * ITEMS_PER_PRODUCER };

typedef struct {
  bucket_priority_queue_mpmc_t queue;
  cmeta_mutex_t ledger_mutex;
  unsigned char seen[TOTAL_ITEMS];
  size_t consumed;
  int invalid;
} mpmc_state;

typedef struct {
  mpmc_state *state;
  size_t producer;
} producer_context;

static void produce(void *argument) {
  producer_context *context = (producer_context *)argument;
  size_t index;
  for (index = 0; index < ITEMS_PER_PRODUCER; ++index) {
    size_t value = context->producer * ITEMS_PER_PRODUCER + index;
    bucket_priority_queue_mpmc_push_blocking(
        &context->state->queue,
        (bucket_priority_mpmc_t)(value % BUCKET_PRIORITY_MPMC_COUNT), value);
  }
}

static void consume(void *argument) {
  mpmc_state *state = (mpmc_state *)argument;
  for (;;) {
    size_t value = TOTAL_ITEMS;
    int finished;
    cmeta_mutex_lock(&state->ledger_mutex);
    finished = state->consumed == TOTAL_ITEMS;
    cmeta_mutex_unlock(&state->ledger_mutex);
    if (finished) return;
    if (!bucket_priority_queue_mpmc_pop_blocking(&state->queue, &value, 5))
      continue;
    cmeta_mutex_lock(&state->ledger_mutex);
    if (value >= TOTAL_ITEMS || state->seen[value] != 0)
      state->invalid = 1;
    else
      state->seen[value] = 1;
    ++state->consumed;
    cmeta_mutex_unlock(&state->ledger_mutex);
  }
}

int main(void) {
  mpmc_state state;
  producer_context producers[PRODUCERS];
  cmeta_thread_t producer_threads[PRODUCERS];
  cmeta_thread_t consumer_threads[CONSUMERS];
  bucket_priority_queue_mpmc_t invalid_queue;
  size_t index;
  size_t value = 991;
  memset(&state, 0, sizeof(state));
  memset(&invalid_queue, 0, sizeof(invalid_queue));

  REQUIRE(!bucket_priority_queue_mpmc_init(NULL, 16, 1));
  REQUIRE(!bucket_priority_queue_mpmc_init(&invalid_queue, 0, 1));
  REQUIRE(!bucket_priority_queue_mpmc_init(&invalid_queue, 3, 1));
  REQUIRE(invalid_queue.impl == NULL);
  REQUIRE(bucket_priority_queue_mpmc_empty(NULL));
  REQUIRE(bucket_priority_queue_mpmc_empty(&invalid_queue));
  REQUIRE(!bucket_priority_queue_mpmc_try_pop(NULL, &value));
  REQUIRE(!bucket_priority_queue_mpmc_pop_blocking(&invalid_queue, &value, 1));
  REQUIRE(value == 991);
  bucket_priority_queue_mpmc_destroy(&invalid_queue);
  bucket_priority_queue_mpmc_destroy(NULL);

  REQUIRE(bucket_priority_queue_mpmc_init(&state.queue, 16, CONSUMERS));
  REQUIRE(bucket_priority_queue_mpmc_empty(&state.queue));
  REQUIRE(!bucket_priority_queue_mpmc_try_push(
      &state.queue, (bucket_priority_mpmc_t)-1, 7));
  REQUIRE(!bucket_priority_queue_mpmc_try_push(
      &state.queue, BUCKET_PRIORITY_MPMC_COUNT, 7));
  REQUIRE(!bucket_priority_queue_mpmc_try_pop(&state.queue, NULL));
  REQUIRE(!bucket_priority_queue_mpmc_pop_blocking(&state.queue, &value, 1));
  REQUIRE(value == 991);

  /* Each priority retains FIFO order, and higher priorities drain first. */
  for (index = 0; index < 4; ++index) {
    REQUIRE(bucket_priority_queue_mpmc_try_push(
        &state.queue, (bucket_priority_mpmc_t)index, index * 10));
    REQUIRE(bucket_priority_queue_mpmc_try_push(
        &state.queue, (bucket_priority_mpmc_t)index, index * 10 + 1));
  }
  for (index = 4; index > 0; --index) {
    REQUIRE(bucket_priority_queue_mpmc_try_pop(&state.queue, &value));
    REQUIRE(value == (index - 1) * 10);
    REQUIRE(bucket_priority_queue_mpmc_try_pop(&state.queue, &value));
    REQUIRE(value == (index - 1) * 10 + 1);
  }
  REQUIRE(bucket_priority_queue_mpmc_empty(&state.queue));

  /* One independently tracked delivery for every producer value. Counting
     dequeues alone would not detect a duplicated value replacing a lost one. */
  cmeta_mutex_init(&state.ledger_mutex);
  REQUIRE(state.ledger_mutex != NULL);
  for (index = 0; index < CONSUMERS; ++index)
    REQUIRE(cmeta_thread_create(&consumer_threads[index], consume, &state) == 0);
  for (index = 0; index < PRODUCERS; ++index) {
    producers[index].state = &state;
    producers[index].producer = index;
    REQUIRE(cmeta_thread_create(&producer_threads[index], produce, &producers[index]) == 0);
  }
  /* Query concurrently with pop: the approximation must still be race-free. */
  for (index = 0; index < TOTAL_ITEMS; ++index) {
    (void)bucket_priority_queue_mpmc_empty(&state.queue);
    cmeta_thread_yield();
  }
  for (index = 0; index < PRODUCERS; ++index)
    REQUIRE(cmeta_thread_join(&producer_threads[index]) == 0);
  for (index = 0; index < CONSUMERS; ++index)
    REQUIRE(cmeta_thread_join(&consumer_threads[index]) == 0);
  REQUIRE(state.consumed == TOTAL_ITEMS && !state.invalid);
  for (index = 0; index < TOTAL_ITEMS; ++index)
    REQUIRE(state.seen[index] == 1);
  REQUIRE(bucket_priority_queue_mpmc_empty(&state.queue));
  cmeta_mutex_destroy(&state.ledger_mutex);
  bucket_priority_queue_mpmc_destroy(&state.queue);
  REQUIRE(state.queue.impl == NULL);
  bucket_priority_queue_mpmc_destroy(&state.queue);
  puts("MPMC priority/FIFO, failure outputs, lifetime and unique delivery: PASS");
  return 0;
}
