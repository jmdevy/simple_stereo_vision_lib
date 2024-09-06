#ifndef CAM_NAV_DEMO_NODE_H
#define CAM_NAV_DEMO_NODE_H


#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/sub_viewport_container.hpp>
#include <godot_cpp/classes/sub_viewport.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/viewport_texture.hpp>
#include <godot_cpp/classes/image.hpp>

using namespace godot;

class CamNavDemoNode : public Node3D{
    GDCLASS(CamNavDemoNode, Node3D)

private:
    // Each eye has the following hierarchy:
    //  - SubViewportContainer
    //      - SubViewport
    //          - Camera3D
    SubViewportContainer        *left_viewport_container = nullptr;
    SubViewport                 *left_viewport = nullptr;
    Camera3D                    *left_camera = nullptr;
    godot::Ref<ViewportTexture> left_texture;

    SubViewportContainer        *right_viewport_container = nullptr;
    SubViewport                 *right_viewport = nullptr;
    Camera3D                    *right_camera = nullptr;
    godot::Ref<ViewportTexture> right_texture;
protected:
    static void _bind_methods();
public:
    CamNavDemoNode();
    ~CamNavDemoNode();

    // How many units apart are the stereo eye origins
    float baseline = 10.0f;

    void _ready() override;
    void _process(double delta) override;
};


#endif  // CAM_NAV_DEMO_NODE_H