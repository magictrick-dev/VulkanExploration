#ifndef SOURCE_UTILITIES_HPP
#define SOURCE_UTILITIES_HPP
#include <cstdint>

struct QueueFamilyIndices
{

    int32_t graphics_family = -1;

};

inline bool 
queue_family_indices_valid(QueueFamilyIndices *queue_family_indices)
{

    return queue_family_indices->graphics_family >= 0;

}

#endif
