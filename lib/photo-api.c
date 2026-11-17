// SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
//
// SPDX-License-Identifier: BSD-3-Clause

#define _GNU_SOURCE

#define G_LOG_DOMAIN "photo-api"

#include <dlfcn.h>
#include <glib.h>

#include "photo-private.h"

// Internals

static struct {
    pthread_mutex_t mutex;
    PhotoCamera *first_camera;
    int64_t startup_time;
    bool init_done;
    bool log_time;
} global_camera_data = {
    .mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP,
};

struct timer_scope_data {
    const char *name;
};

static inline void timer_cleanup(struct timer_scope_data *data)
{
    if (global_camera_data.log_time) {
        g_message("%s: end %ld", data->name,
                  g_get_monotonic_time() - global_camera_data.startup_time);
    }
}

#define timer_scope(_name)                                                      \
    if (global_camera_data.log_time) {                                          \
        g_message("%s: begin %ld", #_name,                                      \
                  g_get_monotonic_time() - global_camera_data.startup_time);    \
    }                                                                           \
    __attribute__((unused)) __attribute__((cleanup(timer_cleanup)))             \
    struct timer_scope_data _name##_timer = {                                   \
        .name = #_name,                                                         \
    }

static bool load_plugin(const char *path)
{
    g_debug("Loading plugin: %s", path);

    // Clear previous errors
    dlerror();

    void *handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        g_warning("dlopen failed: %s", dlerror());
        return false;
    }

    bool (*init)(void) = dlsym(handle, "photo_plugin_init");
    if (!init) {
        g_warning("dlsym failed: %s", dlerror());
        dlclose(handle);
        return false;
    }

    return init();
}

static void probe_plugins(void)
{
    global_camera_data.log_time = getenv("CAMERA_STARTUP_LOG") != NULL;
    global_camera_data.startup_time = g_get_monotonic_time();

    timer_scope(probe_plugins);

#ifndef PLUGIN_DIR
#error "Missing -DPLUGIN_DIR=..."
#endif
    GError *error = NULL;
    GDir *dir = g_dir_open(PLUGIN_DIR, 0, &error);
    if (!dir) {
        if (error) {
            g_warning("Failed to open plugin directory: %s", error->message);
            g_error_free(error);
        }
        return;
    }

    const char *name;
    while ((name = g_dir_read_name(dir))) {
        if (g_str_has_suffix(name, ".so")) {
            char *path = g_build_path("/", PLUGIN_DIR, name, NULL);
            load_plugin(path);
            g_free(path);
        }
    }

    g_dir_close(dir);
}

// called with precapture_lock
static bool is_ready_for_capture(PhotoControlLockState state)
{
    switch (state) {
    case PHOTO_CONTROL_STATE_PENDING:
    case PHOTO_CONTROL_STATE_SEARCHING:
    case PHOTO_CONTROL_STATE_FORCE_LOCKING:
        return false;
    default:
        break;
    }

    return true;
}

// called with precapture_lock
static void try_start_still_capture(PhotoCamera *camera)
{
    PhotoStream *stream = camera->still_capture_stream;

    if (!stream || camera->still_capture_started) {
        return;
    }

    for (int i = 0; i < PHOTO_CONTROL_LOCK_ID_NUM; i++) {
        if (!is_ready_for_capture(camera->lock_states[i]))
            return;
    }

    if (stream->impl->take_picture(stream)) {
        camera->still_capture_started = true;
    } else {
        if (camera->callbacks && camera->callbacks->capture_done) {
            camera->callbacks->capture_done(camera->userdata, false);
        }

        camera->still_capture_stream = NULL;
    }
}

static void free_buffer(PhotoBuffer *buffer)
{
    pthread_mutex_destroy(&buffer->lock);

    buffer->p.backend->photo_buffer = NULL;
    buffer->impl->destroy(buffer->p.backend);
    g_free(buffer);
}

// Backend API (photo-backend.h)

