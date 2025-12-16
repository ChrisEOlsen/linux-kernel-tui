#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <map>

using namespace ftxui;
namespace fs = std::filesystem;

// 1. Safe File Reader (Reads one line, handles errors)
std::string read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file) return "";
    std::string line;
    std::getline(file, line);
    return line;
}

// 2. The "Smart Probe" - Decides what data matters based on where we are
// This is the "Intelligence" of your system.
Element render_metric(const std::string& parent_path, const std::string& device_name) {
    std::string full_path = parent_path + "/" + device_name;
    std::vector<Element> details;

    // A. THERMAL SENSORS (Look for 'temp')
    if (fs::exists(full_path + "/temp")) {
        std::string raw = read_file(full_path + "/temp");
        if (!raw.empty()) {
            try {
                // Convert millidegrees to degrees
                float temp = std::stof(raw) / 1000.0f;
                details.push_back(hbox({
                    text("Temperature: ") | bold,
                    text(std::to_string(temp).substr(0, 4) + " °C") | color(temp > 60 ? Color::Red : Color::Green)
                }));
                // Check if there is a 'type' label
                std::string type = read_file(full_path + "/type");
                if (!type.empty()) details.push_back(text("Sensor Type: " + type));
            } catch (...) {}
        }
    }

    // B. NETWORK (Look for 'operstate' and 'address')
    else if (fs::exists(full_path + "/operstate")) {
        std::string state = read_file(full_path + "/operstate");
        Color state_color = (state == "up") ? Color::Green : Color::Red;
        
        details.push_back(hbox({
            text("Link State: "),
            text(state) | bold | color(state_color)
        }));
        
        std::string mac = read_file(full_path + "/address");
        if (!mac.empty()) details.push_back(text("MAC Address: " + mac));

        // Simple stat check (rx_bytes)
        std::string rx = read_file(full_path + "/statistics/rx_bytes");
        if (!rx.empty()) details.push_back(text("Total Data Rx: " + rx + " bytes"));
    }

    // C. BATTERY (Look for 'capacity')
    else if (fs::exists(full_path + "/capacity")) {
        std::string cap = read_file(full_path + "/capacity");
        std::string status = read_file(full_path + "/status");
        details.push_back(text("Battery Level: " + cap + "%"));
        details.push_back(text("Status: " + status));
    }

    // Default Fallback
    if (details.empty()) {
        return text("No standard metrics found.") | color(Color::GrayLight);
    }

    return vbox(details);
}

int main() {
    auto screen = ScreenInteractive::Fullscreen();

    // 1. Define only the "Interesting" roots
    // We Map "Readable Name" -> "Actual Path"
    std::vector<std::pair<std::string, std::string>> categories = {
        {"🔥 Thermals", "/sys/class/thermal"},
        {"🌐 Network",  "/sys/class/net"},
        {"⚡ Power",    "/sys/class/power_supply"},
        {"💡 LEDs",     "/sys/class/leds"}
    };

    int selected_category = 0;
    int selected_device = 0;

    // 2. Dynamic Data Loading
    // When category changes, we scan that specific directory
    std::vector<std::string> devices;
    auto refresh_devices = [&]() {
        devices.clear();
        std::string target = categories[selected_category].second;
        if (fs::exists(target)) {
            for (const auto& entry : fs::directory_iterator(target)) {
                devices.push_back(entry.path().filename().string());
            }
            std::sort(devices.begin(), devices.end());
        } else {
            devices.push_back("(Category not found on this system)");
        }
        selected_device = 0; // Reset selection
    };
    
    // Initial load
    refresh_devices();

    // 3. Components
    // Left Menu: Categories
    std::vector<std::string> cat_names;
    for(auto& c : categories) cat_names.push_back(c.first);
    
    auto menu_cat = Menu(&cat_names, &selected_category, MenuOption::Vertical());
    
    // Middle Menu: Devices (The list updates automatically because we pass the pointer &devices)
    auto menu_dev = Menu(&devices, &selected_device, MenuOption::Vertical());

    // 4. Layout
    // We create a container to handle focus between the two menus
    auto layout = Container::Horizontal({
        menu_cat,
        menu_dev,
    });

    // Add logic: When category changes, refresh the device list
    // We wrap the renderer to catch the "Change" event isn't strictly necessary with pointers,
    // but for safety we trigger refresh on render.
    
    auto renderer = Renderer(layout, [&] {
        // HACK: Simple way to keep devices synced to category without complex event logic
        // In a real app, use an onChange callback.
        static int last_cat = -1;
        if (last_cat != selected_category) {
            refresh_devices();
            last_cat = selected_category;
        }

        // Get current paths
        std::string current_root = categories[selected_category].second;
        std::string current_device = (devices.empty()) ? "" : devices[selected_device];

        return vbox({
            text(" LINUX KERNEL MONITOR ") | bold | hcenter | bgcolor(Color::Blue),
            separator(),
            hbox({
                // Column 1: Categories
                vbox({
                    text("SENSORS") | bold | hcenter,
                    separator(),
                    menu_cat->Render()
                }) | border | size(WIDTH, EQUAL, 20),

                // Column 2: Devices
                vbox({
                    text("DEVICES") | bold | hcenter,
                    separator(),
                    menu_dev->Render() | vscroll_indicator | frame 
                }) | border | size(WIDTH, EQUAL, 30),

                // Column 3: The Smart Metrics
                vbox({
                    text(" LIVE DATA ") | bold | hcenter,
                    separator(),
                    render_metric(current_root, current_device),
                    filler(),
                    text(" Path: " + current_root + "/" + current_device) | color(Color::GrayLight)
                }) | border | flex
            }) | flex,
            text(" q: Quit | Arrow Keys: Navigate ") | hcenter
        });
    });

    auto component = CatchEvent(renderer, [&](Event event) {
        if (event == Event::Character('q')) {
            screen.Exit();
            return true;
        }
        return false;
    });

    screen.Loop(component);
    return 0;
}