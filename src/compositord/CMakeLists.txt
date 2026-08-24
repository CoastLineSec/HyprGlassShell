qt_add_library(
    hyprshelld-compositord-activation-classifier
    STATIC
        activation_requirement.cpp
        activation_requirement.h
)

target_include_directories(
    hyprshelld-compositord-activation-classifier
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${PROJECT_SOURCE_DIR}/src"
)

target_link_libraries(
    hyprshelld-compositord-activation-classifier
    PUBLIC
        Qt6::Core
        hyprshelld-hyprland-core
)

qt_add_library(
    hyprshelld-compositord-dormant-v2-core
    STATIC
        authority_cas_reducer.cpp
        authority_cas_reducer.h
        authority_records.cpp
        authority_records.h
        canonical_json.cpp
        canonical_json.h
        desired_migration_reducer.cpp
        desired_migration_reducer.h
        identity.cpp
        identity.h
        ordinary_pending_record.cpp
        ordinary_pending_record.h
        observed_authority_facts.cpp
        observed_authority_facts.h
        settled_v2_pending_observation.cpp
        settled_v2_pending_observation.h
        settled_v2_startup_facts.cpp
        settled_v2_startup_facts.h
        startup_reducer.cpp
        startup_reducer.h
)

target_include_directories(
    hyprshelld-compositord-dormant-v2-core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${PROJECT_SOURCE_DIR}/src"
)

target_link_libraries(
    hyprshelld-compositord-dormant-v2-core
    PUBLIC
        Qt6::Core
        hyprshelld-compositord-activation-classifier
    PRIVATE
        double-conversion::double-conversion
)

# Dormant exact-v1 transaction record codecs used only by migration
# qualification. The active transaction continues to own production bytes.
qt_add_library(
    hyprshelld-compositord-legacy-transaction-records
    STATIC
        legacy_transaction_records.cpp
        legacy_transaction_records.h
)

target_include_directories(
    hyprshelld-compositord-legacy-transaction-records
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${PROJECT_SOURCE_DIR}/src"
)

target_link_libraries(
    hyprshelld-compositord-legacy-transaction-records
    PUBLIC
        Qt6::Core
        hyprshelld-compositord-activation-classifier
)

# Dormant exact-v1 ownership and live-activation bridge codecs used only by
# migration qualification. They decode wire observations and own no
# filesystem descriptors or publication authority.
qt_add_library(
    hyprshelld-compositord-legacy-entrypoint-records
    STATIC
        legacy_entrypoint_records.cpp
        legacy_entrypoint_records.h
)

target_include_directories(
    hyprshelld-compositord-legacy-entrypoint-records
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${PROJECT_SOURCE_DIR}/src"
)

target_link_libraries(
    hyprshelld-compositord-legacy-entrypoint-records
    PUBLIC
        Qt6::Core
        hyprshelld-hyprland-core
)

qt_add_library(
    hyprshelld-compositord-core
    STATIC
        renderer.cpp
        renderer.h
        generation.cpp
        generation.h
        store.cpp
        store.h
        transaction.cpp
        transaction.h
)

target_include_directories(
    hyprshelld-compositord-core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${PROJECT_SOURCE_DIR}/src"
)

target_link_libraries(
    hyprshelld-compositord-core
    PUBLIC
        Qt6::Core
        hyprshelld-hyprland-core
        hyprshelld-compositord-activation-classifier
)

# Dormant qualification leaf. It may reuse the pure renderer and generation
# validation helpers, but nothing in the active compositor links this target.
qt_add_library(
    hyprshelld-compositord-dormant-generation-verifier
    STATIC
        dormant_generation_verifier.cpp
        dormant_generation_verifier.h
)

target_include_directories(
    hyprshelld-compositord-dormant-generation-verifier
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${PROJECT_SOURCE_DIR}/src"
)

target_link_libraries(
    hyprshelld-compositord-dormant-generation-verifier
    PUBLIC
        Qt6::Core
        hyprshelld-compositord-core
)

# Dormant source-only qualification of the at-most-two v2 generation byte
# products referenced by one settled record graph. The helper performs no
# filesystem capture, Store operation, prerequisite, action, or publication,
# and no production target links it.
qt_add_library(
    hyprshelld-compositord-settled-v2-generation-byte-graph
    STATIC
        settled_v2_generation_byte_graph.cpp
        settled_v2_generation_byte_graph.h
)

