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

#define JC_QRENCODE_IMPLEMENTATION
#include "jc_qrencode.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

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

static int save_image(JCQRCode* qr, const char* path)
{
    int32_t size = qr->size;

    int32_t border = 0;
    int32_t scale = 1;
    int32_t newsize = scale*(size + 2 * border);
    uint8_t* large = (uint8_t*)malloc( newsize * newsize );

    memset(large, 255, newsize*newsize);

    for( int y = 0; y < size*scale; ++y )
    {
        for( int x = 0; x < size*scale; ++x )
        {
            uint8_t module = qr->data[(y/scale)*256 + (x/scale)];
            large[(y + scale*border) * newsize + x + scale*border] = module;
        }
    }

    int result = stbi_write_png(path, newsize, newsize, 1, large, newsize);
    free(large);

    if(result)
        printf("Wrote to %s\n", path);
    else
        printf("Failed to write to %s\n", path);
    return result;
}

static dmBuffer::HBuffer GenerateImage(JCQRCode* qr, uint32_t* outsize)
{
    int32_t size = qr->size;
    int32_t border = 2;
    int32_t scale = 8;
    int32_t newsize = scale*(size + 2 * border);

    dmBuffer::StreamDeclaration streams_decl[] = {
        {dmHashString64("data"), dmBuffer::VALUE_TYPE_UINT8, 1}
    };

    dmBuffer::HBuffer buffer = 0;
    dmBuffer::Result result = dmBuffer::Create(newsize * newsize, streams_decl, 1, &buffer);
    if (result != dmBuffer::RESULT_OK )
    {
        return 0;
    }

    uint8_t* data;
    uint32_t datasize;
    dmBuffer::GetBytes(buffer, (void**)&data, &datasize);

    memset(data, 255, newsize*newsize);

    for( int y = 0; y < size*scale; ++y )
    {
        for( int x = 0; x < size*scale; ++x )
        {
            uint8_t module = qr->data[(y/scale)*256 + (x/scale)];
            data[(y + scale*border) * newsize + x + scale*border] = module;
        }
    }

    if (outsize)
        *outsize = newsize;
    return buffer;
}

static int Generate(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 2);

    const char* text = luaL_checkstring(L, 1);

    JCQRCode* qr = jc_qrencode((const uint8_t*)text, (uint32_t)strlen(text));
    if( !qr )
    {
        return DM_LUA_ERROR("Failed to encode text: '%s'\n", text);
    }

    uint32_t outsize = 0;
    dmBuffer::HBuffer buffer = GenerateImage(qr, &outsize);

    free(qr); // free the qr code

    // Transfer ownership to Lua
    dmScript::LuaHBuffer luabuffer = {buffer, true};
    dmScript::PushBuffer(L, luabuffer);
    lua_pushinteger(L, outsize);
    return 2;
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
