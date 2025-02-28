#include "rlua.h"
#include <new>
#include <limits>
#include "rdebug_redirect.h"

lua_State* get_host(rlua_State *L);
rlua_State* get_client(lua_State *L);
int  event(rlua_State* cL, lua_State* hL, const char* name);

static int redirect_read(rlua_State* L) {
    remotedebug::redirect& self = *(remotedebug::redirect*)rluaL_checkudata(L, 1, "redirect");
    lua_Integer len = rluaL_optinteger(L, 2, LUAL_BUFFERSIZE);
    if (len > (std::numeric_limits<int>::max)()) {
        return rluaL_error(L, "bad argument #1 to 'read' (invalid number)");
    }
    if (len <= 0) {
        return 0;
    }
    rluaL_Buffer b;
    rluaL_buffinit(L, &b);
    char* buf = rluaL_prepbuffsize(&b, (size_t)len);
    size_t rc = self.read(buf, (size_t)len);
    if (rc == 0) {
        return 0;
    }
    rluaL_pushresultsize(&b, rc);
    return 1;
}

static int redirect_peek(rlua_State* L) {
#if defined(_WIN32)
    remotedebug::redirect& self = *(remotedebug::redirect*)rluaL_checkudata(L, 1, "redirect");
    rlua_pushinteger(L, self.peek());
#else
    rlua_pushinteger(L, LUAL_BUFFERSIZE);
#endif
    return 1;
}

static int redirect_close(rlua_State* L) {
    remotedebug::redirect& self = *(remotedebug::redirect*)rluaL_checkudata(L, 1, "redirect");
    self.close();
    return 0;
}

static int redirect_gc(rlua_State* L) {
    remotedebug::redirect& self = *(remotedebug::redirect*)rluaL_checkudata(L, 1, "redirect");
    self.close();
    self.~redirect();
    return 0;
}

static int redirect(rlua_State* L) {
    const char* lst[] = {"stdin", "stdout", "stderr"};
    remotedebug::std_fd type = (remotedebug::std_fd)(rluaL_checkoption(L, 1, "stdout", lst));
    switch (type) {
    case remotedebug::std_fd::STDIN:
    case remotedebug::std_fd::STDOUT:
    case remotedebug::std_fd::STDERR:
        break;
    default:
        return 0;
    }
    remotedebug::redirect* r = (remotedebug::redirect*)rlua_newuserdata(L, sizeof(remotedebug::redirect));
    new (r) remotedebug::redirect;
    if (!r->open(type)) {
        return 0;
    }
    if (rluaL_newmetatable(L, "redirect")) {
        static rluaL_Reg mt[] = {
            { "read", redirect_read },
            { "peek", redirect_peek },
            { "close", redirect_close },
            { "__gc", redirect_gc },
            { NULL, NULL }
        };
        rluaL_setfuncs(L, mt, 0);
        rlua_pushvalue(L, -1);
        rlua_setfield(L, -2, "__index");
    }
    rlua_setmetatable(L, -2);
    return 1;
}

static int callfunc(lua_State* L) {
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_insert(L, 1);
    lua_call(L, lua_gettop(L) - 1, LUA_MULTRET);
    return lua_gettop(L);
}

static int redirect_print(lua_State* L) {
	rlua_State *cL = get_client(L);
    if (cL) {
        lua_pushnil(L);
        lua_insert(L, 1);
	    int ok = event(cL, L, "print");
        if (ok > 0) {
            return 0;
        }
        lua_remove(L, 1);
    }
    return callfunc(L);
}

static int redirect_f_write(lua_State* L) {
	rlua_State *cL = get_client(L);
    if (cL) {
        if (LUA_TUSERDATA == lua_getfield(L, LUA_REGISTRYINDEX, "_IO_output") && lua_rawequal(L, -1, 1)) {
            lua_pop(L, 1);
            int ok = event(cL, L, "iowrite");
            if (ok > 0) {
                lua_settop(L, 1);
                return 1;
            }
        }
        else {
            lua_pop(L, 1);
        }
    }
    return callfunc(L);
}

static int redirect_io_write(lua_State* L) {
	rlua_State *cL = get_client(L);
    if (cL) {
        lua_pushnil(L);
        lua_insert(L, 1);
	    int ok = event(cL, L, "iowrite");
        if (ok > 0) {
            lua_getfield(L, LUA_REGISTRYINDEX, "_IO_output");
            return 1;
        }
        lua_remove(L, 1);
    }
    return callfunc(L);
}

static int open_print(rlua_State* L) {
    bool enable = rlua_toboolean(L, 1);
    lua_State* hL = get_host(L);
    lua_getglobal(hL, "print");
    enable
        ? lua_pushcclosure(hL, redirect_print, 1)
        : (lua_getupvalue(hL, -1, 1)? lua_remove(hL, -2):(void)0)
        ;
    lua_setglobal(hL, "print");
    return 0;
}

static int open_iowrite(rlua_State* L) {
    bool enable = rlua_toboolean(L, 1);
    lua_State* hL = get_host(L);
    if (LUA_TUSERDATA == lua_getfield(hL, LUA_REGISTRYINDEX, "_IO_output")) {
        if (lua_getmetatable(hL, -1)) {
            lua_pushstring(hL, "write");
            lua_pushvalue(hL, -1);
            lua_rawget(hL, -3);
            enable
                ? lua_pushcclosure(hL, redirect_f_write, 1)
                : (lua_getupvalue(hL, -1, 1)? lua_remove(hL, -2):(void)0)
                ;
            lua_rawset(hL, -3);
            lua_pop(hL, 1);
        }
    }
    lua_pop(hL, 1);
    if (LUA_TTABLE == lua_getglobal(hL, "io")) {
        lua_pushstring(hL, "write");
        lua_pushvalue(hL, -1);
        lua_rawget(hL, -3);
        enable
            ? lua_pushcclosure(hL, redirect_io_write, 1)
            : (lua_getupvalue(hL, -1, 1)? lua_remove(hL, -2):(void)0)
            ;
        lua_rawset(hL, -3);
    }
    lua_pop(hL, 1);
    return 0;
}

extern "C" 
#if defined(_WIN32)
__declspec(dllexport)
#endif
int luaopen_remotedebug_stdio(rlua_State* L) {
    rlua_newtable(L);
    static rluaL_Reg lib[] = {
        { "redirect", redirect },
        { "open_print", open_print },
        { "open_iowrite", open_iowrite },
        { NULL, NULL },
    };
    rluaL_setfuncs(L, lib, 0);
    return 1;
}
