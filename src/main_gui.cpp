#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <sstream>

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

struct AtmosimState {
    enum class WorkMode { Normal, Mixing, FullInput, Tolerances };
    WorkMode current_mode = WorkMode::Normal;

    // --- Primary Optimizer Config ---
    char mix_gases[256] = "plasma,tritium";
    char primer_gases[256] = "oxygen";

    float mixt[2] = { 375.15f, 595.15f };
    float thirt[2] = { 293.15f, 293.15f };
    float pressure_bounds[2] = { 0.0f, 0.0f }; // Init to pressure_cap later
    float lower_target_temp = 0.0f;            // Init to plasma_fire_temp + 0.1f
    float ratio_bound = 3.0f;

    float round_temp_to = 0.01f;
    float round_pressure_to = 0.1f;
    float round_ratio_to = 0.001f;

    char opt_param_name[64] = "radius";
    bool optimise_maximise = true;
    bool optimise_measure_before = false;
    bool step_target_temp = false;

    float max_runtime = 3.0f;
    int sample_rounds = 5;
    float bounds_scale = 0.5f;
    int nthreads = 1;
    int tick_cap = 600;
    int log_level = 2;

    char restrict_pre[256] = "";
    char restrict_post[256] = "";

    std::atomic<bool> is_running{false};
    std::string output_log = "Ready. Adjust parameters and click 'Run Optimization'.";
    std::mutex log_mutex;

    // --- Mixing Tool Config ---
    float mix_perc = 50.0f;
    float mix_t1 = 293.15f;
    float mix_t2 = 293.15f;
    std::string mix_result_log = "";

    // --- Full Input (Simulation) Config ---
    char fi_serial_str[1024] = "";
    std::string fi_result_log = "";

    // --- Tolerances Tool Config ---
    char tol_serial_str[1024] = "";
    float tol_val = 0.95f;
    std::string tol_result_log = "";

    AtmosimState() {
        pressure_bounds[0] = pressure_cap;
        pressure_bounds[1] = pressure_cap;
        lower_target_temp = plasma_fire_temp + 0.1f;
        tol_val = default_tol;
    }
};