target_include_directories(
    hyprshelld-compositord-settled-v2-generation-byte-graph
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${PROJECT_SOURCE_DIR}/src"
)

target_link_libraries(
    hyprshelld-compositord-settled-v2-generation-byte-graph
    PUBLIC
        Qt6::Core
        hyprshelld-compositord-dormant-v2-core
    PRIVATE
        hyprshelld-compositord-dormant-generation-verifier
)

# Dormant exact-v1 generation verifier used only by migration
# qualification. It rerenders recovered bytes without touching a store or
# exposing a production publication type.
qt_add_library(
    hyprshelld-compositord-dormant-generation-v1-verifier
    STATIC
        dormant_generation_v1_verifier.cpp
        dormant_generation_v1_verifier.h
)

target_include_directories(
    hyprshelld-compositord-dormant-generation-v1-verifier
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${PROJECT_SOURCE_DIR}/src"
)

target_link_libraries(
    hyprshelld-compositord-dormant-generation-v1-verifier
    PUBLIC
        Qt6::Core
        hyprshelld-compositord-core
)

# Dormant source-only reducer for the complete reachable v1 byte graph. It
# owns no filesystem reader, publication primitive, journal, or runtime path;
# the active compositor deliberately does not link this target.
qt_add_library(
    hyprshelld-compositord-reachable-v1-preflight
    STATIC
        reachable_v1_preflight.cpp
        reachable_v1_preflight.h
)

target_include_directories(
    hyprshelld-compositord-reachable-v1-preflight
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${PROJECT_SOURCE_DIR}/src"
)

target_link_libraries(
    hyprshelld-compositord-reachable-v1-preflight
    PUBLIC
        Qt6::Core
        hyprshelld-compositord-dormant-v2-core
        hyprshelld-compositord-legacy-transaction-records
        hyprshelld-compositord-legacy-entrypoint-records
        hyprshelld-compositord-dormant-generation-v1-verifier
)

# Dormant descriptor-only recapture of the exact reachable-v1 caller graph.
# It owns no Store, writer, parser, journal, action, repair, runtime, or live
# entrypoint path, and the production compositor deliberately does not link it.
qt_add_library(
    hyprshelld-compositord-dormant-v1-filesystem-capture
    STATIC
        dormant_v1_filesystem_capture.cpp
        dormant_v1_filesystem_capture.h
)

target_include_directories(
    hyprshelld-compositord-dormant-v1-filesystem-capture
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${PROJECT_SOURCE_DIR}/src"
)

target_link_libraries(
    hyprshelld-compositord-dormant-v1-filesystem-capture
    PUBLIC
        Qt6::Core
        hyprshelld-compositord-reachable-v1-preflight
)

# Dormant descriptor-relative repeat-observation of the five fixed state
# records. This archive has no parser, Store, writer, transition, action,
# repair, runtime, or publication authority. No production target links or
# installs it; the focused test compiles its implementation separately so the
# deterministic test seam is absent from this archive's ABI.
qt_add_library(
    hyprshelld-compositord-dormant-fixed-record-capture
    STATIC
        dormant_fixed_record_capture.cpp
        dormant_fixed_record_capture.h
)

target_include_directories(
    hyprshelld-compositord-dormant-fixed-record-capture
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${PROJECT_SOURCE_DIR}/src"
)

target_link_libraries(
    hyprshelld-compositord-dormant-fixed-record-capture
    PUBLIC
        Qt6::Core
)

# Dormant pathless repeat-capture of exact immutable v2 generation trees.
# This archive owns no records, parser, renderer, Store, transition, action,
# repair, runtime, publication, or startup-prerequisite edge. No production
# target links or installs it; the focused test compiles the implementation
# separately so its deterministic seams are absent from this archive's ABI.
qt_add_library(
    hyprshelld-compositord-dormant-v2-generation-tree-capture
    STATIC
        dormant_v2_generation_tree_capture.cpp
        dormant_v2_generation_tree_capture.h
)

target_include_directories(
    hyprshelld-compositord-dormant-v2-generation-tree-capture
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${PROJECT_SOURCE_DIR}/src"
)

