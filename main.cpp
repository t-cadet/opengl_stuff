#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <string>
#include <string_view>
using namespace std;

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

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
    glValidateProgram(program);

    // FIXME: detach shaders too?
    glDeleteShader(vs);
    glDeleteShader(fs);

    return program;
}

int main(void)
{
    GLFWwindow* window;

    /* Initialize the library */
    if (!glfwInit())
        return -1;

    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(640, 480, "Hello World", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    /* Make the window's context current */
    glfwMakeContextCurrent(window);

    if (auto ok = glewInit(); ok != GLEW_OK) {
        cerr << "glewInit failed: " << glewGetErrorString(ok) << "\n";
        return 1;
    }
    cerr << "GL_VERSION=" << glGetString(GL_VERSION) << "\n";

    float positions[6] = {
        -0.5, -0.5,
        +0.5, -0.5,
        +0.0, +0.5,
    };

    unsigned int buffer;
    glGenBuffers(1, &buffer);
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, 6*sizeof(float), positions, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), 0);

    string vertexShaderSource = ReadFile("vertexShader.glsl");
    string fragmentShaderSource = ReadFile("fragmentShader.glsl");

    unsigned int glProgram = CreateGlProgram(vertexShaderSource, fragmentShaderSource);
    glUseProgram(glProgram);

    /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(window))
    {
        /* Render here */
        glClear(GL_COLOR_BUFFER_BIT);

        glDrawArrays(GL_TRIANGLES, 0, 3);

        /* Swap front and back buffers */
        glfwSwapBuffers(window);

        /* Poll for and process events */
        glfwPollEvents();
    }

    glDeleteProgram(glProgram);

    glfwTerminate();
    return 0;
}
