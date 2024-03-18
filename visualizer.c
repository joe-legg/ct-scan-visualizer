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
#define EVENT_QUEUE_CAPACITY 512

#define VERTEX_SHADER_PATH "./shaders/vertex.vert"
#define FRAGMENT_SHADER_PATH "./shaders/fragment.frag"

// types

typedef struct {
    vec3 pos;
    vec3 forward;
    vec3 right;
    vec3 up;
    vec3 world_up;

    float yaw;
    float pitch;

    float speed;
    float look_sensitivity;
} Camera;

typedef struct {
    size_t cap;
    size_t length;
    vec3 *points;
} PointCloud;

typedef enum { FORWARD, BACKWARD, LEFT, RIGHT } Direction;

typedef enum {
    E_SHOULD_QUIT,
    E_WINDOW_RESIZE,
    E_CAMERA_MOVE,
    E_CAMERA_LOOK,
} EventType;

typedef struct {
    EventType type;
    union {
        vec2 mouse_pos;
        Direction camera_direction;

        struct {
            int window_w;
            int window_h;
        };
    };
} Event;

typedef struct {
    Event queue[EVENT_QUEUE_CAPACITY];
    size_t length;
} EventQueue;

typedef struct {
    bool running;

    float delta_time;
    float last_frame_time;

    int window_w;
    int window_h;
    vec2 last_mouse_pos;

    Camera *camera;
    PointCloud *point_cloud;
} World;

typedef struct {
    GLFWwindow *window;

    GLuint shader_program;
    GLuint vao;
    GLuint mvp_uniform;
} RenderContext;

// point cloud

PointCloud *point_cloud_init() {
    PointCloud *point_cloud = malloc(sizeof(PointCloud));
    point_cloud->length = 0;
    point_cloud->cap = 4;
    point_cloud->points = malloc(sizeof(vec3) * point_cloud->cap);
    return point_cloud;
}

void point_cloud_free(PointCloud *point_cloud) {
    free(point_cloud->points);
    free(point_cloud);
}

void point_cloud_append(PointCloud *point_cloud, vec3 point) {
    if (point_cloud->length == point_cloud->cap) {
        point_cloud->cap *= 2;
        point_cloud->points =
            realloc(point_cloud->points, sizeof(vec3) * point_cloud->cap);
    }

    memcpy(point_cloud->points[point_cloud->length], point, sizeof(vec3));
    ++point_cloud->length;
}