bool photo_backend_register_camera(PhotoBackendCamera *backend_camera,
                                   const char *id,
                                   const PhotoCameraImpl *impl)
{
    PhotoCamera *camera = g_new0(PhotoCamera, 1);

    // Set up pointers for backend access
    camera->p.info = &camera->info;
    camera->p.dirty = camera->dirty;
    camera->p.lock_states = camera->lock_states;

    pthread_mutexattr_t mutexattr;
    pthread_mutexattr_init(&mutexattr);
    pthread_mutexattr_settype(&mutexattr, PTHREAD_MUTEX_ERRORCHECK);
    pthread_mutex_init(&camera->lock, &mutexattr);
    pthread_mutex_init(&camera->precapture_lock, &mutexattr);
    pthread_mutexattr_destroy(&mutexattr);

    camera->p.backend = backend_camera;
    camera->impl = impl;
    camera->state = PHOTO_CAMERA_STATE_CLOSED;
    camera->info.id = g_strdup(id);

    backend_camera->photo_camera = camera;

    if (!camera->impl->init(camera)) {
        g_warning("Camera '%s' failed to initialize", id);
        backend_camera->photo_camera = NULL;
        camera->impl->destroy(backend_camera);
        pthread_mutex_destroy(&camera->lock);
        pthread_mutex_destroy(&camera->precapture_lock);
        g_free((char *)camera->info.id);
        g_free(camera);
        return false;
    }

    hold_lock(&global_camera_data.mutex, lock);
    camera->next = global_camera_data.first_camera;
    global_camera_data.first_camera = camera;

    return true;
}

bool photo_backend_init_stream(PhotoBackendCamera *backend_camera,
                               PhotoBackendStream *backend_stream,
                               PhotoStreamUsage usage, const PhotoStreamImpl *impl)
{
    PhotoStream *stream = g_new0(PhotoStream, 1);

    stream->impl = impl;

    stream->p.backend = backend_stream;
    stream->p.usage = usage;
    stream->p.camera = backend_camera->photo_camera;

    backend_stream->photo_stream = stream;

    return true;
}

void photo_backend_begin_state_change(PhotoBackendCamera *backend_camera)
{
    PhotoCamera *camera = backend_camera->photo_camera;

    if (pthread_mutex_lock(&camera->precapture_lock)) {
        g_critical("begin_state_change called during state change?");
    }
}

void photo_backend_end_state_change(PhotoBackendCamera *backend_camera, bool changed)
{
    PhotoCamera *camera = backend_camera->photo_camera;

    if (changed) {
        for (int i = 0; i < PHOTO_CONTROL_LOCK_ID_NUM; i++) {
            PhotoControlLockState old_state = camera->old_lock_states[i];
            PhotoControlLockState new_state = camera->lock_states[i];
            if (old_state == new_state)
                continue;

            for (PhotoControlListener *l = camera->lock_listeners; l; l = l->next) {
                if (l->filter & (1 << i))
                    l->callback(l->userdata, i, old_state, new_state);
            }

            camera->old_lock_states[i] = new_state;
        }

        try_start_still_capture(camera);
    }

    if (pthread_mutex_unlock(&camera->precapture_lock)) {
        g_critical("end_state_change called outside state change?");
    }
}

void photo_backend_notify_shutter(PhotoBackendCamera *backend_camera)
{
    PhotoCamera *camera = backend_camera->photo_camera;

    if (camera->callbacks && camera->callbacks->shutter)
        camera->callbacks->shutter(camera->userdata);
}

void photo_backend_notify_capture(PhotoBackendCamera *backend_camera, bool successful)
{
    PhotoCamera *camera = backend_camera->photo_camera;

    hold_lock_or_return(&camera->precapture_lock, lock);

    if (camera->callbacks && camera->callbacks->capture_done) {
        camera->callbacks->capture_done(camera->userdata, successful);
    }

    camera->still_capture_started = false;
    camera->still_capture_stream = NULL;
}

void photo_backend_notify_disconnected(PhotoBackendCamera *backend_camera)
{
    PhotoCamera *camera = backend_camera->photo_camera;

    if (camera->callbacks && camera->callbacks->disconnected)
        camera->callbacks->disconnected(camera->userdata);
}

void photo_backend_frame_available(PhotoBackendStream *backend_stream)
{
    PhotoStream *stream = backend_stream->photo_stream;

    if (stream->callbacks && stream->callbacks->frame_available)
        stream->callbacks->frame_available(stream->userdata);
}

void photo_backend_stream_stopped(PhotoBackendStream *backend_stream)
{
    PhotoStream *stream = backend_stream->photo_stream;

    if (stream->callbacks && stream->callbacks->stopped)
        stream->callbacks->stopped(stream->userdata);
}

