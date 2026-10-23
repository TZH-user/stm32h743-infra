# =============================================================================
# user_config.cmake - Add your custom source files here
# =============================================================================
#
# This file is included by the generated CMakeLists.txt and allows you to
# add extra source files to the project without modifying generated files
# (which may be overwritten).
#
# To add your own sources, append them to LV_EDITOR_PROJECT_SOURCES:
#
#   list(APPEND LV_EDITOR_PROJECT_SOURCES
#       ${CMAKE_CURRENT_LIST_DIR}/src/my_widget.c
#       ${CMAKE_CURRENT_LIST_DIR}/src/my_screen.c
#   )
#
# Tip:
#   - Use ${CMAKE_CURRENT_LIST_DIR} to get paths relative to this file
#
# =============================================================================

list(APPEND LV_EDITOR_PROJECT_SOURCES
    # ${CMAKE_CURRENT_LIST_DIR}/my_file.c
)

# 仅编辑器预览(Emscripten)构建:编入实机同款中文字库,配合
# signal_separation_ui.c 里的 LV_EDITOR_PREVIEW 分支生效。
# 固件构建不走此分支(字库已在固件 CMake 挂接,避免重复定义)。
if(EMSCRIPTEN)
  list(APPEND LV_EDITOR_PROJECT_SOURCES
      ${CMAKE_CURRENT_LIST_DIR}/../Fonts/font_cn_16.c
  )
endif()