void point_cloud_csv_dump(PointCloud *point_cloud, FILE *fp) {
    fprintf(fp, "x,y,z\n");
    for (int i = 0; i < point_cloud->length; i++)
        fprintf(fp, "%f, %f, %f\n", point_cloud->points[i][0],
                point_cloud->points[i][1], point_cloud->points[i][2]);
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

    uint32_t pixels_count = w * h;
    uint32_t *raster = _TIFFmalloc(pixels_count * sizeof(uint32_t));
    if (!raster) {
        LOG("Failed to allocate memory to read TIFF file: %s\n", filename);
        exit(-1);
    }

    // read the image
    if (!TIFFReadRGBAImage(tif, w, h, raster, 0)) {
        LOG("Failed to read TIFF RGBA points in file: %s\n", filename);
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

PointCloud *point_cloud_load_from_path(const char *path) {
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

    return point_cloud;
}

// camera

void _camera_update_vectors(Camera *camera) {
    // calculate forward
    glm_vec3_zero(camera->forward);
    camera->forward[0] =
        cos(glm_rad(camera->yaw)) * cos(glm_rad(camera->pitch));
    camera->forward[1] = sin(glm_rad(camera->pitch));
    camera->forward[2] =
        sin(glm_rad(camera->yaw)) * cos(glm_rad(camera->pitch));
    glm_normalize(camera->forward);

    // right
    glm_cross(camera->forward, camera->world_up, camera->right);
    glm_vec3_normalize(camera->right);

    // up
    glm_cross(camera->right, camera->forward, camera->up);
    glm_vec3_normalize(camera->up);
}

Camera *camera_init() {
    Camera *camera = malloc(sizeof(Camera));
    glm_vec3_zero(camera->up);
    camera->world_up[1] = 1;
    glm_vec3_zero(camera->pos);

    camera->speed = 8;
    camera->look_sensitivity = 0.02;

    _camera_update_vectors(camera);
    return camera;
}

void camera_free(Camera *camera) { free(camera); }

void camera_get_view_matrix(const Camera *camera, mat4 *dest) {
    vec3 target;
    glm_vec3_add((float *)camera->forward, (float *)camera->pos, target);
    glm_lookat((float *)camera->pos, (float *)target, (float *)camera->up,
               *dest);
}

void camera_move(float delta_time, Camera *camera, Direction direction) {
    float speed = camera->speed * delta_time;
    switch (direction) {
        case FORWARD:
            glm_vec3_muladds(camera->forward, speed, camera->pos);
            break;
        case BACKWARD:
            glm_vec3_mulsubs(camera->forward, speed, camera->pos);
            break;
        case LEFT: {
            vec3 right;
            glm_cross(camera->forward, camera->up, right);
            glm_vec3_mulsubs(right, speed, camera->pos);
            break;
        }
        case RIGHT: {
            vec3 right;
            glm_cross(camera->forward, camera->up, right);
            glm_vec3_muladds(right, speed, camera->pos);
            break;
        }
    }
}

void camera_look(Camera *camera, float xoffset, float yoffset) {
    xoffset *= camera->look_sensitivity;
    yoffset *= camera->look_sensitivity;

    camera->yaw += xoffset;
    camera->pitch -= yoffset;

    // don't allow the user to flip the camera over 90 degrees
    if (camera->pitch > 89.0f)
        camera->pitch = 89.0f;
    else if (camera->pitch < -89.0f)
        camera->pitch = -89.0f;

    _camera_update_vectors(camera);
}

// event queue

Event *event_queue_new_event(EventQueue *event_queue) {
    if (event_queue->length == EVENT_QUEUE_CAPACITY) {
        LOG("Too many events\n");
        exit(-1);
    }
    Event *event = &event_queue->queue[event_queue->length];
    event_queue->length++;
    return event;
}

void event_queue_flush(EventQueue *event_queue) { event_queue->length = 0; }

// rendering

GLuint shader_load(const char *filename, GLenum shader_type) {
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

RenderContext *renderer_init() {
    RenderContext *render_ctx = malloc(sizeof(RenderContext));

    // create a window
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);  // Anti-aliasing

    render_ctx->window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT,
                                          "CT Scan Visualizer", NULL, NULL);
    if (!render_ctx->window) {
        LOG("Failed to initialize window\n");
        exit(-1);
    }

    glfwMakeContextCurrent(render_ctx->window);

    // initialize glad
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        LOG("Failed to initialize glad\n");
        exit(-1);
    }

    // enable depth testing
    glEnable(GL_DEPTH_TEST);

    // load shaders
    GLuint vertex_shader = shader_load(VERTEX_SHADER_PATH, GL_VERTEX_SHADER);
    GLuint fragment_shader =
        shader_load(FRAGMENT_SHADER_PATH, GL_FRAGMENT_SHADER);

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

    // create a uniform for the mvp matrix
    render_ctx->mvp_uniform =
        glGetUniformLocation(render_ctx->shader_program, "mvp");

    return render_ctx;
}

void renderer_free(RenderContext *render_ctx) {
    glDeleteProgram(render_ctx->shader_program);
    // TODO: delete buffers and vertex arrays

    glfwDestroyWindow(render_ctx->window);
    glfwTerminate();
    free(render_ctx);
}

void renderer_update(RenderContext *render_ctx, const World *world) {
    // calculate the view-projection matrix
    mat4 proj;
    glm_perspective(glm_rad(45.0), (float)world->window_w / world->window_h,
                    0.1, 100.0, proj);

    mat4 view;
    camera_get_view_matrix(world->camera, &view);

    mat4 vp;
    glm_mat4_mul(proj, view, vp);

    // draw
    glViewport(0, 0, world->window_w, world->window_h);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(render_ctx->shader_program);

    if (!render_ctx->vao) {
        GLuint vbo = 0;
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * world->point_cloud->length, world->point_cloud->points, GL_STATIC_DRAW);

        // create a vertex array object
        render_ctx->vao = 0;
        glGenVertexArrays(1, &render_ctx->vao);
        glBindVertexArray(render_ctx->vao);
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
    }

    glBindVertexArray(render_ctx->vao);
    mat4 mvp, model;
    glm_mat4_identity(model);
    glm_scale(model, (vec3){0.01, 0.01, 0.01});
    glm_mat4_mul(vp, model, mvp);
    glUniformMatrix4fv(render_ctx->mvp_uniform, 1, GL_FALSE, &mvp[0][0]);
    glPointSize(.000002);
    glDrawArrays(GL_POINTS, 0, world->point_cloud->length);

    glfwSwapBuffers(render_ctx->window);
}

