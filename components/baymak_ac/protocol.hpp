#pragma once

#include "esphome/components/remote_base/remote_base.h"

struct __AcState {

};

typedef struct __AcState AcState;

void send_ac_frame(AcState &state);


namespace esphome {
namespace baymak_ac {

using esphome::remote_base::RemoteTransmitData;

enum BitOrder {
  LSB_FIRST,
  MSB_FIRST
};

inline void sendPulseWidthDistanceArray(RemoteTransmitData *dst,
                                        const uint16_t *durations,
                                        size_t length,
                                        uint32_t carrier_hz = 38000,
                                        bool start_with_mark = true) {
  dst->reset();
  dst->set_carrier_frequency(carrier_hz);
  dst->reserve(length);

  bool mark = start_with_mark;
  for (size_t i = 0; i < length; i++) {
    uint32_t d = durations[i];
    if (d == 0)
      continue;
    if (mark)
      dst->mark(d);
    else
      dst->space(d);
    mark = !mark;
  }
}

inline void sendPulseDistanceWidthFromArray(
    RemoteTransmitData *dst,
    uint32_t carrier_khz,
    uint16_t header_mark,
    uint16_t header_space,
    uint16_t one_mark,
    uint16_t one_space,
    uint16_t zero_mark,
    uint16_t zero_space,
    const uint32_t *data,
    uint16_t nbits,
    BitOrder bit_order = LSB_FIRST) {

  // Worst-case durations count: 2 (header) + 2 * nbits (each bit: mark+space)
  // 104 bits → 210 entries max, which is small.
  constexpr size_t MAX_BITS = 300;  // adjust if you have longer frames
  if (nbits > MAX_BITS) {
    nbits = MAX_BITS;
  }

  uint16_t durations[2 + 2 * MAX_BITS];
  size_t pos = 0;

  // Header
  durations[pos++] = header_mark;
  durations[pos++] = header_space;

  for (uint16_t bit_index = 0; bit_index < nbits; bit_index++) {
    const uint8_t word_index = bit_index / 32;
    const uint8_t bit_in_word = bit_index % 32;

    bool bit;
    if (bit_order == LSB_FIRST) {
      bit = (data[word_index] >> bit_in_word) & 0x1;
    } else {  // MSB_FIRST
      bit = (data[word_index] >> (31 - bit_in_word)) & 0x1;
    }

    if (bit) {
      durations[pos++] = one_mark;
      durations[pos++] = one_space;
    } else {
      durations[pos++] = zero_mark;
      durations[pos++] = zero_space;
    }
  }

  // carrier_khz → Hz
  uint32_t carrier_hz = carrier_khz * 1000U;
  sendPulseWidthDistanceArray(dst, durations, pos, carrier_hz, true);
}

}  // namespace baymak_ac
}  // namespace esphome


