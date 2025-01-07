#include "hid_km.h"
#include "usb_hid_keys.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>

#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/class/usb_hid.h>

#include <string.h>

typedef struct {
    uint32_t qt_key;
    uint8_t hid_key;
    uint8_t modifier;
} qt_to_hid_mapping_t;

#define HID_REPORT_SIZE_M 5
#define HID_REPORT_SIZE_T 7
#define HID_REPORT_SIZE_K 8

#define HID_MOD_LCTRL  KEY_MOD_LCTRL
#define HID_MOD_LSHIFT KEY_MOD_LSHIFT
#define HID_MOD_LALT   KEY_MOD_LALT
#define HID_MOD_LMETA  KEY_MOD_LMETA
#define HID_MOD_RCTRL  KEY_MOD_RCTRL
#define HID_MOD_RSHIFT KEY_MOD_RSHIFT
#define HID_MOD_RALT   KEY_MOD_RALT
#define HID_MOD_RMETA  KEY_MOD_RMETA
#define DataVarAbs 0x02


// #define HID_USAGE_PAGE_DIGITIZER          0x0D
// #define HID_USAGE_DIGITIZER_TOUCH_SCREEN  0x04
// #define HID_USAGE_DIGITIZER_CONTACT_COUNT 0x54
// #define HID_USAGE_DIGITIZER_CONTACT_ID    0x51

// #define SIMPLE_HID_TOUCH_REPORT_DESC() {                \
//     HID_USAGE_PAGE(0x0D),       /* Usage Page (Digitizer) */ \
//     HID_USAGE(0x04),            /* Usage (Touch Screen) */   \
//     HID_COLLECTION(0x01),       /* Application Collection */ \
//         HID_USAGE(0x42),        /* Usage (Tip Switch) */     \
//         HID_LOGICAL_MIN8(0x00), \
//         HID_LOGICAL_MAX8(0x01), \
//         HID_REPORT_SIZE(1),     \
//         HID_REPORT_COUNT(1),    \
//         HID_INPUT(0x02),        /* Data, Variable, Absolute */ \
//         HID_USAGE(0x30),        /* Usage (X) */              \
//         HID_LOGICAL_MIN16(0x00, 0x00), \
//         HID_LOGICAL_MAX16(0xF4, 0x08), \
//         HID_REPORT_SIZE(16),    \
//         HID_REPORT_COUNT(1),    \
//         HID_INPUT(0x02),        /* Data, Variable, Absolute */ \
//         HID_USAGE(0x31),        /* Usage (Y) */              \
//         HID_LOGICAL_MIN16(0x00, 0x00), \
//         HID_LOGICAL_MAX16(0xBE, 0x05), \
//         HID_REPORT_SIZE(16),    \
//         HID_REPORT_COUNT(1),    \
//         HID_INPUT(0x02),        /* Data, Variable, Absolute */ \
//     HID_END_COLLECTION          \
// }

// #define HID_TOUCH_REPORT_DESC() {                            \
//     HID_USAGE_PAGE(HID_USAGE_PAGE_DIGITIZER),                \
//     HID_USAGE(HID_USAGE_DIGITIZER_TOUCH_SCREEN),             \
//     HID_COLLECTION(HID_COLLECTION_APPLICATION),              \
//         /* Contact Data */                                   \
//         HID_COLLECTION(HID_COLLECTION_LOGICAL),              \
//             HID_USAGE(0x42), /* Tip Switch */                \
//             HID_LOGICAL_MIN8(0x00),                          \
//             HID_LOGICAL_MAX8(0x01),                          \
//             HID_REPORT_SIZE(1),                              \
//             HID_REPORT_COUNT(1),                             \
//             HID_INPUT(DataVarAbs),                           \
//             HID_USAGE(0x32), /* In Range */                  \
//             HID_INPUT(DataVarAbs),                           \
//             HID_REPORT_COUNT(6),                             \
//             HID_REPORT_SIZE(8),                              \
//             HID_INPUT(0x03), /* Constant */                  \
//             HID_USAGE(HID_USAGE_DIGITIZER_CONTACT_ID),       \
//             HID_LOGICAL_MIN8(0x00),                          \
//             HID_LOGICAL_MAX8(0x01),                          \
//             HID_REPORT_COUNT(1),                             \
//             HID_INPUT(DataVarAbs),                           \
//             HID_USAGE_PAGE(0x01), /* Generic Desktop */      \
//             HID_USAGE(0x30), /* X Axis */                    \
//             HID_LOGICAL_MIN16(0x00, 0x00),                   \
//             HID_LOGICAL_MAX16(0xF4, 0x08),                   \
//             HID_REPORT_SIZE(16),                             \
//             HID_REPORT_COUNT(1),                             \
//             HID_INPUT(DataVarAbs),                           \
//             HID_USAGE(0x31), /* Y Axis */                    \
//             HID_LOGICAL_MIN16(0x00, 0x00),                   \
//             HID_LOGICAL_MAX16(0xBE, 0x05),                   \
//             HID_REPORT_SIZE(16),                             \
//             HID_REPORT_COUNT(1),                             \
//             HID_INPUT(DataVarAbs),                           \
//         HID_END_COLLECTION,                                  \
//         HID_USAGE(HID_USAGE_DIGITIZER_CONTACT_COUNT),        \
//         HID_LOGICAL_MIN8(0x00),                              \
//         HID_LOGICAL_MAX8(0x01),                              \
//         HID_REPORT_SIZE(8),                                  \
//         HID_REPORT_COUNT(1),                                 \
//         HID_INPUT(DataVarAbs),                               \
//     HID_END_COLLECTION                                       \
// }

