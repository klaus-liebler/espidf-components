#pragma once
#include <inttypes.h>
#include <algorithm>
#include <cmath>
#define TAG "CRGB"

//#define PrepareColorGRB(rgb) ( (((rgb) << 8) & 0x00FF0000) | (((rgb) >> 8) & 0x0000FF00) | (rgb & 0x000000FF) )
//#define PrepareColor PrepareColorGRB
#define PrepareColor

struct CRGB
{
    union
    {
        struct
        {
            union
            {
                uint8_t r;
                uint8_t red;
            };
            union
            {
                uint8_t g;
                uint8_t green;
            };
            union
            {
                uint8_t b;
                uint8_t blue;
            };
            union
            {
                uint8_t a;
                uint8_t alpha;
            };
        };
        uint8_t raw[4];
        uint32_t raw32;
    };
    /// Array access operator to index into the crgb object
    inline uint8_t &operator[](uint8_t x) __attribute__((always_inline))
    {
        return raw[x];
    }

    /// Array access operator to index into the crgb object
    inline const uint8_t &operator[](uint8_t x) const __attribute__((always_inline))
    {
        return raw[x];
    }

    // default values are UNINITIALIZED
    inline CRGB() __attribute__((always_inline)) = default;

    /// allow construction from R, G, B
    inline CRGB(uint8_t ir, uint8_t ig, uint8_t ib, uint8_t ia = 0xFF) __attribute__((always_inline))
    : r(ir), g(ig), b(ib), a(ia)
    {
    }

    inline CRGB(uint32_t colorcode) __attribute__((always_inline))
    : r((colorcode >> 24) & 0xFF), g((colorcode >> 16) & 0xFF), b((colorcode >> 8) & 0xFF), a((colorcode >> 0) & 0xFF)
    {
    }

    // inline CRGB( PredefinedColors colorcode)  __attribute__((always_inline))
    //: r(((uint32_t)colorcode >> 16) & 0xFF), g(((uint32_t)colorcode >> 8) & 0xFF), b(((uint32_t)colorcode >> 0) & 0xFF)
    //{
    //}

    /// divide each of the channels by a constant
    inline CRGB &operator/=(uint8_t d)
    {
        r /= d;
        g /= d;
        b /= d;
        return *this;
    }

    /// right shift each of the channels by a constant
    inline CRGB &operator>>=(uint8_t d)
    {
        r >>= d;
        g >>= d;
        b >>= d;
        return *this;
    }

    /// invert each channel
    inline CRGB operator-()
    {
        CRGB retval;
        retval.r = 255 - r;
        retval.g = 255 - g;
        retval.b = 255 - b;
        return retval;
    }

