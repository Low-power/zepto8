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

vm::vm()
  : m_instructions(0)
{
    lua_State *l = GetLuaState();

    // Store a pointer to us in global state
    *static_cast<vm**>(lua_getextraspace(l)) = this;

    // Automatically yield every 1000 instructions
    lua_sethook(l, &vm::hook, LUA_MASKCOUNT, 1000);

    // Register our Lua module
    lol::LuaObjectHelper::Register<vm>(l);

    // Load font
    m_font.load("data/font.png");

    // Clear memory
    ::memset(get_mem(), 0, SIZE_MEMORY);

    // Initialize Zepto8
    ExecLuaFile("data/zepto8.lua");
}

vm::~vm()
{
}

void vm::hook(lua_State *l, lua_Debug *)
{
    vm *that = *static_cast<vm**>(lua_getextraspace(l));

    // The value 135000 was found using trial and error, but it causes
    // side effects in lots of cases. Use 300000 instead.
    that->m_instructions += 1000;
    if (that->m_instructions >= 300000)
        lua_yield(l, 0);
}

void vm::load(char const *name)
{
    m_cart.load(name);
}

void vm::run()
{
    // Start the cartridge!
    ExecLuaCode("run()");
}

void vm::step(float seconds)
{
    UNUSED(seconds);

    lua_State *l = GetLuaState();
    lua_getglobal(l, "_z8");
    lua_getfield(l, -1, "tick");
    lua_pcall(l, 0, 0, 0);
    lua_remove(l, -1);

    m_instructions = 0;
}

const lol::LuaObjectLibrary* vm::GetLib()
{
    static const lol::LuaObjectLibrary lib = lol::LuaObjectLibrary(
        "_z8",

        // Statics
        {
            { "run",      &dispatch<&vm::api_run> },
            { "menuitem", &dispatch<&vm::api_menuitem> },
            { "cartdata", &dispatch<&vm::api_cartdata> },
            { "reload",   &dispatch<&vm::api_reload> },
            { "peek",     &dispatch<&vm::api_peek> },
            { "peek4",    &dispatch<&vm::api_peek4> },
            { "poke",     &dispatch<&vm::api_poke> },
            { "poke4",    &dispatch<&vm::api_poke4> },
            { "memcpy",   &dispatch<&vm::api_memcpy> },
            { "memset",   &dispatch<&vm::api_memset> },
            { "stat",     &dispatch<&vm::api_stat> },
            { "printh",   &vm::api_printh },
            { "extcmd",   &dispatch<&vm::api_extcmd> },

            { "_update_buttons", &dispatch<&vm::api_update_buttons> },
            { "btn",  &dispatch<&vm::api_btn> },
            { "btnp", &dispatch<&vm::api_btnp> },

            { "cursor", &dispatch<&vm::api_cursor> },
            { "print",  &dispatch<&vm::api_print> },
            { "tonum",  &vm::api_tonum },
            { "tostr",  &vm::api_tostr },

            { "max",   &vm::api_max },
            { "min",   &vm::api_min },
            { "mid",   &vm::api_mid },
            { "ceil",  &vm::api_ceil },
            { "flr",   &vm::api_flr },
            { "cos",   &vm::api_cos },
            { "sin",   &vm::api_sin },
            { "atan2", &vm::api_atan2 },
            { "sqrt",  &vm::api_sqrt },
            { "abs",   &vm::api_abs },
            { "sgn",   &vm::api_sgn },
            { "rnd",   &dispatch<&vm::api_rnd> },
            { "srand", &dispatch<&vm::api_srand> },
            { "band",  &vm::api_band },
            { "bor",   &vm::api_bor },
            { "bxor",  &vm::api_bxor },
            { "bnot",  &vm::api_bnot },
            { "shl",   &vm::api_shl },
            { "shr",   &vm::api_shr },
            { "lshr",  &vm::api_lshr },
            { "rotl",  &vm::api_rotl },
            { "rotr",  &vm::api_rotr },

            { "camera",   &dispatch<&vm::api_camera> },
            { "circ",     &dispatch<&vm::api_circ> },
            { "circfill", &dispatch<&vm::api_circfill> },
            { "clip",     &dispatch<&vm::api_clip> },
            { "cls",      &dispatch<&vm::api_cls> },
            { "color",    &dispatch<&vm::api_color> },
            { "fillp",    &dispatch<&vm::api_fillp> },
            { "fget",     &dispatch<&vm::api_fget> },
            { "fset",     &dispatch<&vm::api_fset> },
            { "line",     &dispatch<&vm::api_line> },
            { "map",      &dispatch<&vm::api_map> },
            { "mget",     &dispatch<&vm::api_mget> },
            { "mset",     &dispatch<&vm::api_mset> },
            { "pal",      &dispatch<&vm::api_pal> },
            { "palt",     &dispatch<&vm::api_palt> },
            { "pget",     &dispatch<&vm::api_pget> },
            { "pset",     &dispatch<&vm::api_pset> },
            { "rect",     &dispatch<&vm::api_rect> },
            { "rectfill", &dispatch<&vm::api_rectfill> },
            { "sget",     &dispatch<&vm::api_sget> },
            { "sset",     &dispatch<&vm::api_sset> },
            { "spr",      &dispatch<&vm::api_spr> },
            { "sspr",     &dispatch<&vm::api_sspr> },

            { "music", &dispatch<&vm::api_music> },
            { "sfx",   &dispatch<&vm::api_sfx> },

            { "time", &dispatch<&vm::api_time> },

            { nullptr, nullptr }
        },

        // Methods
        {
            { nullptr, nullptr },
        },

        // Variables
        {
            { nullptr, nullptr, nullptr },
        });

    return &lib;
}

