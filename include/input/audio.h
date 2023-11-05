#include <fftw3.h>
#include <pthread.h>
#include <stdint.h>

typedef struct {
  pthread_mutex_t lock;
  float *samples_buffer;
  uint16_t samples_buffer_length;
  uint16_t readable;
  fftwf_complex *fft_buffer; // Does not need length variable because it's
                             // double the samples_buffer
  fftwf_plan plan;
  uint32_t sample_rate;
  uint32_t terminate;
} audio_data;