// #define HID_TOUCH_REPORT_DESC() {                            \
//     HID_USAGE_PAGE(HID_USAGE_PAGE_DIGITIZER),                \
//     HID_USAGE(HID_USAGE_DIGITIZER_TOUCH_SCREEN),             \
//     HID_COLLECTION(HID_COLLECTION_APPLICATION),              \
//         /* Contact Count */                                  \
//         HID_USAGE(HID_USAGE_DIGITIZER_CONTACT_COUNT),        \
//         HID_LOGICAL_MIN8(0x00),                              \
//         HID_LOGICAL_MAX8(0x0A), /* Max 10 contacts */        \
//         HID_REPORT_SIZE(8),                                  \
//         HID_REPORT_COUNT(1),                                 \
//         HID_INPUT(0x02), /* Data, Var, Abs */                \
//         \
//         /* Pointer Collection */                            \
//         HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP),               \
//         HID_USAGE(HID_USAGE_GEN_DESKTOP_POINTER),            \
//         HID_COLLECTION(HID_COLLECTION_PHYSICAL),             \
//             /* X Axis */                                     \
//             HID_USAGE(HID_USAGE_GEN_DESKTOP_X),              \
//             HID_LOGICAL_MIN16(0x00, 0x00),                   \
//             HID_LOGICAL_MAX16(0xFF, 0x7F), /* 32767 */       \
//             HID_REPORT_SIZE(16),                             \
//             HID_REPORT_COUNT(1),                             \
//             HID_INPUT(0x02), /* Data, Var, Abs */            \
//             \
//             /* Y Axis */                                     \
//             HID_USAGE(HID_USAGE_GEN_DESKTOP_Y),              \
//             HID_LOGICAL_MIN16(0x00, 0x00),                   \
//             HID_LOGICAL_MAX16(0xFF, 0x7F), /* 32767 */       \
//             HID_REPORT_SIZE(16),                             \
//             HID_REPORT_COUNT(1),                             \
//             HID_INPUT(0x02), /* Data, Var, Abs */            \
//         HID_END_COLLECTION,                                  \
//         \
//         /* Contact ID */                                     \
//         HID_USAGE_PAGE(HID_USAGE_PAGE_DIGITIZER),            \
//         HID_USAGE(HID_USAGE_DIGITIZER_CONTACT_ID),           \
//         HID_LOGICAL_MIN8(0x00),                              \
//         HID_LOGICAL_MAX8(0x0A), /* Max 10 contacts */        \
//         HID_REPORT_SIZE(8),                                  \
//         HID_REPORT_COUNT(1),                                 \
//         HID_INPUT(0x02), /* Data, Var, Abs */                \
//     HID_END_COLLECTION                                       \
// }

