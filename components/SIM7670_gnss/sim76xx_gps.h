/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Origin: https://github.com/espressif/esp-idf/blob/master/examples/peripherals/uart/nmea0183_parser/main/nmea_parser.h
 */

#pragma once

#define GPS_MAX_SATELLITES_IN_USE (12)
#define GPS_MAX_SATELLITES_IN_VIEW (16)
#include <string>
#include <sstream>
#include <iomanip>
/**
 * @brief GPS fix type
 *
 */
typedef enum
{
    GPS_FIX_INVALID, /*!< Not fixed */
    GPS_FIX_GPS,     /*!< GPS */
    GPS_FIX_DGPS,    /*!< Differential GPS */
} gps_fix_t;

/**
 * @brief GPS run type
 *
 */
typedef enum
{
    GPS_RUN_INVALID, /*!< Not fixed */
    GPS_RUN_GPS,     /*!< GPS */
} gps_run_t;

/**
 * @brief
 *
 *
 */
typedef enum
{
    GPS_MODE_INVALID, /*!< Not fixed */
    GPS_MODE_2D,      /*!< 2D GPS */
    GPS_MODE_3D       /*!< 3D GPS */
} gps_fix_mode_t;

/**
 * @brief GPS satellite information
 *
 */
typedef struct
{
    uint8_t num; /*!< Satellite number */
} gps_satellite_t;

/**
 * @brief GPS time
 *
 */
typedef struct
{
    uint8_t hour;      /*!< Hour */
    uint8_t minute;    /*!< Minute */
    uint8_t second;    /*!< Second */
    uint16_t thousand; /*!< Thousand */
} gps_time_t;

/**
 * @brief GPS date
 *
 */
typedef struct
{
    uint8_t day;   /*!< Day (start from 1) */
    uint8_t month; /*!< Month (start from 1) */
    uint16_t year; /*!< Year (start from 2000) */
} gps_date_t;

static inline const char *fix_to_s(gps_fix_t v)
{
    switch (v)
    {
    case GPS_FIX_INVALID:
        return "INV";
    case GPS_FIX_GPS:
        return "GPS";
    case GPS_FIX_DGPS:
        return "DGPS";
    default:
        return "?";
    }
}

static inline const char *run_to_s(gps_run_t v)
{
    switch (v)
    {
    case GPS_RUN_INVALID:
        return "INV";
    case GPS_RUN_GPS:
        return "GPS";
    default:
        return "?";
    }
}

static inline const char *mode_to_s(gps_fix_mode_t v)
{
    switch (v)
    {
    case GPS_MODE_INVALID:
        return "INV";
    case GPS_MODE_2D:
        return "2D";
    case GPS_MODE_3D:
        return "3D";
    default:
        return "?";
    }
}

/**
 * @brief GPS object
 *
 */
struct sim76xx_gps
{
    gps_run_t run;           /*!< run status */
    gps_fix_t fix;           /*!< Fix status */
    gps_date_t date;         /*!< Fix date */
    gps_time_t tim;          /*!< time in UTC */
    float latitude;          /*!< Latitude (degrees) */
    float longitude;         /*!< Longitude (degrees) */
    float altitude;          /*!< Altitude (meters) */
    float speed;             /*!< Ground speed, unit: m/s */
    float cog;               /*!< Course over ground */
    gps_fix_mode_t fix_mode; /*!< Fix mode */
    float dop_h;             /*!< Horizontal dilution of precision */
    float dop_p;             /*!< Position dilution of precision  */
    float dop_v;             /*!< Vertical dilution of precision  */
    gps_satellite_t sat;     /*!< Number of satellites in view */
    float hpa;               /*!< Horizontal Position Accuracy  */
    float vpa;               /*!< Vertical Position Accuracy  */
    inline const std::string google_maps_url() const
    {
        std::stringstream ss;
        ss << "https://www.google.com/maps?q=" << latitude << "," << longitude;
        return ss.str();
    }
    inline const std::string packed_string() const
    {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(5); // <= 1-m precision for floats
        ss << fix_to_s(fix) << ","
           << latitude << "," << longitude << "," << altitude << ","
           << speed << "," << cog << ","
           << mode_to_s(fix_mode) << ","
           << dop_h << "," << dop_p << "," << dop_v << ","
           << (int)sat.num << ","
           << hpa << "," << vpa;

        return ss.str();
    } /*!< Vertical Position Accuracy  */
    inline const std::string pretty_string() const
    {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(5); // <= 1-m precision for floats
        // set precision
        ss << "FIX: " << fix_to_s(fix) << "\n"
           << "LAT:" << latitude << " | LON:" << longitude << " | ALT:" << altitude << "\n"
           << "SPD:" << speed << " - COARSE:" << cog << "\n"
           << "ACCU:" << dop_h << "-" << dop_p << "-" << dop_v << "\n"
           << "VPA:" << mode_to_s(fix_mode) << "-" << vpa << "-HPA:" << hpa << "\n"
           << "SAT:" << (int)sat.num << "\n"
           << "LINK: \n"
           << google_maps_url();
        return ss.str();
    }
};

typedef struct sim76xx_gps sim76xx_gps_t;
