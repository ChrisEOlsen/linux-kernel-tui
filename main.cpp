#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <memory> // Required for std::unique_ptr

using namespace ftxui;
namespace fs = std::filesystem;

std::string read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file) return "";
    std::string line;
    std::getline(file, line);
    return line;
}

class Sensor {
public:
    virtual ~Sensor() = default;
    
    virtual bool is_compatible(const std::string& path) = 0;
    
    virtual Element render(const std::string& path) = 0;
};

class ThermalSensor : public Sensor {
public:
    bool is_compatible(const std::string& path) override {
        // We claim this device if it has a 'temp' file
        return fs::exists(path + "/temp");
    }

    Element render(const std::string& path) override {
        std::string raw = read_file(path + "/temp");
        if (raw.empty()) return text("Error reading temp");

        try {
            float temp = std::stof(raw) / 1000.0f;
            
            // Build the UI component
            auto content = hbox({
                text("Temperature: ") | bold,
                text(std::to_string(temp).substr(0, 4) + " °C") 
                    | color(temp > 60 ? Color::Red : Color::Green)
            });

            // Add sensor type if available
            std::string type = read_file(path + "/type");
            if (!type.empty()) {
                return vbox({ content, text("Sensor Type: " + type) });
            }
            return content;
        } catch (...) {
            return text("Parse Error");
        }
    }
};

class NetworkSensor : public Sensor {
public:
    bool is_compatible(const std::string& path) override {
        return fs::exists(path + "/operstate");
    }

    Element render(const std::string& path) override {
        std::string state = read_file(path + "/operstate");
        auto state_color = (state == "up") ? Color::Green : Color::Red;

        Elements lines;
        lines.push_back(hbox({
            text("Link State: "),
            text(state) | bold | color(state_color)
        }));

        std::string mac = read_file(path + "/address");
        if (!mac.empty()) lines.push_back(text("MAC: " + mac));

        std::string rx = read_file(path + "/statistics/rx_bytes");
        if (!rx.empty()) lines.push_back(text("Data Rx: " + rx + " bytes"));

        return vbox(lines);
    }
};

class PowerSensor : public Sensor {
public:
    bool is_compatible(const std::string& path) override {
        return fs::exists(path + "/capacity");
    }

    Element render(const std::string& path) override {
        std::string cap = read_file(path + "/capacity");
        std::string status = read_file(path + "/status");
        
        return vbox({
            text("Battery Level: " + cap + "%") | bold,
            text("Status: " + status)
        });
    }
};

int main() {
    auto screen = ScreenInteractive::Fullscreen();

    std::vector<std::unique_ptr<Sensor>> drivers;
    drivers.push_back(std::make_unique<ThermalSensor>());
    drivers.push_back(std::make_unique<NetworkSensor>());
    drivers.push_back(std::make_unique<PowerSensor>());

    std::vector<std::pair<std::string, std::string>> categories = {
        {"🔥 Thermals", "/sys/class/thermal"},
        {"🌐 Network",  "/sys/class/net"},
        {"⚡ Power",    "/sys/class/power_supply"},
        {"💡 LEDs",     "/sys/class/leds"}
    };

    int selected_category = 0;
    int selected_device = 0;
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
            devices.push_back("(Category not found)");
        }
        selected_device = 0;
    };
    refresh_devices();

    // Components
    std::vector<std::string> cat_names;
    for(auto& c : categories) cat_names.push_back(c.first);
    
    auto menu_cat = Menu(&cat_names, &selected_category, MenuOption::Vertical());
    auto menu_dev = Menu(&devices, &selected_device, MenuOption::Vertical());

    // Layout & Rendering
    auto layout = Container::Horizontal({ menu_cat, menu_dev });

    auto renderer = Renderer(layout, [&] {
        static int last_cat = -1;
        if (last_cat != selected_category) {
            refresh_devices();
            last_cat = selected_category;
        }

        std::string current_root = categories[selected_category].second;
        std::string current_device = (devices.empty()) ? "" : devices[selected_device];
        std::string full_path = current_root + "/" + current_device;

        Element detail_view = text("No driver matched for this device.") | color(Color::GrayLight);
        
        for (const auto& driver : drivers) {
            if (driver->is_compatible(full_path)) {
                detail_view = driver->render(full_path);
                break; 
            }
        }
        // ---------------------------------------------

        return vbox({
            text(" LINUX KERNEL MONITOR (v2 OOP) ") | bold | hcenter | bgcolor(Color::Blue),
            separator(),
            hbox({
                vbox({ text("SUBSYSTEMS") | bold | hcenter, separator(), menu_cat->Render() }) | border | size(WIDTH, EQUAL, 20),
                vbox({ text("DEVICES") | bold | hcenter, separator(), menu_dev->Render() | vscroll_indicator | frame }) | border | size(WIDTH, EQUAL, 30),
                vbox({ 
                    text(" LIVE METRICS ") | bold | hcenter, 
                    separator(), 
                    detail_view,
                    filler(),
                    text(" Path: " + full_path) | color(Color::GrayLight)
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