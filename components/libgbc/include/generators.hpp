#pragma once

namespace gbc
{
struct sample_t
{
    uint16_t left;
    uint16_t right;
};
struct Generator
{
    virtual void tick(Machine&) = 0;
    virtual sample_t sample(Machine&) = 0;
};

struct WhiteNoise : public Generator
{
    void tick(Machine& machine) override {}
    sample_t sample(Machine& machine) override { return {0, 0}; }
};
} // namespace gbc
