#ifndef SSVL_H
#define SSVL_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <inttypes.h>


// On some platforms, it is required to store large buffers
// in specific areas, allow that to be defined/custom
#ifndef SSVL_MALLOC
#define SSVL_MALLOC malloc
#endif

#ifndef SSVL_FREE
#define SSVL_FREE free
#endif

// Can be customized. ssvl (simple stereo vision lib) may print information when errors occur
#ifndef SSVL_PRINTF
#define SSVL_PRINTF printf
#endif

// Function definition other than the return type
#ifndef SSVL_FUNC
#define SSVL_FUNC extern inline
#endif

// NOTE: Use `#define SSVL_DEBUG` to enable printing, disabled by not being defined by default


// Used throughout library to refer to which camera to interact with
typedef enum ssvl_camera_side_enum {SSVL_LEFT_CAMERA=0, SSVL_RIGHT_CAMERA=1} ssvl_camera_side;

// Various types of errors set in library instance `.error`
typedef enum ssvl_status_codes_enum {SSVL_STATUS_OK=0, SSVL_STATUS_FEED_OVERFLOW=1} ssvl_return_codes;

// Just name `uint8_t` to status for tracking library errors in instance
typedef uint8_t ssvl_status_t;


// Stateful library, library creates an instance of this
// for the user to store in a void* pointer (user should
// not be aware of the internal state or manipulate it
// directly)
// https://stackoverflow.com/a/78291179
typedef struct ssvl_t{
    uint16_t width;                             // Width resolution of camera
    uint16_t height;                            // height resolution of camera

    uint16_t depth_width;                       // Width of depth buffer is width/`search_window_dimensions` cells wide
    uint16_t depth_height;                      // height of depth buffer is height/`search_window_dimensions` cells high

    float baseline_mm;                          // Distance between cameras on same plane in mm
    float field_of_view_degrees;
    float focal_length_pixels;                  // Focal length (distance between sensor and lens) in mm
    float max_depth_mm;

    // Called internally by `ssvl_process`
    // for each window for comparing pixel
    // blocks on the 1D search line for each
    // eye. Defaults to `SAD` but can be
    // replaced by any algorithm that returns
    // a float where higher values represent
    // a higher similarity between the pixels
    // in the passed window location and
    // dimensions in the original and compare
    // buffers
    uint32_t (*aggregate_pixel_comparer)(struct ssvl_t *ssvl,
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
    uint32_t disparity_depth_buffer_size;       // Size, in bytes, of the depth buffer

    uint16_t *frame_buffers[2];                 // Frame buffers where raw 16-bit RGB565 frames are stored
    float *disparity_depth_buffer;              // Depth buffer where calculated depths from disparity map are stored

    uint32_t frame_buffers_amounts[2];          // When using `ssvl_feed(...)`, tracks how much information is stored in corresponding `frame_buffers[...]`

    uint8_t search_window_dimensions;           // When looking for similar pixel blocks, this is the size of the blocks used for comparing. Must be a multiple of

    bool buffers_set;                           // Flag indicating if frame and depth buffers are allocated/set
    bool custom_buffers_set;                    // Flag indicating if frame and depth buffers are memory from outside the library (do not deallocate custom buffers, user's problem)

    void *grayscale_opaque_ptr;
    void (*on_grayscale_cb)(void *grayscale_opaque_ptr, ssvl_camera_side side, uint16_t *grayscale_frame_buffer, uint16_t pixel_width, uint16_t pixel_height);

    void *disparity_opaque_ptr;
    void (*on_disparity_cb)(void *disparity_opaque_ptr, float *disparity_buffer, uint16_t disparity_width, uint16_t disparity_height);

    void *depth_opaque_ptr;
    void (*on_depth_cb)(void *depth_opaque_ptr, float *disparity_depth_buffer, uint16_t depth_width, uint16_t depth_height, float max_depth_mm);

    ssvl_status_t status_code;               // OK by default since 0 by default but gets set to any error code throughout the library
}ssvl_t;


// ///////////////////////////////////////////
//         LIBRARY STATUS AND ERROR
// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv

// Used internally to set library status when a error occurs
SSVL_FUNC void ssvl_set_status_code(ssvl_t *ssvl, ssvl_status_t status_code){
    ssvl->status_code = status_code;
}


SSVL_FUNC ssvl_status_t ssvl_get_status_code(ssvl_t *ssvl){
    return ssvl->status_code;
}


// ///////////////////////////////////////////
//   AGGREGATE PIXEL BLOCK COMPARE FUNCTIONS
// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv

// https://johnwlambert.github.io/stereo/
SSVL_FUNC uint32_t ssvl_sad_comparer(ssvl_t *ssvl, uint16_t *original_cam_buffer, uint16_t *compare_cam_buffer,
                                               uint16_t original_window_x, uint16_t original_window_y,
                                               uint16_t compare_window_x, uint16_t compare_window_y,
                                               uint8_t window_dimensions){
    
    // SAD
    uint32_t sad = 0;

    for(uint16_t y=0; y<window_dimensions; y++){
        for(uint16_t x=0; x<window_dimensions; x++){
            int32_t original_sample = (int32_t)original_cam_buffer[(original_window_y+y)*ssvl->width + (original_window_x+x)];
            int32_t compare_sample = (int32_t)compare_cam_buffer[(compare_window_y+y)*ssvl->width + (compare_window_x+x)];

            sad += abs(original_sample - compare_sample);
        }
    }

    return sad;
}


// ///////////////////////////////////////////
//         LIBRARY SETUP AND STOPPING
// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv

// Initialize the `ssvl` library - store the returned pointer in your program. Allocates:
//  * 2 16-bit cameras_width*cameras_height frame buffers = 2*2*cameras_width*cameras_height bytes
//  * 1 32-bit/float calculated depth buffer = 4*cameras_width*cameras_height bytes bytes
//
// Set `allocate` to `true` if the library should allocate frame and depth buffers, otherwise, set
// false if you're going to call `ssvl_set_buffers` to reuse memory you may already have allocated
SSVL_FUNC void ssvl_init(ssvl_t *ssvl, uint16_t cameras_width, uint16_t cameras_height, uint8_t search_window_dimensions, float baseline_mm, float fov_degrees, bool allocate){
    // if search window square dimensions are not a multiple of the
    // width or height, return NULL and do not create library instance
    if(cameras_width & search_window_dimensions != 0 || cameras_height % search_window_dimensions != 0){
        return;
    }

    // Track these for later usage
    ssvl->width = cameras_width;
    ssvl->height = cameras_height;
    ssvl->search_window_dimensions = search_window_dimensions;
    ssvl->baseline_mm = baseline_mm;
    ssvl->field_of_view_degrees = fov_degrees;

    // https://answers.opencv.org/question/17076/conversion-focal-distance-from-mm-to-pixels/
    // https://gamedev.stackexchange.com/questions/166993/interpreting-focal-length-in-units-of-pixels
    // https://computergraphics.stackexchange.com/questions/10593/is-focal-length-equal-to-the-distance-from-the-optical-center-to-the-near-clippi
    ssvl->focal_length_pixels = ((float)ssvl->width*0.5f) / tanf(fov_degrees * 0.5f * 3.141593f/180.0f);

    // https://stackoverflow.com/a/19423059
    // https://stackoverflow.com/a/75745742
    ssvl->max_depth_mm = ssvl->focal_length_pixels * ssvl->baseline_mm;

    ssvl->grayscale_opaque_ptr = NULL;
    ssvl->on_grayscale_cb = NULL;

    ssvl->disparity_opaque_ptr = NULL;
    ssvl->on_disparity_cb = NULL;

    ssvl->depth_opaque_ptr = NULL;
    ssvl->on_depth_cb = NULL;

    // Depth resolution is only as good as the search window
    ssvl->depth_width = ssvl->width / ssvl->search_window_dimensions;
    ssvl->depth_height = ssvl->height / ssvl->search_window_dimensions;
    ssvl->depth_cell_count = ssvl->depth_width * ssvl->depth_height;

    // Set the default algorithm that compares pixel
    // blocks on 1D search line between left and right
    // camera eyes
    ssvl->aggregate_pixel_comparer = ssvl_sad_comparer;

    // Calculate number of pixels and elements in frame and depth buffers
    ssvl->pixel_count = cameras_width*cameras_height;
    ssvl->frame_buffer_size = ssvl->pixel_count * sizeof(uint16_t);
    ssvl->disparity_depth_buffer_size = ssvl->depth_cell_count * sizeof(float);

    // Stop here if user does not want ssvl to make buffers
    if(allocate == false){
        return;
    }

    // Allocate space for the individual camera frame buffers
    ssvl->frame_buffers[SSVL_LEFT_CAMERA] = (uint16_t*)SSVL_MALLOC(ssvl->frame_buffer_size);
    ssvl->frame_buffers[SSVL_RIGHT_CAMERA] = (uint16_t*)SSVL_MALLOC(ssvl->frame_buffer_size);
    ssvl->frame_buffers_amounts[SSVL_LEFT_CAMERA] = 0;
    ssvl->frame_buffers_amounts[SSVL_RIGHT_CAMERA] = 0;

    // Allocate space for the depth buffer
    ssvl->disparity_depth_buffer = (float*)SSVL_MALLOC(ssvl->disparity_depth_buffer_size);
    
    // Indicate that the buffers are ready
    // and that these are *not* custom buffers
    // (they should be deallocated by the library then)
    ssvl->buffers_set = true;
    ssvl->custom_buffers_set = false;

    #if defined(SSVL_DEBUG)
        SSVL_PRINTF("SSVL:");
        SSVL_PRINTF("\t width (pixels): \t\t\t\t\t\t%d\n", ssvl->width);
        SSVL_PRINTF("\t height (pixels): \t\t\t\t\t\t%d\n", ssvl->height);
        SSVL_PRINTF("\t search_window_dimensions (pixels): \t\t\t\t%d\n", ssvl->search_window_dimensions);
        SSVL_PRINTF("\t baseline (mm): \t\t\t\t\t\t%0.3f\n", ssvl->baseline_mm);
        SSVL_PRINTF("\t FOV (degrees): \t\t\t\t\t\t%0.3f\n", ssvl->field_of_view_degrees);
        SSVL_PRINTF("\t focal length (pixels): \t\t\t\t\t%0.3f\n", ssvl->focal_length_pixels);
        SSVL_PRINTF("\t max depth (mm): \t\t\t\t\t\t%0.3f\n", ssvl->max_depth_mm);
        SSVL_PRINTF("\t max depth (m): \t\t\t\t\t\t%0.3f\n", ssvl->max_depth_mm/1000.0f);
        SSVL_PRINTF("\t frame buffer size (bytes): \t\t\t\t\t%d\n", ssvl->frame_buffer_size);
    #endif
}


// If `allocate` was set to `false` in call to `ssvl_init`, use this function
// to set the 2 frame buffers and 1 depth buffer to custom locations. Returns true
// if set locations successfully, false if not because element count < pixel_count
SSVL_FUNC bool ssvl_set_buffers(ssvl_t *ssvl, uint16_t *frame_buffers[], uint32_t frame_buffers_lengths, float *disparity_depth_buffer, uint32_t disparity_depth_buffer_length){
    // Check that the buffers are long enough to store information for every camera pixel
    if(frame_buffers_lengths < ssvl->pixel_count || disparity_depth_buffer_length < ssvl->pixel_count){
        return false;
    }

    // Set the buffers to the user's custom locations
    ssvl->frame_buffers[0] = frame_buffers[0];
    ssvl->frame_buffers[1] = frame_buffers[1];
    ssvl->disparity_depth_buffer = disparity_depth_buffer;

    // Buffers are ready and are custom (not do deallocate on deinit of library)
    ssvl->buffers_set = true;
    ssvl->custom_buffers_set = true;
}


SSVL_FUNC void ssvl_set_on_grayscale_cb(ssvl_t *ssvl,
                                        void (*on_grayscale_cb)(void *grayscale_opaque_ptr, ssvl_camera_side side, uint16_t *grayscale_frame_buffer, uint16_t pixel_width, uint16_t pixel_height),
                                        void *grayscale_opaque_ptr){
    ssvl->on_grayscale_cb = on_grayscale_cb;
    ssvl->grayscale_opaque_ptr = grayscale_opaque_ptr;
}


SSVL_FUNC void ssvl_set_on_disparity_cb(ssvl_t *ssvl,
                                        void (*on_disparity_cb)(void *disparity_opaque_ptr, float *disparity_buffer, uint16_t disparity_width, uint16_t disparity_height),
                                        void *disparity_opaque_ptr){
    ssvl->on_disparity_cb = on_disparity_cb;
    ssvl->disparity_opaque_ptr = disparity_opaque_ptr;
}


SSVL_FUNC void ssvl_set_on_depth_cb(ssvl_t *ssvl,
                                    void (*on_depth_cb)(void *depth_opaque_ptr, float *disparity_depth_buffer, uint16_t depth_width, uint16_t depth_height, float max_depth_mm),
                                    void *depth_opaque_ptr){
    ssvl->on_depth_cb = on_depth_cb;
    ssvl->depth_opaque_ptr = depth_opaque_ptr;
}


// Give back the memory for various buffers,
// does not deallocate `ssvl_t` structure
SSVL_FUNC void ssvl_destroy(ssvl_t *ssvl){
    // Only deallocate buffers if they are set and not custom
    if(ssvl->buffers_set == true && ssvl->custom_buffers_set == false){
        SSVL_FREE(ssvl->frame_buffers[0]);
        SSVL_FREE(ssvl->frame_buffers[1]);
        SSVL_FREE(ssvl->disparity_depth_buffer);
    }

    // Reset flags
    ssvl->buffers_set = false;
    ssvl->custom_buffers_set = false;
}


// ///////////////////////////////////////////
//          PRIMARY LIBRARY USAGE
// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv

// Converts 16-bit RGB565 to 16-bit grayscale
// https://en.wikipedia.org/wiki/Grayscale#:~:text=Ylinear%2C-,which%20is%20given%20by,-%5B6%5D
SSVL_FUNC void ssvl_convert_rgb565_to_grayscale(uint16_t *buffer, uint32_t pixel_count){
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


SSVL_FUNC uint16_t ssvl_disparity_search(ssvl_t *ssvl, uint16_t left_cell_x, uint16_t left_cell_y){
    // Starting from the same location in the right eye as the left eye,
    // move window from right to left by a single pixel position amount
    // starting at position from left eye
    const int32_t starting_x = left_cell_x * ssvl->search_window_dimensions;
    const uint16_t starting_y = left_cell_y * ssvl->search_window_dimensions;

    int32_t most_similar_x = starting_x;
    const uint16_t most_similar_y = starting_y;
    uint32_t smallest_difference = UINT32_MAX;

    for(int32_t right_x=starting_x; right_x>=0 && right_x<=starting_x; right_x--){
        uint32_t current_difference = ssvl->aggregate_pixel_comparer(ssvl,
                                                        ssvl->frame_buffers[SSVL_LEFT_CAMERA],
                                                        ssvl->frame_buffers[SSVL_RIGHT_CAMERA],
                                                        starting_x,
                                                        starting_y,
                                                        right_x,
                                                        starting_y,
                                                        ssvl->search_window_dimensions);
        
        if(current_difference < smallest_difference){
            smallest_difference = current_difference;
            most_similar_x = right_x;
        }
    }

    // Now that we have the block X with the smallest difference to the one
    // in the left eye, calculate difference in X (disparity)
    uint16_t disparity = abs(starting_x - most_similar_x);

    return disparity;
}


SSVL_FUNC void ssvl_calculate_depth(ssvl_t *ssvl){
    for(int32_t y=0; y<ssvl->depth_height; y++){
        for(int32_t x=0; x<ssvl->depth_width; x++){

            // Get the disparity and assign max depth if disparity close to zero
            float disparity = ssvl->disparity_depth_buffer[y*ssvl->depth_width + x];
            if(disparity >= 1.0f && disparity < ssvl->width){
                // Depth = focal_length_pixels * base_line_mm / disparity_pixels
                ssvl->disparity_depth_buffer[y*ssvl->depth_width + x] = (ssvl->focal_length_pixels * ssvl->baseline_mm / disparity);
            }else{
                ssvl->disparity_depth_buffer[y*ssvl->depth_width + x] = ssvl->max_depth_mm;
            }

        }
    }
}


// Left and right camera buffers are full, process them
SSVL_FUNC bool ssvl_process(ssvl_t *ssvl){
    // We have both frames from both cameras, need to go through
    // and calculate disparity for each pixel block and then the
    // depth for each pixel block

    // Reset these for next incoming frames after processing
    ssvl->frame_buffers_amounts[SSVL_LEFT_CAMERA] = 0;
    ssvl->frame_buffers_amounts[SSVL_RIGHT_CAMERA] = 0;

    // Before relating blocks between left and right eyes, 
    // change the 3 component pixels to single component
    // linear values of intensity, grayscale
    ssvl_convert_rgb565_to_grayscale(ssvl->frame_buffers[SSVL_LEFT_CAMERA], ssvl->pixel_count);
    ssvl_convert_rgb565_to_grayscale(ssvl->frame_buffers[SSVL_RIGHT_CAMERA], ssvl->pixel_count);

    if(ssvl->on_grayscale_cb != NULL) ssvl->on_grayscale_cb(ssvl->grayscale_opaque_ptr, SSVL_LEFT_CAMERA, ssvl->frame_buffers[SSVL_LEFT_CAMERA], ssvl->width, ssvl->height);
    if(ssvl->on_grayscale_cb != NULL) ssvl->on_grayscale_cb(ssvl->grayscale_opaque_ptr, SSVL_RIGHT_CAMERA, ssvl->frame_buffers[SSVL_RIGHT_CAMERA], ssvl->width, ssvl->height);

    for(int32_t left_cell_y=0; left_cell_y<ssvl->depth_height; left_cell_y++){
        for(int32_t left_cell_x=0; left_cell_x<ssvl->depth_width; left_cell_x++){

            uint16_t disparity = ssvl_disparity_search(ssvl, left_cell_x, left_cell_y);
            ssvl->disparity_depth_buffer[left_cell_y*ssvl->depth_width + left_cell_x] = (float)disparity;
        }
    }

    if(ssvl->on_disparity_cb != NULL) ssvl->on_disparity_cb(ssvl->disparity_opaque_ptr, ssvl->disparity_depth_buffer, ssvl->depth_width, ssvl->depth_height);

    ssvl_calculate_depth(ssvl);

    if(ssvl->on_depth_cb != NULL) ssvl->on_depth_cb(ssvl->depth_opaque_ptr, ssvl->disparity_depth_buffer, ssvl->depth_width, ssvl->depth_height, ssvl->max_depth_mm);

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
SSVL_FUNC bool ssvl_feed(ssvl_t *ssvl, ssvl_camera_side side, const uint8_t *buffer, uint32_t buffer_length){
    // Add the additional buffer amount to the count/amount
    ssvl->frame_buffers_amounts[side] += buffer_length;

    // Check, in bytes, for buffer overflow, reset and return error if true
    if(ssvl->frame_buffers_amounts[side] > ssvl->frame_buffer_size){
        ssvl->frame_buffers_amounts[side] = 0;
        ssvl_set_status_code(ssvl, SSVL_STATUS_FEED_OVERFLOW);
        return false;
    }

    // Do the copy to the internal frame buffer
    memcpy(ssvl->frame_buffers[side], buffer, buffer_length);
    
    // Process frames if both buffers are full
    if(ssvl->frame_buffers_amounts[SSVL_LEFT_CAMERA] == ssvl->frame_buffer_size &&
       ssvl->frame_buffers_amounts[SSVL_RIGHT_CAMERA] == ssvl->frame_buffer_size){

        #if defined(SSVL_DEBUG)
            SSVL_PRINTF("PROCESSING\n");
        #endif

        return ssvl_process(ssvl);
    }

    return true;
}


SSVL_FUNC float ssvl_get_max_depth_mm(ssvl_t *ssvl){
    return ssvl->max_depth_mm;
}


#endif  // SSVL_H