    static CRGB FromHSV(uint32_t h, uint32_t s, uint32_t v)
    {
        uint32_t r;
        uint32_t g;
        uint32_t b;

        h %= 360; // h -> [0,360]
        uint32_t rgb_max = v * 2.55f;
        uint32_t rgb_min = rgb_max * (100 - s) / 100.0f;

        uint32_t i = h / 60;
        uint32_t diff = h % 60;

        // RGB adjustment amount by hue
        uint32_t rgb_adj = (rgb_max - rgb_min) * diff / 60;

        switch (i)
        {
        case 0:
            r = rgb_max;
            g = rgb_min + rgb_adj;
            b = rgb_min;
            break;
        case 1:
            r = rgb_max - rgb_adj;
            g = rgb_max;
            b = rgb_min;
            break;
        case 2:
            r = rgb_min;
            g = rgb_max;
            b = rgb_min + rgb_adj;
            break;
        case 3:
            r = rgb_min;
            g = rgb_max - rgb_adj;
            b = rgb_max;
            break;
        case 4:
            r = rgb_min + rgb_adj;
            g = rgb_min;
            b = rgb_max;
            break;
        default:
            r = rgb_max;
            g = rgb_min;
            b = rgb_max - rgb_adj;
            break;
        }
        return CRGB(r, g, b);
    }

/*! \brief Convert HSV to RGB color space

  Converts a given set of HSV values `h', `s', `v' into RGB
  coordinates. The output RGB values are in the range [0, 1], and
  the input HSV values are in the ranges h = [0, 360], and s, v =
  [0, 1], respectively.

  \param fR Red component, used as output, range: [0, 1]
  \param fG Green component, used as output, range: [0, 1]
  \param fB Blue component, used as output, range: [0, 1]
  \param fH Hue component, used as input, range: [0, 360]
  \param fS Hue component, used as input, range: [0, 1]
  \param fV Hue component, used as input, range: [0, 1]

*/
    static CRGB FromHSVNew(float fH, float fS, float fV)
    {
        float fC = fV * fS; // Chroma
        float fHPrime = fmod(fH / 60.0, 6);
        float fX = fC * (1 - fabs(fmod(fHPrime, 2) - 1));
        float fM = fV - fC;

        float fR, fG, fB;

        if (0 <= fHPrime && fHPrime < 1)
        {
            fR = fC;
            fG = fX;
            fB = 0;
        }
        else if (1 <= fHPrime && fHPrime < 2)
        {
            fR = fX;
            fG = fC;
            fB = 0;
        }
        else if (2 <= fHPrime && fHPrime < 3)
        {
            fR = 0;
            fG = fC;
            fB = fX;
        }
        else if (3 <= fHPrime && fHPrime < 4)
        {
            fR = 0;
            fG = fX;
            fB = fC;
        }
        else if (4 <= fHPrime && fHPrime < 5)
        {
            fR = fX;
            fG = 0;
            fB = fC;
        }
        else if (5 <= fHPrime && fHPrime < 6)
        {
            fR = fC;
            fG = 0;
            fB = fX;
        }
        else
        {
            fR = 0;
            fG = 0;
            fB = 0;
        }

        fR += fM;
        fG += fM;
        fB += fM;
        return CRGB(fR*255, fG*255, fB*255, 255);
    }

    static CRGB FromTemperature(float minimum, float maximum, float value)
    {
        float m = (value - minimum) / (maximum - minimum);
        float h = 240.0 + 120.0 * m;
        CRGB crgb = FromHSVNew(h, 1, 1);
        //ESP_LOGI(TAG, "HUE %f-->R%d G%d B%d", h, crgb.r, crgb.g, crgb.b);
        return crgb;
    }

