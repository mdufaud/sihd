#ifndef __SIHD_IMGUI_HPP__
#define __SIHD_IMGUI_HPP__

#include <sihd/imgui/IImguiBackend.hpp>
#include <sihd/imgui/IImguiRenderer.hpp>
#include <sihd/imgui/ImguiBackendNcurses.hpp>
#include <sihd/imgui/ImguiRendererNcurses.hpp>
#include <sihd/imgui/ImguiRunner.hpp>

#if __has_include(<android/configuration.h>)
# include <sihd/imgui/ImguiBackendAndroid.hpp>
#endif
#if __has_include(<GLFW/glfw3.h>)
# include <sihd/imgui/ImguiBackendGlfw.hpp>
#endif
#if __has_include(<SDL3/SDL.h>)
# include <sihd/imgui/ImguiBackendSDL.hpp>
#endif
#if __has_include(<imgui_impl_win32.h>)
# include <sihd/imgui/ImguiBackendWin32.hpp>
#endif
#if __has_include(<d3d11.h>)
# include <sihd/imgui/ImguiRendererDirectX.hpp>
#endif
#if __has_include(<imgui_impl_opengl3.h>)
# include <sihd/imgui/ImguiRendererOpenGL.hpp>
#endif

#endif
