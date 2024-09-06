#include "cam_nav_demo_node.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/engine.hpp>

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

	// Create hierarchy for both eyes
	left_viewport_container = memnew(SubViewportContainer);
	left_viewport 			= memnew(SubViewport);
	left_camera 			= memnew(Camera3D);

	right_viewport_container = memnew(SubViewportContainer);
	right_viewport 			 = memnew(SubViewport);
	right_camera 			 = memnew(Camera3D);

	// Add hierarchy of both eyes
	call_deferred("add_child", left_viewport_container);
	call_deferred("add_child", right_viewport_container);

	left_viewport_container->call_deferred("add_child", left_viewport);
	right_viewport_container->call_deferred("add_child", right_viewport);

	left_viewport->call_deferred("add_child", left_camera);
	right_viewport->call_deferred("add_child", right_camera);

	// Move cameras' baseline distance
	left_camera->set_position(Vector3(-baseline/2.0f, 0.0, 0.0));
	right_camera->set_position(Vector3(baseline/2.0f, 0.0, 0.0));

	// Get textures and images that will be fed into cam_nav every tick
	left_texture = left_viewport->get_texture();
	right_texture = right_viewport->get_texture();
}


CamNavDemoNode::~CamNavDemoNode(){
	// Add your cleanup here.
}


void CamNavDemoNode::_process(double delta){
	if(Engine::get_singleton()->is_editor_hint()){
		return;
	}

	// Get eye images and convert to pixel format cam_nav uses
	godot::Ref<Image> left_image = left_texture.ptr()->get_image();
	godot::Ref<Image> right_image = right_texture.ptr()->get_image();

	left_image.ptr()->convert(Image::FORMAT_RGB565);
	right_image.ptr()->convert(Image::FORMAT_RGB565);

	UtilityFunctions::print(left_image.ptr()->get_format());
}