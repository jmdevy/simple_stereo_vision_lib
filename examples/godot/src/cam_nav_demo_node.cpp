#include "cam_nav_demo_node.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

#define CAMERA_RESOLUTION 256

using namespace godot;


void CamNavDemoNode::_bind_methods(){
}


CamNavDemoNode::CamNavDemoNode(){
	
}


void CamNavDemoNode::_ready(){
	if(Engine::get_singleton()->is_editor_hint()){
		return;
	}

	UtilityFunctions::print("Start!");

	// https://www.reddit.com/r/godot/comments/i1e3s6/gdnative_inputsomething_not_working_any_help/fzxaoqf/
	input_handler = Input::get_singleton();
	input_handler->set_mouse_mode(godot::Input::MouseMode::MOUSE_MODE_CAPTURED);

	// Create hierarchy for both eyes
	left_viewport_container = memnew(SubViewportContainer);
	left_viewport 			= memnew(SubViewport);
	left_camera 			= memnew(Camera3D);
	left_origin				= memnew(Node3D);
	left_rotation_origin    = memnew(Node3D);

	right_viewport_container = memnew(SubViewportContainer);
	right_viewport 			 = memnew(SubViewport);
	right_camera 			 = memnew(Camera3D);
	right_origin			 = memnew(Node3D);
	right_rotation_origin	 = memnew(Node3D);

	// Add hierarchy of both eyes
	call_deferred("add_child", left_viewport_container);
	call_deferred("add_child", right_viewport_container);

	left_viewport_container->call_deferred("add_child", left_viewport);
	right_viewport_container->call_deferred("add_child", right_viewport);

	// Set the render resolution of the view ports
	left_viewport->set_size(Vector2(CAMERA_RESOLUTION, CAMERA_RESOLUTION));
	right_viewport->set_size(Vector2(CAMERA_RESOLUTION, CAMERA_RESOLUTION));

	// Adjust visible view ports in window without modifying actual render resolution
	// Ensure view ports are always 512x512 on the monitor
	float scale = 512/left_viewport->get_size().x;
	left_viewport_container->set_scale(Vector2(scale, scale));
	right_viewport_container->set_scale(Vector2(scale, scale));

	// Move right viewport to the right of the left eye
	right_viewport_container->set_position(Vector2(left_viewport->get_size().x * left_viewport_container->get_scale().x, 0));

	UtilityFunctions::print("Viewport sizes:");
	UtilityFunctions::print(left_viewport->get_size());
	UtilityFunctions::print(right_viewport->get_size());

	left_viewport->call_deferred("add_child", left_origin);
	left_origin->call_deferred("add_child", left_rotation_origin);
	left_rotation_origin->call_deferred("add_child", left_camera);

	right_viewport->call_deferred("add_child", right_origin);
	right_origin->call_deferred("add_child", right_rotation_origin);
	right_rotation_origin->call_deferred("add_child", right_camera);

	// Move cameras' baseline distance
	left_camera->set_position(Vector3(-baseline/2.0f, 0.0, 0.0));
	right_camera->set_position(Vector3(baseline/2.0f, 0.0, 0.0));

	// Get textures and images that will be fed into cam_nav every tick
	left_texture = left_viewport->get_texture();
	right_texture = right_viewport->get_texture();

	// Create the cam_nav library instance
	cam_nav = cam_nav_create(CAMERA_RESOLUTION, CAMERA_RESOLUTION, 4, true);

	if(cam_nav == NULL){
		UtilityFunctions::print("ERROR: Could not create cam_nav library! Likely an issue with search window not being a multiple of the width or height of the camera!");
	}
}


CamNavDemoNode::~CamNavDemoNode(){
	// Add your cleanup here.
}


