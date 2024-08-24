#include "../../cam_nav.h"

void *cam_nav = NULL;

int main(int argc, char* argv[]){

    cam_nav = cam_nav_create(128, 128, true);

    return 0;
}