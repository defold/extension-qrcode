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

uint8_t* g_DebugPixels = 0;
uint32_t g_DebugWidth = 0;
uint32_t g_DebugHeight = 0;


static int Scan(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);

    dmScript::LuaHBuffer* buffer = dmScript::CheckBuffer(L, 1);
    int width = luaL_checkint(L, 2);
    int height = luaL_checkint(L, 3);
    int flip = luaL_checkint(L, 4);

    struct quirc *qr;

    qr = quirc_new();
    if (!qr) {
        return luaL_error(L, "qrcode.scan: Failed to allocate memory");
    }

    uint8_t* filedata = 0;
    // if( dmScript::IsBuffer(L, 4) )
    // {
    //     dmBuffer::HBuffer* buffer2 = dmScript::CheckBuffer(L, 4);

    //     uint8_t* file;
    //     uint32_t filesize;
    //     dmBuffer::GetBytes(*buffer2, (void**)&file, &filesize);

    //     int x,y,n;
    //     //filedata = Xstbi_load("/Users/mathiaswesterdahl/work/external/quirc/defold.png", &x, &y, &n, 0);
    //     filedata = Xstbi_load_from_memory(file, filesize, &x, &y, &n, 0);
    //     if( filedata )
    //     {
    //         width = x;
    //         height = y;
    //     }
    //     printf("filedata: %p  %d %d  %d\n", filedata, width, height, n);
    // }


    if (quirc_resize(qr, width, height) < 0) {
        return luaL_error(L, "qrcode.scan: Failed to allocate video memory");
    }

    uint8_t* image = quirc_begin(qr, &width, &height);

    if( filedata )
    {
        for( int y = 0; y < height; ++y )
        {
            for( int x = 0; x < width; ++x )
            {
                image[y * width + x] = filedata[y * width * 3 + x * 3];
            }
        }

        free(filedata);
    }
    else
    {
        uint8_t* data;
        uint32_t datasize;
        dmBuffer::GetBytes(buffer->m_Buffer, (void**)&data, &datasize);

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
                //image[y * width + x] = data[y * width * 3 + x * 3];
            }
        }

    }

    if( g_DebugPixels != 0 )
    {
        free(g_DebugPixels);
    }
    g_DebugWidth = width;
    g_DebugHeight = height;
    g_DebugPixels = (uint8_t*)malloc( width * height * 3 );
    for( int i = 0; i < width * height; ++i )
    {
        g_DebugPixels[i*3 + 0] = image[i];
        g_DebugPixels[i*3 + 1] = image[i];
        g_DebugPixels[i*3 + 2] = image[i];
    }

    quirc_end(qr);


//stbi_write_png("/Users/mathiaswesterdahl/work/external/quirc/test.png", width, height, 1, image, width);
//printf("Wrote: /Users/mathiaswesterdahl/work/external/quirc/test.png\n");

    int num_codes = quirc_count(qr);
    for( int i = 0; i < num_codes; i++)
    {
        struct quirc_code code;
        struct quirc_data data;

        quirc_extract(qr, i, &code);

        quirc_decode_error_t err = quirc_decode(&code, &data);
        if (err)
        {
            printf("qrcode.scan: quirc decode error: %s\n", quirc_strerror(err));
            num_codes = 0;
            //return luaL_error(L, "qrcode.scan: quirc decode error: %s\n", quirc_strerror(err));
        }
        else
        {
            printf("Data: %s\n", data.payload);
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


static int GetDebugImage(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);

    const dmBuffer::StreamDeclaration streams_decl[] = {
        {dmHashString64("rgb"), dmBuffer::VALUE_TYPE_UINT8, 3},
    };

    dmBuffer::HBuffer buffer;
    dmBuffer::Result r = dmBuffer::Create(g_DebugWidth*g_DebugHeight, streams_decl, 1, &buffer);
    if (r != dmBuffer::RESULT_OK)
    {
        lua_pushnil(L);
        return 1;
    }

    uint8_t* data;
    uint32_t datasize;
    dmBuffer::GetBytes(buffer, (void**)&data, &datasize);

    memcpy(data, g_DebugPixels, datasize);

    dmScript::LuaHBuffer luabuf = { buffer, true };
    PushBuffer(L, luabuf);

    return 1;
}

static const luaL_reg Module_methods[] =
{
    {"scan", Scan},
    {"get_debug_image", GetDebugImage},
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