void RunOptimizationJob(AtmosimState* state) {
    std::string local_log;
    try {
        std::string mg_str = std::string("[") + state->mix_gases + "]";
        std::string pg_str = std::string("[") + state->primer_gases + "]";
        std::vector<gas_ref> mix_g = argp::parse_value<std::vector<gas_ref>>(mg_str);
        std::vector<gas_ref> primer_g = argp::parse_value<std::vector<gas_ref>>(pg_str);

        if (mix_g.empty() || primer_g.empty()) {
            throw std::runtime_error("No mix or primer gases defined.");
        }

        field_ref<bomb_data> opt_param = bomb_data::radius_field;
        try {
            opt_param = argp::parse_value<field_ref<bomb_data>>(state->opt_param_name);
        } catch (...) {
            throw std::runtime_error(std::string("Invalid Optimization Target Parameter: ") + state->opt_param_name);
        }

        std::vector<field_restriction<bomb_data>> pre_restrictions;
        std::vector<field_restriction<bomb_data>> post_restrictions;
        if (strlen(state->restrict_pre) > 0) {
            pre_restrictions = argp::parse_value<std::vector<field_restriction<bomb_data>>>(state->restrict_pre);
        }
        if (strlen(state->restrict_post) > 0) {
            post_restrictions = argp::parse_value<std::vector<field_restriction<bomb_data>>>(state->restrict_post);
        }

        size_t num_mix_ratios = mix_g.size() > 1 ? mix_g.size() - 1 : 0;
        size_t num_primer_ratios = primer_g.size() > 1 ? primer_g.size() - 1 : 0;
        size_t num_ratios = num_mix_ratios + num_primer_ratios;

        std::vector<float> lower_bounds = { std::min(state->mixt[0], state->thirt[0]), state->mixt[0], state->thirt[0], state->pressure_bounds[0] };
        lower_bounds[0] = std::max(state->lower_target_temp, lower_bounds[0]);

        std::vector<float> upper_bounds = { std::max(state->mixt[1], state->thirt[1]), state->mixt[1], state->thirt[1], state->pressure_bounds[1] };
        if (!state->step_target_temp) upper_bounds[0] = lower_bounds[0];

        for (size_t i = 0; i < num_ratios; ++i) {
            lower_bounds.push_back(-state->ratio_bound);
            upper_bounds.push_back(state->ratio_bound);
        }

        bomb_args b_args{
            mix_g, primer_g, state->optimise_measure_before,
            state->round_pressure_to, state->round_temp_to,
            state->round_ratio_to * 0.01f, static_cast<size_t>(state->tick_cap),
            opt_param, pre_restrictions, post_restrictions
        };

        optimiser<bomb_args, opt_val_wrap> optim(
            do_sim, lower_bounds, upper_bounds, state->optimise_maximise, b_args,
            as_seconds(state->max_runtime), static_cast<size_t>(state->sample_rounds),
            state->bounds_scale, static_cast<size_t>(state->log_level)
        );

        optim.n_threads = static_cast<size_t>(state->nthreads);
        optim.find_best();

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

    std::lock_guard<std::mutex> lock(state->log_mutex);
    state->output_log = local_log;
    state->is_running = false;
}

void RenderOptimizerTab(AtmosimState& state) {
    ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.4f);

    if (ImGui::CollapsingHeader("1. Gas Configuration", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextDisabled("Available Gases: %s", list_gases().c_str());
        ImGui::InputText("Mix Gases (csv)", state.mix_gases, IM_ARRAYSIZE(state.mix_gases));
        ImGui::InputText("Primer Gases (csv)", state.primer_gases, IM_ARRAYSIZE(state.primer_gases));
    }

    if (ImGui::CollapsingHeader("2. Thermodynamic Bounds", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat2("Mix Temp Bounds (K)", state.mixt, 1.0f, 0.0f, 10000.0f, "%.2f");
        ImGui::DragFloat2("Primer Temp Bounds (K)", state.thirt, 1.0f, 0.0f, 10000.0f, "%.2f");
        ImGui::DragFloat2("Pressure Bounds (kPa)", state.pressure_bounds, 10.0f, 0.0f, 100000.0f, "%.1f");
        ImGui::DragFloat("Lower Target Temp", &state.lower_target_temp, 1.0f, 0.0f, 10000.0f, "%.2f");
        ImGui::DragFloat("Ratio Bound Limit", &state.ratio_bound, 0.1f, 0.0f, 100.0f, "%.2f");
    }

    if (ImGui::CollapsingHeader("3. Optimizer Engine", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputText("Target Parameter (e.g. radius, ticks)", state.opt_param_name, IM_ARRAYSIZE(state.opt_param_name));

        ImGui::Checkbox("Maximise Parameter", &state.optimise_maximise);
        ImGui::SameLine(ImGui::GetWindowWidth() * 0.25f);
        ImGui::Checkbox("Measure Before Sim", &state.optimise_measure_before);
        ImGui::SameLine(ImGui::GetWindowWidth() * 0.5f);
        ImGui::Checkbox("Step Target Temp (SLOW)", &state.step_target_temp);

        ImGui::InputFloat("Max Runtime (s)", &state.max_runtime, 0.5f, 1.0f, "%.1f");
        ImGui::InputInt("Sample Rounds", &state.sample_rounds);
        ImGui::InputFloat("Bounds Scale", &state.bounds_scale, 0.1f, 0.01f, "%.2f");
        #ifndef __EMSCRIPTEN__
        ImGui::InputInt("Threads", &state.nthreads);
        #endif
        ImGui::InputInt("Tick Cap Limit", &state.tick_cap);
        ImGui::SliderInt("Log Level", &state.log_level, 0, 5);
    }

    if (ImGui::CollapsingHeader("4. Precision & Advanced Filters")) {
        ImGui::InputFloat("Round Temp To", &state.round_temp_to, 0.01f, 0.1f, "%.4f");
        ImGui::InputFloat("Round Pressure To", &state.round_pressure_to, 0.01f, 0.1f, "%.4f");
        ImGui::InputFloat("Round Ratio To (%)", &state.round_ratio_to, 0.001f, 0.01f, "%.4f");
        ImGui::InputText("Pre-sim Restrictions", state.restrict_pre, IM_ARRAYSIZE(state.restrict_pre));
        ImGui::InputText("Post-sim Restrictions", state.restrict_post, IM_ARRAYSIZE(state.restrict_post));
    }

    ImGui::PopItemWidth();
    ImGui::Spacing();

    ImGui::BeginDisabled(state.is_running);
    if (ImGui::Button("Run Optimization", ImVec2(180, 40))) {
        state.is_running = true;
        {
            std::lock_guard<std::mutex> lock(state.log_mutex);
            state.output_log = "Optimizing... (This may take a while depending on constraints)";
        }
        #ifndef __EMSCRIPTEN__
        std::thread(RunOptimizationJob, &state).detach();
        #else
        RunOptimizationJob(&state);
        #endif
    }
    ImGui::EndDisabled();

    ImGui::Separator();

    std::string safe_log;
    {
        std::lock_guard<std::mutex> lock(state.log_mutex);
        safe_log = state.output_log;
    }

    ImGui::InputTextMultiline("##output", const_cast<char*>(safe_log.c_str()), safe_log.capacity() + 1,
                              ImVec2(-FLT_MIN, -FLT_MIN), ImGuiInputTextFlags_ReadOnly);
}

void RenderMixingTab(AtmosimState& state) {
    ImGui::TextWrapped("Utility to find the true percentage mix when dealing with gases of varying temperatures.");
    ImGui::Spacing();

    ImGui::InputFloat("Desired % of First Gas", &state.mix_perc);
    ImGui::InputFloat("Temp of First Gas (K)", &state.mix_t1);
    ImGui::InputFloat("Temp of Second Gas (K)", &state.mix_t2);

    if (ImGui::Button("Calculate Mixing Ratios", ImVec2(200, 30))) {
        float portion = state.mix_perc * 0.01f;
        float n_ratio = portion / (1.0f - portion) * state.mix_t1 / state.mix_t2;
        float n_perc = 100.0f * n_ratio / (1.0f + n_ratio);
        state.mix_result_log = std::format("Required Volume/Moles Configuration:\n  First Gas: {:.2f}%\n  Second Gas: {:.2f}%", n_perc, 100.0f - n_perc);
    }

    ImGui::Separator();
    ImGui::InputTextMultiline("##mixout", const_cast<char*>(state.mix_result_log.c_str()), state.mix_result_log.capacity() + 1,
                              ImVec2(-FLT_MIN, -FLT_MIN), ImGuiInputTextFlags_ReadOnly);
}

void RenderSimulationTab(AtmosimState& state) {
    ImGui::TextWrapped("Simulate and print every tick of a bomb sequentially using a serialized string.");
    ImGui::Spacing();

    ImGui::InputText("Serialized Bomb String", state.fi_serial_str, IM_ARRAYSIZE(state.fi_serial_str));

    if (ImGui::Button("Simulate Tick-by-Tick", ImVec2(200, 30))) {
        try {
            bomb_data data = bomb_data::deserialize(state.fi_serial_str);
            gas_tank tank = data.tank;

            std::ostringstream oss;
            size_t tick = 1;

            while (true) {
                oss << std::format("[Tick {:<2}] Tank status: {}\n", tick, tank.get_status());
                if (!tank.tick() || tank.state != gas_tank::st_intact) break;
                ++tick;
            }

            const char* state_name = "unknown";
            if (tank.state == gas_tank::st_intact) state_name = "intact";
            else if (tank.state == gas_tank::st_ruptured) state_name = "ruptured";
            else if (tank.state == gas_tank::st_exploded) state_name = "exploded";

            oss << std::format("\nFinal Result:\n  Status: {}\n  State: {}\n  Radius: {:.2f}",
                            tank.get_status(), state_name, tank.calc_radius());

            state.fi_result_log = oss.str();
        } catch (const std::exception& e) {
            state.fi_result_log = std::string("Simulation Error: ") + e.what();
        }
    }

    ImGui::Separator();
    ImGui::InputTextMultiline("##simout", const_cast<char*>(state.fi_result_log.c_str()), state.fi_result_log.capacity() + 1,
                              ImVec2(-FLT_MIN, -FLT_MIN), ImGuiInputTextFlags_ReadOnly);
}

void RenderTolerancesTab(AtmosimState& state) {
    ImGui::TextWrapped("Measure thermodynamic tolerances for an already calculated bomb serialised string.");
    ImGui::Spacing();

    ImGui::InputText("Serialized Bomb String", state.tol_serial_str, IM_ARRAYSIZE(state.tol_serial_str));
    ImGui::InputFloat("Tolerance Range Target", &state.tol_val, 0.01f, 0.05f, "%.3f");

    if (ImGui::Button("Measure Tolerances", ImVec2(200, 30))) {
        try {
            bomb_data data = bomb_data::deserialize(state.tol_serial_str);
            data.ticks = data.tank.tick_n(state.tick_cap);
            data.fin_radius = data.tank.calc_radius();
            data.fin_pressure = data.tank.mix.pressure();

            state.tol_result_log = std::format("Tolerances for Target {}:\n{}", state.tol_val, data.measure_tolerances(state.tol_val));
        } catch (const std::exception& e) {
            state.tol_result_log = std::string("Tolerance Error: ") + e.what();
        }
    }

    ImGui::Separator();
    ImGui::InputTextMultiline("##tolout", const_cast<char*>(state.tol_result_log.c_str()), state.tol_result_log.capacity() + 1,
                              ImVec2(-FLT_MIN, -FLT_MIN), ImGuiInputTextFlags_ReadOnly);
}

void RenderAtmosimUI(AtmosimState& state) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("Atmosim Maxcap Calculator", nullptr, window_flags);
    ImGui::PopStyleVar(2);

    if (ImGui::BeginTabBar("ModeTabs")) {
        if (ImGui::BeginTabItem("Primary Optimizer")) {
            state.current_mode = AtmosimState::WorkMode::Normal;
            RenderOptimizerTab(state);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Mixing Tool")) {
            state.current_mode = AtmosimState::WorkMode::Mixing;
            RenderMixingTab(state);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Simulation Tool")) {
            state.current_mode = AtmosimState::WorkMode::FullInput;
            RenderSimulationTab(state);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Tolerances Tool")) {
            state.current_mode = AtmosimState::WorkMode::Tolerances;
            RenderTolerancesTab(state);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

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
#ifndef __EMSCRIPTEN__
    handle_sigint();
#endif
    if (!glfwInit()) return -1;

#ifdef __EMSCRIPTEN__
    // Explicitly request OpenGL ES 3.0 / WebGL 2
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    GLFWwindow* window = glfwCreateWindow(1024, 768, "Atmosim", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
#ifdef __EMSCRIPTEN__
    // For WebGL / OpenGL ES 3, use "#version 300 es"
    ImGui_ImplOpenGL3_Init("#version 300 es");
#else
    ImGui_ImplOpenGL3_Init("#version 130");
#endif

    AtmosimState app_state;

#ifdef __EMSCRIPTEN__
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
