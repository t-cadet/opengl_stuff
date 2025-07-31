#include <GL/glew.h>
#include <GLFW/glfw3.h>

#define STBI_FAILURE_USERMSG
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"

#include <cassert>
#include <cmath>
#include <initializer_list>
#include <iostream>
#include <string>
#include <string_view>
using namespace std;

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/ptrace.h>

struct Image {
  int w, h;
  int channels;
  unsigned char* data;
};

struct GlVertexAttrib {
    unsigned int glType, count;
};

struct ImguiDemoState {
    bool show_demo_window;
    bool show_another_window;
    ImVec4 clear_color;
    ImGuiIO* io;
};

Image ReadImage(const char* path) {
    Image img = {};
    img.data = stbi_load(path, &img.w, &img.h, &img.channels, 0);
    if (!img.data) {
      printf("ReadImage failed: %s\n", stbi_failure_reason());
      exit(1);
    }
    return img;
}

void FreeImage(Image const& img) {
    stbi_image_free(img.data);
}

unsigned int LoadGlTexture(Image img, unsigned int slot = 0) {
    unsigned int texture;

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    GLenum format;
    GLenum internalFormat;

    if (img.channels == 1) {
        format = GL_RED;
        internalFormat = GL_R8;
    } else if (img.channels == 3) {
        format = GL_RGB;
        internalFormat = GL_RGB8;
    } else if (img.channels == 4) {
        format = GL_RGBA;
        internalFormat = GL_RGBA8;
    } else {
        cerr << "Unsupported channel count: " << img.channels << endl;
        exit(1);
    }

    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, img.w, img.h, 0, format, GL_UNSIGNED_BYTE, img.data);
    glActiveTexture(GL_TEXTURE0 + slot);

    return texture;
}

void GlClearErrors() {
    while (glGetError());
}

bool IsDebuggerPresent()
{
    static bool underDebugger = false;
    static bool isCheckedAlready = false;

    if (!isCheckedAlready)
    {
        if (ptrace(PTRACE_TRACEME, 0, 1, 0) < 0) {
             underDebugger = true;
        } else {
            ptrace(PTRACE_DETACH, 0, 1, 0);
        }
        isCheckedAlready = true;
    }
    return underDebugger;
}

void GlPrintErrors(const char* file, const int line, const char* message) {
    while (GLenum error = glGetError()) {
        cerr << "[GL_CHECK ERROR] " << file << ":" << line << " " << message << ": " << hex << error << "\n";
        if (IsDebuggerPresent()) {
            __builtin_debugtrap();
        }
    }
}

