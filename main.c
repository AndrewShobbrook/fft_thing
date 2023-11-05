#include <fftw3.h>
#include <graphics/bgi_back.h>
#include <input/audio.h>
#include <input/portaudio_back.h>
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef AUD_BUFFER_LENGTH
#define AUD_BUFFER_LENGTH 32
#endif

#define NUM_BARS 16

// Struct holding all of the data needed to work with any audio interface.
// Definition in input/audio.h
static audio_data audio = {0};

int main(void) {
  // Allocating buffers to hold the sample data (One sample is like a frame)
  audio.samples_buffer = fftwf_alloc_real(AUD_BUFFER_LENGTH);
  audio.samples_buffer_length = AUD_BUFFER_LENGTH * sizeof(float);
  // Allocating buffer to hold the dft which has n/2 + 1 elements because
  // everything above the nyquist frequency is useless.
  audio.fft_buffer = fftwf_malloc(AUD_BUFFER_LENGTH / 2 + 1);
  // Create an FFTW plan. This is basically what defines which FFT algorithm
  // it'll use. It's done at runtime by the program, so I don't know which one
  // it's using
  audio.plan = fftwf_plan_dft_r2c_1d(AUD_BUFFER_LENGTH, audio.samples_buffer,
                                     audio.fft_buffer, FFTW_ESTIMATE);
  // I set the sample rate in this to 0 so that the audio processing thread can
  // set it as synchronisation
  audio.sample_rate = 0;

  // Here I specify the pthread before creation (Creation is done with pointer)
  pthread_t audio_thread;
  // Initialising the mutex (Mutual exclusion lock) for the audio data structure
  pthread_mutex_init(&audio.lock, NULL);
  // Create the pthread and have it run our audio processing
  // (input/portaudio_back.c)
  pthread_create(&audio_thread, NULL, &portaudio, &audio);
  // Debug check
  printf("pthread made\n");

  // Every second, wake up and check if the sample rate has been set. If it's
  // been set, continue to the audio processing loop
  while (1) {
    sleep(1);

    // Lock the mutex while we read from the audio struct because otherwise we
    // can corrupt the cace (I love multithreading \s)
    pthread_mutex_lock(&audio.lock);
    if (audio.sample_rate != 0) {
      printf("Unlocked\n");
      pthread_mutex_unlock(&audio.lock);
      // Break out of infinite loop
      break;
    }
    pthread_mutex_unlock(&audio.lock);
  }
  printf("We've made it here\n");

  while (1) {
    // Lock and check if  the audio is readable. If it is readable, execute the
    // fourier transform and print out each frequency
    pthread_mutex_lock(&audio.lock);
    if (audio.readable) {
      fftwf_execute_dft_r2c(audio.plan, audio.samples_buffer, audio.fft_buffer);
      for (int i = 0; i < NUM_BARS; i++) {
        printf("%f, ", sqrtf(audio.fft_buffer[i][0] * audio.fft_buffer[i][0] +
                             audio.fft_buffer[i][1] * audio.fft_buffer[i][1]));
      }
      printf("\n");
      audio.readable = 0;
    }
    // Always unlock the mutex because otherwise you will encounter great pain
    pthread_mutex_unlock(&audio.lock);
  }
  return 0;
}
