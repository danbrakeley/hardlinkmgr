# Test suites. One executable per suite: Qt Test runs one QObject test class
# per qExec(), and separate exes give crash isolation and ctest-level
# parallelism. Labels partition the suites:
#   unit   — serverless; run with `ctest -L unit` (or `-LE docker`)
#   docker — needs the Samba fixture (tests/docker/); registered only when
#            docker is on PATH
# Every suite uses HLM_TEST_MAIN from common/TestMain.h instead of QTEST_MAIN.

find_package(Qt6 REQUIRED COMPONENTS Test)

function(hlm_add_test name)
  cmake_parse_arguments(ARG "" "" "SOURCES;LABELS;FIXTURES" ${ARGN})
  qt_add_executable(${name} ${ARG_SOURCES})
  target_link_libraries(${name} PRIVATE hardlinkmgr_core Qt6::Test)
  target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES LABELS "${ARG_LABELS}")
  if(ARG_FIXTURES)
    set_tests_properties(${name} PROPERTIES FIXTURES_REQUIRED "${ARG_FIXTURES}")
  endif()
  if(WIN32)
    # The app's windeployqt run stages everything except Qt6Test.dll (the app
    # doesn't link Test); deploy per test exe so each runs straight from
    # bin/<Config> like the app does. windeployqt only stages the qwindows
    # platform plugin, but the suites run under QT_QPA_PLATFORM=offscreen
    # (see common/TestMain.h) — copy that plugin in ourselves.
    add_custom_command(TARGET ${name} POST_BUILD
      COMMAND Qt6::windeployqt
              --no-translations
              --no-compiler-runtime
              "$<TARGET_FILE:${name}>"
      COMMAND ${CMAKE_COMMAND} -E make_directory
              "$<TARGET_FILE_DIR:${name}>/platforms"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              "$<TARGET_FILE:Qt6::QOffscreenIntegrationPlugin>"
              "$<TARGET_FILE_DIR:${name}>/platforms/"
      VERBATIM)
  endif()
endfunction()

# --- Tier 1: pure unit tests, no server --------------------------------------

hlm_add_test(tst_pathutil             SOURCES unit/tst_pathutil.cpp             LABELS "unit")
hlm_add_test(tst_smbsharespec         SOURCES unit/tst_smbsharespec.cpp         LABELS "unit")
hlm_add_test(tst_matchpairing         SOURCES unit/tst_matchpairing.cpp         LABELS "unit")
hlm_add_test(tst_matchconflicts       SOURCES unit/tst_matchconflicts.cpp       LABELS "unit")
hlm_add_test(tst_filelistmodel        SOURCES unit/tst_filelistmodel.cpp        LABELS "unit")
hlm_add_test(tst_filefilterproxymodel SOURCES unit/tst_filefilterproxymodel.cpp LABELS "unit")
hlm_add_test(tst_matchresultsmodel    SOURCES unit/tst_matchresultsmodel.cpp    LABELS "unit")

# Connect failures that need no server (bogus DNS, unroutable IP).
hlm_add_test(tst_smbsession_offline
  SOURCES integration/tst_smbsession_offline.cpp
  LABELS "unit")

# --- Tier 2/3: suites that need the Samba fixture (label "docker") -----------
#
# ctest starts/stops the container itself via the FIXTURES_SETUP/_CLEANUP pair
# below; `down -v` also wipes the share volume, so every ctest session starts
# pristine. A developer can keep a long-lived fixture instead with
#   docker compose -f tests/docker/docker-compose.yml up -d --build --wait
# — suites only talk SMB and `docker exec` and don't care who started it.

find_program(HLM_DOCKER docker)
if(HLM_DOCKER)
  set(HLM_COMPOSE_FILE ${CMAKE_CURRENT_SOURCE_DIR}/docker/docker-compose.yml)
  add_test(NAME samba_fixture_up
           COMMAND ${HLM_DOCKER} compose -f ${HLM_COMPOSE_FILE} up -d --build --wait)
  add_test(NAME samba_fixture_down
           COMMAND ${HLM_DOCKER} compose -f ${HLM_COMPOSE_FILE} down -v)
  set_tests_properties(samba_fixture_up PROPERTIES
    FIXTURES_SETUP samba
    LABELS "docker"
    TIMEOUT 600)  # first run builds the image
  set_tests_properties(samba_fixture_down PROPERTIES
    FIXTURES_CLEANUP samba
    LABELS "docker")

  hlm_add_test(tst_smbsession
    SOURCES integration/tst_smbsession.cpp common/SmbFixture.cpp
    LABELS "docker"
    FIXTURES samba)

  hlm_add_test(tst_linkrunner
    SOURCES integration/tst_linkrunner.cpp common/SmbFixture.cpp
    LABELS "docker"
    FIXTURES samba)

  hlm_add_test(tst_matchsearcher
    SOURCES integration/tst_matchsearcher.cpp common/SmbFixture.cpp
    LABELS "docker"
    FIXTURES samba)

  hlm_add_test(tst_mainwindow
    SOURCES widget/tst_mainwindow.cpp common/SmbFixture.cpp
    LABELS "docker;widget"
    FIXTURES samba)

  hlm_add_test(tst_filebrowserview
    SOURCES widget/tst_filebrowserview.cpp common/SmbFixture.cpp
    LABELS "docker;widget"
    FIXTURES samba)

  hlm_add_test(tst_hardlinkdialog
    SOURCES widget/tst_hardlinkdialog.cpp common/SmbFixture.cpp
    LABELS "docker;widget"
    FIXTURES samba)

  hlm_add_test(tst_matchfinderpanel
    SOURCES widget/tst_matchfinderpanel.cpp common/SmbFixture.cpp common/TestSupport.cpp
    LABELS "docker;widget"
    FIXTURES samba)
else()
  message(STATUS "docker not found — Samba-backed test suites will not be registered")
endif()
