#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <stdbool.h>

#include <tiffio.h>
#include <glad/glad.h>
#include <cglm/cglm.h>
#include <GLFW/glfw3.h>

#define LOG(...) fprintf(stderr, __VA_ARGS__)

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

#define VERTEX_SHADER_PATH "./shaders/vertex.vert"
#define FRAGMENT_SHADER_PATH "./shaders/fragment.frag"

// types

typedef struct {
    GLFWwindow *window;
    GLuint shader_program;
} RenderContext;

typedef struct {
    size_t cap;
    size_t length;
    vec3 *data;
} PointCloud;

// load images

PointCloud *point_cloud_init() {
    PointCloud *point_cloud = malloc(sizeof(PointCloud));
    point_cloud->length = 0;
    point_cloud->cap = 4;
    point_cloud->data = malloc(sizeof(vec3) * point_cloud->cap);
    return point_cloud;
}

void point_cloud_free(PointCloud *point_cloud) {
    free(point_cloud->data);
    free(point_cloud);
}

void point_cloud_append(PointCloud *point_cloud, vec3 point) {
    if (point_cloud->length == point_cloud->cap) {
        point_cloud->cap *= 2;
        point_cloud->data =
            realloc(point_cloud->data, sizeof(vec3) * point_cloud->cap);
    }

    memcpy(point_cloud->data[point_cloud->length], point, sizeof(vec3));
    ++point_cloud->length;
}

void point_cloud_csv_dump(PointCloud *point_cloud, FILE *fp) {
    LOG("x,y,z\n");
    for (int i = 0; i < point_cloud->length; i++)
        fprintf(fp, "%f, %f, %f\n", point_cloud->data[i][0],
                point_cloud->data[i][1], point_cloud->data[i][2]);
}

void tiff_to_points(const char *filename, int z, PointCloud *point_cloud) {
    TIFF *tif = TIFFOpen(filename, "r");
    if (!tif) {
        LOG("Failed to open TIFF file: %s\n", filename);
        exit(-1);
    }

    uint32_t w, h;
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);

    size_t pixels_count = w * h;
    uint32_t *raster = _TIFFmalloc(pixels_count * sizeof(uint32_t));
    if (!raster) {
        LOG("Failed to allocate memory to read TIFF file: %s\n", filename);
        exit(-1);
    }

    // read the image
    if (!TIFFReadRGBAImage(tif, w, h, raster, 0)) {
        LOG("Failed to read TIFF RGBA data in file: %s\n", filename);
    }
    TIFFClose(tif);

    // convert the raster to points
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int raster_index = y * w + x;
            int r = TIFFGetR(raster[raster_index]);
            int g = TIFFGetG(raster[raster_index]);
            int b = TIFFGetB(raster[raster_index]);

            // only non-black pixels should be converted into points
            if (r || g || b) {
                vec3 point = {x, y, z};
                point_cloud_append(point_cloud, point);
            }
        }
    }
    _TIFFfree(raster);
}

void load_images(const char *path) {
    struct dirent **file_list;
    int file_count = scandir(path, &file_list, NULL, alphasort);
    if (file_count < 0) {
        LOG("Failed to open directory %s\n", path);
        exit(-1);
    }

    PointCloud *point_cloud = point_cloud_init();
    float z = 0;
    for (int i = 0; i < file_count; i++) {
        LOG("loading files: %d/%d\r", i, file_count);

        struct dirent *file = file_list[i];
        if (file->d_type == DT_REG) {
            char *dot = strrchr(file->d_name, '.');
            if (dot && strcmp(dot, ".tif") == 0) {
                char file_path[strlen(path) + strlen(file->d_name) + 2];
                snprintf(file_path, sizeof(file_path), "%s/%s", path,
                         file->d_name);

                tiff_to_points(file_path, z, point_cloud);
                ++z;
            }
        }
        free(file_list[i]);
    }
    LOG("\ndone\n");
    free(file_list);
}

// rendering

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
    glfwWindowHint(GLFW_SAMPLES, 4);  // Anti-aliasing

    GLFWwindow *window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT,
                                          "CT Scan Visualizer", NULL, NULL);

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
    GLuint fragment_shader =
        load_shader(FRAGMENT_SHADER_PATH, GL_FRAGMENT_SHADER);

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

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    RenderContext *render_ctx = malloc(sizeof(RenderContext));
    render_ctx->window = window;
    render_ctx->shader_program = shader_program;

    return render_ctx;
}

void terminate_renderer(RenderContext *render_ctx) {
    glDeleteProgram(render_ctx->shader_program);
    glfwDestroyWindow(render_ctx->window);
    glfwTerminate();
    free(render_ctx);
}

void render(RenderContext *render_ctx) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(render_ctx->shader_program);
    glfwSwapBuffers(render_ctx->window);
}

// input

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
