set(SRC_GRP_MOTION
    Change.h
    Change.cpp
    Authority.h
    Authority.cpp
    PacketMatrix.h
    PacketMatrix.cpp
    PendingChanges.h
    PendingChanges.cpp
    TimeBase.h
    TimeBase.cpp
    Writers.h
    Writers.cpp
    State.h
    State.cpp
    Mobility.h
    Mobility.cpp
    Arbiter.h
    Arbiter.cpp
    MoveIntent.h
    BehaviourModel.h
    SimpleMoves.h
    SimpleMoves.cpp
    DefaultMoves.h
    DefaultMoves.cpp
    PatrolWelding.h
    PatrolWelding.cpp
    TargetKinematics.h
    TargetKinematics.cpp
    TrackingMoves.h
    TrackingMoves.cpp
    ControlMoves.h
    ControlMoves.cpp
    TaxiMove.h
    TaxiMove.cpp
)
source_group("motion" FILES ${SRC_GRP_MOTION})

set(SRC_GRP_MOVEMENT
    movement/JumpArc.h
    movement/MonsterMovePacket.cpp
    movement/MonsterMovePacket.h
    movement/MonsterMovePath.h
    movement/MonsterMoveStop.h
    movement/MoveSpline.cpp
    movement/MoveSpline.h
    movement/MoveSplineFlag.h
    movement/MoveSplineInitArgs.h
    movement/MovementBridge.cpp
    movement/MovementBridge.h
    movement/MovementInfo.cpp
    movement/MovementInfo.h
    movement/WireParity.cpp
    movement/WireParity.h
    movement/packet_builder.cpp
    movement/packet_builder.h
    movement/spline.cpp
    movement/spline.h
    movement/spline.impl.h
    movement/typedefs.h
    movement/util.cpp
)
source_group("motion\\movement" FILES ${SRC_GRP_MOVEMENT})

add_library(motion STATIC ${SRC_GRP_MOTION} ${SRC_GRP_MOVEMENT})

target_include_directories(motion
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(motion
    PUBLIC
        proto
)
