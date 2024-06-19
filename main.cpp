#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION

#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <stdio.h>
#include <math.h>


#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>

#include "stb_image.h"
#include "stb_image_write.h"
#include "stb_image_resize.h"
#include "SDL.h"
#include "SDL_net.h"

#include <Windows.h>

using namespace std;

#define IMAGE_SIZEX 178
#define IMAGE_SIZEY 134

#define SCREENW 800
#define SCREENH 600


#undef main

class Shader
{
public:
    unsigned int ID;
    // constructor generates the shader on the fly
    // ------------------------------------------------------------------------
    Shader(const char* vertexPath, const char* fragmentPath)
    {
        // 1. retrieve the vertex/fragment source code from filePath
        std::string vertexCode;
        std::string fragmentCode;
        std::ifstream vShaderFile;
        std::ifstream fShaderFile;
        // ensure ifstream objects can throw exceptions:
        vShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        fShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        try
        {
            // open files
            vShaderFile.open(vertexPath);
            fShaderFile.open(fragmentPath);
            std::stringstream vShaderStream, fShaderStream;
            // read file's buffer contents into streams
            vShaderStream << vShaderFile.rdbuf();
            fShaderStream << fShaderFile.rdbuf();
            // close file handlers
            vShaderFile.close();
            fShaderFile.close();
            // convert stream into string
            vertexCode = vShaderStream.str();
            fragmentCode = fShaderStream.str();
        }
        catch (std::ifstream::failure& e)
        {
            std::cout << "ERROR::SHADER::FILE_NOT_SUCCESFULLY_READ: " << e.what() << std::endl;
        }
        const char* vShaderCode = vertexCode.c_str();
        const char* fShaderCode = fragmentCode.c_str();
        // 2. compile shaders
        unsigned int vertex, fragment;
        // vertex shader
        vertex = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex, 1, &vShaderCode, NULL);
        glCompileShader(vertex);
        checkCompileErrors(vertex, "VERTEX");
        // fragment Shader
        fragment = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment, 1, &fShaderCode, NULL);
        glCompileShader(fragment);
        checkCompileErrors(fragment, "FRAGMENT");
        // shader Program
        ID = glCreateProgram();
        glAttachShader(ID, vertex);
        glAttachShader(ID, fragment);
        glLinkProgram(ID);
        checkCompileErrors(ID, "PROGRAM");
        // delete the shaders as they're linked into our program now and no longer necessary
        glDeleteShader(vertex);
        glDeleteShader(fragment);
    }
    // activate the shader
    // ------------------------------------------------------------------------
    void use()
    {
        glUseProgram(ID);
    }
    // utility uniform functions
    // ------------------------------------------------------------------------
    void setBool(const std::string& name, bool value) const
    {
        glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)value);
    }
    // ------------------------------------------------------------------------
    void setInt(const std::string& name, int value) const
    {
        glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
    }
    // ------------------------------------------------------------------------
    void setFloat(const std::string& name, float value) const
    {
        glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
    }
    // ------------------------------------------------------------------------
    void setMat2(const std::string& name, const glm::mat2& mat) const
    {
        glUniformMatrix2fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
    }
    // ------------------------------------------------------------------------
    void setMat3(const std::string& name, const glm::mat3& mat) const
    {
        glUniformMatrix3fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
    }
    // ------------------------------------------------------------------------
    void setMat4(const std::string& name, const glm::mat4& mat) const
    {
        glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
    }

private:
    // utility function for checking shader compilation/linking errors.
    // ------------------------------------------------------------------------
    void checkCompileErrors(unsigned int shader, std::string type)
    {
        int success;
        char infoLog[1024];
        if (type != "PROGRAM")
        {
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (!success)
            {
                glGetShaderInfoLog(shader, 1024, NULL, infoLog);
                std::cout << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
            }
        }
        else
        {
            glGetProgramiv(shader, GL_LINK_STATUS, &success);
            if (!success)
            {
                glGetProgramInfoLog(shader, 1024, NULL, infoLog);
                std::cout << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
            }
        }
    }
};

#undef main

// in cm
#define MAZE_X 14
#define MAZE_Y 21

#define PREF_IMAGE_SIZE 128

unsigned char* glbuffer;
unsigned char* glbuffer_out;

