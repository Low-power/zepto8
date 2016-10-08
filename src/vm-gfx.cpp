//
//  ZEPTO-8 — Fantasy console emulator
//
//  Copyright © 2016 Sam Hocevar <sam@hocevar.net>
//
//  This program is free software. It comes without any warranty, to
//  the extent permitted by applicable law. You can redistribute it
//  and/or modify it under the terms of the Do What the Fuck You Want
//  to Public License, Version 2, as published by the WTFPL Task Force.
//  See http://www.wtfpl.net/ for more details.
//

#include <lol/engine.h>

#include "vm.h"

namespace z8
{

using lol::msg;

void vm::setpixel(int x, int y, int color)
{
    x -= m_camera.x;
    y -= m_camera.y;

    if (x < m_clip.aa.x || x >= m_clip.bb.x
         || y < m_clip.aa.y || y >= m_clip.bb.y)
        return;

    int offset = OFFSET_SCREEN + (128 * y + x) / 2;
    int mask = (x & 1) ? 0x0f : 0xf0;
    int p = (x & 1) ? color << 4 : color;
    m_memory[offset] = (m_memory[offset] & mask) | p;
}

int vm::getpixel(int x, int y)
{
    /* pget() is affected by camera() and by clip() */
    x -= m_camera.x;
    y -= m_camera.y;

    if (x < m_clip.aa.x || x >= m_clip.bb.x
         || y < m_clip.aa.y || y >= m_clip.bb.y)
        return 0;

    int offset = OFFSET_SCREEN + (128 * y + x) / 2;
    return (x & 1) ? m_memory[offset] >> 4 : m_memory[offset] & 0xf;
}

void vm::setspixel(int x, int y, int color)
{
    if (x < 0 || x >= 128 || y < 0 || y >= 128)
        return;

    int offset = OFFSET_GFX + (128 * y + x) / 2;
    int mask = (x & 1) ? 0x0f : 0xf0;
    int p = (x & 1) ? color << 4 : color;
    m_memory[offset] = (m_memory[offset] & mask) | p;
}

int vm::getspixel(int x, int y)
{
    if (x < 0 || x >= 128 || y < 0 || y >= 128)
        return 0;

    int offset = OFFSET_GFX + (128 * y + x) / 2;
    return (x & 1) ? m_memory[offset] >> 4 : m_memory[offset] & 0xf;
}

void vm::hline(int x1, int x2, int y, int color)
{
    x1 -= m_camera.x;
    x2 -= m_camera.x;
    y -= m_camera.y;

    if (y < m_clip.aa.y || y >= m_clip.bb.y)
        return;

    if (x1 > x2)
        std::swap(x1, x2);

    x1 = lol::max(x1, m_clip.aa.x);
    x2 = lol::min(x2, m_clip.bb.x - 1);

    if (x1 > x2)
        return;

    int offset = OFFSET_SCREEN + (128 * y) / 2;
    if (x1 & 1)
    {
        m_memory[offset + x1 / 2]
            = (m_memory[offset + x1 / 2] & 0x0f) | (color << 4);
        ++x1;
    }

    if ((x2 & 1) == 0)
    {
        m_memory[offset + x2 / 2]
            = (m_memory[offset + x2 / 2] & 0xf0) | color;
        --x2;
    }

    ::memset(get_mem(offset + x1 / 2), color * 0x11, (x2 - x1 + 1) / 2);
}

void vm::vline(int x, int y1, int y2, int color)
{
    x -= m_camera.x;
    y1 -= m_camera.y;
    y2 -= m_camera.y;

    if (x < m_clip.aa.x || x >= m_clip.bb.x)
        return;

    if (y1 > y2)
        std::swap(y1, y2);

    y1 = lol::max(y1, m_clip.aa.y);
    y2 = lol::min(y2, m_clip.bb.y - 1);

    if (y1 > y2)
        return;

    int mask = (x & 1) ? 0x0f : 0xf0;
    int p = (x & 1) ? color << 4 : color;

    for (int y = y1; y <= y2; ++y)
    {
        int offset = OFFSET_SCREEN + (128 * y + x) / 2;
        m_memory[offset] = (m_memory[offset] & mask) | p;
    }
}

//
// Text
//

int vm::api::cursor(lua_State *l)
{
    vm *that = get_this(l);
    that->m_cursor = lol::ivec2(lua_toclamp64(l, 1), lua_toclamp64(l, 2));
    return 0;
}

int vm::api::print(lua_State *l)
{
    vm *that = get_this(l);

    if (lua_isnone(l, 1))
        return 0;

    char const *str;
    if (lua_isnil(l, 1))
        str = "nil";
    else if (lua_isstring(l, 1))
        str = lua_tostring(l, 1);
    else if (lua_istable(l, 1))
        str = "table";
    else if (lua_isthread(l, 1))
        str = "thread";
    else if (lua_isfunction(l, 1))
        str = "function";
    else
        str = lua_toboolean(l, 1) ? "true" : "false";

    bool use_cursor = lua_isnone(l, 2) || lua_isnone(l, 3);
    int x = use_cursor ? that->m_cursor.x : lua_toclamp64(l, 2);
    int y = use_cursor ? that->m_cursor.y : lua_toclamp64(l, 3);
    if (!lua_isnone(l, 4))
        that->m_color = (int)lua_toclamp64(l, 4) & 0xf;
    int c = that->m_pal[0][that->m_color];
    int initial_x = x;

    auto pixels = that->m_font.Lock<lol::PixelFormat::RGBA_8>();
    for (int n = 0; str[n]; ++n)
    {
        int ch = (int)(uint8_t)str[n];

        if (ch == '\n')
        {
            x = initial_x;
            y += 6;
        }
        else
        {
            int index = ch > 0x20 && ch < 0x9a ? ch - 0x20 : 0;
            int w = index < 0x60 ? 4 : 8;
            int h = 6;

            for (int dy = 0; dy < h - 1; ++dy)
                for (int dx = 0; dx < w - 1; ++dx)
                {
                    if (pixels[(index / 16 * h + dy) * 128 + (index % 16 * w + dx)].r > 0)
                        that->setpixel(x + dx, y + dy, c);
                }

            x += w;
        }
    }

    that->m_font.Unlock(pixels);

    // Add implicit carriage return to the cursor position
    if (use_cursor)
        that->m_cursor = lol::ivec2(initial_x, y + 6);

    return 0;
}

//
// Graphics
//

int vm::api::camera(lua_State *l)
{
    vm *that = get_this(l);
    that->m_camera = lol::ivec2(lua_toclamp64(l, 1), lua_toclamp64(l, 2));
    return 0;
}

int vm::api::circ(lua_State *l)
{
    vm *that = get_this(l);

    int x = lua_toclamp64(l, 1);
    int y = lua_toclamp64(l, 2);
    int r = lua_toclamp64(l, 3);
    if (!lua_isnone(l, 4))
        that->m_color = (int)lua_toclamp64(l, 4) & 0xf;
    int c = that->m_pal[0][that->m_color];

    for (int dx = r, dy = 0, err = 0; dx >= dy; )
    {
        that->setpixel(x + dx, y + dy, c);
        that->setpixel(x + dy, y + dx, c);
        that->setpixel(x - dy, y + dx, c);
        that->setpixel(x - dx, y + dy, c);
        that->setpixel(x - dx, y - dy, c);
        that->setpixel(x - dy, y - dx, c);
        that->setpixel(x + dy, y - dx, c);
        that->setpixel(x + dx, y - dy, c);

        dy += 1;
        err += 1 + 2 * dy;
        // XXX: original Bresenham has a different test, but
        // this one seems to match PICO-8 better.
        if (2 * (err - dx) > r + 1)
        {
            dx -= 1;
            err += 1 - 2 * dx;
        }
    }

    return 0;
}

int vm::api::circfill(lua_State *l)
{
    vm *that = get_this(l);

    int x = lua_toclamp64(l, 1);
    int y = lua_toclamp64(l, 2);
    int r = lua_toclamp64(l, 3);
    if (!lua_isnone(l, 4))
        that->m_color = (int)lua_toclamp64(l, 4) & 0xf;
    int c = that->m_pal[0][that->m_color];

    for (int dx = r, dy = 0, err = 0; dx >= dy; )
    {
        /* Some minor overdraw here, but nothing serious */
        that->hline(x - dx, x + dx, y - dy, c);
        that->hline(x - dx, x + dx, y + dy, c);
        that->vline(x - dy, y - dx, y + dx, c);
        that->vline(x + dy, y - dx, y + dx, c);

        dy += 1;
        err += 1 + 2 * dy;
        // XXX: original Bresenham has a different test, but
        // this one seems to match PICO-8 better.
        if (2 * (err - dx) > r + 1)
        {
            dx -= 1;
            err += 1 - 2 * dx;
        }
    }

    return 0;
}

int vm::api::clip(lua_State *l)
{
    vm *that = get_this(l);

    /* XXX: there were rendering issues with Hyperspace by J.Fry when we were
     * only checking for lua_isnone(l,1) (instead of 4) because first argument
     * was actually "" instead of nil. */
    if (lua_isnone(l, 4))
    {
        that->m_clip = lol::ibox2(0, 0, 128, 128);
    }
    else
    {
        int x0 = lua_toclamp64(l, 1);
        int y0 = lua_toclamp64(l, 2);
        int x1 = x0 + lua_toclamp64(l, 3);
        int y1 = y0 + lua_toclamp64(l, 4);

        that->m_clip = lol::ibox2(lol::max(x0, 0), lol::max(y0, 0),
                                  lol::min(x1, 128), lol::min(y1, 128));
    }

    return 0;
}

int vm::api::cls(lua_State *l)
{
    int c = lua_toclamp64(l, 1);
    vm *that = get_this(l);
    ::memset(&that->m_memory[OFFSET_SCREEN], (c & 0xf) * 0x11, SIZE_SCREEN);
    return 0;
}

int vm::api::color(lua_State *l)
{
    vm *that = get_this(l);
    that->m_color = (int)lua_toclamp64(l, 1) & 0xf;
    return 0;
}

int vm::api::fget(lua_State *l)
{
    if (lua_isnone(l, 1))
        return 0;

    int n = lua_toclamp64(l, 1);
    uint8_t bits = 0;

    if (n >= 0 && n < SIZE_GFX_PROPS)
    {
        vm *that = get_this(l);
        bits = that->m_memory[OFFSET_GFX_PROPS + n];
    }

    if (lua_isnone(l, 2))
        lua_pushnumber(l, bits);
    else
        lua_pushboolean(l, bits & (1 << (int)lua_toclamp64(l, 2)));

    return 1;
}

int vm::api::fset(lua_State *l)
{
    if (lua_isnone(l, 1) || lua_isnone(l, 2))
        return 0;

    int n = lua_toclamp64(l, 1);

    if (n >= 0 && n < SIZE_GFX_PROPS)
    {
        vm *that = get_this(l);
        uint8_t bits = that->m_memory[OFFSET_GFX_PROPS + n];
        int f = lua_toclamp64(l, 2);

        if (lua_isnone(l, 3))
            bits = f;
        else if (lua_toboolean(l, 3))
            bits |= 1 << (int)f;
        else
            bits &= ~(1 << (int)f);

        that->m_memory[OFFSET_GFX_PROPS + n] = bits;
    }

    return 0;
}

int vm::api::line(lua_State *l)
{
    vm *that = get_this(l);

    int x0 = lua_toclamp64(l, 1);
    int y0 = lua_toclamp64(l, 2);
    int x1 = lua_toclamp64(l, 3);
    int y1 = lua_toclamp64(l, 4);
    if (!lua_isnone(l, 5))
        that->m_color = (int)lua_toclamp64(l, 5) & 0xf;
    int c = that->m_pal[0][that->m_color];

    if (x0 == x1 && y0 == y1)
    {
        that->setpixel(x0, y0, c);
    }
    else if (lol::abs(x1 - x0) > lol::abs(y1 - y0))
    {
        for (int x = lol::min(x0, x1); x <= lol::max(x0, x1); ++x)
        {
            int y = lol::round(lol::mix((float)y0, (float)y1, (float)(x - x0) / (x1 - x0)));
            that->setpixel(x, y, c);
        }
    }
    else
    {
        for (int y = lol::min(y0, y1); y <= lol::max(y0, y1); ++y)
        {
            int x = lol::round(lol::mix((float)x0, (float)x1, (float)(y - y0) / (y1 - y0)));
            that->setpixel(x, y, c);
        }
    }

    return 0;
}

int vm::api::map(lua_State *l)
{
    int cel_x = lua_toclamp64(l, 1);
    int cel_y = lua_toclamp64(l, 2);
    int sx = lua_toclamp64(l, 3);
    int sy = lua_toclamp64(l, 4);
    int cel_w = lua_toclamp64(l, 5);
    int cel_h = lua_toclamp64(l, 6);
    int layer = lua_toclamp64(l, 7);

    vm *that = get_this(l);

    for (int dy = 0; dy < cel_h * 8; ++dy)
    for (int dx = 0; dx < cel_w * 8; ++dx)
    {
        int cx = cel_x + dx / 8;
        int cy = cel_y + dy / 8;
        if (cx < 0 || cx >= 128 || cy < 0 || cy >= 64)
            continue;

        int line = cy < 32 ? OFFSET_MAP + 128 * cy
                           : OFFSET_MAP2 + 128 * (cy - 32);
        int sprite = that->m_memory[line + cx];

        uint8_t bits = that->m_memory[OFFSET_GFX_PROPS + sprite];
        if (layer && !(bits & layer))
            continue;

        if (sprite)
        {
            int col = that->getspixel(sprite % 16 * 8 + dx % 8, sprite / 16 * 8 + dy % 8);
            if (!that->m_palt[col])
            {
                int c = that->m_pal[0][col & 0xf];
                that->setpixel(sx + dx, sy + dy, c);
            }
        }
    }

    return 0;
}

int vm::api::mget(lua_State *l)
{
    int x = lua_toclamp64(l, 1);
    int y = lua_toclamp64(l, 2);
    int n = 0;

    if (x >= 0 && x < 128 && y >= 0 && y < 64)
    {
        vm *that = get_this(l);
        int line = y < 32 ? OFFSET_MAP + 128 * y
                          : OFFSET_MAP2 + 128 * (y - 32);
        n = that->m_memory[line + x];
    }

    lua_pushnumber(l, n);
    return 1;
}

int vm::api::mset(lua_State *l)
{
    int x = lua_toclamp64(l, 1);
    int y = lua_toclamp64(l, 2);
    int n = lua_toclamp64(l, 3);

    if (x >= 0 && x < 128 && y >= 0 && y < 64)
    {
        vm *that = get_this(l);
        int line = y < 32 ? OFFSET_MAP + 128 * y
                          : OFFSET_MAP2 + 128 * (y - 32);
        that->m_memory[line + x] = n;
    }

    return 0;
}

int vm::api::pal(lua_State *l)
{
    vm *that = get_this(l);

    if (lua_isnone(l, 1) || lua_isnone(l, 2))
    {
        for (int i = 0; i < 16; ++i)
        {
            that->m_pal[0][i] = that->m_pal[1][i] = i;
            that->m_palt[i] = i ? 0 : 1;
        }
    }
    else
    {
        int c0 = lua_toclamp64(l, 1);
        int c1 = lua_toclamp64(l, 2);
        int p = lua_toclamp64(l, 3);

        that->m_pal[p & 1][c0 & 0xf] = c1 & 0xf;
    }

    return 0;
}

int vm::api::palt(lua_State *l)
{
    vm *that = get_this(l);

    if (lua_isnone(l, 1) || lua_isnone(l, 2))
    {
        for (int i = 0; i < 16; ++i)
            that->m_palt[i] = i ? 0 : 1;
    }
    else
    {
        int c = lua_toclamp64(l, 1);
        int t = lua_toboolean(l, 2);
        that->m_palt[c & 0xf] = t;
    }

    return 0;
}

int vm::api::pget(lua_State *l)
{
    lol::LuaStack s(l);
    lol::LuaFloat x, y, ret;
    s >> x >> y;

    vm *that = get_this(l);
    ret = that->getpixel((int)x - that->m_camera.x, (int)y - that->m_camera.y);

    return s << ret;
}

int vm::api::pset(lua_State *l)
{
    vm *that = get_this(l);

    int x = lua_toclamp64(l, 1);
    int y = lua_toclamp64(l, 2);
    if (!lua_isnone(l, 3))
        that->m_color = (int)lua_toclamp64(l, 3) & 0xf;
    int c = that->m_pal[0][that->m_color];

    that->setpixel(x, y, c);

    return 0;
}

int vm::api::rect(lua_State *l)
{
    vm *that = get_this(l);

    int x0 = lua_toclamp64(l, 1);
    int y0 = lua_toclamp64(l, 2);
    int x1 = lua_toclamp64(l, 3);
    int y1 = lua_toclamp64(l, 4);
    if (!lua_isnone(l, 5))
        that->m_color = (int)lua_toclamp64(l, 5) & 0xf;
    int c = that->m_pal[0][that->m_color];

    if (x0 > x1)
        std::swap(x0, x1);

    if (y0 > y1)
        std::swap(y0, y1);

    that->hline(x0, x1, y0, c);
    that->hline(x0, x1, y1, c);

    if (y0 + 1 < y1)
    {
        that->vline(x0, y0 + 1, y1 - 1, c);
        that->vline(x1, y0 + 1, y1 - 1, c);
    }

    return 0;
}

int vm::api::rectfill(lua_State *l)
{
    vm *that = get_this(l);

    int x0 = lua_toclamp64(l, 1);
    int y0 = lua_toclamp64(l, 2);
    int x1 = lua_toclamp64(l, 3);
    int y1 = lua_toclamp64(l, 4);
    if (!lua_isnone(l, 5))
        that->m_color = (int)lua_toclamp64(l, 5) & 0xf;
    int c = that->m_pal[0][that->m_color];

    for (int y = lol::min(y0, y1); y <= lol::max(y0, y1); ++y)
        that->hline(lol::min(x0, x1), lol::max(x0, x1), y, c);

    return 0;
}

int vm::api::sget(lua_State *l)
{
    vm *that = get_this(l);

    int x = lua_toclamp64(l, 1);
    int y = lua_toclamp64(l, 2);

    lua_pushnumber(l, that->getspixel(x, y));

    return 1;
}

int vm::api::sset(lua_State *l)
{
    vm *that = get_this(l);

    int x = lua_toclamp64(l, 1);
    int y = lua_toclamp64(l, 2);
    int col = lua_isnone(l, 3) ? that->m_color : lua_toclamp64(l, 3);
    int c = that->m_pal[0][col & 0xf];

    that->setspixel(x, y, c);

    return 0;
}

int vm::api::spr(lua_State *l)
{
    vm *that = get_this(l);

    // FIXME: should we abort if n == 0?
    int n = lua_toclamp64(l, 1);
    int x = lua_toclamp64(l, 2);
    int y = lua_toclamp64(l, 3);
    float w = lua_isnoneornil(l, 4) ? 1 : lua_toclamp64(l, 4);
    float h = lua_isnoneornil(l, 5) ? 1 : lua_toclamp64(l, 5);
    int flip_x = lua_toboolean(l, 6);
    int flip_y = lua_toboolean(l, 7);

    for (int j = 0; j < h * 8; ++j)
        for (int i = 0; i < w * 8; ++i)
        {
            int di = flip_x ? w * 8 - 1 - i : i;
            int dj = flip_y ? h * 8 - 1 - j : j;
            int col = that->getspixel(n % 16 * 8 + di, n / 16 * 8 + dj);
            if (!that->m_palt[col])
            {
                int c = that->m_pal[0][col];
                that->setpixel(x + i, y + j, c);
            }
        }

    return 0;
}

int vm::api::sspr(lua_State *l)
{
    vm *that = get_this(l);

    int sx = lua_toclamp64(l, 1);
    int sy = lua_toclamp64(l, 2);
    int sw = lua_toclamp64(l, 3);
    int sh = lua_toclamp64(l, 4);
    int dx = lua_toclamp64(l, 5);
    int dy = lua_toclamp64(l, 6);
    int dw = lua_isnone(l, 7) ? sw : lua_toclamp64(l, 7);
    int dh = lua_isnone(l, 8) ? sh : lua_toclamp64(l, 8);
    int flip_x = lua_toboolean(l, 9);
    int flip_y = lua_toboolean(l, 10);

    // Iterate over destination pixels
    for (int j = 0; j < dh; ++j)
    for (int i = 0; i < dw; ++i)
    {
        int di = flip_x ? dw - 1 - i : i;
        int dj = flip_y ? dh - 1 - j : j;

        // Find source
        int x = sx + sw * di / dw;
        int y = sy + sh * dj / dh;

        int col = that->getspixel(x, y);
        if (!that->m_palt[col])
        {
            int c = that->m_pal[0][col];
            that->setpixel(dx + i, dy + j, c);
        }
    }

    return 0;
}

} // namespace z8

