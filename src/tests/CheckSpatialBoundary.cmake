include("${CMAKE_CURRENT_LIST_DIR}/GateGuards.cmake")
gate_require_source_root(SpatialBoundary)

set(HIERARCHY_HEADERS
    Object/Object.h
    Object/Unit.h
    entities/player/Player.h
    Object/Creature.h
    Object/GameObject.h
    Object/DynamicObject.h
    Object/Corpse.h
    Object/Vehicle.h
    Object/Pet.h
    Object/Totem.h
    Object/TemporarySummon.h)

set(FORBIDDEN_MEMBERS
    GetPositionX GetPositionY GetPositionZ GetOrientation
    GetObjectBoundingRadius GetDistance GetDistance2d GetDistanceZ
    GetDistanceOrder IsWithinDist IsWithinDist2d IsWithinDist3d
    IsWithinDistInMap _IsWithinDist IsInRange IsInRange2d IsInRange3d
    GetAngle HasInArc IsInFront IsInBack IsInFrontInMap IsInBackInMap
    IsInMap IsWithinLOS IsWithinLOSInMap IsPositionValid
    Relocate SetOrientation GetNearPoint GetNearPoint2D GetClosePoint
    GetContactPoint GetRandomPoint UpdateGroundPositionZ
    UpdateAllowedPositionZ GetRespawnCoord SetRespawnCoord ResetRespawnCoord
    GetCombatStartPosition SetCombatStartPosition GetCombatReach
    GetCombatDistance CanReachWithMeleeAttack IsNearWaypoint
    NormalizeRotatedPosition CalculateLocalPositionOf RotateLocalPosition
    GetLocalPositionX GetLocalPositionY GetLocalPositionZ GetLocalOrientation
    GetOrientationFromQuat)

# Decoupling D4l: a header the list names but the tree does not have fails the gate. It used to
# be skipped, so a renamed or moved hierarchy header dropped out of the check and the gate
# stayed green (the D4j review proved it); CheckHeaderReach.cmake fails the same way on a
# header its rules name.
function(spatial_missing_headers HEADERS_VAR OUT_VAR)
  set(MISSING "")
  foreach(HEADER IN LISTS ${HEADERS_VAR})
    if(NOT EXISTS "${SOURCE_ROOT}/src/game/${HEADER}" OR IS_DIRECTORY "${SOURCE_ROOT}/src/game/${HEADER}")
      list(APPEND MISSING "${HEADER}")
    endif()
  endforeach()
  set(${OUT_VAR} "${MISSING}" PARENT_SCOPE)
endfunction()

# Self-test: a present header passes, a renamed one and a directory are reported.
set(SELF_TEST_HEADERS Object/Object.h Object/NoSuchHeader.h Object)
spatial_missing_headers(SELF_TEST_HEADERS SELF_TEST_MISSING)
if(NOT "${SELF_TEST_MISSING}" STREQUAL "Object/NoSuchHeader.h;Object")
  message(FATAL_ERROR "SpatialBoundary self-test failed: [${SELF_TEST_HEADERS}] reported missing "
    "[${SELF_TEST_MISSING}], expected [Object/NoSuchHeader.h;Object]")
endif()

list(LENGTH HIERARCHY_HEADERS HEADER_COUNT)
gate_require_scanned(SpatialBoundary "${HEADER_COUNT}" "headers in HIERARCHY_HEADERS")
spatial_missing_headers(HIERARCHY_HEADERS MISSING_HEADERS)
if(MISSING_HEADERS)
  string(REPLACE ";" "\n  " REPORT "${MISSING_HEADERS}")
  message(FATAL_ERROR
    "SpatialBoundary: a hierarchy header does not exist under src/game, so it would not be checked:\n  ${REPORT}\n"
    "Name the header by its current path in src/tests/CheckSpatialBoundary.cmake (renamed or moved?).")
endif()

set(VIOLATIONS "")

foreach(HEADER IN LISTS HIERARCHY_HEADERS)
  set(PATH "${SOURCE_ROOT}/src/game/${HEADER}")
  file(READ "${PATH}" TEXT)
  foreach(NAME IN LISTS FORBIDDEN_MEMBERS)
    if(TEXT MATCHES "\n[ \t]+[A-Za-z_][A-Za-z_0-9:<>,&\\* \t]*[ \t\\*&]${NAME}[ \t]*\\(")
      list(APPEND VIOLATIONS "${HEADER}: ${NAME}")
    endif()
  endforeach()
endforeach()

file(READ "${SOURCE_ROOT}/src/game/Object/Object.h" OBJECT_H)
foreach(REQUIRED_TEXT
    "Geometry::Placement m_placement"
    "Geometry::Placement const& Where() const")
  string(FIND "${OBJECT_H}" "${REQUIRED_TEXT}" POSITION)
  if(POSITION EQUAL -1)
    list(APPEND VIOLATIONS "Object.h no longer holds the component: ${REQUIRED_TEXT}")
  endif()
endforeach()

if(VIOLATIONS)
  string(REPLACE ";" "\n  " REPORT "${VIOLATIONS}")
  message(FATAL_ERROR
    "Spatial geometry is back on the object hierarchy:\n  ${REPORT}\n"
    "Ask the component instead: obj->Where().DistanceTo(other->Where()).")
endif()

message(STATUS "Spatial boundary intact: the hierarchy owns no geometry")
