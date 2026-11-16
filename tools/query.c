// SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
//
// SPDX-License-Identifier: BSD-3-Clause

#include <stdio.h>
#include <stdlib.h>

#include <photo-api.h>

const char *camera_facing_str(PhotoCameraFacing facing)
{
    return facing == PHOTO_CAMERA_FACING_FRONT ? "front" :
           facing == PHOTO_CAMERA_FACING_BACK ? "back" : "unknown";
}

void list_single_camera(void *userdata, const PhotoCameraInfo *info)
{
    printf("'%s' (%s)\n", info->id, camera_facing_str(info->facing));
}

int dump_camera_info(PhotoCamera *camera)
{
    const PhotoCameraInfo *info = photo_camera_get_info(camera);
    if (!info) {
        fprintf(stderr, "Failed to get camera info\n");
        return 1;
    }

    printf("id: %s\n", info->id);
    printf("facing: %s\n", camera_facing_str(info->facing));
    printf("orientation: %d\n", info->orientation);
    printf("active array: %dx%d\n", info->active_array_width, info->active_array_height);

    PhotoControls *controls = photo_camera_get_controls(camera);

    const PhotoControlFloatRange *ae = photo_control_query_float(controls, PHOTO_CONTROL_AE_COMPENSATION);
    if (ae) {
        printf("AE compensation:\n"
               "  min: %f\n"
               "  max: %f\n"
               "  step: %f\n",
               ae->min, ae->max, ae->step);
    }

    const PhotoBitMask *focus_modes = photo_control_query_enum(controls, PHOTO_CONTROL_FOCUS_MODE);
    if (focus_modes) {
        printf("available focus modes:\n");
        if (photo_check_bit(focus_modes, PHOTO_AUTO_FOCUS))
            printf("  - auto\n");
        if (photo_check_bit(focus_modes, PHOTO_CONTINUOUS_AUTO_FOCUS))
            printf("  - continuous\n");
        if (photo_check_bit(focus_modes, PHOTO_MANUAL_FOCUS))
            printf("  - manual\n");
    }

    const PhotoBitMask *flash_modes = photo_control_query_enum(controls, PHOTO_CONTROL_FLASH_MODE);
    if (flash_modes) {
        printf("available flash modes:\n");
        if (photo_check_bit(flash_modes, PHOTO_AUTO_FLASH))
            printf("  - auto\n");
        if (photo_check_bit(flash_modes, PHOTO_AUTO_FLASH_REDEYE))
            printf("  - auto with red-eye reduction\n");
        if (photo_check_bit(flash_modes, PHOTO_FLASH_ON))
            printf("  - on\n");
        if (photo_check_bit(flash_modes, PHOTO_FLASH_OFF))
            printf("  - off\n");
        if (photo_check_bit(flash_modes, PHOTO_FLASH_TORCH))
            printf("  - torch\n");
    }

    const PhotoControlFloatRange *lens_focus = photo_control_query_float(controls, PHOTO_CONTROL_LENS_FOCUS);
    if (lens_focus) {
        printf("lens focus range:\n"
               "  min: %f\n"
               "  max: %f\n",
               lens_focus->min, lens_focus->max);
    }

    const PhotoControlFloatRange *zoom = photo_control_query_float(controls, PHOTO_CONTROL_ZOOM);
    if (zoom) {
        printf("zoom range:\n"
               "  min: %f\n"
               "  max: %f\n",
               zoom->min, zoom->max);
    }

    return 0;
}

void print_usage(void)
{
    fprintf(stderr,
            "Usage:\n"
            "  camera-query         - list available cameras\n"
            "  camera-query <id>    - dump camera info\n");
}

int main(int argc, char *argv[])
{
    int ret;

    if (argc == 1) {
        photo_enumerate_cameras(list_single_camera, NULL);
        return 0;
    } else if (argc != 2 || argv[1][0] == '-') {
        // We don't have any options at the moment.
        // Print usage e.g. for --help or when too many arguments are given.
        print_usage();
        return 1;
    }

    PhotoCamera *camera = photo_camera_open(argv[1], NULL, NULL);
    if (camera) {
        ret = dump_camera_info(camera);
        photo_camera_close(camera);
        return ret;
    } else {
        fprintf(stderr, "Failed to open camera\n");
    }

    return 1;
}