    typedef enum
    {
        /// Predefined RGB colors
        AliceBlue = PrepareColor(0xF0F8FFFF),
        Amethyst = PrepareColor(0x9966CCFF),
        AntiqueWhite = PrepareColor(0xFAEBD7FF),
        Aqua = PrepareColor(0x00FFFFFF),
        Aquamarine = PrepareColor(0x7FFFD4FF),
        Azure = PrepareColor(0xF0FFFFFF),
        Beige = PrepareColor(0xF5F5DCFF),
        Bisque = PrepareColor(0xFFE4C4FF),
        Black = PrepareColor(0x000000FF),
        BlanchedAlmond = PrepareColor(0xFFEBCDFF),
        Blue = PrepareColor(0x0000FFFF),
        BlueViolet = PrepareColor(0x8A2BE2FF),
        Brown = PrepareColor(0xA52A2AFF),
        BurlyWood = PrepareColor(0xDEB887FF),
        CadetBlue = PrepareColor(0x5F9EA0FF),
        Chartreuse = PrepareColor(0x7FFF00FF),
        Chocolate = PrepareColor(0xD2691EFF),
        Coral = PrepareColor(0xFF7F50FF),
        CornflowerBlue = PrepareColor(0x6495EDFF),
        Cornsilk = PrepareColor(0xFFF8DCFF),
        Crimson = PrepareColor(0xDC143CFF),
        Cyan = PrepareColor(0x00FFFFFF),
        DarkBlue = PrepareColor(0x00008BFF),
        DarkCyan = PrepareColor(0x008B8BFF),
        DarkGoldenrod = PrepareColor(0xB8860BFF),
        DarkGray = PrepareColor(0xA9A9A9FF),
        DarkGrey = PrepareColor(0xA9A9A9FF),
        DarkGreen = PrepareColor(0x006400FF),
        DarkKhaki = PrepareColor(0xBDB76BFF),
        DarkMagenta = PrepareColor(0x8B008BFF),
        DarkOliveGreen = PrepareColor(0x556B2FFF),
        DarkOrange = PrepareColor(0xFF8C00FF),
        DarkOrchid = PrepareColor(0x9932CCFF),
        DarkRed = PrepareColor(0x8B0000FF),
        DarkSalmon = PrepareColor(0xE9967AFF),
        DarkSeaGreen = PrepareColor(0x8FBC8FFF),
        DarkSlateBlue = PrepareColor(0x483D8BFF),
        DarkSlateGray = PrepareColor(0x2F4F4FFF),
        DarkSlateGrey = PrepareColor(0x2F4F4FFF),
        DarkTurquoise = PrepareColor(0x00CED1FF),
        DarkViolet = PrepareColor(0x9400D3FF),
        DeepPink = PrepareColor(0xFF1493FF),
        DeepSkyBlue = PrepareColor(0x00BFFFFF),
        DimGray = PrepareColor(0x696969FF),
        DimGrey = PrepareColor(0x696969FF),
        DodgerBlue = PrepareColor(0x1E90FFFF),
        FireBrick = PrepareColor(0xB22222FF),
        FloralWhite = PrepareColor(0xFFFAF0FF),
        ForestGreen = PrepareColor(0x228B22FF),
        Fuchsia = PrepareColor(0xFF00FFFF),
        Gainsboro = PrepareColor(0xDCDCDCFF),
        GhostWhite = PrepareColor(0xF8F8FFFF),
        Gold = PrepareColor(0xFFD700FF),
        Goldenrod = PrepareColor(0xDAA520FF),
        Gray = PrepareColor(0x808080FF),
        Grey = PrepareColor(0x808080FF),
        Green = PrepareColor(0x00FF00FF),
        GreenYellow = PrepareColor(0xADFF2FFF),
        Honeydew = PrepareColor(0xF0FFF0FF),
        HotPink = PrepareColor(0xFF69B4FF),
        IndianRed = PrepareColor(0xCD5C5CFF),
        Indigo = PrepareColor(0x4B0082FF),
        Ivory = PrepareColor(0xFFFFF0FF),
        Khaki = PrepareColor(0xF0E68CFF),
        Lavender = PrepareColor(0xE6E6FAFF),
        LavenderBlush = PrepareColor(0xFFF0F5FF),
        LawnGreen = PrepareColor(0x7CFC00FF),
        LemonChiffon = PrepareColor(0xFFFACDFF),
        LightBlue = PrepareColor(0xADD8E6FF),
        LightCoral = PrepareColor(0xF08080FF),
        LightCyan = PrepareColor(0xE0FFFFFF),
        LightGoldenrodYellow = PrepareColor(0xFAFAD2FF),
        LightGreen = PrepareColor(0x90EE90FF),
        LightGrey = PrepareColor(0xD3D3D3FF),
        LightPink = PrepareColor(0xFFB6C1FF),
        LightSalmon = PrepareColor(0xFFA07AFF),
        LightSeaGreen = PrepareColor(0x20B2AAFF),
        LightSkyBlue = PrepareColor(0x87CEFAFF),
        LightSlateGray = PrepareColor(0x778899FF),
        LightSlateGrey = PrepareColor(0x778899FF),
        LightSteelBlue = PrepareColor(0xB0C4DEFF),
        LightYellow = PrepareColor(0xFFFFE0FF),
        Lime = PrepareColor(0x00FF00FF),
        LimeGreen = PrepareColor(0x32CD32FF),
        Linen = PrepareColor(0xFAF0E6FF),
        Magenta = PrepareColor(0xFF00FFFF),
        Maroon = PrepareColor(0x800000FF),
        MediumAquamarine = PrepareColor(0x66CDAAFF),
        MediumBlue = PrepareColor(0x0000CDFF),
        MediumOrchid = PrepareColor(0xBA55D3FF),
        MediumPurple = PrepareColor(0x9370DBFF),
        MediumSeaGreen = PrepareColor(0x3CB371FF),
        MediumSlateBlue = PrepareColor(0x7B68EEFF),
        MediumSpringGreen = PrepareColor(0x00FA9AFF),
        MediumTurquoise = PrepareColor(0x48D1CCFF),
        MediumVioletRed = PrepareColor(0xC71585FF),
        MidnightBlue = PrepareColor(0x191970FF),
        MintCream = PrepareColor(0xF5FFFAFF),
        MistyRose = PrepareColor(0xFFE4E1FF),
        Moccasin = PrepareColor(0xFFE4B5FF),
        NavajoWhite = PrepareColor(0xFFDEADFF),
        Navy = PrepareColor(0x000080FF),
        OldLace = PrepareColor(0xFDF5E6FF),
        Olive = PrepareColor(0x808000FF),
        OliveDrab = PrepareColor(0x6B8E23FF),
        Orange = PrepareColor(0xFFA500FF),
        OrangeRed = PrepareColor(0xFF4500FF),
        Orchid = PrepareColor(0xDA70D6FF),
        PaleGoldenrod = PrepareColor(0xEEE8AAFF),
        PaleGreen = PrepareColor(0x98FB98FF),
        PaleTurquoise = PrepareColor(0xAFEEEEFF),
        PaleVioletRed = PrepareColor(0xDB7093FF),
        PapayaWhip = PrepareColor(0xFFEFD5FF),
        PeachPuff = PrepareColor(0xFFDAB9FF),
        Peru = PrepareColor(0xCD853FFF),
        Pink = PrepareColor(0xFFC0CBFF),
        Plaid = PrepareColor(0xCC5533FF),
        Plum = PrepareColor(0xDDA0DDFF),
        PowderBlue = PrepareColor(0xB0E0E6FF),
        Purple = PrepareColor(0x800080FF),
        Red = PrepareColor(0xFF0000FF),
        RosyBrown = PrepareColor(0xBC8F8FFF),
        RoyalBlue = PrepareColor(0x4169E1FF),
        SaddleBrown = PrepareColor(0x8B4513FF),
        Salmon = PrepareColor(0xFA8072FF),
        SandyBrown = PrepareColor(0xF4A460FF),
        SeaGreen = PrepareColor(0x2E8B57FF),
        Seashell = PrepareColor(0xFFF5EEFF),
        Sienna = PrepareColor(0xA0522DFF),
        Silver = PrepareColor(0xC0C0C0FF),
        SkyBlue = PrepareColor(0x87CEEBFF),
        SlateBlue = PrepareColor(0x6A5ACDFF),
        SlateGray = PrepareColor(0x708090FF),
        SlateGrey = PrepareColor(0x708090FF),
        Snow = PrepareColor(0xFFFAFAFF),
        SpringGreen = PrepareColor(0x00FF7FFF),
        SteelBlue = PrepareColor(0x4682B4FF),
        Tan = PrepareColor(0xD2B48CFF),
        Teal = PrepareColor(0x008080FF),
        Thistle = PrepareColor(0xD8BFD8FF),
        Tomato = PrepareColor(0xFF6347FF),
        Turquoise = PrepareColor(0x40E0D0FF),
        Violet = PrepareColor(0xEE82EEFF),
        Wheat = PrepareColor(0xF5DEB3FF),
        White = PrepareColor(0xFFFFFFFF),
        WhiteSmoke = PrepareColor(0xF5F5F5FF),
        Yellow = PrepareColor(0xFFFF00FF),
        YellowGreen = PrepareColor(0x9ACD32FF),

        // LED RGB color that roughly approximates
        // the color of incandescent fairy lights,
        // assuming that you're using FastLED
        // color correction on your LEDs (recommended).
        FairyLight = PrepareColor(0xFFE42DFF),
        // If you are using no color correction, use this
        FairyLightNCC = PrepareColor(0xFF9D2AFF),
        TRANSPARENT = PrepareColor(0x00000000)

    } HTMLColorCode;
};

inline bool operator==(const CRGB &lhs, const CRGB &rhs)
{
    return (lhs.r == rhs.r) && (lhs.g == rhs.g) && (lhs.b == rhs.b);
}

inline bool operator!=(const CRGB &lhs, const CRGB &rhs)
{
    return !(lhs == rhs);
}
#undef TAG