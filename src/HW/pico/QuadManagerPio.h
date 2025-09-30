#pragma once
#include "hardware/pio.h"
#include "pico/stdlib.h"

// Include the PIO program generated header from your .pio file:
#include "hw/pio/quad_encoder.pio.h"  // from pico-examples (pio/quadrature_encoder)

namespace zlkm {
namespace hw {

// Zero-allocation, fixed-size manager
template <int N>
class QuadManagerPIO {
 public:
  // pinsA[i] = GPIO number for encoder i's A channel.
  // B is assumed to be pinsA[i] + 1 (place encoders on adjacent pins).
  explicit QuadManagerPIO(PIO pio, std::array<uint8_t, N> const& pinsA,
                          float sample_clkdiv = 50.0f)
      : pio_(pio) {
    // Load the program once per PIO
    prog_offset_ = pio_add_program(pio_, &quad_encoder_program);

    for (int i = 0; i < N; ++i) {
      const uint pinA = pinsA[i];
      const uint pinB = pinA + 1;

      // Basic pin setup (inputs with pull-ups)
      gpio_pull_up(pinA);
      gpio_pull_up(pinB);
      pio_gpio_init(pio_, pinA);
      pio_gpio_init(pio_, pinB);

      // Claim one SM per encoder
      int sm = pio_claim_unused_sm(pio_, true);

      gpio_pull_up(pinA);
      gpio_pull_up(pinA + 1);
      pio_gpio_init(pio_, pinA);
      pio_gpio_init(pio_, pinA + 1);

      pio_sm_config c = quad_encoder_program_get_default_config(prog_offset_);
      sm_config_set_in_pins(&c, pinA);
      sm_config_set_clkdiv(&c, sample_clkdiv);
      sm_config_set_in_shift(&c, true, true, 2);

      pio_sm_init(pio_, sm, prog_offset_, &c);
      pio_sm_set_enabled(pio_, sm, true);

      enc_[i].sm = sm;
      enc_[i].pinA = pinA;
      enc_[i].count = 0;
    }
  }

  // Drain FIFOs and apply deltas. Call this in your main loop or a timer IRQ.
  void update() {
    for (int i = 0; i < N; ++i) {
      auto& e = enc_[i];
      while (!pio_sm_is_rx_fifo_empty(pio_, e.sm)) {
        uint32_t w = pio_sm_get(pio_, e.sm);

        // Sample sits in bits 31..30 when shift_right = true
        uint8_t curr = (uint8_t)((w >> 30) & 0x3);

        if (e.prev <= 3) {
          uint8_t idx = (uint8_t)((e.prev << 2) | curr);
          int8_t d = kDecodeLUT[idx];
          if (d) e.count += e.invert ? -d : d;
        }
        e.prev = curr;
      }
    }
  }

  // Read/write position counters
  int32_t read(int idx) const { return enc_[idx].count; }
  void write(int idx, int32_t pos) { enc_[idx].count = pos; }

  // Simple helper to reverse an encoder's direction (software swap)
  void invert(int idx, bool inv) { enc_[idx].invert = inv; }

 private:
  struct Enc {
    int sm = -1;
    uint8_t prev = 0xFF;
    uint pinA = 0;
    int32_t count = 0;
    bool invert = false;
  };

  static constexpr int8_t kDecodeLUT[16] = {
      /*00->00*/ 0,  /*00->01*/ +1, /*00->10*/ -1, /*00->11*/ 0,
      /*01->00*/ -1, /*01->01*/ 0,  /*01->10*/ 0,  /*01->11*/ +1,
      /*10->00*/ +1, /*10->01*/ 0,  /*10->10*/ 0,  /*10->11*/ -1,
      /*11->00*/ 0,  /*11->01*/ -1, /*11->10*/ +1, /*11->11*/ 0};

  PIO pio_;
  int prog_offset_ = -1;
  std::array<Enc, N> enc_;
};

}  // namespace hw

}  // namespace zlkm