#ifndef CAM_NAV_H
#define CAM_NAV_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>


// On some platforms, it is required to store large buffers
// in specific areas, allow that to be defined/custom
#ifndef CAM_NAV_MALLOC
#define CAM_NAV_MALLOC malloc
#endif

#ifndef CAM_NAV_FREE
#define CAM_NAV_FREE free
#endif

// Can be customized. cam_nav may print information when errors occur
#ifndef CAM_NAV_PRINTF
#define CAM_NAV_PRINTF printf
#endif


// Used throughout library to refer to which camera to interact with
typedef enum cam_nav_camera_side_enum {CAM_NAV_LEFT_CAMERA=0, CAM_NAV_RIGHT_CAMERA=1} cam_nav_camera_side;

// Various types of errors set in library instance `.error`
typedef enum cam_nav_status_codes_enum {CAM_NAV_STATUS_OK=0, CAM_NAV_STATUS_FEED_OVERFLOW=1} cam_nav_return_codes;

// Just name `uint8_t` to status for tracking library errors in instance
typedef uint8_t can_nav_status_t;


// Stateful library, library creates an instance of this
// for the user to store in a void* pointer (user should
// not be aware of the internal state or manipulate it
// directly)
// https://stackoverflow.com/a/78291179
typedef struct cam_nav_t{
    uint16_t width;                             // Width resolution of camera
    uint16_t height;                            // height resolution of camera

    float baseline_mm;                          // Distance between cameras on same plane in mm
    float focal_length_mm;                      // Focal length (distance between sensor and lens) in mm

    // Called internally by `cam_nav_process`
    // for each window for comparing pixel
    // blocks on the 1D search line for each
    // eye. Defaults to `SAD` but can be
    // replaced by any algorithm that returns
    // a float where higher values represent
    // a higher similarity between the pixels
    // in the passed window location and
    // dimensions in the original and compare
    // buffers
    float (*aggregate_pixel_comparer)(cam_nav_t *cam_nav,
                                      uint16_t *original_cam_buffer,
                                      uint16_t *compare_cam_buffer,
                                      uint32_t buffer_stride,
                                      uint16_t window_x,
                                      uint16_t window_y,
                                      uint8_t window_dimensions);

    uint32_t pixel_count;                       // Number of pixels in an individual camera
    uint32_t frame_buffer_size;                 // Size, in bytes, of individual frame buffers
    uint32_t depth_buffer_size;                 // Size, in bytes, of the depth buffer

    uint16_t *frame_buffers[2];                 // Frame buffers where raw 16-bit RGB565 frames are stored
    float *depth_buffer;                        // Depth buffer where calculated depths from disparity map are stored

    uint32_t frame_buffers_amounts[2];          // When using `cam_nav_feed(...)`, tracks how much information is stored in corresponding `frame_buffers[...]`

    uint8_t search_window_dimensions;           // When looking for similar pixel blocks, this is the size of the blocks used for comparing. Must be a multiple of

    bool buffers_set;                           // Flag indicating if frame and depth buffers are allocated/set
    bool custom_buffers_set;                    // Flag indicating if frame and depth buffers are memory from outside the library (do not deallocate custom buffers, user's problem)

    can_nav_status_t status_code;               // OK by default since 0 by default but gets set to any error code throughout the library
}cam_nav_t;



// ///////////////////////////////////////////
//         LIBRARY STATUS AND ERROR
// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv

// Used internally to set library status when a error occurs
inline void cam_nav_set_status_code(cam_nav_t *cam_nav, can_nav_status_t status_code){
    cam_nav->status_code = status_code;
}


inline can_nav_status_t cam_nav_get_status_code(cam_nav_t *cam_nav){
    return cam_nav->status_code;
}


// ///////////////////////////////////////////
//   AGGREGATE PIXEL BLOCK COMPARE FUNCTIONS
// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv

float cam_nav_sad_comparer(cam_nav_t *cam_nav, uint16_t *original_cam_buffer, uint16_t *compare_cam_buffer, uint32_t buffer_stride, uint16_t window_x, uint16_t window_y, uint8_t window_dimensions){
    
}


// ///////////////////////////////////////////
//         LIBRARY SETUP AND STOPPING
// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv

// Create and initialize the `cam_nav` library - store the returned pointer in your program. Allocates:
//  * 2 16-bit cameras_width*cameras_height frame buffers = 2*2*cameras_width*cameras_height bytes
//  * 1 32-bit/float calculated depth buffer = 4*cameras_width*cameras_height bytes bytes
//
// Set `allocate` to `true` if the library should allocate frame and depth buffers, otherwise, set
// false if you're going to call `cam_nav_set_buffers` to reuse memory you may already have allocated
inline void *cam_nav_create(uint16_t cameras_width, uint16_t cameras_height, uint8_t search_window_dimensions, float baseline_mm, float focal_length_mm, bool allocate){
    // Create the library instance for the user to store and re-use
    cam_nav_t *cam_nav = (cam_nav_t*)CAM_NAV_MALLOC(sizeof(cam_nav_t));

    // if search window square dimensions are not a multiple of the
    // width or height, return NULL and do not create library instance
    if(cameras_width & search_window_dimensions != 0 || cameras_height % search_window_dimensions != 0){
        return NULL;
    }

    // Track these for later usage
    cam_nav->width = cameras_width;
    cam_nav->height = cameras_height;
    cam_nav->search_window_dimensions = search_window_dimensions;
    cam_nav->baseline_mm == baseline_mm;
    cam_nav->focal_length_mm == focal_length_mm;

    // Set the default algorithm that compares pixel
    // blocks on 1D search line between left and right
    // camera eyes
    cam_nav->aggregate_pixel_comparer = cam_nav_sad_comparer;

    // Calculate number of pixels and elements in frame and depth buffers
    cam_nav->pixel_count = cameras_width*cameras_height;
    cam_nav->frame_buffer_size = cam_nav->pixel_count * sizeof(uint16_t);
    cam_nav->depth_buffer_size = cam_nav->pixel_count * sizeof(float);

    // Stop here if user does not want cam_nav to make buffers
    if(allocate == false){
        return (void*)cam_nav;
    }

    // Allocate space for the individual camera frame buffers
    cam_nav->frame_buffers[0] = (uint16_t*)CAM_NAV_MALLOC(cam_nav->frame_buffer_size);
    cam_nav->frame_buffers[1] = (uint16_t*)CAM_NAV_MALLOC(cam_nav->frame_buffer_size);

    // Allocate space for the depth buffer
    cam_nav->depth_buffer = (float*)CAM_NAV_MALLOC(cam_nav->depth_buffer_size);
    
    // Indicate that the buffers are ready
    // and that these are *not* custom buffers
    // (they should be deallocated by the library then)
    cam_nav->buffers_set = true;
    cam_nav->custom_buffers_set = false;

    return (void*)cam_nav;
}