#define GL_CHECK(x) \
    if (true) { \
        GlClearErrors(); \
        x; \
        GlPrintErrors(__FILE__, __LINE__, #x); \
    } else

long GetFileSize(FILE* file) {
    if (fseek(file, 0, SEEK_END) == -1) {
        perror("fseek");
        exit(errno);
    }

    long size = ftell(file);
    if (size == -1) {
        perror("ftell");
        exit(errno);
    }

    rewind(file);

    return size;
}

std::string ReadFile(const char* path) {
    FILE* file = fopen(path, "rb");
    if (file == nullptr) {
        perror("fopen");
        exit(errno);
    }

    string fileContent;
    long fileSize = GetFileSize(file);
    fileContent.resize(fileSize);

    size_t readCount = fread(fileContent.data(), 1, fileSize, file);
    if (readCount != fileSize) {
        perror("fread");
        exit(errno);
    }

    if (fclose(file) == EOF) {
        perror("fclose");
        exit(errno);
    }

    return fileContent;
}

unsigned int CreateGlShader(unsigned int type, string& sourceCode) {
    unsigned int shader = glCreateShader(type);
    const char* sourceCodeData = sourceCode.data();
    glShaderSource(shader, 1, &sourceCodeData, nullptr);
    glCompileShader(shader);

    int result; glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
    if (!result) {
        int len; glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        char* message = (char*)alloca(len*sizeof(char));
        glGetShaderInfoLog(shader, len, &len, message);
        const char* shaderTypeStr = type == GL_VERTEX_SHADER ? "vertex" : (type == GL_FRAGMENT_SHADER ? "fragment" : "unknown");
        cerr << "ERROR: CompileShader (" << shaderTypeStr << "): " << message << "\n";
        exit(1);
    }

    return shader;
}

unsigned int CreateGlProgram(string& vertexShader, string& fragmentShader) {
    unsigned int program = glCreateProgram();
    unsigned int vs = CreateGlShader(GL_VERTEX_SHADER, vertexShader);
    unsigned int fs = CreateGlShader(GL_FRAGMENT_SHADER, fragmentShader);

    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    int result;
    glGetProgramiv(program, GL_LINK_STATUS, &result);
    if (!result) {
        int len;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
        char* message = (char*)alloca(len * sizeof(char));
        glGetProgramInfoLog(program, len, &len, message);
        cerr << "ERROR: LinkProgram: " << message << "\n";
        exit(1);
    }

    glValidateProgram(program);

    // FIXME: detach shaders too?
    glDeleteShader(vs);
    glDeleteShader(fs);

    return program;
}

unsigned int CreateGlVertexArray() {
    unsigned int va;
    glGenVertexArrays(1, &va);
    glBindVertexArray(va);
    return va;
}

unsigned int CreateGlBufferEx(void* data, size_t byteSize, GLenum target) {
    unsigned int buffer;
    glGenBuffers(1, &buffer);
    glBindBuffer(target, buffer);
    glBufferData(target, byteSize, data, GL_STATIC_DRAW);
    return buffer;
}
#define CreateGlBuffer(data, target) CreateGlBufferEx(data, sizeof(data), target)

unsigned int GetGlTypeSize(unsigned int glType) {
    unsigned int glTypeSize = 0;
    switch (glType) {
        case GL_FLOAT: glTypeSize = 4; break;
        default: assert(false && "unknown targetSize");
    }
    return glTypeSize;
}

void EnableGlVertexAttribArray(initializer_list<GlVertexAttrib> vertexAttributes) {
    unsigned int stride = 0;
    for (auto const& attrib : vertexAttributes) {
        stride += attrib.count*GetGlTypeSize(attrib.glType);
    }

    size_t offset = 0;
    unsigned int i = 0;
    for (auto const& attrib : vertexAttributes) {
        glEnableVertexAttribArray(i);
        glVertexAttribPointer(i, attrib.count, attrib.glType, GL_FALSE, stride, (void*)offset);
        offset += attrib.count*GetGlTypeSize(attrib.glType);
        ++i;
    }

    assert(offset == stride);
}

void DisplayImguiDemo(ImguiDemoState& state) {
    // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
    if (state.show_demo_window)
        ImGui::ShowDemoWindow(&state.show_demo_window);

    // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
    {
        static float f = 0.0f;
        static int counter = 0;

        ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

        ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
        ImGui::Checkbox("Demo Window", &state.show_demo_window);      // Edit bools storing our window open/close state
        ImGui::Checkbox("Another Window", &state.show_another_window);

        ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
        ImGui::ColorEdit3("clear color", (float*)&state.clear_color); // Edit 3 floats representing a color

        if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
            counter++;
        ImGui::SameLine();
        ImGui::Text("counter = %d", counter);

        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / state.io->Framerate, state.io->Framerate);
        ImGui::End();
    }

    // 3. Show another simple window.
    if (state.show_another_window)
    {
        ImGui::Begin("Another Window", &state.show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
        ImGui::Text("Hello from another window!");
        if (ImGui::Button("Close Me"))
            state.show_another_window = false;
        ImGui::End();
    }
}

int main(void)
{
    GLFWwindow* window;

    /* Initialize the library */
    if (!glfwInit())
        return -1;

    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(1200, 900, "Hello World", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    /* Make the window's context current */
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (auto ok = glewInit(); ok != GLEW_OK) {
        cerr << "glewInit failed: " << glewGetErrorString(ok) << "\n";
        return 1;
    }
    cerr << "GL_VERSION=" << glGetString(GL_VERSION) << "\n";

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // // Setup scaling
    // ImGuiStyle& style = ImGui::GetStyle();
    // style.ScaleAllSizes(main_scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
    // style.FontScaleDpi = main_scale;        // Set initial font scale. (using io.ConfigDpiScaleFonts=true makes this unnecessary. We leave both here for documentation purpose)

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    ImguiDemoState imguiDemoState = {};
    imguiDemoState.show_demo_window = true;
    imguiDemoState.show_another_window = false;
    imguiDemoState.clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    imguiDemoState.io = &io;

    float positions[] = {
        -0.5, +0.5, 0.0, 1.0,
        -0.5, -0.5, 0.0, 0.0,
        +0.5, -0.5, 1.0, 0.0,
        +0.5, +0.5, 1.0, 1.0,
    };

    unsigned int indices[] = {
        0, 1, 2,
        2, 3, 0,
    };

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    unsigned int va = CreateGlVertexArray();
    unsigned int vertexBuffer = CreateGlBuffer(positions, GL_ARRAY_BUFFER);
    EnableGlVertexAttribArray({
        {GL_FLOAT, 2},
        {GL_FLOAT, 2},
    });

    unsigned int indexBuffer = CreateGlBuffer(indices, GL_ELEMENT_ARRAY_BUFFER);

    string vertexShaderSource = ReadFile("vertexShader.glsl");
    string fragmentShaderSource = ReadFile("fragmentShader.glsl");

    unsigned int glProgram = CreateGlProgram(vertexShaderSource, fragmentShaderSource);
    glUseProgram(glProgram);

    // int uColorLocation = glGetUniformLocation(glProgram, "uColor");
    // assert(uColorLocation != -1);

    stbi_set_flip_vertically_on_load(1);
    Image img = ReadImage("logo.jpg");

    unsigned int textureSlot = 0;
    unsigned int texture = LoadGlTexture(img, textureSlot);
    assert(texture > 0);
    FreeImage(img);

    int uTextureLocation = glGetUniformLocation(glProgram, "uTexture");
    assert(uTextureLocation != -1);
    glUniform1i(uTextureLocation, textureSlot);

    float scaleX = 0.0;
    float scaleY = 0.0;
    int uMvpLocation = glGetUniformLocation(glProgram, "uMvp");
    assert(uMvpLocation != -1);

    // glBindVertexArray(0);
    // glBindBuffer(GL_ARRAY_BUFFER, 0);
    // glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    // glUseProgram(0);

    float dt = 0.0f;
    while (!glfwWindowShouldClose(window))
    {
        /* Render here */
        glClear(GL_COLOR_BUFFER_BIT);

        float sinDt1 = (1.0f + sinf(1*dt)) / 2.0f;
        float sinDt2 = (1.0f + sinf(2*dt)) / 2.0f;
        float sinDt3 = (1.0f + sinf(3*dt)) / 2.0f;

        glBindVertexArray(va);
        glUseProgram(glProgram);
        // glUniform4f(uColorLocation, sinDt1, sinDt2, sinDt3, 1.0f);

        float mvp[16] = {
            1.5f + scaleX, 0.0          , 0.0, 0.0,
            0.0          , 2.0f + scaleY, 0.0, 0.0,
            0.0          , 0.0          , 1.5, 0.0,
            0.0          , 0.0          , 0.0, 2.0,
        };
        glUniformMatrix4fv(uMvpLocation, 1, 0, &mvp[0]);

        GL_CHECK(glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr));

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // DisplayImguiDemo(imguiDemoState);
        {
            ImGui::Begin("Hello, world!");
            ImGui::SliderFloat("scale X", &scaleX, -1.0f, 1.0f);
            ImGui::SliderFloat("scale Y", &scaleY, -1.0f, 1.0f);
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        /* Swap front and back buffers */
        glfwSwapBuffers(window);

        /* Poll for and process events */
        glfwPollEvents();
        dt += 1.0f/60.0f;
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteTextures(1, &texture);
    glDeleteProgram(glProgram);

    glfwTerminate();
    return 0;
}
