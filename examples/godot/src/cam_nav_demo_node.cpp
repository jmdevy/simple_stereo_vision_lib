#include "cam_nav_demo_node.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

#define CAMERA_RESOLUTION 256
#define SEARCH_WINDOW_DIMENSIONS 4

using namespace godot;


// Debug function that can be invoked by `cam_nav` just
// after the input feed frames are converted to grayscale.
// This is just for debugging.
void on_grayscale(void *grayscale_opaque_ptr, cam_nav_camera_side side, uint16_t *grayscale_frame_buffer, uint16_t pixel_width, uint16_t pixel_height){
	// Get the class instance back and make reference variables for
	// texture and image for this side
	CamNavDemoNode *instance = (CamNavDemoNode*)grayscale_opaque_ptr;
	godot::Ref<ImageTexture>    grayscale_texture;
    godot::Ref<Image>           grayscale_image;
	
	// Determine the side
	if(side == CAM_NAV_LEFT_CAMERA){
		grayscale_texture = instance->left_grayscale_texture;
		grayscale_image = instance->left_grayscale_image;
	}else{
		grayscale_texture = instance->right_grayscale_texture;
		grayscale_image = instance->right_grayscale_image;
	}

	// For each incoming grayscale single component value
	// from `cam_nav`, get it and convert it from 16-bit
	// int luminance to 0.0 ~ 1.0 luminance and then set
	// pixel using that since output texture is 8-bit
	for(uint16_t y=0; y<pixel_height; y++){
		for(uint16_t x=0; x<pixel_width; x++){
			float magnitude = (float)grayscale_frame_buffer[y*pixel_width + x] / (float)UINT16_MAX;
			grayscale_image.ptr()->set_pixel(x, y, Color(magnitude, magnitude, magnitude));
		}
	}

	// Update the image to show it on screen
	grayscale_texture.ptr()->set_image(grayscale_image);
}


void on_disparity(void *disparity_opaque_ptr, float *disparity_buffer, uint16_t disparity_width, uint16_t disparity_height){
	// Get the class instance back and make reference variables for
	// texture and image for this side
	CamNavDemoNode *instance = (CamNavDemoNode*)disparity_opaque_ptr;
	godot::Ref<ImageTexture>    disparity_texture = instance->disparity_texture;
    godot::Ref<Image>           disparity_image = instance->disparity_image;

	// For each incoming grayscale single component value
	// from `cam_nav`, get it and convert it from 16-bit
	// int luminance to 0.0 ~ 1.0 luminance and then set
	// pixel using that since output texture is 8-bit
	for(uint16_t y=0; y<disparity_width; y++){
		for(uint16_t x=0; x<disparity_height; x++){
			float magnitude = ((float)disparity_buffer[y*disparity_width + x] / ((float)CAMERA_RESOLUTION)) * 20.0f;
			disparity_image.ptr()->set_pixel(x, y, Color(magnitude, magnitude, magnitude));
		}
	}

	// Update the image to show it on screen
	disparity_texture.ptr()->set_image(disparity_image);
}


