#include <input/audio.h>
#include <input/portaudio_back.h>
#include <math.h>
#include <portaudio.h>
#include <pthread.h>
#include <stdio.h>

// Sample rate is a constant define
#define SAMPLE_RATE 41000
// Setting frames to 0 tells portaudio that it can select the optimal number of
// frames per callback
#define FRAMES 0

// This struct holds the information needed to properly stream to the fftw
// buffer
typedef struct {
  // Frame index tells us what frame we were at at the start of the callback
  int frame_index;
  // This tells us how many frames we fit into the buffer (Sent through the
  // audio_info struct)
  int max_frame_index;
} PaTestData;

PaTestData callback_data = {0};

// This is our pointer to the audio data passed from main. It's in a global
// because we can guarantee that it's not accessed except when safe to do so,
// therefore for simplicity I'm keeping it global for easy access between our
// functions in this file
audio_data *audio_info = {0};

int record_callback(const void *input_buffer, void *output_buffer,
                    unsigned long frames_per_buffer,
                    const PaStreamCallbackTimeInfo *time,
                    PaStreamCallbackFlags status_flags, void *user_data) {
  PaTestData *data = (PaTestData *)user_data;
  float *read_buffer = (float *)input_buffer;
  float *playback_buffer = (float *)output_buffer;
  float *out_buffer = audio_info->samples_buffer;
  unsigned long frames_to_calc;
  // We calculate how many frames are left, and then compare that to the number
  // of frames in the buffer, we then pick how many frames to read to the buffer
  // based on that
  unsigned long frames_left = data->max_frame_index - data->frame_index;

  if (frames_left > frames_per_buffer)
    frames_to_calc = frames_per_buffer;
  else
    frames_to_calc = frames_left;

  // Remember to lock your mutexes kids, I ended up with an infinite loop while
  // writing this program accidentally because I'd written mutex lock instead of
  // unlock in main.c

  pthread_mutex_lock(&audio_info->lock);
  // Check that the microphone buffer has our information, then push it's info
  // to the fft buffer and the playback buffer
  if (read_buffer != NULL) {
    for (int i = 0; i < frames_to_calc; i++) {
      // We do some fun math here called a hann window. Essentially, because of
      // the way the fft works, the function operates on the idea that it's
      // input repeats infinitely, so if there's a mismatch between the start
      // and end points, it can leak frequencies across the fourier transform.
      // Therefore, we window the function so it tapers off near the start and
      // end points, you can see a graph of what it looks like on it's wikipedia
      // page.
      // Thank this legend for letting me be confident with my
      // implementation because I was certain I'd manage to mess up the math
      // somewhere
      // https://stackoverflow.com/questions/3555318/implement-hann-window
      out_buffer[i + data->frame_index] =
          *read_buffer * (0.5 * (1 - cosf(2 * M_PI * (i + data->frame_index) /
                                              data->max_frame_index -
                                          1)));
      *playback_buffer++ = *read_buffer;
      *playback_buffer++ = *read_buffer++;
    }
    data->frame_index += frames_to_calc;

    // If there were less frames than in the buffer, just playback that info to
    // the microphone (Don't bother writing to the buffer) and set the readable
    // flag to 1 (Tells the main function to process the buffer)
    if (frames_left < frames_per_buffer) {
      for (int i = 0; i < frames_per_buffer - frames_left; i++) {
        *playback_buffer++ = *read_buffer;
        *playback_buffer++ = *read_buffer++;
      }
      audio_info->readable = 1;
      data->frame_index = 0;
    }
    // If the microphone input doesn't exist write silence to the buffers,
    // that's 0 in the PaFloat32 format which I'm using
  } else {
    for (int i = 0; i < frames_to_calc; i++) {
      out_buffer[i + data->frame_index] = 0;
      *playback_buffer++ = 0;
      *playback_buffer++ = 0;
    }
    data->frame_index += frames_to_calc;
    if (frames_left < frames_per_buffer) {
      for (int i = 0; i < frames_per_buffer - frames_left; i++) {
        *playback_buffer++ = 0;
        *playback_buffer++ = 0;
      }
      audio_info->readable = 1;
      data->frame_index = 0;
    }
  }
  // Remember to unlock your mutexes lol
  pthread_mutex_unlock(&audio_info->lock);
  return paContinue;
}

void *portaudio(void *audio) {
  // Set our global pointer
  audio_info = (audio_data *)audio;
  // Set our callback data so it knows how to fill the buffer
  callback_data.frame_index = 0;
  callback_data.max_frame_index =
      audio_info->samples_buffer_length / sizeof(float);

  // I declare the parameter variables here (For input and output)
  PaStreamParameters inputParameters, outputParameters;
  // Initialise the library, then if it returns an error it can go kill the
  // thread, though I haven't implemented anything for after that so the main
  // function continues looping into the void
  PaError err = Pa_Initialize();
  if (err != paNoError)
    goto error;
  // Printing stuff
  printf("PortAudio Version Number: %s", Pa_GetVersionInfo()->versionText);

  // Get the default device for input, and if it returns nothing kill the thread
  inputParameters.device = Pa_GetDefaultInputDevice();
  if (inputParameters.device == paNoDevice)
    goto error;
  // Only one input channel
  inputParameters.channelCount = 1;
  // Set the format for each audio "frame"
  inputParameters.sampleFormat = paFloat32;
  // I just went for default lowest latency because that's what the
  // documentation says to use most of the time
  inputParameters.suggestedLatency =
      Pa_GetDeviceInfo(inputParameters.device)->defaultLowInputLatency;
  // We're not sending any host api specific info
  inputParameters.hostApiSpecificStreamInfo = NULL;

  // Do the same stuff for the output device, but we have two channels, which
  // means output[0] = left then output[1] = right and so on
  outputParameters.device = Pa_GetDefaultOutputDevice();
  if (outputParameters.device == paNoDevice)
    goto error;
  outputParameters.channelCount =
      2; // Interleaved, left then right, for every frame
  outputParameters.sampleFormat = paFloat32;
  outputParameters.suggestedLatency =
      Pa_GetDeviceInfo(outputParameters.device)->defaultLowInputLatency;
  outputParameters.hostApiSpecificStreamInfo = NULL;

  PaStream *stream;
  // Open the stream with out input output parameters and pass our callback data
  // as well (Though it was already a global) we also pass the callback function
  err = Pa_OpenStream(&stream, &inputParameters, &outputParameters, SAMPLE_RATE,
                      FRAMES, 0, &record_callback, &callback_data);
  if (err != paNoError)
    goto error;

  // Synchronise the threads by setting this before starting the stream
  audio_info->sample_rate = SAMPLE_RATE;
  err = Pa_StartStream(stream);
  if (err != paNoError)
    goto error;

  // Infinite loop because theoretically the thing should run forever and also
  // because I can't be bothered writing terminate code
  while (1) {
  }
  // Terminate everything, except the program will never reach this, and the
  // operating system cleans up for me instead (Very messy and terrible
  // practice, but I can't be bothered making it better)
  err = Pa_StopStream(stream);
  if (err != paNoError)
    goto error;
  Pa_Terminate();
  return 0;

error:
  Pa_Terminate();
  fprintf(stderr,
          "An error occured during PortAudio stream stuff\nError Number: "
          "%d\nError Meessage: %s\n",
          err, Pa_GetErrorText(err));
  return NULL;
}