vm* vm::New(lua_State* l, int argc)
{
    // FIXME: I have no idea what this function is for
    UNUSED(l);
    msg::info("requesting new(%d) on vm\n", argc);
    return nullptr;
}

void vm::button(int index, int state)
{
    m_buttons[1][index] = state;
}

void vm::mouse(lol::ivec2 coords, int buttons)
{
    m_mouse.x = (double)coords.x;
    m_mouse.y = (double)coords.y;
    m_mouse.b = (double)buttons;
}

//
// System
//

int vm::api_run(lua_State *l)
{
    // Initialise VM state (TODO: check what else to init)
    ::memset(m_buttons, 0, sizeof(m_buttons));

    // Load cartridge code and call _z8.run() on it
    lua_getglobal(l, "_z8");
    lua_getfield(l, -1, "run");
    luaL_loadstring(l, m_cart.get_lua().C());
    lua_pcall(l, 1, 0, 0);

    return 0;
}

int vm::api_menuitem(lua_State *l)
{
    UNUSED(l);
    msg::info("z8:stub:menuitem\n");
    return 0;
}

int vm::api_cartdata(lua_State *l)
{
    int x = (int)lua_tonumber(l, 1);
    msg::info("z8:stub:cartdata %d\n", x);
    return 0;
}

int vm::api_reload(lua_State *l)
{
    int dst = 0, src = 0, size = OFFSET_CODE;

    // PICO-8 fully reloads the cartridge if not all arguments are passed
    if (!lua_isnone(l, 3))
    {
        dst = (int)lua_tofix32(l, 1) & 0xffff;
        src = (int)lua_tofix32(l, 2) & 0xffff;
        size = (int)lua_tofix32(l, 3);
    }

    if (size <= 0)
        return 0;

    size = size & 0xffff;

    // Attempting to write outside the memory area raises an error. Everything
    // else seems legal, especially reading from anywhere.
    if (dst + size > SIZE_MEMORY)
        return luaL_error(l, "bad memory access");

    // If reading from after the cart, fill with zeroes
    if (src > OFFSET_CODE)
    {
        int amount = lol::min(size, 0x10000 - src);
        ::memset(get_mem(dst), 0, amount);
        dst += amount;
        src = (src + amount) & 0xffff;
        size -= amount;
    }

    // Now copy possibly legal data
    int amount = lol::min(size, OFFSET_CODE - src);
    ::memcpy(get_mem(dst), m_cart.get_rom().data() + src, amount);
    dst += amount;
    size -= amount;

    // If there is anything left to copy, it’s zeroes again
    ::memset(get_mem(dst), 0, size);

    return 0;
}