void CamNavDemoNode::_process(float delta){
	if(Engine::get_singleton()->is_editor_hint()){
		return;
	}

	// Get eye images and convert to pixel format cam_nav uses
	godot::Ref<Image> left_image = left_texture.ptr()->get_image();
	godot::Ref<Image> right_image = right_texture.ptr()->get_image();

	left_image.ptr()->convert(Image::FORMAT_RGB565);
	right_image.ptr()->convert(Image::FORMAT_RGB565);

	PackedByteArray left_byte_array = left_image.ptr()->get_data();
	PackedByteArray right_byte_array = right_image.ptr()->get_data();

	if(cam_nav_feed(cam_nav, CAM_NAV_LEFT_CAMERA, left_byte_array.ptr(), left_byte_array.size()) == false){
		UtilityFunctions::print("ERROR: Too much data for left cam_nav eye!");
	}

	if(cam_nav_feed(cam_nav, CAM_NAV_RIGHT_CAMERA, right_byte_array.ptr(), right_byte_array.size()) == false){
		UtilityFunctions::print("ERROR: Too much data for right cam_nav eye!");
	}
}


void CamNavDemoNode::_physics_process(float delta){
	if(Engine::get_singleton()->is_editor_hint()){
		return;
	}

	// https://github.com/GarbajYT/godot_updated_fps_controller/blob/main/FPS_controller_4.0_CSharp/Player.cs
	direction = Vector3(0, 0, 0);

	float head_rotation = get_global_transform().basis.get_euler().y;
	float front_back_input = input_handler->get_action_strength("movement_backward") - input_handler->get_action_strength("movement_forward");
	float side_side_input = input_handler->get_action_strength("movement_right") - input_handler->get_action_strength("movement_left");
	float up_down_input = input_handler->get_action_strength("movement_up") - input_handler->get_action_strength("movement_down");

	direction = Vector3(side_side_input, up_down_input, front_back_input).rotated(Vector3(0, 1, 0), head_rotation).normalized();

	Vector3 hvel = velocity;
	Vector3 target = direction;
	target *= max_speed;

	float accel;
	if(direction.dot(hvel) > 0){
		accel = acceleration;
	}else{
		accel = deacceleration;
	}

	hvel = hvel.lerp(target, accel * delta);
	velocity.x = hvel.x;
	velocity.y = hvel.y;
	velocity.z = hvel.z;
	set_velocity(velocity);
	move_and_slide();

	left_origin->set_rotation(get_rotation());
	right_origin->set_rotation(get_rotation());

	left_origin->set_position(get_position());
	right_origin->set_position(get_position());
}


void CamNavDemoNode::_input(const godot::Ref<InputEvent> &event){
	if(Engine::get_singleton()->is_editor_hint()){
		return;
	}

	// Cast event
	godot::Ref<InputEventMouseMotion> mouseEvent = event;

	// Check if cast is actually to mouse motion event (https://github.com/godotengine/godot-cpp/issues/528#issuecomment-790153228)
	if(mouseEvent.is_valid() == false){
		return;
	}
	
	// Get relative movement/offset
	Vector2 relative = mouseEvent.ptr()->get_relative();

	// Rotate head up/down (yes)
	left_rotation_origin->rotate_x(-godot::Math::deg_to_rad(relative.y * mouse_sensitivity));
	right_rotation_origin->rotate_x(-godot::Math::deg_to_rad(relative.y * mouse_sensitivity));

	// Rotate head left/right (no)
	rotate_y(godot::Math::deg_to_rad(-relative.x * mouse_sensitivity));
	
	// Clamp so head can't over extend up/down (yes)
	Vector3 left_camera_rotation = left_rotation_origin->get_rotation();
	left_camera_rotation.x = godot::Math::clamp(left_camera_rotation.x, -1.55334f, 1.55334f);
	left_rotation_origin->set_rotation(left_camera_rotation);

	Vector3 right_camera_rotation = right_rotation_origin->get_rotation();
	right_camera_rotation.x = godot::Math::clamp(right_camera_rotation.x, -1.55334f, 1.55334f);
	right_rotation_origin->set_rotation(right_camera_rotation);
}