#include "../../simple_stereo_vision_lib.h"

void *ssvl = NULL;

int main(int argc, char* argv[]){

    ssvl = ssvl_create(128, 128, true);

    return 0;
}