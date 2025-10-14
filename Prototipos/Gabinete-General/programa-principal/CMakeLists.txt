# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS 
    "components/common/module_manager"
    "components/common/ui_framework"
    "components/common/hardware"
    "components/dimmer_module"
    "components/buck_module"
    "components/inverter_module"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(programa-principal)

# Allow multiple definition of UI symbols from different modules
# This is needed because each module has its own UI with similar symbol names
# Each module's UI is self-contained and doesn't interfere with others at runtime
idf_build_set_property(LINK_OPTIONS "-Wl,--allow-multiple-definition" APPEND)
