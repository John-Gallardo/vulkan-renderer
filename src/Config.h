#pragma once

namespace Config{
inline constexpr int         width              {1000};
inline constexpr int         height             {1000};
inline constexpr const char *title              {"Vulkan Renderer"};
inline constexpr int         version            {1};
inline constexpr int         maxFramesInFlight  {2};
inline constexpr int         maxBindlessTextures{256};
}
