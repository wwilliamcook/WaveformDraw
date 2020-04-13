#include <GLFW/glfw3.h>
#include <portaudio.h>
#include <iostream>
#include <mutex>

#define WINDOW_WIDTH  (640)
#define WINDOW_HEIGHT (480)

#define SAMPLE_RATE   (44100)
#define FRAMES_PER_BUFFER  (64)
#define TABLE_SIZE   (2205)


/*
 * class: Wavetable
 *
 * Stores an array of samples.
 *
 * Samples are stored in two arrays, write_samples and read_samples.
 * - write_samples is prioritized for rendering the graphical display.
 * - read_samples is prioritized for being read very frequently by the
 *   portaudio callback function.
 * read_samples is kept in sync with write_samples through intermittent
 *   read/writes which are timed to avoid race conditions with the callback.
 */
class Wavetable {
private:
    char reading;  // bool: callback is reading from read_samples
    char writing;  // bool: write_samples is being copied to read_samples
    int i, last_sample_write_i, i_diff;
    float last_sample_write_val, val_diff;
    int copy_i;  // index to begin copying write_samples to read_samples at

public:
    float write_samples[TABLE_SIZE];  // Samples used to render display.
    float read_samples[TABLE_SIZE];  // Samples used for playing audio

public:
    Wavetable(void) :
        reading(0),
        writing(0),
        i(-1),
        last_sample_write_i(-1),
        i_diff(-1),
        last_sample_write_val(0),
        val_diff(0),
        copy_i(0)
    {
        for (i = 0; i < TABLE_SIZE; i++) {
            write_samples[i] = 0;
            read_samples[i] = 0;
        }
    }

    /*
     * function: mouseDown
     *
     * Updates samples with mouse cursor data. Call any frame that the left
     *   button is pressed.
     *
     * Args:
     *   cur_sample_write_i: sample index that the cursor is currently positioned at
     *   cur_sample_write_val: value to write to selected sample
     */
    void mouseDown(int cur_sample_write_i, float cur_sample_write_val) {
        if (last_sample_write_i != -1) {
            i_diff = cur_sample_write_i - last_sample_write_i;
            val_diff = cur_sample_write_val - last_sample_write_val;

            if (i_diff > 0) {
                for (i = 0; i <= i_diff; i++) {
                    write_samples[last_sample_write_i + i] = last_sample_write_val + ((float)(i) / (float)(i_diff)) * val_diff;
                }
            }
            else if (i_diff < 0) {
                for (i = 0; i >= i_diff; i--) {
                    write_samples[last_sample_write_i + i] = last_sample_write_val + ((float)(i) / (float)(i_diff)) * val_diff;
                }
            }
            else
                write_samples[cur_sample_write_i] = cur_sample_write_val;
        }
        else {
            write_samples[cur_sample_write_i] = cur_sample_write_val;
        }

        last_sample_write_i = cur_sample_write_i;
        last_sample_write_val = cur_sample_write_val;

        copySamples();
    }

    /*
     * function: mouseUp
     *
     * Call any frame that the left button is not pressed.
     */
    void mouseUp(void) {
        last_sample_write_i = -1;

        copySamples();
    }

    /*
     * function: copySamples
     *
     * Copies as many samples as possible from write_samples to read_samples.
     *   Returns when read callback begins reading or all samples have been
     *   copied.
     */
    void copySamples(void) {
        writing = 1;

        for (i = copy_i; i < TABLE_SIZE; i++) {
            if (reading) {
                writing = 0;
                copy_i = i;
                return;
            }
            read_samples[i] = write_samples[i];
        }
        for (i = 0; i < copy_i; i++) {
            if (reading) {
                writing = 0;
                copy_i = i;
                return;
            }
            read_samples[i] = write_samples[i];
        }

        writing = 0;
        copy_i = 0;
    }

    /*
     * function: beginReading
     *
     * Allows reads from read_samples by disabling sync from write_samples.
     * Blocks until ready to read (very short, see Wavetable::copySamples()).
     */
    void beginReading(void) {
        reading = 1;
        while (writing);  // Wait until read_samples is ready
    }

    /*
     * function: doneReading
     *
     * Re-enables sync from write_samples to read_samples.
     */
    void doneReading(void) {
        reading = 0;
    }

    bool getReading() {
        return (bool)reading;
    }

    bool getWriting() {
        return (bool)writing;
    }
};

/*
 * struct: paData
 */
typedef struct {
    Wavetable* wavetable;
    float frequency;
    float step_size;  // frequency / SAMPLE_RATE * TABLE_SIZE
    const float table_size_float = (float)TABLE_SIZE;
    const float table_size_over_sample_rate = (float)TABLE_SIZE / (float)SAMPLE_RATE;
    float current_sample;  // float to delay quantization

    /*
     * function: setFrequency
     *
     * Sets the frequency and updates step_size.
     */
    void setFrequency(float new_frequency) {
        frequency = new_frequency;
        while (wavetable->getReading());  // Wait to be able to modify step_size
        step_size = frequency * table_size_over_sample_rate;
    }
} paData;

