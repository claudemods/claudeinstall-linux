# === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
#
#   SPDX-FileCopyrightText: 2020 Adriaan de Groot <groot@kde.org>
#   SPDX-License-Identifier: BSD-2-Clause
#

# When debugging the partitioning widget, or experimenting, you may
# want to allow unsafe partitioning choices (e.g. doing things to the
# current disk). Set DEBUG_PARTITION_UNSAFE to allow that (it turns off
# some filtering of devices). If you **do** allow unsafe partitioning,
# it will error out at runtime unless you **also** switch **off**
# DEBUG_PARTITION_BAIL_OUT, at which point you are welcome to shoot
# yourself in the foot.
#
# Independently, DEBUG_PARTITION_SKIP will not do the actual partitioning
# through KPMCore, but it **will** save the global storage setup as if
# it has done the partitioning. This is going to confuse subsequent
# modules since the partitions on disk won't match GS, but it can be
# useful for debugging simulated installations that don't need to
# mount the target filesystems.
option(DEBUG_PARTITION_UNSAFE "Allow unsafe partitioning choices." OFF)
option(DEBUG_PARTITION_BAIL_OUT "Unsafe partitioning will error out on exec." ON)
option(DEBUG_PARTITION_SKIP "Don't actually do any partitioning." OFF)

# This is very chatty, useful mostly if you don't know what KPMCore offers.
option(DEBUG_FILESYSTEMS "Log all available Filesystems from KPMCore." OFF)

include_directories(${CMAKE_SOURCE_DIR}) # For 3rdparty

set(_partition_defs)
if(DEBUG_PARTITION_UNSAFE)
    if(DEBUG_PARTITION_BAIL_OUT)
        list(APPEND _partition_defs DEBUG_PARTITION_BAIL_OUT)
    endif()
    list(APPEND _partition_defs DEBUG_PARTITION_UNSAFE)
endif()
if(DEBUG_FILESYSTEMS)
    list(APPEND _partition_defs DEBUG_FILESYSTEMS)
endif()
if(DEBUG_PARTITION_SKIP)
    list(APPEND _partition_defs DEBUG_PARTITION_SKIP)
endif()

include(KPMcoreHelper)

if(KPMcore_FOUND)
    include_directories(${PROJECT_BINARY_DIR}/src/libclaudeinstallui)

    add_subdirectory(tests)

    # The headless part: KPMcore wrappers, the models, the partitioning actions,
    # the jobs and the Config. Shared by all frontends. (It needs QtGui for the
    # colours of the models, but nothing of the window system or Widgets.)
    claudeinstall_add_config_library(partition_core
        SOURCES
            Config.cpp

            core/BootLoaderModel.cpp
            core/ColorUtils.cpp
            core/DeviceList.cpp
            core/DeviceModel.cpp
            core/KPMHelpers.cpp
            core/DirFSRestrictLayout.cpp
            core/OsproberEntry.cpp
            core/PartitionActions.cpp
            core/PartitionCoreModule.cpp
            core/PartitionInfo.cpp
            core/PartitionLayout.cpp
            core/PartitionModel.cpp
            core/PartUtils.cpp
            jobs/AutoMountManagementJob.cpp
            jobs/ChangeFilesystemLabelJob.cpp
            jobs/ClearMountsJob.cpp
            jobs/ClearTempMountsJob.cpp
            jobs/CreatePartitionJob.cpp
            jobs/CreatePartitionTableJob.cpp
            jobs/CreateVolumeGroupJob.cpp
            jobs/DeactivateVolumeGroupJob.cpp
            jobs/DeletePartitionJob.cpp
            jobs/FillGlobalStorageJob.cpp
            jobs/FormatPartitionJob.cpp
            jobs/PartitionJob.cpp
            jobs/RemoveVolumeGroupJob.cpp
            jobs/ResizePartitionJob.cpp
            jobs/ResizeVolumeGroupJob.cpp
            jobs/SetPartitionFlagsJob.cpp
        LINK_LIBRARIES
            ${qtname}::Gui
            ${qtname}::Concurrent
            claudeinstall::kpmcore
            ${kfname}::CoreAddons
        DEFINITIONS
            ${_partition_defs}
    )

    claudeinstall_add_plugin(partition
        TYPE viewmodule
        EXPORT_MACRO PLUGINDLLEXPORT_PRO
        SOURCES
            PartitionViewStep.cpp

            gui/BootInfoWidget.cpp
            gui/ChoicePage.cpp
            gui/CreatePartitionDialog.cpp
            gui/CreateVolumeGroupDialog.cpp
            gui/DeviceInfoWidget.cpp
            gui/EditExistingPartitionDialog.cpp
            gui/EncryptWidget.cpp
            gui/ListPhysicalVolumeWidgetItem.cpp
            gui/PartitionPage.cpp
            gui/PartitionBarsView.cpp
            gui/PartitionDialogHelpers.cpp
            gui/PartitionLabelsView.cpp
            gui/PartitionSizeController.cpp
            gui/PartitionSplitterWidget.cpp
            gui/ResizeVolumeGroupDialog.cpp
            gui/ScanningDialog.cpp
            gui/VolumeGroupBaseDialog.cpp
        UI
            gui/ChoicePage.ui
            gui/CreatePartitionDialog.ui
            gui/CreatePartitionTableDialog.ui
            gui/EditExistingPartitionDialog.ui
            gui/EncryptWidget.ui
            gui/PartitionPage.ui
            gui/VolumeGroupBaseDialog.ui
        LINK_PRIVATE_LIBRARIES
            partition_core
            claudeinstall::kpmcore
            ${kfname}::CoreAddons
        COMPILE_DEFINITIONS ${_partition_defs}
        SHARED_LIB
    )

    # The text-mode page (only with WITH_TUI): automatic partitioning (erase disk)
    claudeinstall_add_tui_plugin(partition
        SOURCES
            PartitionTuiStep.cpp
        LINK_LIBRARIES
            partition_core
    )
else()
    if(NOT KPMcore_FOUND)
        claudeinstall_skip_module( "partition (missing suitable KPMcore)" )
    else()
        claudeinstall_skip_module( "partition (missing dependencies for KPMcore)" )
    endif()
endif()
