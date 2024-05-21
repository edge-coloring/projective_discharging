#include <iostream>
#include <string>
#include <filesystem>
#include <boost/program_options.hpp>
#include <spdlog/spdlog.h>
#include "cartwheel.hpp"

namespace fs = std::filesystem;
using std::string;

int main(const int ac, const char* const* const av) {
    using namespace boost::program_options;
    options_description description("Options");
    description.add_options()
        ("degree,d", value<string>(), "Hub's degree to generate wheel (subwheel) file")
        ("wheel,w", value<string>(), "The wheel (subwheel) file to evaluate")
        ("conf,c", value<string>(), "The directory which includes configuration files")
        ("send_case,s", value<string>(), "The directory which includes send case (.rule extension)")
        ("rule,r", value<string>(), "The directory which includes rule files")
        ("max_degree,m", value<int>(), "Maximum degree to check (e.g. if you choose degree from {5, 6, 7, 8, 9+}, set max_degree 9)")
        ("outdir,o", value<string>(), "The directory that wheel (subwheel) files are placed")
        ("help,H", "Display options")
        ("verbosity,v", value<int>()->default_value(0), "1 for debug, 2 for trace");

    variables_map vm;
    store(parse_command_line(ac, av, description), vm);
    notify(vm);

    if (vm.count("help")) {
        description.print(std::cout);
        return 0;
    }
    if (vm.count("verbosity")) {
        auto v = vm["verbosity"].as<int>();
        if (v == 1) {
            spdlog::set_level(spdlog::level::debug);
        }
        if (v == 2) {
            spdlog::set_level(spdlog::level::trace);
        }
    }
    if (vm.count("degree")) {
        Degree degree = Degree::fromString(vm["degree"].as<string>());
        if (!vm.count("conf")) {
            spdlog::warn("Specify directory which includes configuration files");
            exit(1);
        }
        if (!vm.count("send_case")) {
            spdlog::warn("Specify directory which includes send_case files");
            exit(1);
        }
        if (!vm.count("max_degree")) {
            spdlog::warn("Specify max_degree");
            exit(1);
        }
        if (!vm.count("outdir")) {
            spdlog::warn("Specify output directory");
            exit(1);
        }
        auto confdir = vm["conf"].as<string>();
        auto send_casedir = vm["send_case"].as<string>();
        int max_degree = vm["max_degree"].as<int>();
        auto outdir = vm["outdir"].as<string>();
        assert(degree.fixed());
        generateWheels(degree.lower(), confdir, send_casedir, max_degree, outdir);
    }
    if (vm.count("wheel")) {
        auto filename = vm["wheel"].as<string>();
        if (!vm.count("rule")) {
            spdlog::warn("Specify directory which includes rule files");
            exit(1);
        }
        if (!vm.count("conf")) {
            spdlog::warn("Specify directory which includes configuration files");
            exit(1);
        }
        if (!vm.count("max_degree")) {
            spdlog::warn("Specify max_degree");
            exit(1);
        }
        auto rulesdir = vm["rule"].as<string>();
        auto confsdir = vm["conf"].as<string>();
        auto casesdir = vm["send_case"].as<string>();
        int max_degree = vm["max_degree"].as<int>();
        if (fs::path(filename).extension() == ".wheel") {
            evaluateWheel(filename, rulesdir, casesdir, confsdir, max_degree);
        } 
    }

    return 0;
}