// If `allocate` was set to `false` in call to `cam_nav_create`, use this function
// to set the 2 frame buffers and 1 depth buffer to custom locations. Returns true
// if set locations successfully, false if not because element count < pixel_count
inline bool cam_nav_set_buffers(cam_nav_t *cam_nav, uint16_t *frame_buffers[], uint32_t frame_buffers_lengths, float *depth_buffer, uint32_t depth_buffer_length){
    // Check that the buffers are long enough to store information for every camera pixel
    if(frame_buffers_lengths < cam_nav->pixel_count || depth_buffer_length < cam_nav->pixel_count){
        return false;
    }

    // Set the buffers to the user's custom locations
    cam_nav->frame_buffers[0] = frame_buffers[0];
    cam_nav->frame_buffers[1] = frame_buffers[1];
    cam_nav->depth_buffer = depth_buffer;

    // Buffers are ready and are custom (not do deallocate on deinit of library)
    cam_nav->buffers_set = true;
    cam_nav->custom_buffers_set = true;
}


// Give back the memory for various buffers
inline void cam_nav_destroy(cam_nav_t *cam_nav){
    // Only deallocate buffers if they are set and not custom
    if(cam_nav->buffers_set == true && cam_nav->custom_buffers_set == false){
        CAM_NAV_FREE(cam_nav->frame_buffers[0]);
        CAM_NAV_FREE(cam_nav->frame_buffers[1]);
        CAM_NAV_FREE(cam_nav->depth_buffer);
    }

    // Always free this since allocated by library
    CAM_NAV_FREE(cam_nav);

    // Reset flags
    cam_nav->buffers_set = false;
    cam_nav->custom_buffers_set = false;
}


// ///////////////////////////////////////////
//          PRIMARY LIBRARY USAGE
// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv

inline bool cam_nav_hog(cam_nav_t *cam_nav){

}


// https://johnwlambert.github.io/stereo/
inline bool cam_nav_depth(cam_nav_t *cam_nav){

}


// https://johnwlambert.github.io/stereo/
inline bool cam_nav_disparity(cam_nav_t *cam_nav){

}


// Left and right camera buffers are full, process them
inline bool cam_nav_process(cam_nav_t *cam_nav){
    // Reset these for next incoming frames after processing
    cam_nav->frame_buffers_amounts[CAM_NAV_LEFT_CAMERA] = 0;
    cam_nav->frame_buffers_amounts[CAM_NAV_RIGHT_CAMERA] = 0;

    return true;
}


// Copies the incoming `buffer` to internal library frame buffer.
// Returns `true` when:
//  * Fed a buffer chunk but still haven't been provided enough buffers yet to complete the frame
//  * Fed a buffer chunk but reached the end and ended up with exactly enough buffers to complete the frame
// Returns `false` when:
//  * Fed a buffer chunk but the addition of this buffer resulted in too much data needed to complete the frame
//    (User is expected to crop their buffers or incoming `buffer_len`s so as to understand the information they
//     are putting into the library)
inline bool cam_nav_feed(void *cam_nav_instance, cam_nav_camera_side side, const uint8_t *buffer_u8, uint32_t buffer_length_u8){
    cam_nav_t *cam_nav = (cam_nav_t*)cam_nav_instance;

    // Add the additional buffer amount to the count/amount
    cam_nav->frame_buffers_amounts[side] += buffer_length_u8;

    // Check, in bytes, for buffer overflow, reset and return error if true
    if(cam_nav->frame_buffers_amounts[side] > cam_nav->frame_buffer_size){
        cam_nav->frame_buffers_amounts[side] = 0;
        cam_nav_set_status_code(cam_nav, CAM_NAV_STATUS_FEED_OVERFLOW);
        return false;
    }

    // Do the copy to the internal frame buffer
    memcpy(cam_nav->frame_buffers[side], buffer_u8, buffer_length_u8);
    
    // Process frames if both buffers are full
    if(cam_nav->frame_buffers_amounts[CAM_NAV_LEFT_CAMERA] == cam_nav->frame_buffer_size &&
       cam_nav->frame_buffers_amounts[CAM_NAV_RIGHT_CAMERA] == cam_nav->frame_buffer_size){
        return cam_nav_process(cam_nav);
    }

    return true;
}


#endif  // CAM_NAV_H