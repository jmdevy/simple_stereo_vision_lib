# cam_nav Example
This example uses Godot 4.3 and GDExtension to run C code utilizing the `cam_nav` library to traverse a virtual/simulated 3D environment in a scene setup using the Godot 4.3 editor.

# Layout:
* `godot-cpp`: submodule needed to build Godot GDExtension source in the `src` folder 
* `src`: code the defines the GDExtension which which utilizes the `cam_nav` library with camera feeds from Godot (using a custom defined node implemented using GDExtension). This is needed to build a .gdextension file to define the custom node placed into a scene in the `cam_nav_demo` folder
* `cam_nav_demo`: Godot project that can be opened in the Godot 4.3 editor. Press the run button to see the example/demo run

# Setup, Build, and Run on Windows
(general GDExtension resources: https://docs.godotengine.org/en/stable/tutorials/scripting/gdextension/gdextension_cpp_example.html)

1. Clone this repository and init submodules (**godot-cpp**): `git clone --recurse-submodules -j8 https://github.com/jmdevy/cam_nav.git`
2. Move into **godot-cpp** submodule: `cd cam_nav/examples/godot/godot-cpp`
3. Build C++ bindings/**godot-cpp** with debug symbols and for 64-bit architectures: `scons platform=windows target=template_debug -j8 debug_symbols=yes bits=64 custom_api_file=../src/extension_api.json`
4. Move example top-level to build the extension using the `SConstruct` file: : `cd ..`
5. Build the GDExtension with debug symbols: `scons platform=windows target=template_debug -j8 debug_symbols=yes` (outputs go to `cam_nav_demo/bin` and are used by `cam_nav_demo/bin/camnav.gdextension` to tell Godot about the new custom node)
5. Download and install Godot 4.3 Editor: https://godotengine.org/download
6. Open Godot 4.3 Editor and import project/scene at `cam_nav/examples/godot/cam_nav_demo/main.tscn`
7. Press the run button in the top-right or press F5

Done!