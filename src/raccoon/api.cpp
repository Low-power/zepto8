//
//  ZEPTO-8 — Fantasy console emulator
//
//  Copyright © 2016—2019 Sam Hocevar <sam@hocevar.net>
//
//  This program is free software. It comes without any warranty, to
//  the extent permitted by applicable law. You can redistribute it
//  and/or modify it under the terms of the Do What the Fuck You Want
//  to Public License, Version 2, as published by the WTFPL Task Force.
//  See http://www.wtfpl.net/ for more details.
//

#if HAVE_CONFIG_H
#   include "config.h"
#endif

#include <lol/engine.h>

extern "C" {
#include "quickjs/quickjs.h"
}

#include "zepto8.h"
#include "raccoon/vm.h"
#include "raccoon/font.h"

namespace z8::raccoon
{

template<typename T> static void setpixel(T &data, int x, int y, uint8_t c)
{
    uint8_t &p = data[y][x / 2];
    p = (p & (x & 1 ? 0x0f : 0xf0)) | (x & 1 ? c << 4 : c);
}

template<typename T> static uint8_t getpixel(T const &data, int x, int y)
{
    uint8_t const &p = data[y][x / 2];
    return (x & 1 ? p >> 4 : p) & 0xf;
}

int vm::api_read(int p)
{
    return m_ram[p & 0xffff];
}

void vm::api_write(int p, int x)
{
    m_ram[p & 0xffff] = x;
}

void vm::api_palset(int n, int r, int g, int b)
{
    m_ram.palette[n & 0xf] = lol::u8vec3(r, g, b);
}

void vm::api_pset(int x, int y, int c)
{
    if (x < 0 || x >= 128 || y < 0 || y >= 64)
        return;
    setpixel(m_ram.screen, x, y, c & 15);
}

void vm::api_palm(int c0, int c1)
{
    uint8_t &data = m_ram.palmod[c0 & 0xf];
    data = (data & 0xf0) | (c1 & 0xf);
}

void vm::api_palt(int c, int v)
{
    uint8_t &data = m_ram.palmod[c & 0xf];
    data = (data & 0x7f) | (v ? 0x80 : 0x00);
}

JSValue vm::api_btnp(int argc, JSValueConst *argv)
{
    int x, y;
    if (JS_ToInt32(m_ctx, &x, argv[0]))
        return JS_EXCEPTION;
    if (JS_ToInt32(m_ctx, &y, argv[1]))
        return JS_EXCEPTION;
    lol::msg::info("stub: btnp(%d, %d)\n", x, y);
    return JS_NewInt32(m_ctx, 0);
}

JSValue vm::api_fget(int argc, JSValueConst *argv)
{
    int n, f;
    if (JS_ToInt32(m_ctx, &n, argv[0]))
        return JS_EXCEPTION;
    if (n < 0 || n >= 192)
        return JS_UNDEFINED;
    uint8_t field = m_ram.flags[n];
    if (argc == 1)
        return JS_NewInt32(m_ctx, field);
    if (JS_ToInt32(m_ctx, &f, argv[1]))
        return JS_EXCEPTION;
    return JS_NewInt32(m_ctx, (field >> f) & 0x1);
}

JSValue vm::api_fset(int argc, JSValueConst *argv)
{
    int n, f, v;
    if (JS_ToInt32(m_ctx, &n, argv[0]))
        return JS_EXCEPTION;
    if (JS_ToInt32(m_ctx, &f, argv[1]))
        return JS_EXCEPTION;
    if (n < 0 || n >= 192)
        return JS_UNDEFINED;
    if (argc == 3)
    {
        if (JS_ToInt32(m_ctx, &v, argv[2]))
            return JS_EXCEPTION;
        uint8_t mask = 1 << f;
        f = (m_ram.flags[n] & ~mask) | (v ? mask : 0);
    }
    m_ram.flags[n] = f;
    return JS_UNDEFINED;
}

JSValue vm::api_cls(int argc, JSValueConst *argv)
{
    int c;
    if (argc == 0 || JS_ToInt32(m_ctx, &c, argv[0]))
        c = 0;
    memset(m_ram.screen, (c & 15) * 0x11, sizeof(m_ram.screen));
    return JS_UNDEFINED;
}

void vm::api_cam(int x, int y)
{
    m_ram.camera.x = (int16_t)x;
    m_ram.camera.y = (int16_t)y;
}

void vm::api_map(int celx, int cely, int sx, int sy, int celw, int celh)
{
    sx -= m_ram.camera.x;
    sy -= m_ram.camera.y;
    for (int y = 0; y < celh; ++y)
    for (int x = 0; x < celw; ++x)
    {
        if (celx + x < 0 || celx + x >= 128 || cely + y < 0 || cely + y >= 64)
            continue;
        int n = m_ram.map[cely + y][celx + x];
        int startx = sx + x * 8, starty = sy + y * 8;
        int sprx = n % 16 * 8, spry = n / 16 * 8;

        for (int dy = 0; dy < 8; ++dy)
        for (int dx = 0; dx < 8; ++dx)
        {
            if (startx + dx < 0 || startx + dx >= 128 ||
                starty + dy < 0 || starty + dy >= 128)
                continue;
            if (sprx + dx < 0 || sprx + dx >= 128 ||
                spry + dy < 0 || spry + dy >= 96)
                continue;
            auto c = getpixel(m_ram.sprites, sprx + dx, spry + dy);
            if (m_ram.palmod[c] & 0x80)
                continue;
            c = m_ram.palmod[c] & 0xf;
            setpixel(m_ram.screen, startx + dx, starty + dy, c);
        }
    }
}

void vm::api_rect(int x, int y, int w, int h, int c)
{
    lol::msg::info("stub: rect(%d, %d, %d, %d, %d)\n", x, y, w, h, c);
}

void vm::api_rectfill(int x, int y, int w, int h, int c)
{
    x -= m_ram.camera.x;
    y -= m_ram.camera.y;
    int x0 = std::max(x, 0);
    int x1 = std::min(x + w, 127);
    int y0 = std::max(y, 0);
    int y1 = std::min(y + h, 127);
    for (y = y0; y <= y1; ++y)
        for (x = x0; x <= x1; ++x)
            setpixel(m_ram.screen, x, y, c & 15);
}

JSValue vm::api_spr(int argc, JSValueConst *argv)
{
    int n, x, y, fx, fy;
    double w, h;
    if (JS_ToInt32(m_ctx, &n, argv[0]))
        return JS_EXCEPTION;
    if (JS_ToInt32(m_ctx, &x, argv[1]))
        return JS_EXCEPTION;
    if (JS_ToInt32(m_ctx, &y, argv[2]))
        return JS_EXCEPTION;
    if (argc < 4 || JS_ToFloat64(m_ctx, &w, argv[3]))
        w = 1.0;
    if (argc < 5 || JS_ToFloat64(m_ctx, &h, argv[4]))
        h = 1.0;
    if (argc < 6 || JS_ToInt32(m_ctx, &fx, argv[5]))
        fx = 0;
    if (argc < 7 || JS_ToInt32(m_ctx, &fy, argv[6]))
        fy = 0;
    x -= m_ram.camera.x;
    y -= m_ram.camera.y;
    int sx = n % 16 * 8, sy = n / 16 * 8;
    int sw = (int)(w * 8), sh = (int)(h * 8);
    for (int dy = 0; dy < sh; ++dy)
        for (int dx = 0; dx < sw; ++dx)
        {
            if (x + dx < 0 || x + dx >= 128 || y + dy < 0 || y + dy >= 128)
                continue;
            auto c = getpixel(m_ram.sprites,
                              fx ? sx + sw - 1 - dx : sx + dx,
                              fy ? sy + sh - 1 - dy : sy + dy);
            if (m_ram.palmod[c] & 0x80)
                continue;
            c = m_ram.palmod[c] & 0xf;
            setpixel(m_ram.screen, x + dx, y + dy, c);
        }
    return JS_UNDEFINED;
}

JSValue vm::api_print(int argc, JSValueConst *argv)
{
    int x, y, c;
    if (JS_ToInt32(m_ctx, &x, argv[0]))
        return JS_EXCEPTION;
    if (JS_ToInt32(m_ctx, &y, argv[1]))
        return JS_EXCEPTION;
    char const *str = JS_ToCString(m_ctx, argv[2]);
    if (JS_ToInt32(m_ctx, &c, argv[3]))
        return JS_EXCEPTION;
    x -= m_ram.camera.x;
    y -= m_ram.camera.y;
    c &= 15;
    for (int n = 0; str[n]; ++n)
    {
        uint8_t ch = (uint8_t)str[n];
        if (ch < 0x20 || ch >= 0x80)
            continue;
        uint32_t data = font_data[ch - 0x20];

        if (ch == ',')
            x -= 1;

        for (int dx = 0; dx < 3; ++dx)
        for (int dy = 0; dy < 7; ++dy)
            if (data & (1 << (dx * 8 + dy)))
                setpixel(m_ram.screen, x + dx, y + dy, c);
        if (data & 0xff0000)
            x += 4;
        else if (data & 0xff00)
            x += 3;
        else if (data & 0xff)
            x += 2;
        else if (ch == ' ')
            x += 4;
    }

    JS_FreeCString(m_ctx, str);
    return JS_UNDEFINED;
}

JSValue vm::api_rnd(int argc, JSValueConst *argv)
{
    double x;
    if (argc == 0 || JS_ToFloat64(m_ctx, &x, argv[0]))
        x = 1.0;
    return JS_NewFloat64(m_ctx, lol::rand(x));
}

double vm::api_mid(double x, double y, double z)
{
    return x > y ? y > z ? y : std::min(x, z)
                 : x > z ? x : std::min(y, z);
}

int vm::api_mget(int x, int y)
{
    if (x < 0 || x >= 128 || y < 0 || y >= 64)
        return 0;
    return m_ram.map[y][x];
}

void vm::api_mset(int x, int y, int n)
{
    if (x < 0 || x >= 128 || y < 0 || y >= 64)
        return;
    m_ram.map[y][x] = n;
}

void vm::api_mus(int n)
{
    lol::msg::info("stub: mus(%d)\n", n);
}

}
