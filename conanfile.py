from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.build import check_min_cppstd
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout
from conan.tools.files import get, copy
from conan.tools.scm import Version
import os

required_conan_version = ">=2.10"


class RealTimeMonitorRecipe(ConanFile):
    name = "real-time-monitor"
    version = "0.1.0"
    url = "https://github.com/Genesis-Embodied-AI/real-time-monitor"
    homepage = "https://github.com/Genesis-Embodied-AI/real-time-monitor"
    description = "A real time probe/monitor to debug your software"
    license = "CeCILL-C"
    topics = ("real time, debug")
    package_type = "library"
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {"shared": False, "fPIC": True}

    # Sources are located in the same place as this recipe, copy them to the recipe
    exports_sources = "CMakeLists.txt", "lib/*", "cmake/*"

    # def source(self):
    #     get(self, **self.conan_data["sources"][self.version], strip_root=True)

    def configure(self):
        if self.options.get_safe("shared"):
            self.options.rm_safe("fPIC")

    def layout(self):
        cmake_layout(self)

    def validate(self):
        if self.settings.compiler.get_safe("cppstd"):
            check_min_cppstd(self, 23)

        if self.settings.os not in ["Linux"]:
            raise ConanInvalidConfiguration(
                f"{self.ref} is not supported on {self.settings.os}.")

        if self.settings.compiler != "gcc":
            raise ConanInvalidConfiguration(
                f"{self.ref} is not supported on {self.settings.compiler}.")

        if self.settings.compiler == 'gcc' and Version(self.settings.compiler.version) < "11":
            raise ConanInvalidConfiguration("Building requires GCC >= 11")

    def requirements(self):
        pass


    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["BUILD_MONITOR"] = "OFF"
        tc.cache_variables["BUILD_TOOLS"] = "OFF"
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        src_folders = ["lib/include"]
        for folder in src_folders:
            copy(self, "*.h", os.path.join(self.source_folder, folder),
                os.path.join(self.package_folder, "include"))

        copy(self, "*.a", self.build_folder,
             os.path.join(self.package_folder, "lib"), keep_path=False)
        copy(self, "*.so", self.build_folder,
             os.path.join(self.package_folder, "lib"), keep_path=False)
        copy(self, "LICENSE", self.source_folder,
             os.path.join(self.package_folder, "licenses"))

    def package_info(self):
        self.cpp_info.libs = ["rtm"]
