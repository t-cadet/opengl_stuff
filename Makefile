run: main
	LD_LIBRARY_PATH="." ./main
	
main: main.cpp imgui.so
	clang++ -Iimgui -ggdb -std=c++20 -lglfw -lGL -lGLEW imgui.so main.cpp -o main

imgui.so: imgui/*.cpp
	clang++ -shared -Iimgui -ggdb -std=c++20 \
	 imgui/imgui.cpp \
	 imgui/imgui_draw.cpp \
	 imgui/imgui_widgets.cpp \
	 imgui/imgui_tables.cpp \
	 imgui/imgui_demo.cpp \
	 imgui/backends/imgui_impl_glfw.cpp \
	 imgui/backends/imgui_impl_opengl3.cpp \
	 -o imgui.so \
	
