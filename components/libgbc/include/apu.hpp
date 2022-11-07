#pragma once
#include "common.hpp"
#include <array>
#include <cstdint>
#include <functional>

namespace gbc
{
class APU
{
public:
    APU(Machine& mach);
    using audio_stream_t = std::function<void(uint16_t, uint16_t)>;

    void on_audio_out(audio_stream_t);
    void simulate();

    uint8_t read(uint16_t, uint8_t& reg);
    void write(uint16_t, uint8_t, uint8_t& reg);

    // serialization
    int restore_state(const std::vector<uint8_t>&, int);
    void serialize_state(std::vector<uint8_t>&) const;

    Machine& machine() noexcept { return m_machine; }

private:
    struct generator_t
    {
    };
    struct channel_t
    {
        bool generators_enabled[4] = {false};
        bool enabled = true;
    };

    struct state_t
    {
        bool nothing = false;

    } m_state;

    Machine& m_machine;
    audio_stream_t m_audio_out;
};
} // namespace gbc