static const uint8_t hid_mouse_abs_report_desc[]={
    0x05, 0x01,        /* Usage Page (Generic Desktop) */
    0x09, 0x02,        /* Usage (Mouse) */
    0xa1, 0x01,        /* Collection (Application) */
      0x09, 0x01,      /*   Usage (Pointer) */
      0xa1, 0x00,      /*   Collection (Physical) */

        /* 3개 버튼 (Left, Right, Middle) */
        0x05, 0x09,    /*     Usage Page (Button) */
        0x19, 0x01,    /*     Usage Minimum (Button 1) */
        0x29, 0x03,    /*     Usage Maximum (Button 3) */
        0x15, 0x00,    /*     Logical Minimum (0) */
        0x25, 0x01,    /*     Logical Maximum (1) */
        0x95, 0x03,    /*     Report Count (3 buttons) */
        0x75, 0x01,    /*     Report Size (1 bit) */
        0x81, 0x02,    /*     Input (Data,Var,Abs) */

        /* 버튼 외 나머지 5bit 패딩 */
        0x95, 0x01,    
        0x75, 0x05,
        0x81, 0x03,    /*     Input (Cnst,Var,Abs) */

        /* X Absolute */
        0x05, 0x01,    /*     Usage Page (Generic Desktop) */
        0x09, 0x30,    /*     Usage (X) */
        0x15, 0x00,    /*     Logical Min (0) */
        0x26, 0xFF, 0x7F, /*  Logical Max (32767) */
        0x75, 0x10,    /*     Report Size (16 bits) */
        0x95, 0x01,    /*     Report Count (1) */
        0x81, 0x02,    /*     Input (Data,Var,Abs) */

        /* Y Absolute */
        0x09, 0x31,    /*     Usage (Y) */
        0x15, 0x00,    /*     Logical Min (0) */
        0x26, 0xFF, 0x7F, /*  Logical Max (32767) */
        0x75, 0x10,    /*     Report Size (16 bits) */
        0x95, 0x01,    /*     Report Count (1) */
        0x81, 0x02,    /*     Input (Data,Var,Abs) */

      0xc0,           /*   End Collection (Physical) */
    0xc0              /* End Collection (Application) */
};