int vm::api_peek(lua_State *l)
{
    // Note: peek() is the same as peek(0)
    int addr = (int)lua_tofix32(l, 1);
    if (addr < 0 || addr >= SIZE_MEMORY)
        return 0;

    lua_pushnumber(l, m_memory[addr]);
    return 1;
}

int vm::api_peek4(lua_State *l)
{
    int addr = (int)lua_tofix32(l, 1) & 0xffff;
    uint8_t const *p = get_mem(addr);
    int32_t bits = 0;
    for (int i = 0; i < 4; ++i)
    {
        /* This code handles partial reads by adding zeroes */
        if (addr + i < SIZE_MEMORY)
            bits |= p[i] << (8 * i);
        else if (addr + i >= 0x10000)
            bits |= p[i - 0x10000] << (8 * i);
    }

    lua_pushfix32(l, fix32::frombits(bits));
    return 1;
}

int vm::api_poke(lua_State *l)
{
    // Note: poke() is the same as poke(0, 0)
    int addr = (int)lua_tofix32(l, 1);
    int val = (int)lua_tofix32(l, 2);
    if (addr < 0 || addr > SIZE_MEMORY - 1)
        return luaL_error(l, "bad memory access");

    m_memory[addr] = (uint8_t)val;
    return 0;
}

int vm::api_poke4(lua_State *l)
{
    // Note: poke4() is the same as poke(0, 0)
    int addr = (int)lua_tofix32(l, 1);
    if (addr < 0 || addr > SIZE_MEMORY - 4)
        return luaL_error(l, "bad memory access");

    uint32_t x = (uint32_t)lua_tofix32(l, 2).bits();
    uint8_t *p = get_mem(addr);
    p[0] = x;
    p[1] = x >> 8;
    p[2] = x >> 16;
    p[3] = x >> 24;

    return 0;
}

int vm::api_memcpy(lua_State *l)
{
    int dst = (int)lua_tofix32(l, 1);
    int src = (int)lua_tofix32(l, 2) & 0xffff;
    int size = (int)lua_tofix32(l, 3);

    if (size <= 0)
        return 0;

    size = size & 0xffff;

    // Attempting to write outside the memory area raises an error. Everything
    // else seems legal, especially reading from anywhere.
    if (dst < 0 || dst + size > SIZE_MEMORY)
    {
        msg::info("z8:segv:memcpy(0x%x,0x%x,0x%x)\n", src, dst, size);
        return luaL_error(l, "bad memory access");
    }

    // If source is outside main memory, this will be memset(0). But we
    // delay the operation in case the source and the destinations overlap.
    int saved_dst = dst, saved_size = 0;
    if (src > SIZE_MEMORY)
    {
        saved_size = lol::min(size, 0x10000 - src);
        dst += saved_size;
        src = (src + saved_size) & 0xffff;
        size -= saved_size;
    }

    // Now copy possibly legal data
    int amount = lol::min(size, SIZE_MEMORY - src);
    memmove(get_mem(dst), get_mem(src), amount);
    dst += amount;
    size -= amount;

    // Fill possible zeroes we saved before, and if there is still something
    // to copy, it’s zeroes again
    ::memset(get_mem(saved_dst), 0, saved_size);
    ::memset(get_mem(dst), 0, size);

    return 0;
}

int vm::api_memset(lua_State *l)
{
    int dst = (int)lua_tofix32(l, 1);
    int val = (int)lua_tofix32(l, 2) & 0xff;
    int size = (int)lua_tofix32(l, 3);

    if (size <= 0)
        return 0;

    size = size & 0xffff;

    if (dst < 0 || dst + size > SIZE_MEMORY)
    {
        msg::info("z8:segv:memset(0x%x,0x%x,0x%x)\n", dst, val, size);
        return luaL_error(l, "bad memory access");
    }

    ::memset(get_mem(dst), val, size);

    return 0;
}