bool photo_backend_init_buffer(PhotoBackendStream *backend_stream,
                               PhotoBackendBuffer *backend_buffer,
                               unsigned int num_planes, PhotoBufferPlaneInfo *planes,
                               const PhotoBufferImpl *impl)
{
    PhotoStream *stream = backend_stream->photo_stream;

    if (num_planes > PHOTO_BUFFER_MAX_PLANES) {
        g_critical("trying to allocate too many planes: %d", num_planes);
        impl->destroy(backend_buffer);
        return false;
    }

    PhotoBuffer *buffer = g_new0(PhotoBuffer, 1);

    pthread_mutex_init(&buffer->lock, NULL);
    buffer->impl = impl;

    memcpy(buffer->plane_info, planes, num_planes * sizeof(PhotoBufferPlaneInfo));

    buffer->info.num_planes = num_planes;
    buffer->info.planes = buffer->plane_info;

    buffer->p.backend = backend_buffer;
    buffer->p.stream = stream;
    buffer->p.info = &buffer->info;
    buffer->p.planes = buffer->plane_data;

    backend_buffer->photo_buffer = buffer;

    return true;
}

void photo_backend_bind_buffer(PhotoBackendBuffer *backend_buffer)
{
    PhotoBuffer *buffer = backend_buffer->photo_buffer;
    PhotoStream *stream = buffer->p.stream;

    if (stream->callbacks && stream->callbacks->buffer_created)
        stream->callbacks->buffer_created(stream->userdata, buffer);
}

void photo_backend_unbind_buffer(PhotoBackendBuffer *backend_buffer)
{
    PhotoBuffer *buffer = backend_buffer->photo_buffer;
    PhotoStream *stream = NULL;
    bool can_free = true;

    {
        hold_lock(&buffer->lock, buffer_lock);

        stream = buffer->p.stream;

        if (buffer->acquired) {
            buffer->p.stream = NULL;
            can_free = false;
        }
    }

    if (stream->callbacks && stream->callbacks->buffer_invalidated)
        stream->callbacks->buffer_invalidated(stream->userdata, buffer);

    // If the stream didn't release the buffer, we can't do anything
    // but wait. The backend should not deallocate the buffer before
    // buffer->impl->destroy() is called.

    if (can_free) {
        free_buffer(buffer);
    }
}

// Public API (photo-api.h)

void photo_enumerate_cameras(PhotoCameraIteratorCallback cb, void *userdata)
{
    g_return_if_fail(cb);
    hold_lock(&global_camera_data.mutex, lock);

    if (!global_camera_data.init_done) {
        global_camera_data.init_done = true;
        probe_plugins();
    }

    PhotoCamera *camera = global_camera_data.first_camera;
    while (camera) {
        cb(userdata, &camera->info);
        camera = camera->next;
    }
}

PhotoCamera *photo_camera_open(const char *id, const PhotoCameraCallbacks *cb, void *userdata)
{
    hold_lock(&global_camera_data.mutex, lock);

    if (!global_camera_data.init_done) {
        global_camera_data.init_done = true;
        probe_plugins();
    }

    timer_scope(camera_open);

    PhotoCamera *camera = global_camera_data.first_camera;
    while (camera) {
        hold_lock(&camera->lock, camera_lock);

        if (!id || strcmp(camera->info.id, id) == 0) {
            g_assert(camera->state == PHOTO_CAMERA_STATE_CLOSED ||
                     camera->state == PHOTO_CAMERA_STATE_OPENED ||
                     camera->state == PHOTO_CAMERA_STATE_RUNNING);

            if (camera->state != PHOTO_CAMERA_STATE_CLOSED) {
                if (id) {
                    g_warning("Camera '%s' is already open!", id);
                    return NULL;
                } else {
                    camera = camera->next;
                    continue;
                }
            }

            camera->callbacks = cb;
            camera->userdata = userdata;

            if (!camera->impl->open(camera)) {
                camera->impl->close(camera);
                if (id) {
                    return NULL;
                } else {
                    camera = camera->next;
                    continue;
                }
            }

            camera->state = PHOTO_CAMERA_STATE_OPENED;
            return camera;
        }

        camera = camera->next;
    }

    if (id) {
        g_warning("Camera '%s' does not exist", id);
    } else {
        g_warning("No available cameras found");
    }

    return NULL;
}

void photo_camera_close(PhotoCamera *camera)
{
    g_return_if_fail(camera);
    hold_lock_or_return(&camera->lock, lock);
    g_return_if_fail(camera->state == PHOTO_CAMERA_STATE_OPENED);

    PhotoControlListener *tmp, *l = camera->lock_listeners;
    while (l) {
        tmp = l->next;
        free(l);
        l = tmp;
    }

    camera->lock_listeners = NULL;

    camera->impl->close(camera);
    camera->state = PHOTO_CAMERA_STATE_CLOSED;
    camera->callbacks = NULL;
    camera->userdata = NULL;
}

