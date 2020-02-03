/*-----------------------------------------*\
|  PoseidonZRGBController.h                 |
|                                           |
|  Definitions and types for Thermaltake    |
|  Poseidon Z RGB Keyboard lighting         |
|  controller                               |
|                                           |
|  Adam Honse (CalcProgrammer1) 12/25/2019  |
\*-----------------------------------------*/

#include "RGBController.h"

#include <string>
#include <hidapi/hidapi.h>

#pragma once

#define POSEIDONZ_START       0x07
#define POSEIDONZ_PROFILE     0x01
#define POSEIDONZ_LED_CMD     0x0E
#define POSEIDONZ_RED_GRN_CH  0x01
#define POSEIDONZ_BLU_CH      0x02

enum
{
    POSEIDONZ_MODE_STATIC       = 0x00,
    POSEIDONZ_MODE_REACTIVE     = 0x01,
    POSEIDONZ_MODE_ARROW_FLOW   = 0x02,
    POSEIDONZ_MODE_WAVE         = 0x03,
    POSEIDONZ_MODE_RIPPLE       = 0x04
};

enum
{
    POSEIDONZ_PROFILE_P1        = 0x01,
    POSEIDONZ_PROFILE_P2        = 0x02,
    POSEIDONZ_PROFILE_P3        = 0x03,
    POSEIDONZ_PROFILE_P4        = 0x04,
    POSEIDONZ_PROFILE_P5        = 0x05
};

enum
{
    POSEIDONZ_BRIGHTNESS_MIN    = 0x00,
    POSEIDONZ_BRIGHTNESS_MAX    = 0x04
};

enum
{
    POSEIDONZ_COLOR_RED         = 0x01,
    POSEIDONZ_COLOR_GREEN       = 0x02,
    POSEIDONZ_COLOR_BLUE        = 0x03
};

class PoseidonZRGBController
{
public:
    PoseidonZRGBController(hid_device* dev_handle);
    ~PoseidonZRGBController();

    char* GetDeviceName();

    void SetLEDsDirect(std::vector<RGBColor> colors);
    void SetLEDs(std::vector<RGBColor> colors);
    
private:
    char                    device_name[32];
    hid_device*             dev;

    void SendControl
        (
        unsigned char   profile_to_activate,
        unsigned char   profile_to_edit,
        unsigned char   direction,
        unsigned char   mode,
        unsigned char   brightness
        );
    
    void SendColor
        (
        unsigned char   profile_to_edit,
        unsigned char   color_channel,
        unsigned char*  color_data
        );
};
