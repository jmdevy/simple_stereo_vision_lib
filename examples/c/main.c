#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"

#define SSVL_DEBUG
#include "ssvl.h"

#include <stdio.h>
#include <fcntl.h> 
#include <unistd.h>

uint32_t convert_RGB888_RGB565(stbi_uc *image24bit, int width, int height, int channels){
    uint16_t *image16bit = (uint16_t*)(image24bit);

    uint32_t pixel_count = width * height;

    for(uint32_t ipx=0; ipx<pixel_count; ipx++){
        const uint32_t pixel_channel_index = ipx * channels;

        uint8_t r = image24bit[pixel_channel_index];
        uint8_t g = image24bit[pixel_channel_index+1];
        uint8_t b = image24bit[pixel_channel_index+2];

        r = r >> 3;
        g = g >> 2;
        b = b >> 3;

        uint16_t rgb565_pixel = 0;
        rgb565_pixel |= (r << 11);
        rgb565_pixel |= (g << 5);
        rgb565_pixel |= (b << 0);
        image16bit[ipx] = rgb565_pixel;
    }

    // Return the byte size
    return pixel_count * 2;
}

int write_pgm(float *image_gray, uint16_t width, uint16_t height){
    int fd = open("output.pgm", O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if (fd == -1) {
        perror("could not open file");
        return EXIT_FAILURE;
    }

    char buffer[7] = { 0 };
    int size = 0;

    // Write the header
    write(fd, "P2\n", 3);

    size = snprintf(buffer, 7, "%d ", width);
    write(fd, buffer, size);

    size = snprintf(buffer, 7, "%d\n", height);
    write(fd, buffer, size);

    size = snprintf(buffer, 7, "%d\n", UINT16_MAX);
    write(fd, buffer, size);

    for(uint16_t y=0; y<height; y++){
        for(uint16_t x=0; x<width; x++){
            uint32_t index = y * width + x;
            const uint16_t gray_pixel = image_gray[index] * UINT16_MAX;
            size = snprintf(buffer, 7, "%d ", gray_pixel);
            write(fd, buffer, size);
        }

        write(fd, "\n", 1);
    }

    // Close the file
    if (close(fd) == -1) {
        perror("could not close file");
        return EXIT_FAILURE;
    }
}

void on_grayscale_cb(void *grayscale_opaque_ptr, ssvl_camera_side side, uint16_t *grayscale_frame_buffer, uint16_t pixel_width, uint16_t pixel_height){
    printf("TEST0\n");
}

void on_disparity_cb(void *disparity_opaque_ptr, float *disparity_buffer, uint16_t disparity_width, uint16_t disparity_height){
    printf("TEST1\n");
}

void on_depth_cb(void *depth_opaque_ptr, float *disparity_depth_buffer, uint16_t depth_width, uint16_t depth_height, float max_depth_mm){
    printf("TEST2\n");

    write_pgm(disparity_depth_buffer, depth_width, depth_height);
}


int main(int argc, char* argv[]){
    int lwidth, lheight, lchannels;
    int rwidth, rheight, rchannels;

    // Assuming we're in the examples/c/build directory, go back one directory
    stbi_uc *limage = stbi_load("../stereo-pairs/tsukuba/imL.png", &lwidth, &lheight, &lchannels, 3);
    stbi_uc *rimage = stbi_load("../stereo-pairs/tsukuba/imR.png", &rwidth, &rheight, &rchannels, 3);

    printf("lwidth: %d\n", lwidth);
    printf("lheight: %d\n", lheight);
    printf("lchannels: %d\n", lchannels);

    printf("rwidth: %d\n", rwidth);
    printf("rheight: %d\n", rheight);
    printf("rchannels: %d\n", rchannels);

    uint32_t limage16bit_size = convert_RGB888_RGB565(limage, lwidth, lheight, lchannels);
    uint32_t rimage16bit_size = convert_RGB888_RGB565(rimage, rwidth, rheight, rchannels);

    printf("limage16bit_size: %d\n", limage16bit_size);
    printf("rimage16bit_size: %d\n", rimage16bit_size);

    ssvl_t ssvl;
    ssvl_init(&ssvl, lwidth, lheight, 4, 10.0, 70.0, true);

    ssvl_set_on_grayscale_cb(&ssvl, on_grayscale_cb, NULL);
    ssvl_set_on_disparity_cb(&ssvl, on_disparity_cb, NULL);
    ssvl_set_on_depth_cb(&ssvl, on_depth_cb, NULL);

    if(!ssvl_feed(&ssvl, SSVL_LEFT_CAMERA, limage, limage16bit_size)){
        printf("ERROR: %d\n", ssvl_get_status_code(&ssvl));
    }

    if(!ssvl_feed(&ssvl, SSVL_RIGHT_CAMERA, rimage, rimage16bit_size)){
        printf("ERROR: %d\n", ssvl_get_status_code(&ssvl));
    }

    stbi_image_free(limage);
    stbi_image_free(rimage);

    return 0;
}