void on_depth(void *depth_opaque_ptr, float *depth_buffer, uint16_t depth_width, uint16_t depth_height, float max_depth_mm){
	// Get the class instance back and make reference variables for
	// texture and image for this side
	CamNavDemoNode *instance = (CamNavDemoNode*)depth_opaque_ptr;
	godot::Ref<ImageTexture>    depth_texture = instance->depth_texture;
    godot::Ref<Image>           depth_image = instance->depth_image;

	// For each incoming grayscale single component value
	// from `cam_nav`, get it and convert it from 16-bit
	// int luminance to 0.0 ~ 1.0 luminance and then set
	// pixel using that since output texture is 8-bit
	for(uint16_t y=0; y<depth_width; y++){
		for(uint16_t x=0; x<depth_height; x++){
			float max_depth_mm = cam_nav_get_max_depth_mm(instance->cam_nav);
			float depth_mm = (float)depth_buffer[y*depth_width + x];

			if(depth_mm >= max_depth_mm){
				depth_image.ptr()->set_pixel(x, y, Color(0.0f, 0.0f, 0.0f));
			}else{
				depth_image.ptr()->set_pixel(x, y, Color(0.0f, depth_mm/max_depth_mm, 0.0f));
			}
			
		}
	}

	// Update the image to show it on screen
	depth_texture.ptr()->set_image(depth_image);
}





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
	// Ensure view ports are always 256x256 on the monitor
	float scale = 256.0f/left_viewport->get_size().x;
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


	// Create outputs for images generated from library
	left_grayscale_viewport_container = memnew(SubViewportContainer);
	left_grayscale_viewport = memnew(SubViewport);
	left_grayscale_camera = memnew(Camera2D);
	left_grayscale_texture_rect = memnew(TextureRect);
	left_grayscale_texture.instantiate();

	right_grayscale_viewport_container = memnew(SubViewportContainer);
	right_grayscale_viewport = memnew(SubViewport);
	right_grayscale_camera = memnew(Camera2D);
	right_grayscale_texture_rect = memnew(TextureRect);
	right_grayscale_texture.instantiate();

	disparity_viewport_container = memnew(SubViewportContainer);
	disparity_viewport = memnew(SubViewport);
	disparity_camera = memnew(Camera2D);
	disparity_texture_rect = memnew(TextureRect);
	disparity_texture.instantiate();
	disparity_texture_rect->set_texture_filter(godot::CanvasItem::TextureFilter::TEXTURE_FILTER_NEAREST);

	depth_viewport_container = memnew(SubViewportContainer);
	depth_viewport = memnew(SubViewport);
	depth_camera = memnew(Camera2D);
	depth_texture_rect = memnew(TextureRect);
	depth_texture.instantiate();
	depth_texture_rect->set_texture_filter(godot::CanvasItem::TextureFilter::TEXTURE_FILTER_NEAREST);

	scale *= (float)SEARCH_WINDOW_DIMENSIONS;
	disparity_viewport_container->set_scale(Vector2(scale, scale));
	depth_viewport_container->set_scale(Vector2(scale, scale));

	Node3D *parent = get_parent_node_3d();

	parent->call_deferred("add_child", left_grayscale_viewport_container);
	parent->call_deferred("add_child", right_grayscale_viewport_container);
	parent->call_deferred("add_child", disparity_viewport_container);
	parent->call_deferred("add_child", depth_viewport_container);

	left_grayscale_viewport_container->call_deferred("add_child", left_grayscale_viewport);
	right_grayscale_viewport_container->call_deferred("add_child", right_grayscale_viewport);
	disparity_viewport_container->call_deferred("add_child", disparity_viewport);
	depth_viewport_container->call_deferred("add_child", depth_viewport);

	left_grayscale_viewport->call_deferred("add_child", left_grayscale_camera);
	right_grayscale_viewport->call_deferred("add_child", right_grayscale_camera);
	disparity_viewport->call_deferred("add_child", disparity_camera);
	depth_viewport->call_deferred("add_child", depth_camera);

	left_grayscale_camera->call_deferred("add_child", left_grayscale_texture_rect);
	right_grayscale_camera->call_deferred("add_child", right_grayscale_texture_rect);
	disparity_camera->call_deferred("add_child", disparity_texture_rect);
	depth_camera->call_deferred("add_child", depth_texture_rect);

	left_grayscale_viewport->set_size(Vector2(CAMERA_RESOLUTION, CAMERA_RESOLUTION));
	right_grayscale_viewport->set_size(Vector2(CAMERA_RESOLUTION, CAMERA_RESOLUTION));
	disparity_viewport->set_size(Vector2(CAMERA_RESOLUTION/SEARCH_WINDOW_DIMENSIONS, CAMERA_RESOLUTION/SEARCH_WINDOW_DIMENSIONS));
	depth_viewport->set_size(Vector2(CAMERA_RESOLUTION/SEARCH_WINDOW_DIMENSIONS, CAMERA_RESOLUTION/SEARCH_WINDOW_DIMENSIONS));

	left_grayscale_viewport_container->set_size(Vector2(CAMERA_RESOLUTION, CAMERA_RESOLUTION));
	right_grayscale_viewport_container->set_size(Vector2(CAMERA_RESOLUTION, CAMERA_RESOLUTION));
	disparity_viewport_container->set_size(Vector2(CAMERA_RESOLUTION/SEARCH_WINDOW_DIMENSIONS, CAMERA_RESOLUTION/SEARCH_WINDOW_DIMENSIONS));
	depth_viewport_container->set_size(Vector2(CAMERA_RESOLUTION/SEARCH_WINDOW_DIMENSIONS, CAMERA_RESOLUTION/SEARCH_WINDOW_DIMENSIONS));

	left_grayscale_viewport_container->set_position(Vector2(0, CAMERA_RESOLUTION));
	right_grayscale_viewport_container->set_position(Vector2(CAMERA_RESOLUTION, CAMERA_RESOLUTION));
	disparity_viewport_container->set_position(Vector2(CAMERA_RESOLUTION*2, CAMERA_RESOLUTION));
	depth_viewport_container->set_position(Vector2(CAMERA_RESOLUTION*3, CAMERA_RESOLUTION));

	left_grayscale_texture_rect->set_position(Vector2(-CAMERA_RESOLUTION/2, -CAMERA_RESOLUTION/2));
	right_grayscale_texture_rect->set_position(Vector2(-CAMERA_RESOLUTION/2, -CAMERA_RESOLUTION/2));
	disparity_texture_rect->set_position(Vector2(-CAMERA_RESOLUTION/SEARCH_WINDOW_DIMENSIONS/2, -CAMERA_RESOLUTION/SEARCH_WINDOW_DIMENSIONS/2));
	depth_texture_rect->set_position(Vector2(-CAMERA_RESOLUTION/SEARCH_WINDOW_DIMENSIONS/2, -CAMERA_RESOLUTION/SEARCH_WINDOW_DIMENSIONS/2));

	// Create the output debug grayscale textures in 8-bit grayscale since that's all Godot offers
	left_grayscale_image = Image::create(CAMERA_RESOLUTION, CAMERA_RESOLUTION, false, godot::Image::FORMAT_L8);
	left_grayscale_image.ptr()->fill(Color(1.0f, 0.0f, 0.0f));

	right_grayscale_image = Image::create(CAMERA_RESOLUTION, CAMERA_RESOLUTION, false, godot::Image::FORMAT_L8);
	right_grayscale_image.ptr()->fill(Color(1.0f, 1.0f, 0.0f));

	disparity_image = Image::create(CAMERA_RESOLUTION/SEARCH_WINDOW_DIMENSIONS, CAMERA_RESOLUTION/SEARCH_WINDOW_DIMENSIONS, false, godot::Image::FORMAT_RF);
	disparity_image.ptr()->fill(Color(1.0f, 1.0f, 0.0f));

	depth_image = Image::create(CAMERA_RESOLUTION/SEARCH_WINDOW_DIMENSIONS, CAMERA_RESOLUTION/SEARCH_WINDOW_DIMENSIONS, false, godot::Image::FORMAT_RGF);
	depth_image.ptr()->fill(Color(1.0f, 1.0f, 1.0f));

	left_grayscale_texture.ptr()->set_image(left_grayscale_image);
	right_grayscale_texture.ptr()->set_image(right_grayscale_image);
	disparity_texture.ptr()->set_image(disparity_image);
	depth_texture.ptr()->set_image(depth_image);

	left_grayscale_texture_rect->set_texture(left_grayscale_texture);
	right_grayscale_texture_rect->set_texture(right_grayscale_texture);
	disparity_texture_rect->set_texture(disparity_texture);
	depth_texture_rect->set_texture(depth_texture);

	// Create the cam_nav library instance
	cam_nav = cam_nav_create(CAMERA_RESOLUTION, CAMERA_RESOLUTION, SEARCH_WINDOW_DIMENSIONS, baseline*1000.0f, left_camera->get_fov(), true);

	if(cam_nav == NULL){
		UtilityFunctions::print("ERROR: Could not create cam_nav library! Likely an issue with search window not being a multiple of the width or height of the camera!");
	}

	cam_nav_set_on_grayscale_cb(cam_nav, on_grayscale, this);
	cam_nav_set_on_disparity_cb(cam_nav, on_disparity, this);
	cam_nav_set_on_depth_cb(cam_nav, on_depth, this);
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

	// Feed and process frames in `cam_nav`
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