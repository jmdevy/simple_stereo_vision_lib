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

// Stateful library, library creates an instance of this
// for the user to store in a void* pointer (user should
// not be aware of the internal state or manipulate it
// directly)
// https://stackoverflow.com/a/78291179
typedef struct cam_nav_t{
    uint32_t pixel_count;                       // Number of pixels in an individual camera
    uint32_t frame_buffer_size;                 // Size, in bytes, of individual frame buffers
    uint32_t depth_buffer_size;                 // Size, in bytes, of the depth buffer

    uint16_t *frame_buffers[2];                 // Frame buffers where raw 16-bit RGB565 frames are stored
    float *depth_buffer;                        // Depth buffer where calculated depths from disparity map are stored

    uint32_t frame_buffers_amounts[2];          // When using `cam_nav_feed(...)`, tracks how much information is stored in corresponding `frame_buffers[...]`

    bool buffers_set;                           // Flag indicating if frame and depth buffers are allocated/set
    bool custom_buffers_set;                    // Flag indicating if frame and depth buffers are memory from outside the library (do not deallocate custom buffers, user's problem)
}cam_nav_t;



// ///////////////////////////////////////////
//         LIBRARY SETUP AND STOPPING
// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv


// Create and initialize the `cam_nav` library - store the returned pointer in your program. Allocates:
//  * 2 16-bit cameras_width*cameras_height frame buffers = 2*2*cameras_width*cameras_height bytes
//  * 1 32-bit/float calculated depth buffer = 4*cameras_width*cameras_height bytes bytes
//
// Set `allocate` to `true` if the library should allocate frame and depth buffers, otherwise, set
// false if you're going to call `cam_nav_set_buffers` to reuse memory you may already have allocated
inline void *cam_nav_create(uint16_t cameras_width, uint16_t cameras_height, bool allocate){
    // Create the library instance for the user to store and re-use
    cam_nav_t *cam_nav = (cam_nav_t*)CAM_NAV_MALLOC(sizeof(cam_nav_t));

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


}


// Copies the incoming `buffer` to internal library frame buffer.
// Returns `true` when:
//  * Fed a buffer chunk but still haven't been provided enough buffers yet to complete the frame
//  * Fed a buffer chunk but reached the end and ended up with exactly enough buffers to complete the frame
// Returns `false` when:
//  * Fed a buffer chunk but the addition of this buffer resulted in too much data needed to complete the frame
//    (User is expected to crop their buffers or incoming `buffer_len`s so as to understand the information they
//     are putting into the library)
inline bool cam_nav_feed(cam_nav_t *cam_nav, cam_nav_camera_side side, uint16_t *buffer, uint32_t buffer_length){
    // Add the additional buffer amount to the count/amount
    cam_nav->frame_buffers_amounts[side] += buffer_length;

    // Check, in bytes, for buffer overflow, reset and return error if true
    if(cam_nav->frame_buffers_amounts[side] > cam_nav->frame_buffer_size){
        cam_nav->frame_buffers_amounts[side] = 0;
        return false;
    }

    // Do the copy to internal fraem buffer
    memcpy(cam_nav->frame_buffers[side], buffer, buffer_length);
    
    // Process frames if both buffers are full
    if(cam_nav->frame_buffers_amounts[CAM_NAV_LEFT_CAMERA] == cam_nav->frame_buffer_size &&
       cam_nav->frame_buffers_amounts[CAM_NAV_RIGHT_CAMERA] == cam_nav->frame_buffer_size){
        cam_nav_process(cam_nav);
    }

    return true;
}


#endif  // CAM_NAV_H