#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <algorithm>
#include <array>
#include <cstdlib>
#include <random>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include "shader.h"
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void mouse_button_callback(GLFWwindow* window, int button, int action, int modsdouble);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void RenderUI();
glm::vec3 camPos = glm::vec3(10.0f, 10.0f, 10.0f);
glm::vec3 camFront = glm::vec3(-1.0f, -1.0f, -1.0f);
glm::vec3 camUp = glm::vec3(0.0f, 0.0f, 1.0f);
float sensitivity = 5.0f;
bool focused = false;

bool firstMouse = true;
float yaw = 90.0f;	// yaw is initialized to -90.0 degrees since a yaw of 0.0 results in a direction vector pointing to the right so we initially rotate a bit to the left.
float pitch = 0.0f;
float lastX = 1920.0f / 2.0;
float lastY = 1080.0 / 2.0;
float fov = 45.0f;
// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

float deltaTimeFrame = .0f;
float lastFrame = .0f;

const float PI = 3.14159265359f;

struct Node {
    glm::vec3 p;
    glm::vec3 v;
    float m;
    std::vector<glm::vec3> f;
};

struct Segment {
    glm::vec4 orient;
    Node n1;
    Node n2;
    float currLength;
    float refLength;
    float stretchRatio;
    glm::vec3 tangent;
    glm::vec3 SA_Strain;
    glm::vec3 angularVel;
    glm::vec3 crossSectArea;
    glm::mat3x3 massSecondMOI;
    glm::mat3x3 BT_Transform;
    glm::mat3x3 SS;
};

struct Curve {
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec3> color;
    std::vector<Segment> segments;
};

void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    float camSpeed = static_cast<float>(sensitivity * deltaTimeFrame);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camPos += camSpeed * camFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camPos -= camSpeed * camFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camPos -= camSpeed * glm::normalize(glm::cross(camFront, camUp));
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camPos += camSpeed * glm::normalize(glm::cross(camFront, camUp));
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        camPos += camSpeed * camUp;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        camPos -= camSpeed * camUp;
    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS)
        glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
    if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS)
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS)
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void updateBufferData(unsigned int& bufIndex, std::vector<glm::vec3>& data) {
    glBindBuffer(GL_ARRAY_BUFFER, bufIndex);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3 * data.size(), &data[0], GL_DYNAMIC_DRAW);
}