/* This routine will be called by the PortAudio engine when audio is needed.
   It may called at interrupt level on some machines so don't do anything
   that could mess up the system like calling malloc() or free().
*/
static int paCallback(const void* inputBuffer, void* outputBuffer,
    unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo* timeInfo,
    PaStreamCallbackFlags statusFlags,
    void* userData) {
/* Cast data passed through stream to our structure. */
    paData* data = (paData*)userData;
    float* out = (float*)outputBuffer;
    unsigned int i;
    (void)inputBuffer; /* Prevent unused variable warning. */

    data->wavetable->beginReading();

    for (i = 0; i < framesPerBuffer; i++) {
        *out++ = data->wavetable->read_samples[(int)(data->current_sample) % TABLE_SIZE];
        data->current_sample += data->step_size;
        // Manual modulo back into range
        while (data->current_sample >= data->table_size_float)
            data->current_sample -= data->table_size_float;
    }

    data->wavetable->doneReading();

    return paContinue;
}

/*
 * function: portaudioErrorAndQuit
 *
 * Terminates portaudio and prints error messages.
 */
void portaudioErrorAndQuit(PaError &err) {
    Pa_Terminate();
    std::cerr << "An error occured while using the portaudio stream" << std::endl;
    std::cerr << "Error number: " << err << std::endl;
    std::cerr << "Error message: " << Pa_GetErrorText(err) << std::endl;
    exit(EXIT_FAILURE);
}


int main(void)
{
    GLFWwindow* window;
    PaStreamParameters outputParameters;
    PaStream* stream;
    PaError err;
    paData data;
    Wavetable wavetable;
    int mouse_state;
    double cursor_x, cursor_y;
    int cur_sample_write_i;
    float cur_sample_write_val;
    float x;
    const float x_delta = 2. / (TABLE_SIZE - 1);  // Distance between waveform
                                                  // nodes in window.
    int i;

    data.wavetable = &wavetable;
    data.setFrequency(440);

    // Initialize portaudio
    err = Pa_Initialize();
    if (err != paNoError) {
        std::cerr << "Error: Unable to initialize portaudio. Exiting." << std::endl;
        portaudioErrorAndQuit(err);
    }

    outputParameters.device = Pa_GetDefaultOutputDevice();
    if (outputParameters.device == paNoDevice) {
        std::cerr << "Error: No default output device. Exiting." << std::endl;
        portaudioErrorAndQuit(err);
    }

    outputParameters.channelCount = 1;
    outputParameters.sampleFormat = paFloat32;  // 32-bit floating point output
    outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;

    err = Pa_OpenStream(
        &stream,
        NULL,  // no input
        &outputParameters,
        SAMPLE_RATE,
        FRAMES_PER_BUFFER,
        paClipOff,      // we won't output out of range samples so don't bother clipping them
        paCallback,
        &data);
    if (err != paNoError) {
        glfwTerminate();
        portaudioErrorAndQuit(err);
    }

    // Initialize GLFW
    if (!glfwInit()) {
        exit(EXIT_FAILURE);
    }

    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "WaveformDraw", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        Pa_Terminate();
        exit(EXIT_FAILURE);
    }

    /* Make the window's context current */
    glfwMakeContextCurrent(window);

    err = Pa_StartStream(stream);
    if (err != paNoError) {
        glfwTerminate();
        portaudioErrorAndQuit(err);
    }

    /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(window))
    {
        /* Render here */
        glClear(GL_COLOR_BUFFER_BIT);

        mouse_state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
        if (mouse_state == GLFW_PRESS) {
            glfwGetCursorPos(window, &cursor_x, &cursor_y);

            cur_sample_write_i = (int)roundf(cursor_x / (float)(WINDOW_WIDTH) * (float)(TABLE_SIZE - 1));
            cur_sample_write_val = -cursor_y / (float)(WINDOW_HEIGHT) * 2. + 1.;

            if (cur_sample_write_i < 0)
                cur_sample_write_i = 0;
            else if (cur_sample_write_i >= TABLE_SIZE)
                cur_sample_write_i = TABLE_SIZE - 1;

            wavetable.mouseDown(cur_sample_write_i, cur_sample_write_val);
        }
        else {
            wavetable.mouseUp();
        }

        glColor3f(1, 1, 1);
        glBegin(GL_LINES);
        x = -1.;
        for (i = 1; i < TABLE_SIZE; i++) {
            glVertex2f(x, wavetable.write_samples[i - 1]);
            x += x_delta;
            glVertex2f(x, wavetable.write_samples[i]);
        }
        glEnd();

        /* Swap front and back buffers */
        glfwSwapBuffers(window);

        /* Poll for and process events */
        glfwPollEvents();
    }

    Pa_Terminate();
    glfwTerminate();

    return 0;
}