const PhotoCameraInfo *photo_camera_get_info(PhotoCamera *camera)
{
    g_return_val_if_fail(camera, NULL);
    hold_lock_or_return_val(&camera->lock, lock, NULL);
    g_return_val_if_fail(camera->state == PHOTO_CAMERA_STATE_OPENED ||
                         camera->state == PHOTO_CAMERA_STATE_RUNNING, NULL);

    return &camera->info;
}

PhotoControls *photo_camera_get_controls(PhotoCamera *camera)
{
    g_return_val_if_fail(camera, NULL);
    hold_lock_or_return_val(&camera->lock, lock, NULL);
    g_return_val_if_fail(camera->state == PHOTO_CAMERA_STATE_OPENED ||
                         camera->state == PHOTO_CAMERA_STATE_RUNNING, NULL);

    // See photo-controls.h
    return camera;
}

static void photo_stream_build_config_cb(void *userdata, const PhotoAvailableConfig *info)
{
    PhotoFixedConfig *chosen = userdata;

    // If the filter matches multiple configurations, select the one with
    // the largest size.
    if ((info->width.max * info->height.max) > (chosen->width * chosen->height)) {
        chosen->width = info->width.max;
        chosen->height = info->height.max;

        chosen->format = info->format;
        chosen->min_fps = info->min_fps.max;
        chosen->max_fps = info->max_fps.max;

        chosen->native_format = info->native_format;
    }
}

static PhotoStreamConfig *photo_stream_configure_locked(PhotoStream *stream,
                                                        const PhotoConfig *config)
{
    PhotoStreamConfig *stream_config = g_new0(PhotoStreamConfig, 1);

    PhotoAvailableConfig tmp = { 0 };

    stream->impl->enumerate_configs(stream, config, photo_stream_build_config_cb,
                                    &stream_config->info, PHOTO_STREAM_CONFIG_RECOMMENDED, &tmp);

    if (stream_config->info.width == 0) {
        stream->impl->enumerate_configs(stream, config, photo_stream_build_config_cb,
                                        &stream_config->info, 0, &tmp);
    }

    if (stream_config->info.width == 0) {
        g_free(stream_config);
        return NULL;
    }

    stream_config->stream = stream;

    return stream_config;
}

PhotoStream *photo_camera_create_stream(PhotoCamera *camera, PhotoStreamUsage usage,
                                        const PhotoStreamCallbacks *callbacks, void *userdata)
{
    g_return_val_if_fail(camera, NULL);
    hold_lock_or_return_val(&camera->lock, lock, NULL);
    g_return_val_if_fail(camera->state == PHOTO_CAMERA_STATE_OPENED ||
                         camera->state == PHOTO_CAMERA_STATE_RUNNING, NULL);

    PhotoStream *stream = camera->impl->create_stream(camera, usage);
    if (stream) {
        stream->callbacks = callbacks;
        stream->userdata = userdata;

        PhotoStreamConfig *stream_config = photo_stream_configure_locked(stream, NULL);
        if (!stream_config) {
            g_warning("Camera '%s' was unable to find an initial stream configuration",
                      camera->info.id);
            stream->p.backend->photo_stream = NULL;
            stream->impl->destroy(stream->p.backend);
            g_free(stream);
            return NULL;
        }

        stream->config = stream_config;
        stream->p.config = &stream_config->info;
    }

    return stream;
}

bool photo_camera_request_streams(PhotoCamera *camera, int count, PhotoStream * const *streams)
{
    timer_scope(request_streams);
    g_return_val_if_fail(camera, false);
    g_return_val_if_fail(count > 0, false);
    for (int i = 0; i < count; i++) {
        g_return_val_if_fail(streams[i], false);
    }
    hold_lock_or_return_val(&camera->lock, lock, false);
    g_return_val_if_fail(camera->state == PHOTO_CAMERA_STATE_OPENED, false);
    g_assert(!camera->p.streams);

    camera->p.streams = g_try_malloc_n(count, sizeof(PhotoStream *));
    if (!camera->p.streams) {
        return false;
    }

    camera->p.num_streams = count;
    for (int i = 0; i < count; i++) {
        streams[i]->active = true;
        camera->p.streams[i] = streams[i];
    }

    if (!camera->impl->request_streams(camera)) {
        for (int i = 0; i < count; i++) {
            streams[i]->active = false;
        }
        camera->impl->release_streams(camera);
        g_free(camera->p.streams);
        camera->p.streams = NULL;
        camera->p.num_streams = 0;
        return false;
    }

    camera->state = PHOTO_CAMERA_STATE_RUNNING;

    return true;
}

