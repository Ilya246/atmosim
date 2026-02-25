#include <iostream>
#include <string>
#include <vector>

#include <argparse/args.hpp>
#include <argparse/read.hpp>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>

#include "constants.hpp"
#include "optimiser.hpp"
#include "gas.hpp"
#include "sim.hpp"
#include "utility.hpp"

using namespace std;
using namespace asim;

template<typename T>
T get_input() {
    while (true) {
        while (cin.peek() == '\n') cin.get();
        std::string val_str;
        getline(cin, val_str);
        if (val_str.back() == '\n') val_str.pop_back();
        if(!cin) {
            if (status_SIGINT) throw runtime_error("Got SIGINT");
            cin.clear();
            cout << "Invalid input. Try again: ";
        }
        try {
            T val = argp::parse_value<T>(val_str);
            return val;
        } catch (const argp::read_error& e) {
            cout << "Invalid input. Try again: ";
        }
    }
}

template<typename T>
T input_or_default(const T& default_value) {
    if (cin.get() == '\n') return default_value;
    cin.unget();
    T val = get_input<T>();
    return val;
}

template<typename T>
bool try_input(T& into) {
    if (cin.get() == '\n') return false;
    cin.unget();
    into = get_input<T>();
    return true;
}

struct AtmosimState {
    // Mode Switching
    enum class WorkMode { Normal, Mixing, FullInput, Tolerances };
    WorkMode current_mode = WorkMode::Normal;

    // Normal Mode Input Configuration
    char mix_gases[256] = "plasma,tritium";
    char primer_gases[256] = "oxygen";
    
    float mixt[2] = { 375.15f, 595.15f };
    float thirt[2] = { 293.15f, 293.15f };
    
    // Bounds & Limits
    float pressure_bounds[2] = { 0.0f, 0.0f }; // Initialized to pressure_cap later
    float lower_target_temp = 0.0f; // Initialized to plasma_fire_temp + 0.1f
    int tick_cap = 600; // Sensible default instead of numeric_limits<size_t>::max()
    float ratio_bound = 3.0f;
    
    // Accuracy / Rounding
    float round_temp_to = 0.01f;
    float round_pressure_to = 0.1f;
    float round_ratio_to = 0.001f;
    bool step_target_temp = false;
    
    // Optimizer params
    bool optimise_maximise = true;
    bool optimise_measure_before = false;
    float max_runtime = 3.0f;
    int sample_rounds = 5;
    float bounds_scale = 0.5f;
    int nthreads = 1;

    // Concurrency & Output
    std::atomic<bool> is_running{false};
    std::string output_log = "Ready. Adjust parameters and click 'Run Optimization'.";
    std::mutex log_mutex;
    
    AtmosimState() {
        // Init runtime constants
        pressure_bounds[0] = pressure_cap;
        pressure_bounds[1] = pressure_cap;
        lower_target_temp = plasma_fire_temp + 0.1f;
    }
};

void RunOptimizationJob(AtmosimState* state) {
    std::string local_log;
    try {
        // Format input strings to match the expected argparse list format "[gas1,gas2]"
        std::string mg_str = std::string("[") + state->mix_gases + "]";
        std::string pg_str = std::string("[") + state->primer_gases + "]";

        vector<gas_ref> mix_g = argp::parse_value<vector<gas_ref>>(mg_str);
        vector<gas_ref> primer_g = argp::parse_value<vector<gas_ref>>(pg_str);

        if (mix_g.empty() || primer_g.empty()) {
            throw runtime_error("No mix or primer gases defined.");
        }

        size_t num_mix_ratios = mix_g.size() > 1 ? mix_g.size() - 1 : 0;
        size_t num_primer_ratios = primer_g.size() > 1 ? primer_g.size() - 1 : 0;
        size_t num_ratios = num_mix_ratios + num_primer_ratios;

        vector<float> lower_bounds = { std::min(state->mixt[0], state->thirt[0]), state->mixt[0], state->thirt[0], state->pressure_bounds[0] };
        lower_bounds[0] = std::max(state->lower_target_temp, lower_bounds[0]);
        
        vector<float> upper_bounds = { std::max(state->mixt[1], state->thirt[1]), state->mixt[1], state->thirt[1], state->pressure_bounds[1] };
        if (!state->step_target_temp) {
            upper_bounds[0] = lower_bounds[0];
        }

        for (size_t i = 0; i < num_ratios; ++i) {
            lower_bounds.push_back(-state->ratio_bound);
            upper_bounds.push_back(state->ratio_bound);
        }

        // Constraints and Parameters
        // Note: Pre/Post restrictions omitted from GUI for brevity, empty vectors passed.
        vector<field_restriction<bomb_data>> pre_restrictions, post_restrictions;
        field_ref<bomb_data> opt_param = bomb_data::radius_field; // Hardcoded default for simplicity

        bomb_args b_args{
            mix_g, primer_g, state->optimise_measure_before, 
            state->round_pressure_to, state->round_temp_to, 
            state->round_ratio_to * 0.01f, static_cast<size_t>(state->tick_cap), 
            opt_param, pre_restrictions, post_restrictions
        };

        optimiser<bomb_args, opt_val_wrap> optim(
            do_sim, lower_bounds, upper_bounds, state->optimise_maximise, b_args,
            as_seconds(state->max_runtime), static_cast<size_t>(state->sample_rounds), 
            state->bounds_scale, 2 /* log_level */
        );
        
        optim.n_threads = static_cast<size_t>(state->nthreads);
        optim.find_best();

        // Serialize results
        std::ostringstream oss;
        if (optim.best_result.data != nullptr) {
            oss << "Best Configuration Found:\n" 
                << optim.best_result.data->print_full() << "\n\n"
                << "Serialized string: " << optim.best_result.data->serialize() << "\n\n"
                << default_tol << "x Tolerances:\n" << optim.best_result.data->measure_tolerances();
        } else {
            oss << "No viable recipes found within constraints.";
        }
        local_log = oss.str();

    } catch (const std::exception& e) {
        local_log = std::string("Error during execution: ") + e.what();
    }

    // Synchronize result back to main thread safely
    std::lock_guard<std::mutex> lock(state->log_mutex);
    state->output_log = local_log;
    state->is_running = false;
}

