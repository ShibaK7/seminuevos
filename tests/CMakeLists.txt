# Pruebas unitarias con QtTest, registradas en ctest:
#   ctest --test-dir <carpeta de build> --output-on-failure
#
# Cada prueba es un ejecutable de consola aparte (sin WIN32: ctest necesita su
# salida estándar y su código de salida) y enlaza SOLO la biblioteca de la capa
# que prueba. Esa es la segunda cerca de la arquitectura: si el dominio usara
# algo de Qt Sql, sus pruebas no enlazarían.

# Carpeta del compilador. Ahí están libstdc++, libgcc y libwinpthread de
# MinGW, sin las que un ejecutable de prueba no arranca.
get_filename_component(SEMINUEVOS_TOOLCHAIN_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)

# seminuevos_add_test(<nombre> SOURCES <archivos>... [LIBS <bibliotecas>...])
function(seminuevos_add_test name)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBS" ${ARGN})
    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "seminuevos_add_test(${name}): falta SOURCES")
    endif()

    add_executable(${name} ${ARG_SOURCES})
    # tests/support: dobles de prueba de los puertos y las vistas. src/ es la
    # raíz de los headers del front, que viven junto a su .cpp.
    target_include_directories(${name} PRIVATE
        "${PROJECT_SOURCE_DIR}/include"
        "${PROJECT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/support"
    )
    target_link_libraries(${name} PRIVATE ${ARG_LIBS} Qt6::Core Qt6::Test)

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

# --- Dominio ---------------------------------------------------------------
seminuevos_add_test(tst_cashpaymentlimit
    SOURCES domain/tst_cashpaymentlimit.cpp
    LIBS seminuevos_domain
)

seminuevos_add_test(tst_acquiredvehicle
    SOURCES domain/tst_acquiredvehicle.cpp domain/vehiclefixtures.h
    LIBS seminuevos_domain
)

seminuevos_add_test(tst_vehicle
    SOURCES domain/tst_vehicle.cpp domain/vehiclefixtures.h
    LIBS seminuevos_domain
)

seminuevos_add_test(tst_vehiclepolymorphism
    SOURCES domain/tst_vehiclepolymorphism.cpp domain/vehiclefixtures.h
    LIBS seminuevos_domain
)

seminuevos_add_test(tst_uploadformatpolicy
    SOURCES domain/tst_uploadformatpolicy.cpp
    LIBS seminuevos_domain
)

# --- Aplicación -----------------------------------------------------------------
seminuevos_add_test(tst_authenticationservice
    SOURCES application/tst_authenticationservice.cpp support/fakes.h
    LIBS seminuevos_application
)

seminuevos_add_test(tst_registrationservice
    SOURCES application/tst_registrationservice.cpp support/fakes.h support/registrationfixtures.h
    LIBS seminuevos_application
)

# --- Adaptadores ---------------------------------------------------------------
seminuevos_add_test(tst_localfilestorage
    SOURCES adapters/tst_localfilestorage.cpp
    LIBS seminuevos_storage
)

# --- Presentación ---------------------------------------------------------------
seminuevos_add_test(tst_wizardnavigator
    SOURCES presentation/tst_wizardnavigator.cpp
    LIBS seminuevos_presentation
)

seminuevos_add_test(tst_loginpresenter
    SOURCES presentation/tst_loginpresenter.cpp support/fakes.h
    LIBS seminuevos_presentation
)

seminuevos_add_test(tst_pooledtaskrunner
    SOURCES presentation/tst_pooledtaskrunner.cpp
    LIBS seminuevos_presentation
)

seminuevos_add_test(tst_inventorypresenter
    SOURCES presentation/tst_inventorypresenter.cpp support/fakes.h
    LIBS seminuevos_presentation
)

seminuevos_add_test(tst_invoiceattachment
    SOURCES presentation/tst_invoiceattachment.cpp
    LIBS seminuevos_presentation
)

seminuevos_add_test(tst_vehiclewizardpresenter
    SOURCES presentation/tst_vehiclewizardpresenter.cpp support/fakes.h support/wizardfakes.h
            support/registrationfixtures.h
    LIBS seminuevos_presentation
)

# Las de vistas enlazan seminuevos_views: widgets reales, sin base ni disco.
seminuevos_add_test(tst_formsupport
    SOURCES presentation/tst_formsupport.cpp
    LIBS seminuevos_views
)

seminuevos_add_test(tst_inventoryview
    SOURCES presentation/tst_inventoryview.cpp support/fakes.h
    LIBS seminuevos_views
)

# Humo del asistente con sus widgets reales.
seminuevos_add_test(tst_vehiclewizardview
    SOURCES presentation/tst_vehiclewizardview.cpp support/fakes.h
    LIBS seminuevos_views
)

# --- Integración con PostgreSQL (opcional) ---------------------------------------
# Necesita `docker compose up -d`. Se activa con -DSEMINUEVOS_DB_TESTS=ON y se
# corre con `ctest -L db`.
option(SEMINUEVOS_DB_TESTS "Pruebas de integración contra PostgreSQL" OFF)
if(SEMINUEVOS_DB_TESTS)
    seminuevos_add_test(tst_registration_db
        SOURCES adapters/tst_registration_db.cpp
        LIBS seminuevos_application seminuevos_persistence seminuevos_storage seminuevos_contract
    )
    set_tests_properties(tst_registration_db PROPERTIES LABELS "db")
    target_compile_definitions(tst_registration_db PRIVATE
        SEMINUEVOS_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
endif()