void photo_camera_release_streams(PhotoCamera *camera)
{
    g_return_if_fail(camera);
    hold_lock_or_return(&camera->lock, lock);
    g_return_if_fail(camera->state == PHOTO_CAMERA_STATE_OPENED ||
                     camera->state == PHOTO_CAMERA_STATE_RUNNING);

    if (camera->p.streams) {
        for (int i = 0; i < camera->p.num_streams; i++) {
            PhotoStream *stream = camera->p.streams[i];
            if (stream->p.running) {
                stream->p.running = false;
                stream->impl->stop(stream);
            }
            stream->active = false;
        }
        camera->impl->release_streams(camera);
        g_free(camera->p.streams);
        camera->p.streams = NULL;
        camera->p.num_streams = 0;
    }

    camera->state = PHOTO_CAMERA_STATE_OPENED;
}

PhotoConfig *photo_config_new(void)
{
    PhotoConfig *config = g_new(PhotoConfig, 1);

    config->format = 0;
    config->width = -1;
    config->height = -1;
    config->min_fps = -1;
    config->max_fps = -1;

    return config;
}

void photo_config_set_format(PhotoConfig *config, unsigned int fourcc)
{
    g_return_if_fail(config);

    config->format = fourcc;
}

void photo_config_set_size(PhotoConfig *config, int width, int height)
{
    g_return_if_fail(config);
    g_return_if_fail((width > 0 && height > 0) || (width == -1 || height == -1));

    config->width = width;
    config->height = height;
}

void photo_config_set_fps_range(PhotoConfig *config, int min_fps, int max_fps)
{
    g_return_if_fail(config);
    g_return_if_fail((min_fps > 0 && max_fps > 0) || (min_fps == -1 || max_fps == -1));

    config->min_fps = min_fps;
    config->max_fps = max_fps;
}

void photo_config_free(PhotoConfig *config)
{
    g_free(config);
}

void photo_stream_enumerate_configs(PhotoStream *stream, const PhotoConfig *filter,
                                    PhotoConfigIteratorCallback cb, void *userdata,
                                    PhotoStreamConfigFlags flags)
{
    timer_scope(enumerate_configs);
    g_return_if_fail(stream);
    g_return_if_fail(cb);

    PhotoAvailableConfig tmp = { 0 };
    stream->impl->enumerate_configs(stream, filter, cb, userdata, flags, &tmp);
}

PhotoStreamConfig *photo_stream_configure(PhotoStream *stream, const PhotoConfig *config)
{
    timer_scope(stream_configure);
    g_return_val_if_fail(stream, NULL);
    hold_lock_or_return_val(&stream->p.camera->lock, lock, NULL);
    g_return_val_if_fail(!stream->active, NULL);

    return photo_stream_configure_locked(stream, config);
}

const PhotoFixedConfig *photo_stream_config_get_info(PhotoStreamConfig *stream_config)
{
    g_return_val_if_fail(stream_config, NULL);

    return &stream_config->info;
}

void photo_stream_config_apply(PhotoStreamConfig *stream_config)
{
    g_return_if_fail(stream_config);
    g_return_if_fail(stream_config->stream);
    PhotoStream *stream = stream_config->stream;
    hold_lock_or_return(&stream->p.camera->lock, lock);
    g_return_if_fail(!stream->active);

    g_free(stream->config);
    stream->config = stream_config;
    stream->p.config = &stream_config->info;
}

void photo_stream_config_discard(PhotoStreamConfig *stream_config)
{
    g_free(stream_config);
}

bool photo_stream_start(PhotoStream *stream)
{
    timer_scope(stream_start);
    g_return_val_if_fail(stream, false);
    hold_lock_or_return_val(&stream->p.camera->lock, lock, false);
    g_return_val_if_fail(stream->active && !stream->p.running, false);

    stream->p.running = true;
    if (!stream->impl->start(stream)) {
        stream->p.running = false;
        stream->impl->stop(stream);
        return false;
    }

    return true;
}