int main() {
    //set glfw
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "3D Curves for GS", NULL, NULL);
    if (window == NULL) {
        printf("Failed to create GLFW Window\n");
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        printf("Failed to init GLAD");
        return -1;
    }

    int dims[3] = { 1,1,1 };
    float space = 4.0f;
    int len = dims[0] * dims[1] * dims[2];
    Curve curve;

    int rings = 8;
    for (int i = 0; i < 256; i++) {
        float angle = i * 2 * PI / (256.0f / rings);
        float x = glm::cos(angle);
        float y = glm::sin(angle);
        float z = 5.0f - i * 0.01f;
        Segment s{};
        s.n1.p = glm::vec3(x, y, z);
        angle = (i + 1) * 2 * PI / (256.0f / rings);
        x = glm::cos(angle);
        y = glm::sin(angle);
        z = 5.0f - (i + 1) * 0.01f;
        s.n2.p = glm::vec3(x, y, z);
        curve.segments.push_back(s);
        curve.vertices.push_back(s.n1.p);
    }
    

    unsigned int vao, vbo_pos, vbo_col;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo_pos);
    glGenBuffers(1, &vbo_col);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_pos);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3 * curve.vertices.size(), &curve.vertices[0], GL_DYNAMIC_DRAW);
    // position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    /*
    glBindBuffer(GL_ARRAY_BUFFER, vbo_col);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3 * c.size(), &c[0], GL_DYNAMIC_DRAW);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    */

    unsigned int VAO_plane;
    glGenVertexArrays(1, &VAO_plane);

    float axisVertices[] = {
        0.0f,0.0f,0.0f,
        0.0f,0.0f,1.0f,
        0.0f,0.0f,1.0f,
        0.0f,0.0f,1.0f,

        0.0f,0.0f,0.0f,
        0.0f,1.0f,0.0f,
        0.0f,1.0f,0.0f,
        0.0f,1.0f,0.0f,

        0.0f,0.0f,0.0f,
        1.0f,0.0f,0.0f,
        1.0f,0.0f,0.0f,
        1.0f,0.0f,0.0f
    };

    unsigned int VBO_pos_p;
    glGenBuffers(1, &VBO_pos_p);

    glBindVertexArray(VAO_plane);

    glBindBuffer(GL_ARRAY_BUFFER, VBO_pos_p);
    glBufferData(GL_ARRAY_BUFFER, sizeof(axisVertices), &axisVertices[0], GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    
    std::vector<glm::vec3> gridVertices;
    glm::vec3 orig = glm::vec3(-4.0f, -4.0f, 0.0f);
    for (int i = 0; i < 64; i++) {
        glm::vec3 p1(i % 8, i / 8, 0.0f);
        glm::vec3 p2(i % 8 + 1, i / 8, 0.0f);
        glm::vec3 p3(i % 8, i / 8 + 1, 0.0f);
        glm::vec3 p4(i % 8 + 1, i / 8 + 1, 0.0f);

        gridVertices.push_back(orig + p1);
        gridVertices.push_back(orig + p2);

        gridVertices.push_back(orig + p1);
        gridVertices.push_back(orig + p3);

        gridVertices.push_back(orig + p2);
        gridVertices.push_back(orig + p4);

        gridVertices.push_back(orig + p3);
        gridVertices.push_back(orig + p4);
    }

    unsigned int VAO_grid;
    glGenVertexArrays(1, &VAO_grid);

    unsigned int VBO_grid;
    glGenBuffers(1, &VBO_grid);

    glBindVertexArray(VAO_grid);

    glBindBuffer(GL_ARRAY_BUFFER, VBO_grid);
    glBufferData(GL_ARRAY_BUFFER, gridVertices.size() * 3 * sizeof(float), &gridVertices[0], GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    int width, height;
    Shader shader = Shader("C:\\Src\\shaders\\vertCoss.glsl", "C:\\Src\\shaders\\fragCoss.glsl");

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.Colors[ImGuiCol_WindowBg].w = 1.0f;

    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    //io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);

    float radius = 0.3f;
    float orthoScale = 10.0f;
    glm::vec3 lightPos = { 10.0f,10.f,10.f };
    bool ortho = false;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    //ImGui::SetNextWindowPos(viewport->Pos);
    //ImGui::SetNextWindowSize(viewport->Size);
    //ImGui::SetNextWindowViewport(viewport->ID);
    
    static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
    while (!glfwWindowShouldClose(window))
    {
        // input
        // -----
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTimeFrame = currentFrame - lastFrame;
        lastFrame = currentFrame;
        processInput(window);

        // render
        // ------
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.use();
        glm::mat4 view = glm::lookAt(camPos, camPos + camFront, camUp);
        shader.setMat4("view", view);

        glfwGetWindowSize(window, &width, &height);

        float ratio = (float)width / (float)height;
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.01f, 100000.0f);
        if (ortho)
            projection = glm::ortho(-orthoScale * ratio, orthoScale * ratio, -orthoScale, orthoScale, -1000.0f, 1000.0f);

        shader.setMat4("projection", projection);
        glm::mat4 model = glm::mat4(1.0f);
        shader.setMat4("model", model);
        shader.setFloat("rad", radius);
        shader.setVec3("lightPos", lightPos);
        glBindVertexArray(vao);
        glDrawArrays(GL_LINE_STRIP, 0, curve.vertices.size());
        glBindVertexArray(VAO_plane);
        glDrawArrays(GL_LINES, 0, 12);
        glBindVertexArray(VAO_grid);
        glDrawArrays(GL_LINES, 0, gridVertices.size());
        //start of imgui init stuff
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::DockSpaceOverViewport(viewport, dockspace_flags);
      
        ImGui::Begin("Render Settings");
        ImGui::DragFloat3("Light Pos", &lightPos[0], 0.05);
        ImGui::Checkbox("Orthographic", &ortho);
        if (ortho)
            ImGui::DragFloat("Orthographic Scale", &orthoScale, 0.05, 0.01f, 100.0f);
        ImGui::End();
        ImGui::Render();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    //stbi_image_free(data);
    glfwTerminate();
    return 0;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and 
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
    if (focused) {

        float xpos = static_cast<float>(xposIn);
        float ypos = static_cast<float>(yposIn);

        if (firstMouse)
        {
            lastX = xpos;
            lastY = ypos;
            firstMouse = false;
        }

        float xoffset = xpos - lastX;
        float yoffset = ypos - lastY; // reversed since y-coordinates go from bottom to top
        lastX = xpos;
        lastY = ypos;

        float mouseSens = 0.2f;
        xoffset *= mouseSens;
        yoffset *= mouseSens;

        yaw -= xoffset;
        pitch -= yoffset;

        // make sure that when pitch is out of bounds, screen doesn't get flipped
        if (pitch > 89.0f)
            pitch = 89.0f;
        if (pitch < -89.0f)
            pitch = -89.0f;

        glm::vec3 front;
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.z = sin(glm::radians(pitch));
        camFront = glm::normalize(front);

    }
    else {
        float xpos = static_cast<float>(xposIn);
        float ypos = static_cast<float>(yposIn);
        lastX = xpos;
        lastY = ypos;
    }
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int modsdouble)
{
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {

    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
        if (focused)
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        else
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        focused = !focused;
    }
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{

    sensitivity += 0.2f * static_cast<float>(yoffset);
    if (sensitivity < 0) {
        sensitivity = 0.01f;
    }

    fov -= (float)yoffset;
    if (fov < 1.0f)
        fov = 1.0f;
    if (fov > 45.0f)
        fov = 45.0f;
}

