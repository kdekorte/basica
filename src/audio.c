#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "audio.h"

#define AUDIO_RATE 44100
#define AUDIO_AMPLITUDE 9000
#define BASICA_TICKS_PER_SECOND 18.2

static MIX_Mixer *mixer = NULL;
static int audio_attempted = 0;
static int audio_ready = 0;
static int mixer_initialized = 0;
static int shutdown_registered = 0;

/* audio_shutdown - Clean up SDL_mixer resources. Registered with atexit()
 * so audio is properly torn down on program exit */
static void audio_shutdown(void) {
    if (mixer) {
        MIX_DestroyMixer(mixer);
        mixer = NULL;
    }
    if (mixer_initialized) {
        MIX_Quit();
        mixer_initialized = 0;
    }
}

/* audio_init - Lazy-initialize the SDL_mixer audio subsystem and create
 * a mono 44.1kHz mixer device. Only runs once; subsequent calls return
 * the cached result */
static int audio_init(void) {
    if (audio_attempted) return audio_ready;
    audio_attempted = 1;

    if (!MIX_Init()) return 0;
    mixer_initialized = 1;
    if (!shutdown_registered) {
        atexit(audio_shutdown);
        shutdown_registered = 1;
    }

    SDL_AudioSpec spec;
    spec.format = SDL_AUDIO_S16;
    spec.channels = 1;
    spec.freq = AUDIO_RATE;

    mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec);
    if (!mixer) return 0;

    audio_ready = 1;
    return 1;
}

/* ticks_to_ms - Convert BASICA timer ticks (18.2 ticks/sec) to
 * milliseconds, rounding to the nearest integer */
static int ticks_to_ms(double ticks) {
    if (ticks <= 0) return 0;
    return (int)((ticks * 1000.0 / BASICA_TICKS_PER_SECOND) + 0.5);
}

/* delay_ms - Sleep for the given number of milliseconds using SDL_Delay */
static void delay_ms(int ms) {
    if (ms > 0) SDL_Delay(ms);
}

/* make_square_wave - Generate a square wave MIX_Audio buffer at the given
 * frequency and duration. Returns NULL on failure */
static MIX_Audio *make_square_wave(double frequency, int ms) {
    if (frequency <= 0 || ms <= 0) return NULL;

    int samples = (AUDIO_RATE * ms) / 1000;
    if (samples <= 0) samples = 1;

    Sint16 *data = (Sint16 *)SDL_malloc((size_t)samples * sizeof(Sint16));
    if (!data) return NULL;

    double phase = 0.0;
    double step = frequency / AUDIO_RATE;
    for (int i = 0; i < samples; i++) {
        data[i] = (phase < 0.5) ? AUDIO_AMPLITUDE : -AUDIO_AMPLITUDE;
        phase += step;
        phase -= floor(phase);
    }

    SDL_AudioSpec spec;
    spec.format = SDL_AUDIO_S16;
    spec.channels = 1;
    spec.freq = AUDIO_RATE;

    MIX_Audio *audio = MIX_LoadRawAudioNoCopy(mixer, data, (size_t)samples * sizeof(Sint16), &spec, true);
    if (!audio) SDL_free(data);
    return audio;
}

/* audio_sound - Play a square wave tone at the given frequency (Hz) for
 * the given duration (in BASICA timer ticks). Implements the SOUND
 * statement. Falls back to a silent delay if audio init fails */
void audio_sound(double frequency, double duration_ticks) {
    int ms = ticks_to_ms(duration_ticks);
    if (frequency <= 0 || ms <= 0) return;

    if (audio_init()) {
        MIX_Audio *audio = make_square_wave(frequency, ms);
        if (audio) {
            MIX_PlayAudio(mixer, audio);
            delay_ms(ms);
            MIX_DestroyAudio(audio);
            return;
        }
    }

    delay_ms(ms);
}

/* read_number - Parse a decimal integer from the MML string, advancing
 * the pointer past the digits. Returns 0 if no digits are found */
static int read_number(const char **p) {
    int value = 0;
    while (isdigit((unsigned char)**p)) {
        value = value * 10 + (**p - '0');
        (*p)++;
    }
    return value;
}

/* note_frequency - Convert a semitone (0-11) and octave to a frequency
 * in Hz using the standard A440 tuning formula */
static double note_frequency(int note, int octave) {
    int midi = (octave + 1) * 12 + note;
    return 440.0 * pow(2.0, (midi - 69) / 12.0);
}

/* note_ms - Calculate the duration of a note in milliseconds given the
 * tempo (BPM), note length (e.g. 4 = quarter note), and number of dots
 * (each dot adds half the remaining duration) */
static int note_ms(int tempo, int length, int dots) {
    if (tempo <= 0) tempo = 120;
    if (length <= 0) length = 4;

    double ms = (60000.0 / tempo) * (4.0 / length);
    double add = ms / 2.0;
    for (int i = 0; i < dots; i++) {
        ms += add;
        add /= 2.0;
    }
    return (int)(ms + 0.5);
}

/* audio_play - Parse and play a Music Macro Language (MML) string.
 * Implements the PLAY statement. Supports note letters A-G with sharps/
 * flats, octave (O), tempo (T), length (L), pause (P/R), octave
 * shift (< >), and dotted notes */
void audio_play(const char *mml) {
    int tempo = 120;
    int octave = 4;
    int default_length = 4;
    const char *p = mml ? mml : "";

    while (*p) {
        char cmd = (char)toupper((unsigned char)*p++);

        if (isspace((unsigned char)cmd)) continue;

        if (cmd == 'T') {
            int value = read_number(&p);
            if (value > 0) tempo = value;
        } else if (cmd == 'O') {
            int value = read_number(&p);
            if (value >= 0 && value <= 8) octave = value;
        } else if (cmd == 'L') {
            int value = read_number(&p);
            if (value > 0) default_length = value;
        } else if (cmd == '>') {
            if (octave < 8) octave++;
        } else if (cmd == '<') {
            if (octave > 0) octave--;
        } else if (cmd == 'P' || cmd == 'R') {
            int length = read_number(&p);
            if (length <= 0) length = default_length;
            int dots = 0;
            while (*p == '.') {
                dots++;
                p++;
            }
            delay_ms(note_ms(tempo, length, dots));
        } else if (cmd >= 'A' && cmd <= 'G') {
            static const int notes[] = {
                9, 11, 0, 2, 4, 5, 7
            };
            int note = notes[cmd - 'A'];
            if (*p == '#' || *p == '+') {
                note++;
                p++;
            } else if (*p == '-') {
                note--;
                p++;
            }
            if (note < 0) note += 12;
            if (note > 11) note -= 12;

            int length = read_number(&p);
            if (length <= 0) length = default_length;
            int dots = 0;
            while (*p == '.') {
                dots++;
                p++;
            }

            int ms = note_ms(tempo, length, dots);
            audio_sound(note_frequency(note, octave), ms * BASICA_TICKS_PER_SECOND / 1000.0);
        }
    }
}
