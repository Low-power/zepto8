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

#pragma once

#include <lol/engine.h>

#include <vector>

#include "analyzer.h"
#include "pico8/memory.h"

// The cart class
// ——————————————
// Represents a PICO-8 cartridge. Can load and unpack .p8 and .p8.png files,
// so that the VM can then load their content into memory.

namespace z8::pico8
{

class cart
{
public:
    cart()
    {}

    bool load(std::string const &filename);

    memory const &get_rom() const
    {
        return m_rom;
    }

    memory &get_rom()
    {
        return m_rom;
    }

    std::vector<uint8_t> &get_label()
    {
        return m_label;
    }

    std::string const &get_code() const
    {
        return m_code;
    }

    std::string const &get_lua()
    {
        if (m_lua.length() == 0)
            m_lua = analyzer().fix(m_code);
        return m_lua;
    }

    std::vector<uint8_t> get_compressed_code() const;
    std::vector<uint8_t> get_bin() const;
    std::string get_p8() const;
    lol::image get_png() const;

private:
    bool load_png(std::string const &filename);
    bool load_p8(std::string const &filename);

    memory m_rom;
    std::vector<uint8_t> m_label;
    std::string m_code, m_lua;
    int m_version;
};

} // namespace z8::pico8

