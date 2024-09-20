#ifndef CAM_NAV_H
#define CAM_NAV_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <float.h>


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

    uint16_t depth_width;                       // Width of depth buffer is width/`search_window_dimensions` cells wide
    uint16_t depth_height;                      // height of depth buffer is height/`search_window_dimensions` cells high

    float baseline_mm;                          // Distance between cameras on same plane in mm
    float field_of_view_degrees;
    float focal_length_pixels;                  // Focal length (distance between sensor and lens) in mm
    float max_depth_mm;

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
    uint32_t (*aggregate_pixel_comparer)(cam_nav_t *cam_nav,
                                      uint16_t *original_cam_buffer,
                                      uint16_t *compare_cam_buffer,
                                      uint16_t original_window_x,
                                      uint16_t original_window_y,
                                      uint16_t compare_window_x,
                                      uint16_t compare_window_y,
                                      uint8_t window_dimensions);

    uint32_t pixel_count;                       // Number of pixels in an individual camera
    uint32_t depth_cell_count;                  // Number of depth cells total after search window subdivision
    uint32_t frame_buffer_size;                 // Size, in bytes, of individual frame buffers
    uint32_t depth_buffer_size;                 // Size, in bytes, of the depth buffer

    uint16_t *frame_buffers[2];                 // Frame buffers where raw 16-bit RGB565 frames are stored
    float *depth_buffer;                        // Depth buffer where calculated depths from disparity map are stored

    uint32_t frame_buffers_amounts[2];          // When using `cam_nav_feed(...)`, tracks how much information is stored in corresponding `frame_buffers[...]`

    uint8_t search_window_dimensions;           // When looking for similar pixel blocks, this is the size of the blocks used for comparing. Must be a multiple of

    bool buffers_set;                           // Flag indicating if frame and depth buffers are allocated/set
    bool custom_buffers_set;                    // Flag indicating if frame and depth buffers are memory from outside the library (do not deallocate custom buffers, user's problem)

    void *grayscale_opaque_ptr;
    void (*on_grayscale_cb)(void *grayscale_opaque_ptr, cam_nav_camera_side side, uint16_t *grayscale_frame_buffer, uint16_t pixel_width, uint16_t pixel_height);

    void *disparity_opaque_ptr;
    void (*on_disparity_cb)(void *disparity_opaque_ptr, float *disparity_buffer, uint16_t disparity_width, uint16_t disparity_height);

    void *depth_opaque_ptr;
    void (*on_depth_cb)(void *depth_opaque_ptr, float *depth_buffer, uint16_t depth_width, uint16_t depth_height, float max_depth_mm);

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

