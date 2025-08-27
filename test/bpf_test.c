#include "../filter/bpf.h"

#define SAMPLE_RATE 50000.0f
#define DURATION    1.0f
#define NUM_SAMPLES (SAMPLE_RATE * DURATION)

static f32 generate_test_signal(int sample_index) {
  f32 t             = sample_index / SAMPLE_RATE;
  f32 signal_50hz   = SIN(2 * PI * 50.0f * t);
  f32 signal_1000hz = SIN(2 * PI * 1000.0f * t);
  return signal_50hz + signal_1000hz;
}

int main() {
  bpf_cfg_t cfg = {
      .fs       = SAMPLE_RATE,
      .f_center = 1000.0f,
      .bw       = 10.0f,
  };

  bpf_filter_t filter;
  bpf_init(&filter, cfg);

  printf("Sample,Input,Output,1000Hz\n");

  for (int i = 0; i < NUM_SAMPLES; i++) {
    f32 input = generate_test_signal(i);
    bpf_exec_in(&filter, input);

    printf("%d,%.6f,%.6f,%.6f\n", i, input, filter.out.y0, SIN(2 * PI * 1000.0f * i / SAMPLE_RATE));
  }

  return 0;
}
