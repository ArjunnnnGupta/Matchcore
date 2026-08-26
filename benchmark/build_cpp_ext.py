from pybind11.setup_helpers import Pybind11Extension, build_ext
from setuptools import setup

ext_modules = [
    Pybind11Extension(
        "cpp_pipeline",
        ["cpp_pipeline/pipeline.cpp"],
        cxx_std=17,
        extra_compile_args=["-O3"],
    ),
]

setup(
    name="cpp_pipeline",
    ext_modules=ext_modules,
    cmdclass={"build_ext": build_ext},
)
