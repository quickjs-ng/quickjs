#include <node_api.h>
#include "../common.h"

napi_value CreateObject(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  napi_value obj;
  NAPI_CALL(env, napi_create_object(env, &obj));

  NAPI_CALL(env, napi_set_named_property(env, obj, "msg", args[0]));

  return obj;
}

napi_value Init(napi_env env, napi_value exports) {
  NAPI_CALL(env,
      napi_create_function(env, "exports", -1, CreateObject, NULL, &exports));
  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
