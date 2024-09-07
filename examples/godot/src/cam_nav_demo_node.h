#ifndef CAM_NAV_DEMO_NODE_H
#define CAM_NAV_DEMO_NODE_H


#include <godot_cpp/classes/character_body3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/sub_viewport_container.hpp>
#include <godot_cpp/classes/sub_viewport.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/viewport_texture.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include "../../../cam_nav.h"

using namespace godot;

class CamNavDemoNode : public CharacterBody3D{
    GDCLASS(CamNavDemoNode, CharacterBody3D)

private:
    // Each eye has the following hierarchy:
    //  - SubViewportContainer
    //      - SubViewport
    //          - Camera3D
    SubViewportContainer        *left_viewport_container = nullptr;
    SubViewport                 *left_viewport = nullptr;
    Camera3D                    *left_camera = nullptr;
    godot::Ref<ViewportTexture> left_texture;
    Node3D *left_origin;
	Node3D *left_rotation_origin;

    SubViewportContainer        *right_viewport_container = nullptr;
    SubViewport                 *right_viewport = nullptr;
    Camera3D                    *right_camera = nullptr;
    godot::Ref<ViewportTexture> right_texture;
    Node3D *right_origin;
	Node3D *right_rotation_origin;

    void *cam_nav = nullptr;
protected:
    static void _bind_methods();
public:
    CamNavDemoNode();
    ~CamNavDemoNode();

    // How many units apart are the stereo eye origins
    float baseline = 2.0f;

    void _ready();
	void _process(float delta);
	void _physics_process(float delta);
	void _input(const godot::Ref<InputEvent> &event);

    // Constants related to movement
	float max_speed = 45.0f;
	float jump_speed = 5.0f;
	float acceleration = 6.0f;
	float deacceleration = 10.0f;
	float max_slope_angle = 45.0f;
	float mouse_sensitivity = 0.45f;

    Input *input_handler;
	Vector3 direction;
	Vector3 velocity;
};


#endif  // CAM_NAV_DEMO_NODE_H