const qt_to_hid_mapping_t qt_hid_map[] = {
    // ----------------------------
    // Regular Keys and Symbols
    // ----------------------------
    
    // Space
    {0x20, KEY_SPACE, 0}, // Qt::Key_Space

    // Symbols with Shift modifiers
    {0x21, KEY_1, HID_MOD_LSHIFT}, // Qt::Key_Exclam '!'
    {0x22, KEY_APOSTROPHE, HID_MOD_LSHIFT}, // Qt::Key_QuoteDbl '"'
    {0x23, KEY_3, HID_MOD_LSHIFT}, // Qt::Key_NumberSign '#'
    {0x24, KEY_4, HID_MOD_LSHIFT}, // Qt::Key_Dollar '$'
    {0x25, KEY_5, HID_MOD_LSHIFT}, // Qt::Key_Percent '%'
    {0x26, KEY_7, HID_MOD_LSHIFT}, // Qt::Key_Ampersand '&'
    {0x27, KEY_APOSTROPHE, 0}, // Qt::Key_Apostrophe '''
    {0x28, KEY_9, HID_MOD_LSHIFT}, // Qt::Key_ParenLeft '('
    {0x29, KEY_0, HID_MOD_LSHIFT}, // Qt::Key_ParenRight ')'
    {0x2a, KEY_KPASTERISK, 0}, // Qt::Key_Asterisk '*'
    {0x2b, KEY_EQUAL, HID_MOD_LSHIFT}, // Qt::Key_Plus '+'
    {0x2c, KEY_COMMA, 0}, // Qt::Key_Comma ','
    {0x2d, KEY_MINUS, 0}, // Qt::Key_Minus '-'
    {0x2e, KEY_DOT, 0}, // Qt::Key_Period '.'
    {0x2f, KEY_SLASH, 0}, // Qt::Key_Slash '/'

    // Numbers
    {0x30, KEY_0, 0}, // Qt::Key_0 '0'
    {0x31, KEY_1, 0}, // Qt::Key_1 '1'
    {0x32, KEY_2, 0}, // Qt::Key_2 '2'
    {0x33, KEY_3, 0}, // Qt::Key_3 '3'
    {0x34, KEY_4, 0}, // Qt::Key_4 '4'
    {0x35, KEY_5, 0}, // Qt::Key_5 '5'
    {0x36, KEY_6, 0}, // Qt::Key_6 '6'
    {0x37, KEY_7, 0}, // Qt::Key_7 '7'
    {0x38, KEY_8, 0}, // Qt::Key_8 '8'
    {0x39, KEY_9, 0}, // Qt::Key_9 '9'

    // More Symbols
    {0x3a, KEY_SEMICOLON, HID_MOD_LSHIFT}, // Qt::Key_Colon ':'
    {0x3b, KEY_SEMICOLON, 0}, // Qt::Key_Semicolon ';'
    {0x3c, KEY_COMMA, HID_MOD_LSHIFT}, // Qt::Key_Less '<'
    {0x3d, KEY_EQUAL, 0}, // Qt::Key_Equal '='
    {0x3e, KEY_DOT, HID_MOD_LSHIFT}, // Qt::Key_Greater '>'
    {0x3f, KEY_SLASH, HID_MOD_LSHIFT}, // Qt::Key_Question '?'
    {0x40, KEY_2, HID_MOD_LSHIFT}, // Qt::Key_At '@'
    // Letters
    {0x41, KEY_A, 0}, // Qt::Key_A 'A'
    {0x42, KEY_B, 0}, // Qt::Key_B 'B'
    {0x43, KEY_C, 0}, // Qt::Key_C 'C'
    {0x44, KEY_D, 0}, // Qt::Key_D 'D'
    {0x45, KEY_E, 0}, // Qt::Key_E 'E'
    {0x46, KEY_F, 0}, // Qt::Key_F 'F'
    {0x47, KEY_G, 0}, // Qt::Key_G 'G'
    {0x48, KEY_H, 0}, // Qt::Key_H 'H'
    {0x49, KEY_I, 0}, // Qt::Key_I 'I'
    {0x4a, KEY_J, 0}, // Qt::Key_J 'J'
    {0x4b, KEY_K, 0}, // Qt::Key_K 'K'
    {0x4c, KEY_L, 0}, // Qt::Key_L 'L'
    {0x4d, KEY_M, 0}, // Qt::Key_M 'M'
    {0x4e, KEY_N, 0}, // Qt::Key_N 'N'
    {0x4f, KEY_O, 0}, // Qt::Key_O 'O'
    {0x50, KEY_P, 0}, // Qt::Key_P 'P'
    {0x51, KEY_Q, 0}, // Qt::Key_Q 'Q'
    {0x52, KEY_R, 0}, // Qt::Key_R 'R'
    {0x53, KEY_S, 0}, // Qt::Key_S 'S'
    {0x54, KEY_T, 0}, // Qt::Key_T 'T'
    {0x55, KEY_U, 0}, // Qt::Key_U 'U'
    {0x56, KEY_V, 0}, // Qt::Key_V 'V'
    {0x57, KEY_W, 0}, // Qt::Key_W 'W'
    {0x58, KEY_X, 0}, // Qt::Key_X 'X'
    {0x59, KEY_Y, 0}, // Qt::Key_Y 'Y'
    {0x5a, KEY_Z, 0}, // Qt::Key_Z 'Z'

    // Additional Symbols
    {0x5b, KEY_LEFTBRACE, 0}, // Qt::Key_BracketLeft '['
    {0x5c, KEY_BACKSLASH, 0}, // Qt::Key_Backslash '\'
    {0x5d, KEY_RIGHTBRACE, 0}, // Qt::Key_BracketRight ']'
    {0x5e, KEY_GRAVE, HID_MOD_LSHIFT}, // Qt::Key_AsciiCircum '^'
    {0x5f, KEY_MINUS, HID_MOD_LSHIFT}, // Qt::Key_Underscore '_'
    {0x60, KEY_GRAVE, 0}, // Qt::Key_QuoteLeft '`'

    {0x7b, KEY_LEFTBRACE, HID_MOD_LSHIFT}, // Qt::Key_BraceLeft '{'
    {0x7c, KEY_BACKSLASH, HID_MOD_LSHIFT}, // Qt::Key_Bar '|'
    {0x7d, KEY_RIGHTBRACE, HID_MOD_LSHIFT}, // Qt::Key_BraceRight '}'
    {0x7e, KEY_GRAVE, HID_MOD_LSHIFT}, // Qt::Key_AsciiTilde '~'

    // ----------------------------
    // Function Keys
    // ----------------------------
    {0x01000030, KEY_F1, 0}, // Qt::Key_F1
    {0x01000031, KEY_F2, 0}, // Qt::Key_F2
    {0x01000032, KEY_F3, 0}, // Qt::Key_F3
    {0x01000033, KEY_F4, 0}, // Qt::Key_F4
    {0x01000034, KEY_F5, 0}, // Qt::Key_F5
    {0x01000035, KEY_F6, 0}, // Qt::Key_F6
    {0x01000036, KEY_F7, 0}, // Qt::Key_F7
    {0x01000037, KEY_F8, 0}, // Qt::Key_F8
    {0x01000038, KEY_F9, 0}, // Qt::Key_F9
    {0x01000039, KEY_F10, 0}, // Qt::Key_F10
    {0x0100003a, KEY_F11, 0}, // Qt::Key_F11
    {0x0100003b, KEY_F12, 0}, // Qt::Key_F12
    {0x0100003c, KEY_F13, 0}, // Qt::Key_F13
    {0x0100003d, KEY_F14, 0}, // Qt::Key_F14
    {0x0100003e, KEY_F15, 0}, // Qt::Key_F15
    {0x0100003f, KEY_F16, 0}, // Qt::Key_F16
    {0x01000040, KEY_F17, 0}, // Qt::Key_F17
    {0x01000041, KEY_F18, 0}, // Qt::Key_F18
    {0x01000042, KEY_F19, 0}, // Qt::Key_F19
    {0x01000043, KEY_F20, 0}, // Qt::Key_F20
    {0x01000044, KEY_F21, 0}, // Qt::Key_F21
    {0x01000045, KEY_F22, 0}, // Qt::Key_F22
    {0x01000046, KEY_F23, 0}, // Qt::Key_F23
    {0x01000047, KEY_F24, 0}, // Qt::Key_F24
    {0x01000048, KEY_NONE, 0}, // Qt::Key_F25
    {0x01000049, KEY_NONE, 0}, // Qt::Key_F26
    {0x0100004a, KEY_NONE, 0}, // Qt::Key_F27
    {0x0100004b, KEY_NONE, 0}, // Qt::Key_F28
    {0x0100004c, KEY_NONE, 0}, // Qt::Key_F29
    {0x0100004d, KEY_NONE, 0}, // Qt::Key_F30
    {0x0100004e, KEY_NONE, 0}, // Qt::Key_F31
    {0x0100004f, KEY_NONE, 0}, // Qt::Key_F32
    {0x01000050, KEY_NONE, 0}, // Qt::Key_F33
    {0x01000051, KEY_NONE, 0}, // Qt::Key_F34
    {0x01000052, KEY_NONE, 0}, // Qt::Key_F35

    // ----------------------------
    // Modifier Keys
    // ----------------------------
    {0x01000020, KEY_LEFTSHIFT, HID_MOD_LSHIFT}, // Qt::Key_Shift
    {0x01000021, KEY_LEFTCTRL, HID_MOD_LCTRL},   // Qt::Key_Control
    {0x01000022, KEY_LEFTMETA, HID_MOD_LMETA},   // Qt::Key_Meta
    {0x01000023, KEY_LEFTALT, HID_MOD_LALT},     // Qt::Key_Alt
    {0x01001103, KEY_RIGHTALT, HID_MOD_RALT},    // Qt::Key_AltGr

    // ----------------------------
    // Lock Keys
    // ----------------------------
    {0x01000024, KEY_CAPSLOCK, 0}, // Qt::Key_CapsLock
    {0x01000025, KEY_NUMLOCK, 0}, // Qt::Key_NumLock
    {0x01000026, KEY_SCROLLLOCK, 0}, // Qt::Key_ScrollLock

    // ----------------------------
    // Special Keys
    // ----------------------------
    {0x01000000, KEY_ESC, 0}, // Qt::Key_Escape
    {0x01000001, KEY_TAB, 0}, // Qt::Key_Tab
    {0x01000002, KEY_BACKSPACE, 0}, // Qt::Key_Backtab
    {0x01000003, KEY_BACKSPACE, 0}, // Qt::Key_Backspace
    {0x01000004, KEY_ENTER, 0}, // Qt::Key_Return
    {0x01000005, KEY_KPENTER, 0}, // Qt::Key_Enter
    {0x01000006, KEY_INSERT, 0}, // Qt::Key_Insert
    {0x01000007, KEY_DELETE, 0}, // Qt::Key_Delete
    {0x01000008, KEY_PAUSE, 0}, // Qt::Key_Pause
    {0x01000009, KEY_SYSRQ, 0}, // Qt::Key_Print
    {0x0100000a, KEY_SYSRQ, 0}, // Qt::Key_SysReq
    // {0x0100000b, KEY_CLEAR, 0}, // Qt::Key_Clear

    {0x01000010, KEY_HOME, 0}, // Qt::Key_Home
    {0x01000011, KEY_END, 0}, // Qt::Key_End
    {0x01000012, KEY_LEFT, 0}, // Qt::Key_Left
    {0x01000013, KEY_UP, 0}, // Qt::Key_Up
    {0x01000014, KEY_RIGHT, 0}, // Qt::Key_Right
    {0x01000015, KEY_DOWN, 0}, // Qt::Key_Down
    {0x01000016, KEY_PAGEUP, 0}, // Qt::Key_PageUp
    {0x01000017, KEY_PAGEDOWN, 0}, // Qt::Key_PageDown

    {0x01000053, KEY_FRONT, 0}, // Qt::Key_Super_L
    {0x01000054, KEY_FRONT, 0}, // Qt::Key_Super_R
    {0x01000055, KEY_PROPS, 0}, // Qt::Key_Menu
    {0x01000056, KEY_NONE, 0}, // Qt::Key_Hyper_L
    {0x01000057, KEY_NONE, 0}, // Qt::Key_Hyper_R
    {0x01000058, KEY_HELP, 0}, // Qt::Key_Help
    {0x01000059, KEY_NONE, 0}, // Qt::Key_Direction_L
    {0x01000060, KEY_NONE, 0}, // Qt::Key_Direction_R

    // ----------------------------
    // Additional Special Keys
    // ----------------------------
    {0x0a0, KEY_NONE, 0}, // Qt::Key_nobreakspace
    {0x0a1, KEY_NONE, 0}, // Qt::Key_exclamdown
    {0x0a2, KEY_NONE, 0}, // Qt::Key_cent
    {0x0a3, KEY_NONE, 0}, // Qt::Key_sterling
    {0x0a4, KEY_NONE, 0}, // Qt::Key_currency
    {0x0a5, KEY_YEN, 0}, // Qt::Key_yen
    {0x0a6, KEY_NONE, 0}, // Qt::Key_brokenbar
    {0x0a7, KEY_NONE, 0}, // Qt::Key_section
    {0x0a8, KEY_NONE, 0}, // Qt::Key_diaeresis
    {0x0a9, KEY_NONE, 0}, // Qt::Key_copyright
    {0x0aa, KEY_NONE, 0}, // Qt::Key_ordfeminine
    {0x0ab, KEY_NONE, 0}, // Qt::Key_guillemotleft
    {0x0ac, KEY_NONE, 0}, // Qt::Key_notsign
    {0x0ad, KEY_NONE, 0}, // Qt::Key_hyphen
    {0x0ae, KEY_NONE, 0}, // Qt::Key_registered
    {0x0af, KEY_NONE, 0}, // Qt::Key_macron
    {0x0b0, KEY_NONE, 0}, // Qt::Key_degree
    {0x0b1, KEY_NONE, 0}, // Qt::Key_plusminus
    {0x0b2, KEY_NONE, 0}, // Qt::Key_twosuperior
    {0x0b3, KEY_NONE, 0}, // Qt::Key_threesuperior
    {0x0b4, KEY_NONE, 0}, // Qt::Key_acute
    {0x0b5, KEY_NONE, 0}, // Qt::Key_micro
    {0x0b6, KEY_NONE, 0}, // Qt::Key_paragraph
    {0x0b7, KEY_NONE, 0}, // Qt::Key_periodcentered
    {0x0b8, KEY_NONE, 0}, // Qt::Key_cedilla
    {0x0b9, KEY_NONE, 0}, // Qt::Key_onesuperior
    {0x0ba, KEY_NONE, 0}, // Qt::Key_masculine
    {0x0bb, KEY_NONE, 0}, // Qt::Key_guillemotright
    {0x0bc, KEY_NONE, 0}, // Qt::Key_onequarter
    {0x0bd, KEY_NONE, 0}, // Qt::Key_onehalf
    {0x0be, KEY_NONE, 0}, // Qt::Key_threequarters
    {0x0bf, KEY_NONE, 0}, // Qt::Key_questiondown

    {0x0c0, KEY_NONE, 0}, // Qt::Key_Agrave
    {0x0c1, KEY_NONE, 0}, // Qt::Key_Aacute
    {0x0c2, KEY_NONE, 0}, // Qt::Key_Acircumflex
    {0x0c3, KEY_NONE, 0}, // Qt::Key_Atilde
    {0x0c4, KEY_NONE, 0}, // Qt::Key_Adiaeresis
    {0x0c5, KEY_NONE, 0}, // Qt::Key_Aring
    {0x0c6, KEY_NONE, 0}, // Qt::Key_AE
    {0x0c7, KEY_NONE, 0}, // Qt::Key_Ccedilla
    {0x0c8, KEY_NONE, 0}, // Qt::Key_Egrave
    {0x0c9, KEY_NONE, 0}, // Qt::Key_Eacute
    {0x0ca, KEY_NONE, 0}, // Qt::Key_Ecircumflex
    {0x0cb, KEY_NONE, 0}, // Qt::Key_Ediaeresis
    {0x0cc, KEY_NONE, 0}, // Qt::Key_Igrave
    {0x0cd, KEY_NONE, 0}, // Qt::Key_Iacute
    {0x0ce, KEY_NONE, 0}, // Qt::Key_Icircumflex
    {0x0cf, KEY_NONE, 0}, // Qt::Key_Idiaeresis
    {0x0d0, KEY_NONE, 0}, // Qt::Key_ETH
    {0x0d1, KEY_NONE, 0}, // Qt::Key_Ntilde
    {0x0d2, KEY_NONE, 0}, // Qt::Key_Ograve
    {0x0d3, KEY_NONE, 0}, // Qt::Key_Oacute
    {0x0d4, KEY_NONE, 0}, // Qt::Key_Ocircumflex
    {0x0d5, KEY_NONE, 0}, // Qt::Key_Otilde
    {0x0d6, KEY_O, 0}, // Qt::Key_Odiaeresis 'Ö' mapped to 'O'
    {0x0d7, KEY_KPMINUS, 0}, // Qt::Key_multiply '×' mapped to Keypad Minus
    // {0x0d8, KEY_OOBLIQUE, 0}, // Qt::Key_Ooblique 'Ø'
    {0x0d9, KEY_U, 0}, // Qt::Key_Ugrave 'Ù' mapped to 'U'
    {0x0da, KEY_U, 0}, // Qt::Key_Uacute 'Ú' mapped to 'U'
    {0x0db, KEY_U, 0}, // Qt::Key_Ucircumflex 'Û' mapped to 'U'
    {0x0dc, KEY_U, 0}, // Qt::Key_Udiaeresis 'Ü' mapped to 'U'
    {0x0dd, KEY_Y, 0}, // Qt::Key_Yacute 'Ý' mapped to 'Y'
    {0x0de, KEY_NONE, 0}, // Qt::Key_THORN 'Þ'
    {0x0df, KEY_NONE, 0}, // Qt::Key_ssharp 'ß'
    // {0x0f7, KEY_DIVISION, 0}, // Qt::Key_division '÷'
    {0x0ff, KEY_NONE, 0}, // Qt::Key_ydiaeresis 'ÿ'

    // ----------------------------
    // Media and Additional Keys (Optional)
    // ----------------------------
    // Depending on your application's needs, you can map additional media keys here.
    // Example:
    // {0x... , KEY_MEDIA_PLAYPAUSE, 0}, // Qt::Key_Media_PlayPause
    // {0x... , KEY_MEDIA_VOLUMEUP, 0}, // Qt::Key_Media_VolumeUp
    // Add more as needed.

    // ----------------------------
    // Unmapped or Unsupported Keys
    // ----------------------------
    // Keys that do not have a direct HID equivalent are mapped to KEY_NONE.
    // You can handle these cases as needed in your application logic.
    
    // Example:
    // {0x01000053, KEY_NONE, 0}, // Qt::Key_Super_L (already mapped as KEY_FRONT)
    // {0x01000054, KEY_NONE, 0}, // Qt::Key_Super_R (already mapped as KEY_FRONT)
    // etc.
};