// https://johnwlambert.github.io/stereo/
inline uint32_t cam_nav_sad_comparer(cam_nav_t *cam_nav, uint16_t *original_cam_buffer, uint16_t *compare_cam_buffer,
                                               uint16_t original_window_x, uint16_t original_window_y,
                                               uint16_t compare_window_x, uint16_t compare_window_y,
                                               uint8_t window_dimensions){
    
    // SAD
    uint32_t sad = 0;

    for(uint16_t y=0; y<window_dimensions; y++){
        for(uint16_t x=0; x<window_dimensions; x++){
            int32_t original_sample = (int32_t)original_cam_buffer[(original_window_y+y)*cam_nav->width + (original_window_x+x)];
            int32_t compare_sample = (int32_t)compare_cam_buffer[(compare_window_y+y)*cam_nav->width + (compare_window_x+x)];

            sad += abs(original_sample - compare_sample);
        }
    }

    return sad;
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
inline cam_nav_t *cam_nav_create(uint16_t cameras_width, uint16_t cameras_height, uint8_t search_window_dimensions, float baseline_mm, float fov_degrees, bool allocate){
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
    cam_nav->baseline_mm = baseline_mm;
    cam_nav->field_of_view_degrees = fov_degrees;

    // https://answers.opencv.org/question/17076/conversion-focal-distance-from-mm-to-pixels/
    // https://gamedev.stackexchange.com/questions/166993/interpreting-focal-length-in-units-of-pixels
    // https://computergraphics.stackexchange.com/questions/10593/is-focal-length-equal-to-the-distance-from-the-optical-center-to-the-near-clippi
    cam_nav->focal_length_pixels = ((float)cam_nav->width*0.5f) / tanf(fov_degrees * 0.5f * 3.141593f/180.0f);

    // https://stackoverflow.com/a/19423059
    cam_nav->max_depth_mm = cam_nav->focal_length_pixels * (cam_nav->baseline_mm / 1.0f);

    cam_nav->grayscale_opaque_ptr = NULL;
    cam_nav->on_grayscale_cb = NULL;

    cam_nav->disparity_opaque_ptr = NULL;
    cam_nav->on_disparity_cb = NULL;

    cam_nav->depth_opaque_ptr = NULL;
    cam_nav->on_depth_cb = NULL;

    // Depth resolution is only as good as the search window
    cam_nav->depth_width = cam_nav->width / cam_nav->search_window_dimensions;
    cam_nav->depth_height = cam_nav->height / cam_nav->search_window_dimensions;
    cam_nav->depth_cell_count = cam_nav->depth_width * cam_nav->depth_height;

    // Set the default algorithm that compares pixel
    // blocks on 1D search line between left and right
    // camera eyes
    cam_nav->aggregate_pixel_comparer = cam_nav_sad_comparer;

    // Calculate number of pixels and elements in frame and depth buffers
    cam_nav->pixel_count = cameras_width*cameras_height;
    cam_nav->frame_buffer_size = cam_nav->pixel_count * sizeof(uint16_t);
    cam_nav->depth_buffer_size = cam_nav->depth_cell_count * sizeof(float);

    // Stop here if user does not want cam_nav to make buffers
    if(allocate == false){
        return cam_nav;
    }

    // Allocate space for the individual camera frame buffers
    cam_nav->frame_buffers[CAM_NAV_LEFT_CAMERA] = (uint16_t*)CAM_NAV_MALLOC(cam_nav->frame_buffer_size);
    cam_nav->frame_buffers[CAM_NAV_RIGHT_CAMERA] = (uint16_t*)CAM_NAV_MALLOC(cam_nav->frame_buffer_size);

    // Allocate space for the depth buffer
    cam_nav->depth_buffer = (float*)CAM_NAV_MALLOC(cam_nav->depth_buffer_size);
    
    // Indicate that the buffers are ready
    // and that these are *not* custom buffers
    // (they should be deallocated by the library then)
    cam_nav->buffers_set = true;
    cam_nav->custom_buffers_set = false;

    CAM_NAV_PRINTF("CAM_NAV:");
    CAM_NAV_PRINTF("\t width (pixels): \t\t\t\t\t\t%d", cam_nav->width);
    CAM_NAV_PRINTF("\t height (pixels): \t\t\t\t\t\t%d", cam_nav->height);
    CAM_NAV_PRINTF("\t search_window_dimensions (pixels): \t%d", cam_nav->search_window_dimensions);
    CAM_NAV_PRINTF("\t baseline (mm): \t\t\t\t\t\t%0.3f", cam_nav->baseline_mm);
    CAM_NAV_PRINTF("\t FOV (degrees): \t\t\t\t\t\t%0.3f", cam_nav->field_of_view_degrees);
    CAM_NAV_PRINTF("\t focal length (pixels): \t\t\t\t%0.3f", cam_nav->focal_length_pixels);
    CAM_NAV_PRINTF("\t max depth (mm): \t\t\t\t\t\t%0.3f", cam_nav->max_depth_mm);
    CAM_NAV_PRINTF("\t max depth (m): \t\t\t\t\t\t%0.3f", cam_nav->max_depth_mm/1000.0f);

    return cam_nav;
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


inline void cam_nav_set_on_grayscale_cb(cam_nav_t *cam_nav,
                                        void (*on_grayscale_cb)(void *grayscale_opaque_ptr, cam_nav_camera_side side, uint16_t *grayscale_frame_buffer, uint16_t pixel_width, uint16_t pixel_height),
                                        void *grayscale_opaque_ptr){
    cam_nav->on_grayscale_cb = on_grayscale_cb;
    cam_nav->grayscale_opaque_ptr = grayscale_opaque_ptr;
}


inline void cam_nav_set_on_disparity_cb(cam_nav_t *cam_nav,
                                        void (*on_disparity_cb)(void *disparity_opaque_ptr, float *disparity_buffer, uint16_t disparity_width, uint16_t disparity_height),
                                        void *disparity_opaque_ptr){
    cam_nav->on_disparity_cb = on_disparity_cb;
    cam_nav->disparity_opaque_ptr = disparity_opaque_ptr;
}


inline void cam_nav_set_on_depth_cb(cam_nav_t *cam_nav,
                                    void (*on_depth_cb)(void *depth_opaque_ptr, float *depth_buffer, uint16_t depth_width, uint16_t depth_height, float max_depth_mm),
                                    void *depth_opaque_ptr){
    cam_nav->on_depth_cb = on_depth_cb;
    cam_nav->depth_opaque_ptr = depth_opaque_ptr;
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

// Converts 16-bit RGB565 to 16-bit grayscale
// https://en.wikipedia.org/wiki/Grayscale#:~:text=Ylinear%2C-,which%20is%20given%20by,-%5B6%5D
inline void cam_nav_convert_rgb656_to_grayscale(uint16_t *buffer, uint32_t pixel_count){
    // Masks for bits for each color channel
    const uint16_t r_mask = 0b1111100000000000;
    const uint16_t g_mask = 0b0000011111100000;
    const uint16_t b_mask = 0b0000000000011111;

    // Amount to shift right to get bits as normal byte int
    const uint8_t r_shift_amount = 11;
    const uint8_t g_shift_amount = 5;
    const uint8_t b_shift_amount = 0;

    // Total magnitude of each color channel is when all bits
    // in the channel are 1, which happens to be the mask
    const uint16_t r_total_magnitude = r_mask >> r_shift_amount;
    const uint16_t g_total_magnitude = g_mask >> g_shift_amount;
    const uint16_t b_total_magnitude = b_mask >> b_shift_amount;

    for(uint32_t i=0; i<pixel_count; i++){
        // Extract the color channel bits and shift all the way right
        uint16_t r = (buffer[i] & r_mask) >> r_shift_amount;
        uint16_t g = (buffer[i] & g_mask) >> g_shift_amount;
        uint16_t b = (buffer[i] & b_mask) >> b_shift_amount;

        // Normalize each channel to 0.0 ~ 1.0
        float r_normal = (float)r / (float)r_total_magnitude;
        float g_normal = (float)g / (float)g_total_magnitude;
        float b_normal = (float)b / (float)b_total_magnitude;

        // https://en.wikipedia.org/wiki/Grayscale#:~:text=Ylinear%2C-,which%20is%20given%20by,-%5B6%5D
        // 0.0 ~ 1.0
        float luminance = 0.2126*r_normal + 0.7152*g_normal + 0.07122*b_normal;

        // Put the linear value back into the buffer for
        // disparity pixel block comparisons as a int
        // with 16-bits for resolution
        buffer[i] = (uint16_t)(luminance * (float)UINT16_MAX);
    }
}


inline uint16_t cam_nav_disparity_search(cam_nav_t *cam_nav, uint16_t left_cell_x, uint16_t left_cell_y){
    // Starting from the same location in the right eye as the left eye,
    // move window from right to left by a single pixel position amount
    // starting at position from left eye
    const uint16_t starting_x = left_cell_x * cam_nav->search_window_dimensions;
    const uint16_t starting_y = left_cell_y * cam_nav->search_window_dimensions;

    uint16_t most_similar_x = starting_x;
    const uint16_t most_similar_y = starting_y;
    uint32_t smallest_difference = UINT32_MAX;

    for(int32_t right_x=starting_x; right_x>=0 && right_x<=starting_x; right_x--){
        uint32_t current_difference = cam_nav->aggregate_pixel_comparer(cam_nav,
                                                        cam_nav->frame_buffers[CAM_NAV_LEFT_CAMERA],
                                                        cam_nav->frame_buffers[CAM_NAV_RIGHT_CAMERA],
                                                        starting_x,
                                                        starting_y,
                                                        right_x,
                                                        starting_y,
                                                        cam_nav->search_window_dimensions);
        
        if(current_difference < smallest_difference){
            smallest_difference = current_difference;
            most_similar_x = right_x;
        }
    }

    // Now that we have the block X with the smallest difference to the one
    // in the left eye, calculate difference in X (disparity)
    return abs(starting_x - most_similar_x);



    // // Starting from same location in right eye, move left in right
    // // eye by a search window amount each time until reach end then
    // // calculate difference in x (disparity) for most similar block's
    // // x
    // uint16_t most_similar_block_x = left_cell_x;
    // uint32_t smallest_difference = UINT32_MAX;

    // int32_t right_cell_y = left_cell_y;
    // for(int32_t right_cell_x=left_cell_x; right_cell_x>=0 && right_cell_x<=left_cell_x; right_cell_x--){
    //     uint32_t current_difference = cam_nav->aggregate_pixel_comparer(cam_nav,
    //                                                     cam_nav->frame_buffers[CAM_NAV_LEFT_CAMERA],
    //                                                     cam_nav->frame_buffers[CAM_NAV_RIGHT_CAMERA],
    //                                                     left_cell_x*cam_nav->search_window_dimensions,
    //                                                     left_cell_y*cam_nav->search_window_dimensions,
    //                                                     right_cell_x*cam_nav->search_window_dimensions,
    //                                                     right_cell_y*cam_nav->search_window_dimensions,
    //                                                     cam_nav->search_window_dimensions);
        
    //     if(current_difference < smallest_difference){
    //         smallest_difference = current_difference;
    //         most_similar_block_x = right_cell_x;
    //     }
    // }

    // // Now that we have the block X with the smallest difference to the one
    // // in the left eye, calculate difference in X (disparity)
    // uint16_t left_abs_x = (float)(left_cell_x*cam_nav->search_window_dimensions);
    // uint16_t most_similar_abs_x = (float)(most_similar_block_x*cam_nav->search_window_dimensions);
    // return abs(left_abs_x - most_similar_abs_x);
}


inline void cam_nav_calculate_depth(cam_nav_t *cam_nav){
    for(int32_t y=0; y<cam_nav->depth_height; y++){
        for(int32_t x=0; x<cam_nav->depth_width; x++){

            float disparity = cam_nav->depth_buffer[y*cam_nav->depth_width + x];
            // if(disparity >= -1e-3 && disparity <= 1e-3){
            //     cam_nav->depth_buffer[y*cam_nav->depth_width + x] = FLT_MAX;
            // }else{
            cam_nav->depth_buffer[y*cam_nav->depth_width + x] = (cam_nav->focal_length_pixels * (cam_nav->baseline_mm / disparity)) / cam_nav->max_depth_mm;
            // }

        }
    }
}


// Left and right camera buffers are full, process them
inline bool cam_nav_process(cam_nav_t *cam_nav){
    // We have both frames from both cameras, need to go through
    // and calculate disparity for each pixel block and then the
    // depth for each pixel block

    // Reset these for next incoming frames after processing
    cam_nav->frame_buffers_amounts[CAM_NAV_LEFT_CAMERA] = 0;
    cam_nav->frame_buffers_amounts[CAM_NAV_RIGHT_CAMERA] = 0;

    // Before relating blocks between left and right eyes, 
    // change the 3 component pixels to single component
    // linear values of intensity, grayscale
    cam_nav_convert_rgb656_to_grayscale(cam_nav->frame_buffers[CAM_NAV_LEFT_CAMERA], cam_nav->pixel_count);
    cam_nav_convert_rgb656_to_grayscale(cam_nav->frame_buffers[CAM_NAV_RIGHT_CAMERA], cam_nav->pixel_count);

    if(cam_nav->on_grayscale_cb != NULL) cam_nav->on_grayscale_cb(cam_nav->grayscale_opaque_ptr, CAM_NAV_LEFT_CAMERA, cam_nav->frame_buffers[CAM_NAV_LEFT_CAMERA], cam_nav->width, cam_nav->height);
    if(cam_nav->on_grayscale_cb != NULL) cam_nav->on_grayscale_cb(cam_nav->grayscale_opaque_ptr, CAM_NAV_RIGHT_CAMERA, cam_nav->frame_buffers[CAM_NAV_RIGHT_CAMERA], cam_nav->width, cam_nav->height);

    for(int32_t left_cell_y=0; left_cell_y<cam_nav->depth_height; left_cell_y++){
        for(int32_t left_cell_x=0; left_cell_x<cam_nav->depth_width; left_cell_x++){

            uint16_t disparity = cam_nav_disparity_search(cam_nav, left_cell_x, left_cell_y);
            cam_nav->depth_buffer[left_cell_y*cam_nav->depth_width + left_cell_x] = (float)disparity;

        }
    }

    if(cam_nav->on_disparity_cb != NULL) cam_nav->on_disparity_cb(cam_nav->disparity_opaque_ptr, cam_nav->depth_buffer, cam_nav->depth_width, cam_nav->depth_height);

    cam_nav_calculate_depth(cam_nav);

    if(cam_nav->on_depth_cb != NULL) cam_nav->on_depth_cb(cam_nav->depth_opaque_ptr, cam_nav->depth_buffer, cam_nav->depth_width, cam_nav->depth_height, cam_nav->max_depth_mm);

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