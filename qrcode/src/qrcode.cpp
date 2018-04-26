#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define EXTENSION_NAME QRCode
#define LIB_NAME "QRCode"
#define MODULE_NAME "qrcode"

// Defold SDK
#define DLIB_LOG_DOMAIN LIB_NAME
#include <dmsdk/sdk.h>

#include "quirc/quirc.h"

// #define STB_IMAGE_IMPLEMENTATION
// #include "stb_image.h"
// #define STB_IMAGE_WRITE_IMPLEMENTATION
// #include "stb_image_write.h"


// GENERATE
// https://github.com/nayuki/QR-Code-generator

struct QRCodeContext
{
    int width;
    int height;
};

QRCodeContext g_QRContext;

static int Scan(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);

    dmScript::LuaHBuffer* buffer = dmScript::CheckBuffer(L, 1);
    int width = luaL_checkint(L, 2);
    int height = luaL_checkint(L, 3);
    int flip = luaL_checkint(L, 4);

    struct quirc* qr;

    qr = quirc_new();
    if (!qr) {
        return luaL_error(L, "qrcode.scan: Failed to allocate memory");
    }

    if (quirc_resize(qr, width, height) < 0) {
        return luaL_error(L, "qrcode.scan: Failed to allocate video memory");
    }

    uint8_t* image = quirc_begin(qr, &width, &height);

    uint8_t* data;
    uint32_t datasize;
    dmBuffer::GetBytes(buffer->m_Buffer, (void**)&data, &datasize);

    // Make it grey scale
    for( int y = 0; y < height; ++y )
    {
        for( int x = 0; x < width; ++x )
        {
            float r = (float)data[y * width * 3 + x * 3 + 0] / 255.0f;
            float g = (float)data[y * width * 3 + x * 3 + 1] / 255.0f;
            float b = (float)data[y * width * 3 + x * 3 + 2] / 255.0f;

            float value = (r+g+b)/3.0f;

            value = value*value;
            value += 0.1f;
            if(value>1.0f)
            {
                value = 1.0f;
            }

            if( flip == 0 )
                image[y * width + x] = (uint8_t)(value * 255.0f);
            else
                image[y * width + (width - x - 1)] = (uint8_t)(value * 255.0f);
        }
    }
    
    quirc_end(qr);

    int num_codes = quirc_count(qr);
    for( int i = 0; i < num_codes; i++)
    {
        struct quirc_code code;
        struct quirc_data data;

        quirc_extract(qr, i, &code);

        quirc_decode_error_t err = quirc_decode(&code, &data);
        if (err)
        {
            num_codes = 0;
        }
        else
        {
            lua_pushstring(L, (const char*)data.payload);
        }
        break;
    }

    quirc_destroy(qr);

    if( num_codes == 0 )
    {
        lua_pushnil(L);
    }

    return 1;
}

static int Generate(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);

    dmScript::LuaHBuffer* buffer = dmScript::CheckBuffer(L, 1);
    int width = luaL_checkint(L, 2);
    int height = luaL_checkint(L, 3);
    int flip = luaL_checkint(L, 4);
}

static const luaL_reg Module_methods[] =
{
    {"scan", Scan},
    {"generate", Generate},
    {0, 0}
};

static void LuaInit(lua_State* L)
{
    int top = lua_gettop(L);
    luaL_register(L, MODULE_NAME, Module_methods);

#define SETCONSTANT(name) \
        lua_pushnumber(L, (lua_Number) name); \
        lua_setfield(L, -2, #name);\

#undef SETCONSTANT

    lua_pop(L, 1);
    assert(top == lua_gettop(L));
}

dmExtension::Result AppInitializeQRCode(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

dmExtension::Result InitializeQRCode(dmExtension::Params* params)
{
    LuaInit(params->m_L);
    return dmExtension::RESULT_OK;
}

dmExtension::Result AppFinalizeQRCode(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

dmExtension::Result FinalizeQRCode(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}


DM_DECLARE_EXTENSION(EXTENSION_NAME, LIB_NAME, AppInitializeQRCode, AppFinalizeQRCode, InitializeQRCode, 0, 0, FinalizeQRCode)
