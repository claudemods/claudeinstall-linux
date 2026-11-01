# === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
#
#   SPDX-License-Identifier: BSD-2-Clause
#
claudeinstall_add_plugin(distroprofile
    TYPE job
    EXPORT_MACRO PLUGINDLLEXPORT_PRO
    SOURCES
        DistroProfileJob.cpp
    LINK_PRIVATE_LIBRARIES
        crypt
    REQUIRES
        mount
    WEIGHT 20
    SHARED_LIB
)

# The real login test of the finalize phase: a small PAM client that the job runs inside the
# new system (chroot), so the new system's own PAM stack is what decides, like at the login screen.
set( _logincheck_dir "${CMAKE_INSTALL_LIBDIR}/claudeinstall" )
find_path( PAM_INCLUDE_DIR security/pam_appl.h )
find_library( PAM_LIBRARY pam )
if( PAM_INCLUDE_DIR AND PAM_LIBRARY )
    add_executable( claudeinstall-logincheck logincheck.cpp )
    target_include_directories( claudeinstall-logincheck PRIVATE ${PAM_INCLUDE_DIR} )
    target_link_libraries( claudeinstall-logincheck PRIVATE ${PAM_LIBRARY} )
    install( TARGETS claudeinstall-logincheck RUNTIME DESTINATION ${_logincheck_dir} )
else()
    message( WARNING "PAM development files not found: the real login test (claudeinstall-logincheck) is not built. Install the pam package." )
endif()
target_compile_definitions( claudeinstall_job_distroprofile PRIVATE
    LOGINCHECK_PATH="${CMAKE_INSTALL_PREFIX}/${_logincheck_dir}/claudeinstall-logincheck" )