// state

World *world_init() {
    World *world = malloc(sizeof(World));
    world->running = true;

    world->delta_time = 0;
    world->last_frame_time = 0;

    world->window_w = 0;
    world->window_h = 0;
    glm_vec2_zero(world->last_mouse_pos);

    world->camera = camera_init();
    world->point_cloud = point_cloud_load_from_path("./peromyscus_gossypinus");
    return world;
}

void world_free(World *world) {
    point_cloud_free(world->point_cloud);
    camera_free(world->camera);
    free(world);
}

void world_update(World *world, const EventQueue *event_queue) {
    float current_frame = glfwGetTime();
    world->delta_time = current_frame - world->last_frame_time;
    world->last_frame_time = current_frame;

    for (int i = 0; i < event_queue->length; i++) {
        Event event = event_queue->queue[i];
        switch (event.type) {
            case E_SHOULD_QUIT:
                world->running = false;
                break;
            case E_WINDOW_RESIZE:
                world->window_w = event.window_w;
                world->window_h = event.window_h;
                break;
            case E_CAMERA_MOVE:
                camera_move(world->delta_time, world->camera,
                            event.camera_direction);
                break;
            case E_CAMERA_LOOK: {
                float xoffset = event.mouse_pos[0] - world->last_mouse_pos[0];
                float yoffset = event.mouse_pos[1] - world->last_mouse_pos[1];
                glm_vec2_copy(event.mouse_pos, world->last_mouse_pos);

                camera_look(world->camera, xoffset, yoffset);
                break;
            }
            default:
                LOG("Unkown event: %d\n", event.type);
                break;
        }
    }
}

// input

void input_window_size_callback(GLFWwindow *window, int width, int height) {
    EventQueue *event_queue = glfwGetWindowUserPointer(window);
    Event *e = event_queue_new_event(event_queue);
    e->type = E_WINDOW_RESIZE;
    e->window_w = width;
    e->window_h = height;
}

void input_cursor_callback(GLFWwindow *window, double xpos, double ypos) {
    EventQueue *event_queue = glfwGetWindowUserPointer(window);
    Event *e = event_queue_new_event(event_queue);
    e->type = E_CAMERA_LOOK;
    e->mouse_pos[0] = xpos;
    e->mouse_pos[1] = ypos;
}

void input_update(GLFWwindow *window, EventQueue *event_queue) {
    // TODO: should events have constructor functions?

    event_queue_flush(event_queue);
    glfwPollEvents();
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        Event *e = event_queue_new_event(event_queue);
        e->type = E_SHOULD_QUIT;
    } else if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        Event *e = event_queue_new_event(event_queue);
        e->type = E_CAMERA_MOVE;
        e->camera_direction = FORWARD;
    } else if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        Event *e = event_queue_new_event(event_queue);
        e->type = E_CAMERA_MOVE;
        e->camera_direction = BACKWARD;
    } else if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        Event *e = event_queue_new_event(event_queue);
        e->type = E_CAMERA_MOVE;
        e->camera_direction = LEFT;
    } else if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        Event *e = event_queue_new_event(event_queue);
        e->type = E_CAMERA_MOVE;
        e->camera_direction = RIGHT;
    }
}

void input_init(GLFWwindow *window, EventQueue *event_queue) {
    glfwSetWindowUserPointer(window, event_queue);
    glfwSetWindowSizeCallback(window, input_window_size_callback);
    glfwSetCursorPosCallback(window, input_cursor_callback);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

int main(int argc, char *argv[]) {
    EventQueue event_queue = {0};
    World *world = world_init();
    RenderContext *render_ctx = renderer_init();
    input_init(render_ctx->window, &event_queue);

    while (world->running) {
        input_update(render_ctx->window, &event_queue);
        world_update(world, &event_queue);
        renderer_update(render_ctx, world);
    }

    renderer_free(render_ctx);
    world_free(world);
}
