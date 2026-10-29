#pragma once

/**
 * @brief Position structure
 *
 * Stores 3D position coordinates and orientation.
 */
struct Position
{
    /**
     * @brief Default constructor
     */
    Position() : x(0.0f), y(0.0f), z(0.0f), o(0.0f) {}
    Position(float _x, float _y, float _z, float _o) : x(_x), y(_y), z(_z), o(_o) {}

    float x; ///< X-coordinate
    float y; ///< Y-coordinate
    float z; ///< Z-coordinate
    float o; ///< Orientation (radians)
};