void photo_stream_stop(PhotoStream *stream)
{
    g_return_if_fail(stream);
    hold_lock_or_return(&stream->p.camera->lock, lock);
    g_return_if_fail(stream->active);

    if (stream->p.running) {
        stream->p.running = false;
        stream->impl->stop(stream);
    }
}

bool photo_stream_take_picture(PhotoStream *stream)
{
    g_return_val_if_fail(stream, false);

    PhotoCamera *camera = stream->p.camera;
    hold_lock_or_return_val(&camera->lock, lock, false);
    g_return_val_if_fail(stream->active, false);
    g_return_val_if_fail(!stream->p.running, false);

    hold_lock_or_return_val(&camera->precapture_lock, precapture_lock, false);

    if (camera->still_capture_stream) {
        g_warning("Cannot take another picture while capture is in progress");
        return false;
    }

    camera->still_capture_stream = stream;

    try_start_still_capture(camera);

    return true;
}

PhotoBuffer *photo_stream_get_next_buffer_with_metadata(PhotoStream *stream, int num_meta_reqs,
                                                        PhotoMetadataRequest *meta_reqs)
{
    g_return_val_if_fail(stream, NULL);
    hold_lock_or_return_val(&stream->p.camera->lock, lock, NULL);

    if (!stream->active) {
        return NULL;
    }

    PhotoBuffer *buffer = stream->impl->get_next_buffer(stream, num_meta_reqs, meta_reqs);

    if (buffer) {
        hold_lock(&buffer->lock, buffer_lock);
        buffer->acquired = true;
    }

    return buffer;
}

PhotoBuffer *photo_stream_get_last_buffer(PhotoStream *stream)
{
    g_return_val_if_fail(stream, NULL);
    hold_lock_or_return_val(&stream->p.camera->lock, lock, NULL);

    if (!stream->active) {
        return NULL;
    }

    PhotoBuffer *buffer = NULL, *next_buffer = stream->impl->get_next_buffer(stream, 0, NULL);

    while (next_buffer) {
        buffer = next_buffer;

        next_buffer = stream->impl->get_next_buffer(stream, 0, NULL);
        if (!next_buffer) {
            break;
        }

        hold_lock(&buffer->lock, buffer_lock);
        if (buffer->p.stream) {
            buffer->impl->release(buffer);
        }
    }

    if (buffer) {
        hold_lock(&buffer->lock, buffer_lock);
        buffer->acquired = true;
    }

    return buffer;
}

void photo_stream_destroy(PhotoStream *stream)
{
    if (!stream)
        return;

    hold_lock_or_return(&stream->p.camera->lock, lock);
    g_return_if_fail(!stream->active);

    g_free(stream->config);
    stream->config = NULL;
    stream->p.config = NULL;

    stream->p.backend->photo_stream = NULL;
    stream->impl->destroy(stream->p.backend);
    // At this point (or preferably on photo_camera_release_streams), all
    // buffers should be unbound from the stream.
    g_free(stream);
}

const PhotoBufferInfo *photo_buffer_get_info(PhotoBuffer *buffer)
{
    g_return_val_if_fail(buffer, NULL);

    return &buffer->info;
}

void photo_buffer_set_userdata(PhotoBuffer *buffer, void *userdata)
{
    g_return_if_fail(buffer);
    hold_lock(&buffer->lock, lock);

    buffer->userdata = userdata;
}

void *photo_buffer_get_userdata(PhotoBuffer *buffer)
{
    g_return_val_if_fail(buffer, NULL);
    hold_lock(&buffer->lock, lock);

    return buffer->userdata;
}

const PhotoBufferPlaneData *photo_buffer_map(PhotoBuffer *buffer)
{
    g_return_val_if_fail(buffer, NULL);
    hold_lock(&buffer->lock, lock);

    if (!buffer->mapped) {
        if (!buffer->p.stream || !buffer->impl->map(buffer)) {
            return NULL;
        }

        buffer->mapped = true;
    }

    return buffer->plane_data;
}

void photo_buffer_release(PhotoBuffer *buffer)
{
    g_return_if_fail(buffer);
    bool can_free = true;

    {
        hold_lock(&buffer->lock, lock);
        if (buffer->p.stream) {
            g_return_if_fail(buffer->acquired);

            buffer->impl->release(buffer);
            buffer->mapped = false;
            buffer->acquired = false;
            can_free = false;
        }
    }

    if (can_free) {
        free_buffer(buffer);
    }
}
