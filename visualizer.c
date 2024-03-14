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
    GLuint vao;
    GLuint mvp_uniform;
    mat4 mvp;
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

static const GLfloat cube[] = {
    -1.0f,-1.0f,-1.0f, // triangle 1 : begin
    -1.0f,-1.0f, 1.0f,
    -1.0f, 1.0f, 1.0f, // triangle 1 : end
    1.0f, 1.0f,-1.0f, // triangle 2 : begin
    -1.0f,-1.0f,-1.0f,
    -1.0f, 1.0f,-1.0f, // triangle 2 : end
    1.0f,-1.0f, 1.0f,
    -1.0f,-1.0f,-1.0f,
    1.0f,-1.0f,-1.0f,
    1.0f, 1.0f,-1.0f,
    1.0f,-1.0f,-1.0f,
    -1.0f,-1.0f,-1.0f,
    -1.0f,-1.0f,-1.0f,
    -1.0f, 1.0f, 1.0f,
    -1.0f, 1.0f,-1.0f,
    1.0f,-1.0f, 1.0f,
    -1.0f,-1.0f, 1.0f,
    -1.0f,-1.0f,-1.0f,
    -1.0f, 1.0f, 1.0f,
    -1.0f,-1.0f, 1.0f,
    1.0f,-1.0f, 1.0f,
    1.0f, 1.0f, 1.0f,
    1.0f,-1.0f,-1.0f,
    1.0f, 1.0f,-1.0f,
    1.0f,-1.0f,-1.0f,
    1.0f, 1.0f, 1.0f,
    1.0f,-1.0f, 1.0f,
    1.0f, 1.0f, 1.0f,
    1.0f, 1.0f,-1.0f,
    -1.0f, 1.0f,-1.0f,
    1.0f, 1.0f, 1.0f,
    -1.0f, 1.0f,-1.0f,
    -1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f,
    -1.0f, 1.0f, 1.0f,
    1.0f,-1.0f, 1.0f
};

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
    RenderContext *render_ctx = malloc(sizeof(RenderContext));

    // create a window
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);  // Anti-aliasing

    render_ctx->window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT,
                                          "CT Scan Visualizer", NULL, NULL);

    glfwMakeContextCurrent(render_ctx->window);
    glfwSetWindowSizeCallback(render_ctx->window, window_size_callback);

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
    render_ctx->shader_program = glCreateProgram();
    glAttachShader(render_ctx->shader_program, vertex_shader);
    glAttachShader(render_ctx->shader_program, fragment_shader);
    glLinkProgram(render_ctx->shader_program);

    int success;
    char info_log[512];
    glGetProgramiv(render_ctx->shader_program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(render_ctx->shader_program, 512, NULL, info_log);
        LOG("Failed to create shader program: %s\n", info_log);
        exit(-1);
    }

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    // create buffer object for the cube data
    GLuint vbo = 0;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube), cube, GL_STATIC_DRAW);

    // create a vertex array object
    render_ctx->vao = 0;
    glGenVertexArrays(1, &render_ctx->vao);
    glBindVertexArray(render_ctx->vao);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);

    // create a model-view-projection matrix
    mat4 proj;
    glm_perspective(glm_rad(45.0), (float)WINDOW_WIDTH / WINDOW_HEIGHT, 0.1, 100.0, proj);

    mat4 view;
    glm_lookat((vec3){4,4,-3}, (vec3){0,0,0}, (vec3){0,1,0}, view);

    mat4 model;
    glm_mat4_identity(model);

    glm_mat4_mulN((mat4 *[]){&proj, &view, &model}, 3, render_ctx->mvp);

    // create a uniform for the mvp matrix
    render_ctx->mvp_uniform = glGetUniformLocation(render_ctx->shader_program, "mvp");

    return render_ctx;
}

void terminate_renderer(RenderContext *render_ctx) {
    glDeleteProgram(render_ctx->shader_program);
    // TODO: delete buffers and vertex arrays

    glfwDestroyWindow(render_ctx->window);
    glfwTerminate();
    free(render_ctx);
}

void render(RenderContext *render_ctx) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(render_ctx->shader_program);
    glUniformMatrix4fv(render_ctx->mvp_uniform, 1, GL_FALSE, &render_ctx->mvp[0][0]);
    glEnableVertexAttribArray(0);
    glBindVertexArray(render_ctx->vao);
    glDrawArrays(GL_TRIANGLES, 0, 12 * 3);
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
    //load_images("./microtus_oregoni");

    RenderContext *render_ctx = init_renderer();
    while (!glfwWindowShouldClose(render_ctx->window)) {
        handle_input(render_ctx->window);
        render(render_ctx);
    }
    terminate_renderer(render_ctx);
}
