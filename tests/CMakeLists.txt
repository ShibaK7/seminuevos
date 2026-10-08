# Pruebas unitarias con QtTest, registradas en ctest:
#   ctest --test-dir <carpeta de build> --output-on-failure
#
# Cada prueba es un ejecutable de consola aparte (sin WIN32: ctest necesita su
# salida estándar y su código de salida). Por ahora cada una compila
# directamente las fuentes que prueba, porque todavía no hay bibliotecas por
# capa a las que enlazarse.

# Carpeta del compilador. Ahí están libstdc++, libgcc y libwinpthread de
# MinGW, sin las que un ejecutable de prueba no arranca.
get_filename_component(SEMINUEVOS_TOOLCHAIN_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)

# seminuevos_add_test(<nombre> SOURCES <archivos>...)
function(seminuevos_add_test name)
    cmake_parse_arguments(ARG "" "" "SOURCES" ${ARGN})
    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "seminuevos_add_test(${name}): falta SOURCES")
    endif()

    add_executable(${name} ${ARG_SOURCES})
    target_include_directories(${name} PRIVATE "${PROJECT_SOURCE_DIR}/include")
    target_link_libraries(${name} PRIVATE Qt6::Core Qt6::Test)

    add_test(NAME ${name} COMMAND ${name})
    # En Windows las DLL se buscan en el PATH. Desde Qt Creator la prueba las
    # encuentra porque el IDE arma el PATH, pero desde una consola con ctest
    # no arrancaría sin las de Qt y las de MinGW. offscreen evita que una
    # prueba que llegue a crear widgets intente abrir ventanas.
    #
    # QT_FORCE_STDERR_LOGGING: en Windows, un proceso sin consola propia (como
    # lo lanza ctest desde Git Bash o desde el IDE) manda el reporte de QtTest
    # a OutputDebugString en vez de a la salida estándar, y entonces
    # --output-on-failure no muestra qué comparación falló.
    set_tests_properties(${name} PROPERTIES
        LABELS "unit"
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_FORCE_STDERR_LOGGING=1"
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;PATH=path_list_prepend:${SEMINUEVOS_TOOLCHAIN_BIN}"
    )
endfunction()

seminuevos_add_test(tst_wizardnavigator
    SOURCES
        presentation/tst_wizardnavigator.cpp
        "${PROJECT_SOURCE_DIR}/include/presentation/navigation/wizardnavigator.h"
        "${PROJECT_SOURCE_DIR}/src/presentation/navigation/wizardnavigator.cpp"
)
