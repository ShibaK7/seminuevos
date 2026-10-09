# El front (src/presentation) agrupa por pantalla: en la carpeta de cada una
# viven juntos su .ui, su vista (.h/.cpp) y su presenter. Pero sigue partido en
# dos bibliotecas, porque los presenters no deben ver widgets:
#   - seminuevos_presentation (solo QtCore): la lógica de pantalla;
#   - seminuevos_views (Qt Widgets): los widgets y sus formularios.
#
# Como ya no las separa una carpeta, las separa el nombre del archivo. Es
# lógica (y no puede incluir widgets):
#   - un presenter:                 *presenter.h / *presenter.cpp
#   - una interfaz de vista:        i<nombre>view.h   (ILoginView, IStepView…)
#   - la regla de la autofactura:   invoiceattachment.h / .cpp
#   - lo de common/navigation/ y common/tasks/
# Todo lo demás es vista. Y un archivo que tiene su .ui al lado es siempre una
# vista (por eso inventoryview.h, que empieza con "i", no cuenta como interfaz).
#
# Lo usan CMakeLists.txt (para armar las bibliotecas) y la prueba
# `architecture` (para revisar quién incluye a quién): una sola regla para los
# dos.

# Pone en outvar TRUE si `path` (absoluta) es lógica de pantalla.
function(seminuevos_is_front_logic path outvar)
    get_filename_component(name "${path}" NAME)
    get_filename_component(stem "${path}" NAME_WE)
    get_filename_component(dir "${path}" DIRECTORY)

    set(logic FALSE)
    if(name MATCHES "presenter\\.(h|cpp)$"
       OR name MATCHES "^i[a-z]+view\\.h$"
       OR name MATCHES "^invoiceattachment\\.(h|cpp)$"
       OR path MATCHES "/presentation/common/(navigation|tasks)/")
        set(logic TRUE)
    endif()
    if(logic AND EXISTS "${dir}/${stem}.ui")
        set(logic FALSE)
    endif()
    set(${outvar} "${logic}" PARENT_SCOPE)
endfunction()

# Reparte los archivos del front: logic_var recibe los de lógica y views_var
# los widgets, incluidos los .ui.
function(seminuevos_front_files root logic_var views_var)
    file(GLOB_RECURSE front_files CONFIGURE_DEPENDS
        "${root}/src/presentation/*.h"
        "${root}/src/presentation/*.cpp"
        "${root}/src/presentation/*.ui"
    )
    set(logic_files)
    set(view_files)
    foreach(file IN LISTS front_files)
        seminuevos_is_front_logic("${file}" is_logic)
        if(is_logic)
            list(APPEND logic_files "${file}")
        else()
            list(APPEND view_files "${file}")
        endif()
    endforeach()
    set(${logic_var} "${logic_files}" PARENT_SCOPE)
    set(${views_var} "${view_files}" PARENT_SCOPE)
endfunction()