#define QT_HID_MAP_SIZE (sizeof(qt_hid_map) / sizeof(qt_to_hid_mapping_t))


static K_SEM_DEFINE(usb_sem, 1, 1);	/* starts off "available" */
static void in_ready_cb(const struct device *dev)
{
	ARG_UNUSED(dev);
    printk("in_ready_cb\n");
	k_sem_give(&usb_sem);
}

// Function to get HID key code and modifier from Qt key code
bool get_hid_key(uint32_t qt_key, uint8_t *hid_key, uint8_t *modifier) {
    for (size_t i = 0; i < QT_HID_MAP_SIZE; i++) {
        if (qt_hid_map[i].qt_key == qt_key) {
            *hid_key = qt_hid_map[i].hid_key;
            *modifier = qt_hid_map[i].modifier;
            return true;
        }
    }
    return false; // Key not found
}

// // Current modifier state
// static uint8_t current_modifiers = 0;

// // Function to update modifier state
// void update_modifiers(uint8_t modifier, bool press, uint8_t *cmodifier) {
//     if (press) {
//         current_modifiers |= modifier;
//     } else {
//         current_modifiers &= ~modifier;
//     }

//     *cmodifier = current_modifiers;
// }

const struct device *hid0_dev;
const struct device *hid1_dev;

