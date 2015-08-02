#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include <pthread.h>

#include <rte_launch.h>

#include "task.h"

#define MG_LUA_PATH "[[\
lua/include/?.lua;\
lua/include/?/init.lua;\
lua/include/lib/?.lua;\
lua/include/lib/?/init.lua;\
../lua/include/?.lua;\
../lua/include/?/init.lua;\
../lua/include/lib/?.lua;\
../lua/include/lib/?/init.lua;\
../../lua/include/?.lua;\
../../lua/include/?/init.lua;\
../../lua/include/lib/?.lua;\
../../lua/include/lib/?/init.lua;\
]]"

lua_State* launch_lua() {
	lua_State* L = luaL_newstate();
	luaL_openlibs(L);
	(void) luaL_dostring(L, "package.path = package.path .. ';' .. " MG_LUA_PATH);
	if (luaL_dostring(L, "require 'main'")) {
		printf("Could not run main script: %s\n", lua_tostring(L, -1));
	}
	return L;
}

int lua_core_main(void* arg) {
	int rc = -1;
	struct lua_core_config* cfg = (struct lua_core_config*) arg;
	lua_State* L = launch_lua();
	if (!L) {
		goto error;
	}
	lua_getglobal(L, "main");
	lua_pushstring(L, "slave");
	lua_pushnumber(L, cfg->task_id);
	lua_pushstring(L, cfg->userscript);
	lua_pushstring(L, cfg->args);
	if (lua_pcall(L, 4, 0, 0)) {
		printf("Lua error: %s\n", lua_tostring(L, -1));
		goto error;
	}
	rc = 0;
error:
	free(cfg->args);
	free(cfg->userscript);
	free(cfg);
	if (L) lua_close(L);
	return rc;
}

void * lua_core_main_void(void* arg) {
  printf("void main\n");
	lua_State* L = launch_lua();
	if (!L) {
		goto error;
	}
	lua_getglobal(L, "main");
	lua_pushstring(L, "test");
	lua_pushnumber(L, 0);
	lua_pushstring(L, "lala");
	lua_pushstring(L, "aaa");
	if (lua_pcall(L, 4, 0, 0)) {
		printf("Lua error: %s\n", lua_tostring(L, -1));
		goto error;
	}
error:
	if (L) lua_close(L);
	return NULL;
  ////lua_core_main(arg);
  //while(1){
  //  uint64_t a = 0;
  //  for(a=0;a<99999999;a++){
  //    asm("nop");
  //  }
  //  printf(">>>>>>>> lala\n");
  //}
  //return NULL;
}

void launch_lua_core(int core, uint64_t task_id, char* userscript, char* args) {
	struct lua_core_config* cfg = (struct lua_core_config*) malloc(sizeof(struct lua_core_config));
	cfg->task_id = task_id;
	// copy the strings as they might be freed immediately after the call by the caller
	cfg->args = (char*) malloc(strlen(args) + 1);
	cfg->userscript = (char*) malloc(strlen(userscript) + 1);
	strcpy(cfg->args, args);
	strcpy(cfg->userscript, userscript);
	rte_eal_remote_launch(&lua_core_main, cfg, core);
}

void launch_lua_thread(uint64_t task_id, char* userscript, char* args){
  pthread_t thread;

  printf("here\n");
  printf("script: %s\n", userscript);
  printf("args: %s\n", args);
	struct lua_core_config* cfg = (struct lua_core_config*) malloc(sizeof(struct lua_core_config));
	cfg->task_id = task_id;
	// copy the strings as they might be freed immediately after the call by the caller
	cfg->args = (char*) malloc(strlen(args) + 1);
	cfg->userscript = (char*) malloc(strlen(userscript) + 1);
	strcpy(cfg->args, args);
	strcpy(cfg->userscript, userscript);

  pthread_create(&thread, NULL, &lua_core_main_void, &cfg);

  // TODO: return thread, to be able to join on thread
}
