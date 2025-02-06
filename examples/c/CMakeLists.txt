cmake_minimum_required(VERSION 3.22)

project(main)                                                               # Call project `main`


# Download the test stereo images if not already
if(EXISTS ../stereo-pairs.zip)
    message("-- Already downloaded stereo test image pairs")
else()
    # Download the file
    execute_process(
        COMMAND wget -O stereo-pairs.zip https://vision.middlebury.edu/stereo/submit/stereo-pairs.zip
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    # Unzip the file
    execute_process(
        COMMAND unzip stereo-pairs.zip
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()


add_executable(main main.c)                                                 # Sources for executable named `main`
target_include_directories(main PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)   # Include library header for this example
target_include_directories(main PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/stb)     # Include library header for this example
target_link_libraries(main m)                                               # Link standard math C library