static const uint8_t hid_kbd_report_desc[] = HID_KEYBOARD_REPORT_DESC();
// static const uint8_t hid_touch_report_desc[] = hid_mouse_abs_report_desc();

static const struct hid_ops ops = {
	.int_in_ready = in_ready_cb,
};

bool hid_keyboard_init(void)
{
    
	hid0_dev = device_get_binding("HID_0");
	if (hid0_dev == NULL) {
		printk("Cannot get USB HID 0 Device");
		return 0;
	}
	/* Initialize HID */
	usb_hid_register_device(hid0_dev, hid_kbd_report_desc,
				sizeof(hid_kbd_report_desc), &ops);
	if(usb_hid_init(hid0_dev))
    {
        printk("Failed to initialize HID device\n");
        return false;
    }

    hid1_dev = device_get_binding("HID_1");
    if (hid1_dev == NULL) {
        printk("Cannot get USB HID 1 Device");
        return 0;
    }
    usb_hid_register_device(hid1_dev, hid_mouse_abs_report_desc,
                sizeof(hid_mouse_abs_report_desc), &ops);
    if(usb_hid_init(hid1_dev))
    {
        printk("Failed to initialize HID device\n");
        return false;
    }

    return true;
}

bool hid_keyboard_send_report(uint8_t *report)
{
	k_sem_take(&usb_sem, K_FOREVER);
    return hid_int_ep_write(hid0_dev, report, HID_REPORT_SIZE_K, NULL);
}

