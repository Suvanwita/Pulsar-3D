#include "pulsar/ui.hpp"

#include "pulsar/app_state.hpp"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include "imgui.h"

namespace pulsar {

namespace {

AppState* getApp(GLFWwindow* window) {
    return static_cast<AppState*>(glfwGetWindowUserPointer(window));
}

void framebufferSizeCB(GLFWwindow* window, int w, int h) {
    AppState* app = getApp(window);
    if (app == nullptr) {
        return;
    }
    app->width = w;
    app->height = h;
    glViewport(0, 0, w, h);
}

void mouseButtonCB(GLFWwindow* window, int button, int action, int) {
    if (ImGui::GetIO().WantCaptureMouse) {
        return;
    }

    AppState* app = getApp(window);
    if (app == nullptr) {
        return;
    }

    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        app->cam.dragging = (action == GLFW_PRESS);
        glfwGetCursorPos(window, &app->cam.lastX, &app->cam.lastY);
    }
}

void cursorPosCB(GLFWwindow* window, double x, double y) {
    if (ImGui::GetIO().WantCaptureMouse) {
        return;
    }

    AppState* app = getApp(window);
    if (app == nullptr || !app->cam.dragging) {
        return;
    }

    double dx = x - app->cam.lastX;
    double dy = y - app->cam.lastY;
    app->cam.lastX = x;
    app->cam.lastY = y;
    app->cam.yaw -= static_cast<float>(dx * 0.005f);
    app->cam.pitch += static_cast<float>(dy * 0.005f);
    app->cam.pitch = glm::clamp(app->cam.pitch, -1.5f, 1.5f);
}

void scrollCB(GLFWwindow* window, double, double dy) {
    if (ImGui::GetIO().WantCaptureMouse) {
        return;
    }

    AppState* app = getApp(window);
    if (app == nullptr) {
        return;
    }

    app->cam.dist -= static_cast<float>(dy) * 0.3f;
    app->cam.dist = glm::clamp(app->cam.dist, 0.8f, 1400.0f);
}

void keyCB(GLFWwindow* window, int key, int, int action, int) {
    if (ImGui::GetIO().WantCaptureKeyboard || action != GLFW_PRESS) {
        return;
    }

    AppState* app = getApp(window);
    if (app == nullptr) {
        return;
    }

    switch (key) {
        case GLFW_KEY_ESCAPE:
            glfwSetWindowShouldClose(window, GLFW_TRUE);
            break;
        case GLFW_KEY_SPACE:
            app->rotating = !app->rotating;
            break;
        case GLFW_KEY_P:
            app->showJets = !app->showJets;
            break;
        case GLFW_KEY_M:
            app->showField = !app->showField;
            break;
        case GLFW_KEY_G:
            app->showGrid = !app->showGrid;
            break;
        case GLFW_KEY_R:
            app->showAxis = !app->showAxis;
            break;
        case GLFW_KEY_EQUAL:
        case GLFW_KEY_KP_ADD:
            app->uiRotSpeedAbs = glm::clamp(app->uiRotSpeedAbs + 0.02f, 0.01f, 0.3f);
            app->rotSpeed = -app->uiRotSpeedAbs;
            break;
        case GLFW_KEY_MINUS:
        case GLFW_KEY_KP_SUBTRACT:
            app->uiRotSpeedAbs = glm::clamp(app->uiRotSpeedAbs - 0.02f, 0.01f, 0.3f);
            app->rotSpeed = -app->uiRotSpeedAbs;
            break;
        case GLFW_KEY_UP:
            app->uiInclDegrees = glm::clamp(app->uiInclDegrees + 5.0f, 0.0f, 100.0f);
            app->inclination = glm::radians(app->uiInclDegrees);
            break;
        case GLFW_KEY_DOWN:
            app->uiInclDegrees = glm::clamp(app->uiInclDegrees - 5.0f, 0.0f, 100.0f);
            app->inclination = glm::radians(app->uiInclDegrees);
            break;
        default:
            break;
    }
}

}  // namespace

void registerWindowCallbacks(GLFWwindow* window, AppState& app) {
    glfwSetWindowUserPointer(window, &app);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCB);
    glfwSetMouseButtonCallback(window, mouseButtonCB);
    glfwSetCursorPosCallback(window, cursorPosCB);
    glfwSetScrollCallback(window, scrollCB);
    glfwSetKeyCallback(window, keyCB);
}

