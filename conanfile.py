from conans import ConanFile, CMake

class AutoDriveConan(ConanFile):
    name = "imaging"
    version = "1.0"
    settings = None
    exports = "README.md","start.sh"
    requires = "lms/2.0@lms/stable","cereal/1.2-0@lms/stable","gtest/1.8.0@lms/stable","urg_network/1.2.0@lms/stable","Eigen3/3.2.8@bilke/stable","kalman/1.0@lms/stable","OpenCV/2.4.13@lms/stable" #"SDL2/2.0.4@lasote/stable"
    generators = "cmake"

    def imports(self):
        self.copy("*.so*",dst=".")
        self.copy("*.a*",dst=".")
        self.copy("*.dylib",dst=".")
        self.copy("bin/*",dst=".")
        self.copy("include/*",dst="")
        self.copy("Eigen/*",dst="include/")
