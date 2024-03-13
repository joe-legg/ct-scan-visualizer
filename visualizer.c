#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdbool.h>

#include <tiffio.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define LOG(...) fprintf(stderr, __VA_ARGS__)

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

#define VERTEX_SHADER_PATH "./shaders/vertex.vert"
#define FRAGMENT_SHADER_PATH "./shaders/fragment.frag"

// Load images

void load_tiff(const char *filename) {
    LOG("Reading TIFF file %s\n", filename);
    TIFF *img = TIFFOpen(filename, "r");
    if (!img) {
        LOG("Failed to open TIFF file %s\n", filename);
        exit(-1);
    }

    TIFFClose(img);
}

void load_images(const char *path) {
    struct dirent **file_list;
    int file_count = scandir(path, &file_list, NULL, alphasort);
    if (file_count < 0) {
        LOG("Failed to open directory %s\n", path);
        exit(-1);
    }

    for (int i = 0; i < file_count; i++) {
        struct dirent *file = file_list[i];

        if (file->d_type == DT_REG) {
            char *dot = strrchr(file->d_name, '.');
            if (dot && strcmp(dot, ".tif") == 0) {
                char file_path[strlen(path) + strlen(file->d_name) + 2];
                snprintf(file_path, sizeof(file_path), "%s/%s", path, file->d_name);
                load_tiff(file_path);
            }
        }
        free(file_list[i]);
    }

    free(file_list);
}

// Rendering

typedef struct {
    GLFWwindow *window;
    GLuint shader_program;
} RenderContext;

GLuint load_shader(const char *filename, GLenum shader_type) {
    // read file
    FILE *file = fopen(filename, "r");
    if (!file) {
        LOG("Failed to open file %s\n", filename);
        exit(-1);
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
    if (!shader) {
        LOG("Failed to load shader");
        exit(-1);
    }

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
        exit(-1);
    }

    return shader;
}

void window_size_callback(GLFWwindow *window, int width, int height) {
    glViewport(0, 0, width, height);
}

RenderContext *init_renderer() {
    // create a window
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4); // Anti-aliasing

    GLFWwindow *window = glfwCreateWindow(
        WINDOW_WIDTH, WINDOW_HEIGHT, "CT Scan Visualizer", NULL, NULL
    );

    glfwMakeContextCurrent(window);
    glfwSetWindowSizeCallback(window, window_size_callback);

    // initialize glad
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        LOG("Failed to initialize glad\n");
        exit(-1);
    }

    // enable depth testing
    glEnable(GL_DEPTH_TEST);

    // load shaders
    GLuint vertex_shader = load_shader(VERTEX_SHADER_PATH, GL_VERTEX_SHADER);
    GLuint fragment_shader = load_shader(FRAGMENT_SHADER_PATH, GL_FRAGMENT_SHADER);

    // create the shader program
    GLuint shader_program = glCreateProgram();
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

    // TODO: delete shader program
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    RenderContext *render_ctx = malloc(sizeof(RenderContext));
    render_ctx->window = window;
    render_ctx->shader_program = shader_program;

    return render_ctx;
}

void terminate_renderer(RenderContext *render_ctx) {
    glfwDestroyWindow(render_ctx->window);
    glfwTerminate();
    free(render_ctx);
}

void render(RenderContext *render_ctx) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(render_ctx->shader_program);
    glfwSwapBuffers(render_ctx->window);
}

// Input

void handle_input(GLFWwindow *window) {
    glfwPollEvents();
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, 1);
    }
}

int main(int argc, char *argv[]) {
    load_images("./microtus_oregoni");

    RenderContext *render_ctx = init_renderer();
    while (!glfwWindowShouldClose(render_ctx->window)) {
        handle_input(render_ctx->window);
        render(render_ctx);
    }
    terminate_renderer(render_ctx);
}