target_link_libraries(
    hyprshelld-compositord-dormant-v2-generation-tree-capture
    PUBLIC
        Qt6::Core
)

# Dormant pure adapter from one successful fixed-record repeat capture to the
# existing raw observed-authority classifier. It owns no filesystem operation,
# parser authority, Store, transition, prerequisite, repair, action, or
# publication edge, and no production target links or installs it.
qt_add_library(
    hyprshelld-compositord-captured-observed-authority-facts
    STATIC
        captured_observed_authority_facts.cpp
        captured_observed_authority_facts.h
)

target_include_directories(
    hyprshelld-compositord-captured-observed-authority-facts
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${PROJECT_SOURCE_DIR}/src"
)

target_link_libraries(
    hyprshelld-compositord-captured-observed-authority-facts
    PUBLIC
        Qt6::Core
        hyprshelld-compositord-dormant-fixed-record-capture
        hyprshelld-compositord-dormant-v2-core
)

# Dormant pure adapter from one successful fixed-record repeat capture to the
# existing settled-v2 generation content byte graph. Caller-owned generation
# evidence remains outside the capture, and no production target links or
# installs this archive.
qt_add_library(
    hyprshelld-compositord-captured-settled-v2-generation-byte-graph
    STATIC
        captured_settled_v2_generation_byte_graph.cpp
        captured_settled_v2_generation_byte_graph.h
)

target_include_directories(
    hyprshelld-compositord-captured-settled-v2-generation-byte-graph
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${PROJECT_SOURCE_DIR}/src"
)

target_link_libraries(
    hyprshelld-compositord-captured-settled-v2-generation-byte-graph
    PUBLIC
        Qt6::Core
        hyprshelld-compositord-dormant-fixed-record-capture
        hyprshelld-compositord-settled-v2-generation-byte-graph
)

qt_add_library(
    hyprshelld-compositord-service
    STATIC
        activation_backend.cpp
        activation_backend.h
        entrypoint_publisher.cpp
        hyprland_ipc_runtime.cpp
        compositor_service.cpp
        compositor_service.h
        configuration_authority.h
        shared_border_reconciler.cpp
        shared_border_reconciler.h
        shared_spacing_reconciler.cpp
        shared_spacing_reconciler.h
        shared_visual_source.cpp
        shared_visual_source.h
)

target_include_directories(
    hyprshelld-compositord-service
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${PROJECT_SOURCE_DIR}/src"
)

target_link_libraries(
    hyprshelld-compositord-service
    PUBLIC
        Qt6::Core
        Qt6::DBus
        hyprshelld-hyprland-core
)

set(HYPRSHELLD_COMPOSITOR_ADAPTOR_SOURCES)

qt_add_dbus_adaptor(
    HYPRSHELLD_COMPOSITOR_ADAPTOR_SOURCES
    "${PROJECT_SOURCE_DIR}/interfaces/dbus/org.hyprshelld.Compositor1.xml"
    compositor_service.h
    HyprShelld::Compositor::CompositorService
    compositor1_adaptor
    Compositor1Adaptor
)

qt_add_executable(
    hyprshelld-compositord
    main.cpp
    ${HYPRSHELLD_COMPOSITOR_ADAPTOR_SOURCES}
)

target_compile_definitions(
    hyprshelld-compositord
    PRIVATE
        HYPRSHELLD_HYPRLAND_CATALOG_FILE="${CMAKE_INSTALL_FULL_DATADIR}/hyprshelld/hyprland/config-catalog-v1.json"
        HYPRSHELLD_HYPRLAND_ACTION_CATALOG_FILE="${CMAKE_INSTALL_FULL_DATADIR}/hyprshelld/hyprland/action-catalog-v1.json"
        HYPRSHELLD_HYPRLAND_CONFIG_SCHEMA_FILE="${CMAKE_INSTALL_FULL_DATADIR}/hyprshelld/schemas/hyprland/v1/config.schema.json"
)

target_include_directories(
    hyprshelld-compositord
    PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}"
)

target_link_libraries(
    hyprshelld-compositord
    PRIVATE
        Qt6::Core
        Qt6::DBus
        hyprshelld-compositord-core
        hyprshelld-compositord-service
)

install(
    TARGETS hyprshelld-compositord
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