void applySpaceStyle() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 6.0f;
    s.FrameRounding = 4.0f;
    s.GrabRounding = 4.0f;
    s.WindowBorderSize = 1.0f;
    s.FrameBorderSize = 0.0f;
    s.WindowPadding = ImVec2(12.0f, 10.0f);
    s.ItemSpacing = ImVec2(8.0f, 6.0f);
    s.GrabMinSize = 10.0f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.10f, 0.88f);
    c[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.08f, 0.14f, 1.00f);
    c[ImGuiCol_TitleBgActive] = ImVec4(0.10f, 0.10f, 0.20f, 1.00f);
    c[ImGuiCol_FrameBg] = ImVec4(0.14f, 0.14f, 0.22f, 1.00f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.20f, 0.32f, 1.00f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.24f, 0.24f, 0.38f, 1.00f);
    c[ImGuiCol_SliderGrab] = ImVec4(0.27f, 0.75f, 0.40f, 1.00f);
    c[ImGuiCol_SliderGrabActive] = ImVec4(0.35f, 0.90f, 0.50f, 1.00f);
    c[ImGuiCol_Button] = ImVec4(0.15f, 0.45f, 0.80f, 1.00f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.58f, 0.95f, 1.00f);
    c[ImGuiCol_ButtonActive] = ImVec4(0.10f, 0.35f, 0.70f, 1.00f);
    c[ImGuiCol_CheckMark] = ImVec4(0.27f, 0.85f, 0.45f, 1.00f);
    c[ImGuiCol_Header] = ImVec4(0.15f, 0.15f, 0.28f, 1.00f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.22f, 0.22f, 0.38f, 1.00f);
    c[ImGuiCol_Separator] = ImVec4(0.30f, 0.30f, 0.45f, 0.60f);
    c[ImGuiCol_Text] = ImVec4(0.90f, 0.92f, 1.00f, 1.00f);
    c[ImGuiCol_TextDisabled] = ImVec4(0.45f, 0.48f, 0.58f, 1.00f);
    c[ImGuiCol_Border] = ImVec4(0.28f, 0.30f, 0.48f, 0.70f);
}

void drawImGuiPanel(AppState& app) {
    ImGuiIO& io = ImGui::GetIO();
    const float panelW = 210.0f;
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - panelW - 10.0f, 10.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panelW, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.85f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_AlwaysAutoResize;

    if (ImGui::Begin("##controls", nullptr, flags)) {
        ImGui::TextDisabled("Speed:");
        ImGui::PushItemWidth(-1);
        if (ImGui::SliderFloat("##speed", &app.uiRotSpeedAbs, 0.01f, 0.30f, "")) {
            app.rotSpeed = -app.uiRotSpeedAbs;
        }
        ImGui::PopItemWidth();

        ImGui::Spacing();

        ImGui::TextDisabled("Ambient Light:");
        ImGui::PushItemWidth(-1);
        if (ImGui::SliderFloat("##ambient", &app.uiAmbient, 0.1f, 2.5f, "")) {
            app.ambientStr = app.uiAmbient;
        }
        ImGui::PopItemWidth();

        ImGui::Spacing();

        ImGui::TextDisabled("Specular:");
        ImGui::PushItemWidth(-1);
        if (ImGui::SliderFloat("##specular", &app.uiSpecular, 0.0f, 1.5f, "")) {
            app.specularStr = app.uiSpecular;
        }
        ImGui::PopItemWidth();

        ImGui::Spacing();

        ImGui::TextDisabled("Shininess:");
        ImGui::PushItemWidth(-1);
        if (ImGui::SliderFloat("##shininess", &app.uiShininess, 4.0f, 96.0f, "%.0f")) {
            app.shininess = app.uiShininess;
        }
        ImGui::PopItemWidth();

        ImGui::Spacing();

        ImGui::TextDisabled("Emissive Hotspots:");
        ImGui::PushItemWidth(-1);
        if (ImGui::SliderFloat("##emissive", &app.uiEmissive, 0.0f, 3.0f, "")) {
            app.emissiveStr = app.uiEmissive;
        }
        ImGui::PopItemWidth();

        ImGui::Spacing();

        ImGui::TextDisabled("Inclination:");
        ImGui::PushItemWidth(-1);
        if (ImGui::SliderFloat("##incl", &app.uiInclDegrees, 0.0f, 100.0f, "%.0f deg")) {
            app.inclination = glm::radians(app.uiInclDegrees);
        }
        ImGui::PopItemWidth();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.58f, 0.68f, 1.0f));
        ImGui::SetWindowFontScale(0.75f);
        ImGui::Text("0  15  30  45  60  75  90");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Checkbox("Radiation Jets  [P]", &app.showJets);
        ImGui::Checkbox("Rotation Axis   [R]", &app.showAxis);
        ImGui::Checkbox("Magnetic Field  [M]", &app.showField);
        ImGui::Checkbox("Reference Grid  [G]", &app.showGrid);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        const float btnW = (panelW - 36.0f) * 0.5f;

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.70f, 0.15f, 0.15f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.25f, 0.25f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.55f, 0.10f, 0.10f, 1.00f));
        if (ImGui::Button("Reset", ImVec2(btnW, 0.0f))) {
            app.reset();
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.60f, 0.25f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.78f, 0.35f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.10f, 0.45f, 0.18f, 1.00f));
        const char* playLabel = app.rotating ? "Pause [Spc]" : "Play  [Spc]";
        if (ImGui::Button(playLabel, ImVec2(btnW, 0.0f))) {
            app.rotating = !app.rotating;
        }
        ImGui::PopStyleColor(3);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.40f, 0.42f, 0.55f, 1.0f));
        ImGui::SetWindowFontScale(0.80f);
        ImGui::TextWrapped("+/- Speed  Arrows Inclination\nDrag Orbit  Scroll Zoom");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
    }
    ImGui::End();
}

}  // namespace pulsar
