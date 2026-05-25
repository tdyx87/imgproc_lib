from conan import ConanFile

class ImgprocConan(ConanFile):
    name = "imgproc"
    version = "1.0.0"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"

    def layout(self):
        self.folders.source = "."
        self.folders.build = "build"

    def configure(self):
        # Inject OHOS toolchain when targeting HarmonyOS
        if self.settings.os == "OHOS":
            ohos_ndk = self.conf.get("user.ohos:ndk_path", default=None)
            if not ohos_ndk:
                import os
                ohos_ndk = os.environ.get("OHOS_NDK_HOME", "")
            if ohos_ndk:
                toolchain_path = f"{ohos_ndk}/build/cmake/ohos.toolchain.cmake"
                self.output.info(f"Using OHOS toolchain: {toolchain_path}")
                # Pass toolchain to CMakeToolchain via conf
                self.conf.define("tools.cmake.cmaketoolchain:user_toolchain",
                                 [f"-DCMAKE_TOOLCHAIN_FILE={toolchain_path}"])

    def requirements(self):
        self.requires("libpng/1.6.38")
        self.requires("libjpeg-turbo/2.1.5")
        self.requires("zxing-cpp/2.1.0")
        self.requires("freetype/2.13.0")
        self.requires("zint/2.10.0")

    def build_requirements(self):
        self.tool_requires("ninja/1.11.1")