void RenderAtmosimUI(AtmosimState& state) {
    // 1. Get the main viewport bounds (the GLFW window dimensions)
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    // 2. Lock the window and remove traditional floating window decorations
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | 
                                    ImGuiWindowFlags_NoMove | 
                                    ImGuiWindowFlags_NoResize | 
                                    ImGuiWindowFlags_NoSavedSettings | 
                                    ImGuiWindowFlags_MenuBar;

    // 3. Strip window rounding and borders so it sits flush against the OS window edges
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    // Begin the main window
    ImGui::Begin("Atmosim Maxcap Calculator", nullptr, window_flags);
    
    // Pop the style variables immediately so child elements aren't affected
    ImGui::PopStyleVar(2);

    // --- Menu Bar (Optional, since we kept ImGuiWindowFlags_MenuBar) ---
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Exit")) {
                // To exit cleanly, we'd trigger a flag checked by the main loop.
                // Assuming status_SIGINT breaks the loop in your native build:
                status_SIGINT = 1; 
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    // --- Main Content ---
    if (ImGui::BeginTabBar("ModeTabs")) {
        if (ImGui::BeginTabItem("Primary Optimizer")) {
            state.current_mode = AtmosimState::WorkMode::Normal;

            ImGui::Spacing();
            ImGui::Text("Gas Configuration (comma-separated)");
            ImGui::InputText("Mix Gases", state.mix_gases, IM_ARRAYSIZE(state.mix_gases));
            ImGui::InputText("Primer Gases", state.primer_gases, IM_ARRAYSIZE(state.primer_gases));
            
            ImGui::Separator();
            ImGui::Text("Thermodynamic Bounds");
            ImGui::DragFloat2("Mix Temp Bounds (K)", state.mixt, 1.0f, 0.0f, 10000.0f, "%.2f");
            ImGui::DragFloat2("Primer Temp Bounds (K)", state.thirt, 1.0f, 0.0f, 10000.0f, "%.2f");
            ImGui::DragFloat2("Pressure Bounds (kPa)", state.pressure_bounds, 10.0f, 0.0f, 100000.0f, "%.1f");
            
            ImGui::Separator();
            ImGui::Text("Optimizer Settings");
            ImGui::InputFloat("Max Runtime (s)", &state.max_runtime, 0.5f, 1.0f, "%.1f");
            ImGui::InputInt("Sample Rounds", &state.sample_rounds);
            ImGui::InputInt("Threads", &state.nthreads);
            ImGui::InputInt("Tick Cap", &state.tick_cap);
            ImGui::Checkbox("Maximise Result", &state.optimise_maximise);
            ImGui::SameLine();
            ImGui::Checkbox("Step Target Temp", &state.step_target_temp);

            ImGui::Spacing();
            
            // Execution Guard
            ImGui::BeginDisabled(state.is_running);
            if (ImGui::Button("Run Optimization", ImVec2(150, 40))) {
                state.is_running = true;
                {
                    std::lock_guard<std::mutex> lock(state.log_mutex);
                    state.output_log = "Optimizing... (Please wait)";
                }
                
                // Spawn detached worker thread
                std::thread(RunOptimizationJob, &state).detach();
            }
            ImGui::EndDisabled();

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Mixing Tool")) {
            state.current_mode = AtmosimState::WorkMode::Mixing;
            ImGui::Text("Utility tool logic goes here.");
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::Separator();
    ImGui::Text("Output Console");

    // Output area with automatic scrolling and mutex protection
    std::string safe_log;
    {
        std::lock_guard<std::mutex> lock(state.log_mutex);
        safe_log = state.output_log;
    }

    // Dynamic sizing: Fill the remaining vertical space (-1.0f or -FLT_MIN calculates padding automatically)
    ImGui::InputTextMultiline("##output", const_cast<char*>(safe_log.c_str()), safe_log.capacity() + 1, 
                              ImVec2(-FLT_MIN, -FLT_MIN), ImGuiInputTextFlags_ReadOnly);

    ImGui::End();
}

/// @brief Unified main loop callable natively or via Emscripten
void MainLoopStep(void* arg) {
    AtmosimState* state = static_cast<AtmosimState*>(arg);
    GLFWwindow* window = glfwGetCurrentContext();

    glfwPollEvents();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    RenderAtmosimUI(*state);

    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
}

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

int main(int argc, char* argv[]) {
    handle_sigint(); // Ensure standard utility interrupt holds

    if (!glfwInit()) return -1;

    // GL 3.0 + GLSL 130
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window = glfwCreateWindow(1024, 768, "Atmosim", nullptr, nullptr);
    if (!window) return -1;
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    AtmosimState app_state;

#ifdef __EMSCRIPTEN__
    // Note: When building with emscripten, compile with `-s USE_PTHREADS=1` to support std::thread
    emscripten_set_main_loop_arg(MainLoopStep, &app_state, 0, true);
#else
    while (!glfwWindowShouldClose(window) && !status_SIGINT) {
        MainLoopStep(&app_state);
    }
#endif

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}