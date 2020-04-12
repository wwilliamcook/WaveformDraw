#include <GLFW/glfw3.h>
#include <iostream>

#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 480
#define WAVETABLE_SIZE 2205

int main(void)
{
    GLFWwindow* window;

    /* Initialize the library */
    if (!glfwInit())
        return -1;

    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "WaveformDraw", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    /* Make the window's context current */
    glfwMakeContextCurrent(window);

    int mouse_state;
    int cur_sample_write_i, last_sample_write_i = -1;
    float cur_sample_write_val, last_sample_write_val;
    int i_diff;
    float sample_diff;
    double cursor_x, cursor_y;
    float wavetable_samples[WAVETABLE_SIZE];
    float x;
    float x_delta = 2. / (WAVETABLE_SIZE - 1);
    int i;
    for (i = 0; i < WAVETABLE_SIZE; i++)
        wavetable_samples[i] = 0;

    /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(window))
    {
        /* Render here */
        glClear(GL_COLOR_BUFFER_BIT);

        mouse_state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
        if (mouse_state == GLFW_PRESS) {
            glfwGetCursorPos(window, &cursor_x, &cursor_y);
            cur_sample_write_i = (int)roundf(cursor_x / (float)(WINDOW_WIDTH) * (float)(WAVETABLE_SIZE - 1));
            if (cur_sample_write_i < 0)
                cur_sample_write_i = 0;
            else if (cur_sample_write_i >= WAVETABLE_SIZE)
                cur_sample_write_i = WAVETABLE_SIZE - 1;
            cur_sample_write_val = -cursor_y / WINDOW_HEIGHT * 2 + 1;
            if (last_sample_write_i != -1) {
                i_diff = cur_sample_write_i - last_sample_write_i;
                sample_diff = cur_sample_write_val - last_sample_write_val;
                if (i_diff > 0) {
                    for (i = 0; i <= i_diff; i++) {
                        wavetable_samples[last_sample_write_i + i] = last_sample_write_val + ((float)(i) / (float)(i_diff)) * sample_diff;
                    }
                }
                else if (i_diff < 0) {
                    for (i = 0; i >= i_diff; i--) {
                        wavetable_samples[last_sample_write_i + i] = last_sample_write_val + ((float)(i) / (float)(i_diff)) * sample_diff;
                    }
                }
                else
                    wavetable_samples[cur_sample_write_i] = cur_sample_write_val;
            }

            last_sample_write_i = cur_sample_write_i;
            last_sample_write_val = cur_sample_write_val;
        }
        else
            last_sample_write_i = -1;

        glColor3f(1, 1, 1);
        glBegin(GL_LINES);
        x = -1.;
        for (i = 1; i < WAVETABLE_SIZE; i++) {
            glVertex2f(x, wavetable_samples[i - 1]);
            x += x_delta;
            glVertex2f(x, wavetable_samples[i]);
        }
        glEnd();

/*        glBegin(GL_TRIANGLES);
        glColor3f(.3, .8, .1);  // green
        glVertex2f( .0 - .1,  .5);
        glVertex2f( .5 - .1,  -.5);
        glVertex2f(-.5 - .1,  -.5);
        glColor3f(.3, .2, .7);  // purple
        glVertex2f(.0 + .1, .5);
        glVertex2f(.5 + .1, -.5);
        glVertex2f(-.5 + .1, -.5);
        glEnd();*/

        /* Swap front and back buffers */
        glfwSwapBuffers(window);

        /* Poll for and process events */
        glfwPollEvents();
    }

    glfwTerminate();

    return 0;
}