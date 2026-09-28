#ifndef LIGHTING_H
#define LIGHTING_H

#include <cstddef>

class Lighting
{
public:
    virtual ~Lighting() = default;

    virtual bool set(std::size_t channel, bool on) = 0;
    virtual bool get(std::size_t channel, bool& on) const = 0;
    virtual bool setAll(bool on) = 0;
};

#endif // LIGHTING_H