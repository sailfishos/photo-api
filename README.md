# photo-api

Camera middleware layer

## Overview

This repository contains:

- [controls.yaml](./controls.yaml): Central list of camera controls
- [lib](./lib): `photo-api` core library
- [qt](./qt): `org.sailfishos.PhotoApi` QML wrapper
- [tools](./tools): example programs for testing

### High-level

```
 ------------
| camera app |
 ------------
      |
 -------------------------------------
| org.sailfishos.PhotoApi QML wrapper |
 -------------------------------------
      |                         |
      |                    -----------
      |                   | GStreamer |
      |                    -----------
      |                     /       |
      |      photostreamsrc      encodebin
      |    /                        |
 --------------                     |
| libphoto-api |                    |
 --------------                     |
      |                             |
      :                             :
```

### Low-level implementation (Android Camera2 backend)

```
      :                             :
      |                             |
      |                         -----------
      |                        | droidvenc |
  --------------------          -----------
 | photoplugin-droid2 |             |
_____________________________________________ <-- libhybris boundary
  (droidmediaphoto...)      droidmediacodec
      |                             |
   Camera2 NDK API            stagefright
      |                             |
      +--   Android  frameworks   --+
      |                             |
  camera HAL                    codec HAL
_____________________________________________
                  Kernel
```

## API design

### State management

```
1. Camera listing

photo_enumerate_cameras()
 -> PhotoCameraInfo
 -> PhotoCameraInfo
    - id                 --------.
    - orientation                |
    - ...                        |
 -> ...                          |
                                 |
2. Establishing connection       |
                                 |
photo_camera_open(id)    <-------'
 -> PhotoCamera

3. Individual stream configuration

     ` photo_camera_create_stream(usage, callbacks)
        -> PhotoStream                                       -------.
       photo_camera_create_stream(usage, callbacks)                 |
        -> PhotoStream                                       -------|
       ...                                                          |
                                                                    |
            ` photo_stream_enumerate_configs()                      |
            ` photo_stream_configure()                              |
                 ` photo_stream_config_get_info()                   |
                 ` photo_stream_config_discard()                    |
                   photo_stream_config_apply()                      |
                                                                    |
4. Stream combination configuration and camera initialization       |
                                                                    |
     ` photo_camera_request_streams(streams)           <------------'

5. Capture

            ` photo_stream_start()

            * buffer_created event
            * buffer_invalidated event

            * frame_available event

            ` photo_stream_get_next_buffer()
              photo_stream_get_last_buffer()
               -> PhotoBuffer

                   ` photo_buffer_create_egl_image()
                   ` photo_buffer_map()
                   ` photo_buffer_release()

            ` photo_stream_take_picture() for still image streams

     * shutter event
     * capture_done event
     * disconnected event

            ` photo_stream_stop()

            * stopped event

6. Teardown

     ` photo_camera_release_streams()

            ` photo_stream_destroy()

     ` photo_camera_close()

```

### Camera controls with values

These can be used at any time while the camera is open.

```
     ` photo_camera_get_controls()
        -> PhotoControls

1. Enumeration

            ` photo_control_query_float(id)
              photo_control_query_enum(id)
              ...

2. Modification

            ` photo_control_set_float(id, value)
              photo_control_set_enum(id, value)
              ...

3. Atomic update

            ` photo_controls_commit()
```

### Trigger-and-wait controls (locks)

These are used for precapture sequences (e.g. autofocus) and assume that a viewfinder is running.

```
     ` photo_controls_trigger(lock mask)
       photo_controls_force_lock(lock mask)
       photo_controls_unlock(lock mask)

     ` photo_controls_get_lock_state(lock id)

     ` photo_controls_add_listener(callback, lock mask)
       photo_controls_remove_listener(callback, lock mask)
```

## Internal design

The list of controls or their available values should not be duplicated. Code that needs to be
repeated for each control is generated automatically from the `controls.yaml` file.

The library performs all necessary locking and performs checks to ensure that the frontend API is
being used correctly.

## Backend API design

Backends must be built as shared libraries (`.so` files) containing a `photo_plugin_init` function
and should be placed in the plugin directory (/usr/lib/photo-plugins or /usr/lib64/photo-plugins).

After pre-allocating their private state, backends must call `photo_backend_register_camera`,
`photo_backend_init_stream` or `photo_backend_init_buffer` to initialize the corresponding photo-api
object.

It is currently impossible to undo camera registration. Buffers may outlive the lifetime of their
streams.

Backends use versioned lists of function pointers (PhotoCameraImpl, PhotoStreamImpl, PhotoBufferImpl)
to implement the functionality of photo-api objects. All functions are called with a lock held for
exclusive access to the corresponding object, and it is safe to assume that the objects are in a
valid state. For example, `photo_stream_config_apply` will never be called while a stream is
running.

Controls and their metadata, including information about supported values, are stored inside the
PhotoCamera object and can be accessed by the backend directly. Metadata for supported controls
should be filled in during initialization or inside the implementations of the
`photo_control_query_*` functions.

When a control needs to be modified, the `update_params` backend function is called. It can detect
modified controls using the `dirty` bitmask to avoid wasting time on unnecessary updates.

Backends are required to start at least one separate thread for streaming. Events such as
`photo_backend_notify_capture` are assumed to arrive asynchronously. This approach is already
used in some APIs such as the Android NDK, so they can be integrated without a re-synchronization
layer in the backend plugin.

When changing the control state (only used for locks at the moment), backends must acquire and
release it using the `photo_backend_begin_state_change` and `photo_backend_end_state_change`
functions. The latter automatically calls listeners if its `changed` argument is set to true.

```
SomeBackendCamera {
  /* allocated by backend */

  PhotoBackendCamera {
    PhotoCamera *camera   -------------------------.
  }                                                |
                                                   |
  ... private backend fields ...                   |
}                                                  |
                                                   |
_PhotoCameraPrivate {                              |
  /* allocated by photo-api */                     |
                                                   |
  PhotoCamera {           <------------------------'
    ... public backend fields ...
  }

  ... internal fields ...
}

same for PhotoStream:

SomeBackendStream { PhotoBackendStream }
_PhotoStreamPrivate { PhotoStream }

and same for PhotoBuffer
```

## Qt integration details

This is only a QML plugin at the moment (no public C++ API).

It is mostly a wrapper around the camera state management and controls. The control bindings are
automatically generated from controls.yaml.

For convenience, it also provides some other pieces needed to build a camera app:
- Viewfinder based on EGL for hardware-accelerated zero-copy output
- Image capture component capable of saving image files
