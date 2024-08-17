#ifndef CAM_NAV_DEMO_NODE_HPP
#define CAM_NAV_DEMO_NODE_HPP


#include <godot_cpp/classes/node3d.hpp>


namespace godot {

    class CamNavDemoNode : public Node3D{
        GDCLASS(CamNavDemoNode, Node3D)

    private:

    protected:
        static void _bind_methods();

    public:
        CamNavDemoNode();
        ~CamNavDemoNode();

        void _process(double delta) override;
    };

}


#endif  // CAM_NAV_DEMO_NODE_HPP