bool hid_mouse_abs_send(uint8_t buttons, uint16_t x, uint16_t y)
{
    uint8_t report[5];
    report[0] = buttons;        // 3개 버튼 비트 + padding
    report[1] = (uint8_t)(x & 0xFF);
    report[2] = (uint8_t)(x >> 8);
    report[3] = (uint8_t)(y & 0xFF);
    report[4] = (uint8_t)(y >> 8);
    k_sem_take(&usb_sem, K_FOREVER);
    // 세마포어 대기 등 필요 시 처리
    int err = hid_int_ep_write(hid1_dev, report, sizeof(report), NULL);
    return (err == 0);
}

bool hid_mouse_abs_clear(void)
{
    uint8_t report[5] = {0};
    k_sem_take(&usb_sem, K_FOREVER);
    // 세마포어 대기 등 필요 시 처리
    int err = hid_int_ep_write(hid1_dev, report, sizeof(report), NULL);
    return (err == 0);
}
// bool hid_touch_send_report(uint8_t contact_count, uint16_t x, uint16_t y, uint8_t contact_id)
// {
//     uint8_t report[HID_REPORT_SIZE_TA];

//     // Fill the report buffer
//     // report[0] = (1 << 0) | (1 << 1);  // Tip Switch=1, In Range=1
//     report[0] = contact_id;           // Contact ID
//     report[1] = (uint8_t)(x & 0xFF);  // X Low byte
//     report[2] = (uint8_t)(x >> 8);    // X High byte
//     report[3] = (uint8_t)(y & 0xFF);  // Y Low byte
//     report[4] = (uint8_t)(y >> 8);    // Y High byte
//     report[5] = contact_count;        // Contact Count

