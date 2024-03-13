#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define LOG(...) fprintf(stderr, __VA_ARGS__)

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

// Rendering

GLuint load_shader(const char *filename, GLenum shader_type) {
    // read file
    FILE *file = fopen(filename, "r");
    if (!file) {
        LOG("Failed to open file %s\n", filename);
        return false;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *buffer = malloc(length + 1);
    fread(buffer, 1, length, file);
    buffer[length] = '\0';

    fclose(file);

    // load the shader
    GLuint shader = glCreateShader(shader_type);
    if (!shader) return 0; // error

    glShaderSource(shader, 1, (const GLchar **)&buffer, NULL);
    glCompileShader(shader);

    free(buffer);

    // check status of compilation
    int success;
    char info_log[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 512, NULL, info_log);
        LOG("Shader failed to compile: %s:%s\n", filename, info_log);
        return 0;
    }

    return shader;
}

GLuint vbo = 0;
GLuint vao = 0;
GLuint shader_program;

void init_renderer() {
    float points[] = {
       0.0f,  0.5f,  0.0f,
       0.5f, -0.5f,  0.0f,
      -0.5f, -0.5f,  0.0f
    };

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, 9 * sizeof(float), points, GL_STATIC_DRAW);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);

    GLuint vertex_shader = load_shader("./shaders/vertex.vert", GL_VERTEX_SHADER);
    if (!vertex_shader) exit(-1);

    GLuint fragment_shader = load_shader("./shaders/fragment.frag", GL_FRAGMENT_SHADER);
    if (!fragment_shader) exit(-1);

    // link the shader program
    shader_program = glCreateProgram();
    glAttachShader(shader_program, vertex_shader);
    glAttachShader(shader_program, fragment_shader);
    glLinkProgram(shader_program);

    int success;
    char info_log[512];
    glGetProgramiv(shader_program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader_program, 512, NULL, info_log);
        LOG("Failed to create shader program: %s\n", info_log);
        exit(-1);
    }

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    // TODO: delete shader program
}

void render() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(shader_program);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

// WINDOW

void handle_input(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        glClearColor(0.8f, 0.2f, 0.6f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    } else if (glfwGetKey(window, GLFW_KEY_W) == GLFW_RELEASE) {
        glClearColor(0.1f, 0.2f, 0.6f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }
}

void window_size_callback(GLFWwindow *window, int width, int height) {
    glViewport(0, 0, width, height);
}

int main(int argc, char *argv[]) {
    // create a window
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "CT Scan Visualizer", NULL, NULL);

    glfwMakeContextCurrent(window);
    glfwSetWindowSizeCallback(window, window_size_callback);

    // initialize glad
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        LOG("Failed to initialize glad\n");
        return -1;
    }

    init_renderer();

    while (!glfwWindowShouldClose(window)) {
        handle_input(window);
        render();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
}
