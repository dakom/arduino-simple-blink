// Every test in time_test.c again, with the queue at the largest capacity the library allows:
// too big for the Uno's RAM, but it checks the library is right at its limit.
//
// The capacity is fixed when time.c is compiled (it sizes the queue's arrays), so this test
// compiles its own copy of time.c at the maximum. The linker then uses this copy, and ignores
// the one in the test library, which was built at the firmware's CALLBACKS_TIME_CAPACITY.
#include "lib/collections/handle.h"
#include "lib/collections/time_heap.h"

// The queue is a handle pool plus a time heap, so its limit is theirs. (This check also
// has to come before the #undef below: clangd, the editor's checker, misreads an #undef in
// a file's opening run of # lines, and this line ends that run.)
static_assert(HANDLE_POOL_MAX_CAPACITY == TIME_HEAP_MAX_CAPACITY,
              "the handle pool and the time heap should have the same largest capacity");

// #undef first: Taskfile.yml passes the firmware's value to every test
#undef CALLBACKS_TIME_CAPACITY
#define CALLBACKS_TIME_CAPACITY HANDLE_POOL_MAX_CAPACITY

#include "lib/callbacks/time.c"

// "time_test.c", not "lib/callbacks/time_test.c": the file next to this one
#include "time_test.c"
