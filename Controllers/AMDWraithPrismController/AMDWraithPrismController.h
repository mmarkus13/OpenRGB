/*-----------------------------------------*\
|  AMDWraithPrismController.h               |
|                                           |
|  Definitions and types for AMD Wraith     |
|  Prism lighting controller                |
|                                           |
|  Adam Honse (CalcProgrammer1) 12/6/2019   |
\*-----------------------------------------*/

#include <string>
#include <libusb-1.0/libusb.h>

#pragma once

static const unsigned char max_brightness_fan_logo[] =
{
    0x00,
    0xFF,
    0x7F,
    0xFF
};

static const unsigned char speed_values_fan_logo[][5] =
{
    { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF },                  /* Static                       */
    { 0x96, 0x8C, 0x80, 0x6E, 0x68 },                  /* Color Cycle                  */
    { 0x3C, 0x37, 0x31, 0x2C, 0x26 },                  /* Breathing                    */
};

static const unsigned char max_brightness_ring[] =
{
    0xFF,
    0xFF,
    0x7F,
    0x00,
    0x00,
    0x00,
    0x00,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF
};

static const unsigned char mode_value_ring[] =
{
    0xFF,
    0x03,
    0xFF,
    0x00,
    0x00,
    0x00,
    0x00,
    0x05,
    0xFF,
    0xC3,
    0x4A,
    0x05
};

static const unsigned char speed_values_ring[][5] =
{
    { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF },                   /* Static                       */
    { 0x3C, 0x37, 0x31, 0x2C, 0x26 },                   /* Breathing                    */
    { 0x96, 0x8C, 0x80, 0x6E, 0x68 },                   /* Color Cycle                  */
    { 0x00, 0x00, 0x00, 0x00, 0x00 },                   /*                              */
    { 0x00, 0x00, 0x00, 0x00, 0x00 },                   /*                              */
    { 0x00, 0x00, 0x00, 0x00, 0x00 },                   /*                              */
    { 0x00, 0x00, 0x00, 0x00, 0x00 },                   /*                              */
    { 0x72, 0x68, 0x64, 0x62, 0x61 },                   /* Rainbow                      */
    { 0x00, 0x00, 0x00, 0x00, 0x00 },                   /* Bounce                       */
    { 0x77, 0x74, 0x6E, 0x6B, 0x67 },                   /* Chase                        */
    { 0x77, 0x74, 0x6E, 0x6B, 0x67 },                   /* Swirl                        */
    { 0x00, 0x00, 0x00, 0x00, 0x00 },                   /* Morse Code                   */
};

enum
{
    AMD_WRAITH_PRISM_FAN_LOGO_MODE_STATIC       = 0x01, /* Fan/Logo Static Mode         */
    AMD_WRAITH_PRISM_FAN_LOGO_MODE_COLOR_CYCLE  = 0x02, /* Fan/Logo Color Cycle Mode    */
    AMD_WRAITH_PRISM_FAN_LOGO_MODE_BREATHING    = 0x03, /* Fan/Logo Breathing Mode      */
};

enum
{
    AMD_WRAITH_PRISM_EFFECT_CHANNEL_STATIC      = 0x00, /* Static effect channel        */
    AMD_WRAITH_PRISM_EFFECT_CHANNEL_BREATHING   = 0x01, /* Breathing effect channel     */
    AMD_WRAITH_PRISM_EFFECT_CHANNEL_COLOR_CYCLE = 0x02, /* Color cycle effect channel   */
    AMD_WRAITH_PRISM_EFFECT_CHANNEL_LOGO_LED    = 0x05, /* Logo LED effect channel      */
    AMD_WRAITH_PRISM_EFFECT_CHANNEL_FAN_LED     = 0x06, /* Fan LED effect channel       */
    AMD_WRAITH_PRISM_EFFECT_CHANNEL_RAINBOW     = 0x07, /* Rainbow effect channel       */
    AMD_WRAITH_PRISM_EFFECT_CHANNEL_BOUNCE      = 0x08, /* Bounce effect channel        */
    AMD_WRAITH_PRISM_EFFECT_CHANNEL_CHASE       = 0x09, /* Chase effect channel         */
    AMD_WRAITH_PRISM_EFFECT_CHANNEL_SWIRL       = 0x0A, /* Swirl effect channel         */
    AMD_WRAITH_PRISM_EFFECT_CHANNEL_MORSE       = 0x0B, /* Morse code effect channel    */
};

enum
{
    AMD_WRAITH_PRISM_SPEED_SLOWEST              = 0x00, /* Slowest speed                */
    AMD_WRAITH_PRISM_SPEED_SLOW                 = 0x01, /* Slow speed                   */
    AMD_WRAITH_PRISM_SPEED_NORMAL               = 0x02, /* Normal speed                 */
    AMD_WRAITH_PRISM_SPEED_FAST                 = 0x03, /* Fast speed                   */
    AMD_WRAITH_PRISM_SPEED_FASTEST              = 0x04, /* Fastest speed                */
};

class AMDWraithPrismController
{
public:
    AMDWraithPrismController(libusb_device_handle* dev_handle);
    ~AMDWraithPrismController();

    char* GetDeviceName();

    std::string GetEffectChannelString(unsigned char channel);
    std::string GetFirmwareVersionString();

    void SetRingEffectChannel(unsigned char channel);

    void SetFanMode(unsigned char mode, unsigned char speed, bool random_color);
    void SetFanColor(unsigned char red, unsigned char green, unsigned char blue);

    void SetLogoMode(unsigned char mode, unsigned char speed, bool random_color);
    void SetLogoColor(unsigned char red, unsigned char green, unsigned char blue);

    void SetRingMode(unsigned char mode, unsigned char speed, bool direction, bool random_color);
    void SetRingColor(unsigned char red, unsigned char green, unsigned char blue);

private:
    char                    device_name[32];
    libusb_device_handle*   dev;

    unsigned char           current_fan_mode;
    unsigned char           current_fan_speed;
    bool                    current_fan_random_color;

    unsigned char           current_logo_mode;
    unsigned char           current_logo_speed;
    bool                    current_logo_random_color;

    unsigned char           current_ring_mode;
    unsigned char           current_ring_speed;
    bool                    current_ring_direction;
    bool                    current_ring_random_color;

    void SendEnableCommand();

    void SendApplyCommand();

    void SendEffectChannelUpdate
        (
        unsigned char effect_channel,
        unsigned char speed,
        bool          direction,
        bool          random_color,
        unsigned char mode,
        unsigned char brightness,
        unsigned char red,
        unsigned char green,
        unsigned char blue
        );

    void SendChannelRemap(unsigned char ring_channel, unsigned char logo_channel, unsigned char fan_channel);
};