//     // Wait for USB semaphore to ensure previous transmission is complete
//     k_sem_take(&usb_sem, K_FOREVER);

//     // Send the HID report
//     int ret = hid_int_ep_write(hid1_dev, report, HID_REPORT_SIZE_TA, NULL);
//     if (ret) {
//         printk("Failed to send touch report: %d\n", ret);
//         return false;
//     }
//     else{
//         for(int i = 0; i < HID_REPORT_SIZE_TA; i++)
//         {
//             printk("%02x ", report[i]);
//         }
//         printk("\n");
//     }

//     return true;
// }

// bool hid_touch_clear_report(void)
// {
//     uint8_t report[HID_REPORT_SIZE_TA] = {0};

//     // Set contact count to 0 to indicate no active touches
//     report[0] = 0x00; // Contact Count (1 byte)
//     // All other values remain as 0, indicating no valid touch data

//     // Wait for USB semaphore to ensure previous transmission is complete
//     printk("Waiting for USB semaphore\n");
//     k_sem_take(&usb_sem, K_FOREVER);
//     printk("Waiting done\n");

//     // Send the HID report
//     int ret = hid_int_ep_write(hid1_dev, report, HID_REPORT_SIZE_TA, NULL);
//     if (ret) {
//         printk("Failed to send clear report: %d\n", ret);
//         return false;
//     }

//     // Debug output
//     printk("HID Touch Clear Report Sent:\n");
//     for (int i = 0; i < sizeof(report); i++) {
//         printk("%02X ", report[i]);
//     }
//     printk("\n");

//     return true;
// }

