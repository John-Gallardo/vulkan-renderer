// NOTE: had to do this so vmaImportFunctionsFromVolk() actually gets defined
#include "volk.h"  // IWYU pragma: keep

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