int vm::api_stat(lua_State *l)
{
    int id = (int)lua_tofix32(l, 1);
    fix32 ret(0.0);

    if (id == 0)
    {
        // Perform a GC to avoid accounting for short lifespan objects.
        // Not sure about the performance cost of this.
        lua_gc(l, LUA_GCCOLLECT, 0);

        // From the PICO-8 documentation:
        // x:0 returns current Lua memory useage (0..1024MB)
        int32_t bits = ((int)lua_gc(l, LUA_GCCOUNT, 0) << 16)
                     + ((int)lua_gc(l, LUA_GCCOUNTB, 0) << 6);
        ret = fix32::frombits(bits);
    }
    else if (id == 1)
    {
        // From the PICO-8 documentation:
        // x:1 returns cpu useage for last frame (1.0 means 100% at 30fps)
        // TODO
    }
    else if (id >= 16 && id <= 19)
    {
        // undocumented parameters for stat(n):
        // 16..19: the sfx currently playing on each channel or -1 for none
        ret = fix32(m_channels[id - 16].m_sfx);
    }
    else if (id >= 20 && id <= 23)
    {
        // undocumented parameters for stat(n):
        // 20..23: the currently playing row number (0..31) or -1 for none
        // TODO
    }
    else if (id >= 32 && id <= 34 && *get_mem(0x5f2d) == 1)
    {
        // undocumented mouse support
        ret = id == 32 ? m_mouse.x
            : id == 33 ? m_mouse.y
            : m_mouse.b;
    }

    lua_pushfix32(l, ret);
    return 1;
}

int vm::api_printh(lua_State *l)
{
    char const *str;
    if (lua_isnoneornil(l, 1))
        str = "false";
    else if (lua_isstring(l, 1))
        str = lua_tostring(l, 1);
    else
        str = lua_toboolean(l, 1) ? "true" : "false";

    fprintf(stdout, "%s\n", str);
    fflush(stdout);

    return 0;
}

int vm::api_extcmd(lua_State *l)
{
    char const *str;
    if (lua_isnoneornil(l, 1))
        str = "false";
    else if (lua_isstring(l, 1))
        str = lua_tostring(l, 1);
    else
        str = lua_toboolean(l, 1) ? "true" : "false";

    if (strcmp("label", str) == 0
         || strcmp("screen", str) == 0
         || strcmp("rec", str) == 0
         || strcmp("video", str) == 0)
        msg::info("z8:stub:extcmd(%s)\n", str);

    return 0;
}

//
// I/O
//

int vm::api_update_buttons(lua_State *l)
{
    // Update button state
    for (int i = 0; i < 64; ++i)
    {
        if (m_buttons[1][i])
            ++m_buttons[0][i];
        else
            m_buttons[0][i] = 0;
    }

    return 0;
}

int vm::api_btn(lua_State *l)
{
    if (lua_isnone(l, 1))
    {
        int bits = 0;
        for (int i = 0; i < 16; ++i)
            bits |= m_buttons[0][i] ? 1 << i : 0;
        lua_pushnumber(l, bits);
    }
    else
    {
        int index = (int)lua_tonumber(l, 1) + 8 * (int)lua_tonumber(l, 2);
        lua_pushboolean(l, m_buttons[0][index]);
    }

    return 1;
}

int vm::api_btnp(lua_State *l)
{
    auto was_pressed = [](int i)
    {
        // “Same as btn() but only true when the button was not pressed the last frame”
        if (i == 1)
            return true;
        // “btnp() also returns true every 4 frames after the button is held for 15 frames.”
        if (i > 15 && i % 4 == 0)
            return true;
        return false;
    };

    if (lua_isnone(l, 1))
    {
        int bits = 0;
        for (int i = 0; i < 16; ++i)
            bits |= was_pressed(m_buttons[0][i]) ? 1 << i : 0;
        lua_pushnumber(l, bits);
    }
    else
    {
        int index = (int)lua_tonumber(l, 1) + 8 * (int)lua_tonumber(l, 2);
        lua_pushboolean(l, was_pressed(m_buttons[0][index]));
    }

    return 1;
}

//
// Deprecated
//

int vm::api_time(lua_State *l)
{
    float time = lol::fmod(m_timer.Poll(), 65536.0f);
    lua_pushnumber(l, time < 32768.f ? time : time - 65536.0f);
    return 1;
}

} // namespace z8