unsigned char* recvbuffer;

int main() {
    unsigned char* imageData;
    int px, py, pn;
    int startposx, startposy;
    int endposx, endposy;
    unsigned int* mazeData;
    unsigned char* correctPath; // temp, will be replaced by mazeData
    vector<int> pathPointsX;
    vector<int> pathPointsY;
    float textureOffset = 0;
    float viewz = 0; float viewy = 0;
    double cursorposx, cursorposy;
    glm::mat4 projection;

    const float mazeRatio = (float)MAZE_Y / (float)MAZE_X;

    // receive image data
    SDLNet_Init();
    TCPsocket server; // r
    IPaddress serverIP;
    SDLNet_ResolveHost(&serverIP, "172.23.190.156", 1234);
    server = SDLNet_TCP_Open(&serverIP);
    recvbuffer = (unsigned char*)malloc(178 * 128 * sizeof(unsigned char));

    for (int i = 0; i < 178 * 128; i++) {
        SDLNet_TCP_Recv(server, recvbuffer + i, sizeof(unsigned char));
    }


    // initialize glfw
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(600, 600 * (float)MAZE_Y / (float)MAZE_X, "EV3MAZE", NULL, NULL);
    glfwMakeContextCurrent(window);

    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
//    glViewport(0, 0, SCREENW, SCREENH);

    Shader shader("vertex.vs", "fragment.fs");

    float texturePlane[] = {
        1.0f, 1.0f, 0.0f,     1.0f, 1.0f,
        1.0f, -1.0f, 0.5f,    1.0f, 0.0f,
        -1.0f, -1.0f, 0.5f,   0.0f, 0.0f,
        -1.0f, 1.0f, 0.0f,    0.0f, 1.0f,
    };

    unsigned int indices[] = {
        0, 1, 3, // first triangle
        1, 2, 3  // second triangle
    };

    unsigned int mazeTexture;
    unsigned int VBO, VAO, EBO;
    glGenTextures(1, &mazeTexture);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    glGenVertexArrays(1, &VAO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(texturePlane), texturePlane, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_DYNAMIC_DRAW);

    // position at
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // texture at
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindTexture(GL_TEXTURE_2D, mazeTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_RED); // copy R to G
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED); // copy R to B

    unsigned char* textureData = NULL;
    stbi_set_flip_vertically_on_load(true);
    textureData = stbi_load("texture3.bmp", &px, &py, &pn, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, px, py, 0, GL_RED, GL_UNSIGNED_BYTE, textureData);

    shader.setInt("ourTexture", 0);

    while (!glfwWindowShouldClose(window)) {

        glfwPollEvents();

        // input
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
            texturePlane[4] -= 0.01;
            texturePlane[19] -= 0.01;
        }
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
            texturePlane[4] += 0.01;
            texturePlane[19] += 0.01;
        }
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
            texturePlane[2] += 0.01;
            texturePlane[17] += 0.01;
            texturePlane[1] += 0.005;
            texturePlane[16] += 0.005;
        }
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
            texturePlane[2] -= 0.01;
            texturePlane[17] -= 0.01;
            texturePlane[1] -= 0.005;
            texturePlane[16] -= 0.005;
        }
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) viewy += 0.05;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) viewy -= 0.05;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) viewz += 0.1;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) viewz -= 0.1;
        if (glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS) break;
        
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
            startposx = cursorposx;
            startposy = cursorposy;
            while (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) { glfwPollEvents(); }
            printf("new start pos: x = %d, y = %d \n", startposx, startposy);
        }
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
            endposx = cursorposx;
            endposy = cursorposy;
            while (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) { glfwPollEvents(); }
            printf("new end pos: x = %d, y = %d \n", endposx, endposy);
        }

        glfwGetCursorPos(window, &cursorposx, &cursorposy);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(texturePlane), texturePlane, GL_DYNAMIC_DRAW);

        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glActiveTexture(GL_TEXTURE0);
        glad_glBindTexture(GL_TEXTURE_2D, mazeTexture);

        shader.use();
        projection = glm::perspective(glm::radians(40.0f), 800.f / 600.f, 0.1f, 100.0f);
        shader.setMat4("projection", projection);
        shader.setMat4("view", glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, viewy, viewz)));
        //transformation = glm::scale(glm::mat4(1.0f), glm::vec3(1, 1, 1));
        //transformation = glm::rotate(transformation, rotation, glm::vec3(1.0f, 0.0f, 0.0f));
        //shader.setMat4("transformation", transformation);
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        glfwSwapBuffers(window);
        Sleep(1000 / 60);
    }

    glbuffer = (unsigned char*) malloc(600 * ((float)MAZE_Y / (float)MAZE_X) * 600 * 3);
    glbuffer_out = (unsigned char*)malloc(PREF_IMAGE_SIZE * ((float)MAZE_Y / (float)MAZE_X) * PREF_IMAGE_SIZE * 3);

    glReadPixels(0, 0, 600, 600 * (float)MAZE_Y / (float)MAZE_X, GL_RGB, GL_UNSIGNED_BYTE, glbuffer);
    stbi_set_flip_vertically_on_load(false);
    stbi_flip_vertically_on_write(true);
    stbir_resize_uint8(glbuffer, 600, 600 * (float)MAZE_Y / (float)MAZE_X, 0, glbuffer_out, PREF_IMAGE_SIZE, (float)PREF_IMAGE_SIZE * ((float)MAZE_Y / (float)MAZE_X), 0, 3);
    stbi_write_bmp("glOut.bmp", 600, 600 * (float)MAZE_Y / (float)MAZE_X, 3, glbuffer);
    stbi_write_bmp("glOut2.bmp", PREF_IMAGE_SIZE, (float)PREF_IMAGE_SIZE * ((float)MAZE_Y / (float)MAZE_X), 3, glbuffer_out);
    free(glbuffer);
    free(glbuffer_out);
    glbuffer_out = stbi_load("glOut2.bmp", &px, &py, &pn, 1);
    

    // initialize maze;
    mazeData = (unsigned int*) malloc(sizeof(unsigned int)* PREF_IMAGE_SIZE* PREF_IMAGE_SIZE* (mazeRatio));
    correctPath = (unsigned char*) malloc(PREF_IMAGE_SIZE * PREF_IMAGE_SIZE * (mazeRatio));

    for (int i = 0; i < PREF_IMAGE_SIZE * PREF_IMAGE_SIZE * (mazeRatio); i++) { correctPath[i] = 0; }
    for (int i = 0; i < PREF_IMAGE_SIZE * PREF_IMAGE_SIZE * (mazeRatio); i++) { mazeData[i] = 0; }

    startposx /= 600 / (float)PREF_IMAGE_SIZE;
    startposy /= (600 * ((float)MAZE_Y / (float)MAZE_X)) / (PREF_IMAGE_SIZE * ((float)MAZE_Y / (float)MAZE_X));

    mazeData[startposy * PREF_IMAGE_SIZE + startposx] = 1;
    for (int iteration_count = 0; iteration_count < 250; iteration_count++) {

        for (int i = PREF_IMAGE_SIZE + 1; i < PREF_IMAGE_SIZE * PREF_IMAGE_SIZE * mazeRatio; i++) {
            if (mazeData[i] != 0) {
                // check cardinal directions
                if (glbuffer_out[i - 1] <= 200 && mazeData[i - 1] == 0) { // left
                    mazeData[i - 1] = mazeData[i] + 1;
                }
                if (glbuffer_out[i + 1] <= 200 && mazeData[i + 1] == 0) {// right
                    mazeData[i + 1] = mazeData[i] + 1;
                }
                if (glbuffer_out[PREF_IMAGE_SIZE + i] <= 200 && mazeData[PREF_IMAGE_SIZE + i] == 0) { // up
                    mazeData[i + PREF_IMAGE_SIZE] = mazeData[i] + 1;
                }
                if (glbuffer_out[i - PREF_IMAGE_SIZE] <= 200 && mazeData[i - PREF_IMAGE_SIZE] == 0) { // down
                    mazeData[i - PREF_IMAGE_SIZE] = mazeData[i] + 1;
                }
            }
        }
    }

    std::printf("////////FIRST STAGE/////////\n\n");
    printf("adjusted start position: x = %d y = %d\n", startposx, startposy);
    std::printf("////MAZE DATA////\n");

    ofstream outMaze;
    outMaze.open("maze_data.txt", std::fstream::out);
    for (int i = 0; i < PREF_IMAGE_SIZE * ((float)MAZE_Y / (float)MAZE_X); i++) {
        for (int j = 0; j < PREF_IMAGE_SIZE; j++) {
            printf("%d ", mazeData[(i * PREF_IMAGE_SIZE) + j]);
            outMaze << mazeData[(i * PREF_IMAGE_SIZE) + j] << " ";
            if (mazeData[(i * PREF_IMAGE_SIZE) + j] < 100) {
                outMaze << " ";
                printf(" ");
                if (mazeData[(i * PREF_IMAGE_SIZE) + j] < 10) {
                    outMaze << " ";
                    printf(" ");
                }
            }
        }
        outMaze << "\n";
        printf("\n");
    }
    stbi_write_bmp("maze_data.bmp", PREF_IMAGE_SIZE, IMAGE_SIZEY, 1, mazeData);
    outMaze.close();
    std::printf("////IMAGE_DATA////\n");
    for (int i = 0; i < IMAGE_SIZEY; i++) {
        for (int j = 0; j < IMAGE_SIZEX; j++) {
            std::printf("%d ", glbuffer_out[i * PREF_IMAGE_SIZE + j]);
            if (glbuffer_out[i * PREF_IMAGE_SIZE + j] < 100) {
                std::printf(" ");
                if (glbuffer_out[i * PREF_IMAGE_SIZE + j] < 10) {
                    std::printf(" ");
                }
            }
        }
        std::printf("\n");
    }

    for (int i = 0; i < PREF_IMAGE_SIZE * PREF_IMAGE_SIZE * (mazeRatio); i++) {
        if (mazeData[i] == 0) mazeData[i] = 65535;
    }

    endposx /= 600 / (float)PREF_IMAGE_SIZE;
    endposy /= (600 * ((float)MAZE_Y / (float)MAZE_X)) / (PREF_IMAGE_SIZE * ((float)MAZE_Y / (float)MAZE_X));

    int cursorPos = (endposy * PREF_IMAGE_SIZE) + endposx;
    int counter = 0;

    if (mazeData[endposy * PREF_IMAGE_SIZE + endposx] == 65535) return -1;
    std::printf("maze data at cursor pos pos %d\n", mazeData[cursorPos]);

    for (int i = 0; i < mazeData[endposy * PREF_IMAGE_SIZE + endposx]; i++) {
        if (mazeData[cursorPos - 1] < mazeData[cursorPos]) { // left
            correctPath[cursorPos] = 1;
            cursorPos--;
        }
        if (mazeData[cursorPos + 1] < mazeData[cursorPos]) { // right
            correctPath[cursorPos] = 1;
            cursorPos++;
        }
        if (mazeData[cursorPos - PREF_IMAGE_SIZE] < mazeData[cursorPos]) { // up
            correctPath[cursorPos] = 1;
            cursorPos -= PREF_IMAGE_SIZE;
        }
        if (mazeData[cursorPos + PREF_IMAGE_SIZE] < mazeData[cursorPos]) { // down
            correctPath[cursorPos] = 1;
            cursorPos += PREF_IMAGE_SIZE;
        }
        counter++;

        if (counter == 7) {
            pathPointsX.push_back(cursorPos % PREF_IMAGE_SIZE);
            pathPointsY.push_back(cursorPos / PREF_IMAGE_SIZE * mazeRatio);
            counter = 0;
        }

        // std::printf("%d\n", counter);
    }

    std::printf("\n\n////////SECOND STAGE/////////\n\n");
    printf("adjusted end position: x = %d y = %d\n", endposx, endposy);
    std::printf("////CORRECT PATH DATA////\n");
    for (int i = 0; i < PREF_IMAGE_SIZE * mazeRatio; i++) {
        for (int j = 0; j < PREF_IMAGE_SIZE; j++) {
            std::printf("%d ", correctPath[i * PREF_IMAGE_SIZE + j]);
            if (correctPath[i * PREF_IMAGE_SIZE + j] == 1) {
                correctPath[i * PREF_IMAGE_SIZE + j] = 255;
            }
            else {
                correctPath[i * PREF_IMAGE_SIZE + j] = 0;
            }

        }
        std::printf("\n");
    }
    stbi_write_bmp("correct_path.bmp", PREF_IMAGE_SIZE, PREF_IMAGE_SIZE * (mazeRatio), 1, correctPath);
    std::printf("////CORRECT PATH POINTS////\n");
    // reset correctPath image data
    for (int i = 0; i < PREF_IMAGE_SIZE * PREF_IMAGE_SIZE * mazeRatio; i++) correctPath[i] = 0;
    for (int i = 0; i < pathPointsX.size(); i++) {
        printf("point %d: x = %d y = %d\n", i, pathPointsX[i], pathPointsY[i]);
        correctPath[pathPointsY[i] * IMAGE_SIZEX + pathPointsX[i]] = 255;
    }
    stbi_write_bmp("correct_path_points.bmp", PREF_IMAGE_SIZE, PREF_IMAGE_SIZE * (mazeRatio), 1, correctPath);
    // third stage only creates a list of commands to follow
    // atan2(leftLimbStartPos[i].y - targetPoints[targetIndex].y, leftLimbStartPos[i].x - targetPoints[targetIndex].x);

    double currentDirection = 0; // right
    vector<int> commandType;
    vector<int> commandValue;
    commandType.push_back(0);
    commandValue.push_back(0);
    for (int i = 1; i < pathPointsX.size(); i++) {

        if (abs(atan2(pathPointsY[i - 1] - pathPointsY[i], pathPointsX[i - 1] - pathPointsX[i]) - currentDirection) > glm::radians(9.0f)) {
            commandType.push_back(1);
            if (glm::degrees(atan2(pathPointsY[i - 1] - pathPointsY[i], pathPointsX[i - 1] - pathPointsX[i]) - currentDirection) > 180) {
                commandValue.push_back((360 - glm::degrees(atan2(pathPointsY[i - 1] - pathPointsY[i], pathPointsX[i - 1] - pathPointsX[i]) - currentDirection)) * -1);
            }
            else if (glm::degrees(atan2(pathPointsY[i - 1] - pathPointsY[i], pathPointsX[i - 1] - pathPointsX[i]) - currentDirection) < -180) {
                commandValue.push_back((360 + glm::degrees(atan2(pathPointsY[i - 1] - pathPointsY[i], pathPointsX[i - 1] - pathPointsX[i]) - currentDirection)));
            }
            else {
                commandValue.push_back(glm::degrees(atan2(pathPointsY[i - 1] - pathPointsY[i], pathPointsX[i - 1] - pathPointsX[i]) - currentDirection));
            }
            commandType.push_back(0);
            commandValue.push_back(hypot(pathPointsX[i - 1] - pathPointsX[i], pathPointsY[i - 1] - pathPointsY[i]));

            currentDirection = atan2(pathPointsY[i - 1] - pathPointsY[i], pathPointsX[i - 1] - pathPointsX[i]);
        }
        else {
            commandValue.back() += (hypot(pathPointsX[i - 1] - pathPointsX[i], pathPointsY[i - 1] - pathPointsY[i]));
        }

    }
    for (int i = 0; i < commandType.size(); i++) {
        if (commandType[i] == 1) {
            commandValue[i] *= -1;
        }
    }

    int send_data = 0;
    for (int i = commandType.size() - 1; i > 0; i--) {
        if (commandType[i] == 1) {
            printf("commands %d type: turn, value = %d\n", i, commandValue[i]);
        }
        else {
            printf("commands %d type: forward, value = %d\n", i, commandValue[i]);
        }
        send_data = commandType[i]+1;
        SDLNet_TCP_Send(server, &send_data, sizeof(int));
        send_data = commandValue[i];
        SDLNet_TCP_Send(server, &send_data, sizeof(int));
        Sleep(2000);
    }
    send_data = 3;
    SDLNet_TCP_Send(server, &send_data, sizeof(int));
    SDLNet_Quit();
    
    std::printf("\n\n////////THIRD STAGE/////////\n\n");
    std::printf("////ROBOT PATH DATA////\n");
    for (int i = commandType.size() - 1; i > 0; i--) {
        if (commandType[i] == 1) {
            printf("commands %d type: turn, value = %d\n", i, commandValue[i]);
        }
        else {
            printf("commands %d type: forward, value = %d\n", i, commandValue[i]);
        }
    }
    // glfw init
    

    return 0;
}